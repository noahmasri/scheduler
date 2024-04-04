// can reduce my priority
#include <inc/lib.h>

void forktree(const char *cur);

#define DEPTH 3

void
forkchild(const char *cur, char branch)
{
	char nxt[DEPTH + 1];

	if (strlen(cur) >= DEPTH)
		return;

	snprintf(nxt, DEPTH + 1, "%s%c", cur, branch);
	if (fork() == 0) {
		forktree(nxt);
	}
}

void
forktree(const char *cur)
{
	cprintf("i am environment %08x in tree", sys_getenvid());
	cprintf("with priority: %08x\n", sys_check_priority(sys_getenvid()));

	forkchild(cur, '1');
}

void
umain(int argc, char **argv)
{
	int id = sys_getenvid();
	cprintf("i am environment %08x\n", id);
	cprintf("my priority is %08x\n", sys_check_priority(id));
	sys_reduce_priority(2);
	cprintf("my priority is now %08x\n", sys_check_priority(id));
	forktree("");
	sys_yield();
	cprintf("i am environment %08x ", sys_getenvid());
	cprintf("exiting with priority %08x\n",
	        sys_check_priority(sys_getenvid()));
}
