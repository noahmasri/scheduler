#include <inc/assert.h>
#include <inc/x86.h>
#include <kern/spinlock.h>
#include <kern/env.h>
#include <kern/pmap.h>
#include <kern/monitor.h>

void sched_halt(void);
int lapicr(int index);

struct sched_stats sched_stats = { 0 };

#ifdef ROUND_ROBIN
struct Env *
round_robin()
{
	struct Env *idle = NULL;
	if (!curenv) {
		int i = 0;
		while (i < NENV && envs[i].env_status != ENV_RUNNABLE) {
			i++;
		}
		if (envs[i].env_status == ENV_RUNNABLE)
			idle = &envs[i];
	} else {
		int i = ENVX(curenv->env_id) + 1;
		while (curenv != &envs[i]) {
			if (i >= NENV) {
				i = 0;
			} else {
				if (envs[i].env_status == ENV_RUNNABLE) {
					idle = &envs[i];
					break;
				}
				i++;
			}
		}
	}
	return idle;
}
#endif

#define TICR (0x0380 / 4)  // Timer Initial Count
#define TCCR (0x0390 / 4)  // Timer Current Count
#ifdef MLFQ
#define CANT_COLAS 5
#define S 50

struct Env *
search_runnable_in_queue(struct Env *first)
{
	struct Env *curr = first;
	do {
		if (curr->env_status == ENV_RUNNABLE)
			return curr;
		if (curr->env_queue != first->env_queue ||
		    curr->env_queue != curr->next_in_q->env_queue)
			return NULL;
		curr = curr->next_in_q;
	} while (curr->env_id != first->env_id);

	return NULL;
}

void
update_priorities()
{
	struct Env *curr = colas[CANT_COLAS - 1].first;
	if (curr) {
		struct Env *first = curr;
		do {
			curr->runtime = 0;
			curr = curr->next_in_q;
		} while (curr->env_id != first->env_id);
	}

	for (int i = CANT_COLAS - 2; i >= 0; i--) {
		while (colas[i].first) {
			struct Env *e = colas[i].first;
			delete_from_queue(e);
			e->env_queue = CANT_COLAS - 1;
			update_queue(e);
			e->runtime = 0;
		}
	}
}

void
reduce_env_priority(struct Env *env)
{
	if (env->env_queue == 0)
		return;

	delete_from_queue(env);
	env->env_queue--;
	update_queue(env);
	env->runtime = 0;
}

struct Env *
search_queues_from_n_to_m(int n, int m)
{
	struct Env *idle;
	for (int i = n; i > m; i--) {
		if (colas[i].first) {
			idle = search_runnable_in_queue(colas[i].first);
			if (idle)
				return idle;
		}
	}
	return NULL;
}

struct Env *
search_from_top_to_bottom_q(struct Env *next_in_q, int prior_queue)
{
	struct Env *idle;
	idle = search_queues_from_n_to_m(CANT_COLAS - 1, prior_queue);
	if (!idle) {  // si no habia nada en las mas altas
		idle = search_runnable_in_queue(
		        next_in_q);  // busco en la del actual
		if (!idle) {
			if (curenv->env_status == ENV_RUNNING)
				idle = curenv;
			else
				idle = search_queues_from_n_to_m(
				        prior_queue - 1,
				        -1);  // busco en las mas bajas que yo
		}
	}
	return idle;
}

int
max_runtime_in_queue(size_t n)
{
	int queue_multiplier = (CANT_COLAS - n);
	int tic = lapicr(TICR);
	return 3 * tic * queue_multiplier;
}

struct Env *
mlfq()
{
	struct Env *idle = NULL;
	if (!curenv) {
		return search_queues_from_n_to_m(CANT_COLAS - 1, -1);
	}

	int runtime_in_queue = curenv->runtime;
	size_t prior_queue = curenv->env_queue;
	struct Env *next_in_q = curenv->next_in_q;
	if (curenv->runtime > max_runtime_in_queue(curenv->env_queue)) {
		reduce_env_priority(curenv);
	}
	return search_from_top_to_bottom_q(next_in_q, prior_queue);
}
#endif

void
update_sched_stats(struct Env *env_to_run)
{
	if (sched_stats.sched_calls >= 4096) {
		sched_stats.sched_calls++;
		return;
	}
	sched_stats.envs_runs[sched_stats.sched_calls].envid = env_to_run->env_id;
#ifdef MLFQ
	sched_stats.envs_runs[sched_stats.sched_calls].priority =
	        env_to_run->env_queue;
#endif
	sched_stats.sched_calls++;
}

