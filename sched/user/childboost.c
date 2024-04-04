// priority check
#include <inc/lib.h>

void
umain(int argc, char **argv)
{
	int id = sys_getenvid();
	cprintf("i am environment %08x\n", id);
	cprintf("my priority is %08x\n", sys_check_priority(id));
	while (sys_check_priority(id) > 2) {
		int i = 3 + 7;  // wastes cpu
	}

	cprintf("i am still environment %08x\n", thisenv->env_id);
	cprintf("my priority now is %08x\n", sys_check_priority(id));

	envid_t child_id = fork();
	if (child_id == 0) {
		int child_id = sys_getenvid();
		cprintf("i am child process environment %08x\n", child_id);
		cprintf("my priority is %08x\n", sys_check_priority(child_id));
		int i = 1;
		do {
			i++;
		} while (sys_check_priority(id) >= 3);
		cprintf("i am child process environment %08x ", child_id);
		cprintf("and i ran in loop %08x times\n ", i);
	} else {
		sys_yield();
		cprintf("i am father environment %08x,", id);
		cprintf(" my priority is %08x and now i will increase child "
		        "priority\n",
		        sys_check_priority(id));
		sys_set_priority(child_id, 4);
	}

	cprintf("i am environment %08x ", sys_getenvid());
	cprintf("exiting with priority %08x", sys_check_priority(sys_getenvid()));
}