// Choose a user environment to run and run it.
void
sched_yield(void)
{
	struct Env *idle = NULL;

	// Implement simple round-robin scheduling.
	//
	// Search through 'envs' for an ENV_RUNNABLE environment in
	// circular fashion starting just after the env this CPU was
	// last running.  Switch to the first such environment found.
	//
	// If no envs are runnable, but the environment previously
	// running on this CPU is still ENV_RUNNING, it's okay to
	// choose that environment.
	//
	// Never choose an environment that's currently running on
	// another CPU (env_status == ENV_RUNNING). If there are
	// no runnable environments, simply drop through to the code
	// below to halt the cpu.

	// Your code here
	// Wihtout scheduler, keep runing the last environment while it exists
	int curr_runtime = lapicr(TICR) - lapicr(TCCR);
	sched_stats.total_runtime += curr_runtime;
#ifdef ROUND_ROBIN
	idle = round_robin();
#endif
#ifdef MLFQ
	if (sched_stats.total_runtime >=
	    S * lapicr(TICR) * (sched_stats.amount_boosts + 1)) {
		sched_stats.amount_boosts++;
		update_priorities();
	}
	if (curenv)
		curenv->runtime += curr_runtime;
	idle = mlfq();
#endif

	if (idle) {
		update_sched_stats(idle);
		env_run(idle);
	} else if (curenv && curenv->env_status == ENV_RUNNING &&
	           curenv->env_cpunum == cpunum()) {
		update_sched_stats(curenv);
		env_run(curenv);
	}

	// sched_halt never returns
	sched_halt();
}

void
print_stats()
{
	cprintf("\n--SCHEDULER STATS--\n");
	cprintf("total time it took processes to finish 0x%08x ",
	        sched_stats.total_runtime);
	cprintf("and scheduler was called 0x%08x times. \n",
	        sched_stats.sched_calls);
#ifdef MLFQ
	cprintf("priorities were reset 0x%08x times. \n",
	        sched_stats.amount_boosts);
#endif

	cprintf("environments run history (ordered by timestamp):\n");
	int runs = sched_stats.sched_calls;
	if (runs > 4096) {
		cprintf("---showing only first 4096---\n");
		runs = 4096;
	}
	for (int i = 0; i < runs; i++) {
		cprintf("   -environment %08x ", sched_stats.envs_runs[i].envid);
#ifdef MLFQ
		cprintf("with priority %08x", sched_stats.envs_runs[i].priority);
#endif
		cprintf("\n");
	}

	cprintf("\nenvironments turnarounds:\n");
	for (int i = 0; i < sched_stats.dead_environments; i++) {
		cprintf("   -environment %08x",
		        sched_stats.env_turnaround[i].envid);
		cprintf(" had a turnaround of 0x%08x (in clock ticks)\n",
		        sched_stats.env_turnaround[i].time);
		cprintf(" and ran 0x%08x times\n",
		        sched_stats.env_turnaround[i].runs);
	}
}

// Halt this CPU when there is nothing to do. Wait until the
// timer interrupt wakes it up. This function never returns.
//
void
sched_halt(void)
{
	int i;

	// For debugging and testing purposes, if there are no runnable
	// environments in the system, then drop into the kernel monitor.
	for (i = 0; i < NENV; i++) {
		if ((envs[i].env_status == ENV_RUNNABLE ||
		     envs[i].env_status == ENV_RUNNING ||
		     envs[i].env_status == ENV_DYING)) {
			break;
		}
	}
	if (i == NENV) {
		cprintf("No runnable environments in the system!\n");
		print_stats();
		while (1)
			monitor(NULL);
	}


	// Mark that no environment is running on this CPU
	curenv = NULL;
	lcr3(PADDR(kern_pgdir));

	// Mark that this CPU is in the HALT state, so that when
	// timer interupts come in, we know we should re-acquire the
	// big kernel lock
	xchg(&thiscpu->cpu_status, CPU_HALTED);

	// Release the big kernel lock as if we were "leaving" the kernel
	unlock_kernel();

	// Once the scheduler has finishied it's work, print statistics on
	// performance. Your code here

	// Reset stack pointer, enable interrupts and then halt.
	asm volatile("movl $0, %%ebp\n"
	             "movl %0, %%esp\n"
	             "pushl $0\n"
	             "pushl $0\n"
	             "sti\n"
	             "1:\n"
	             "hlt\n"
	             "jmp 1b\n"
	             :
	             : "a"(thiscpu->cpu_ts.ts_esp0));
}
