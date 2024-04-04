// after a certain time all process return to highest queue
#include <inc/lib.h>

void
waste_cpu_till_boost()
{
	while (sys_check_priority(sys_getenvid()) != 4) {
		int i = 3 + 7;
	}
}

void
umain(int argc, char **argv)
{
	int id = sys_getenvid();
	cprintf("i am environment %08x\n", id);
	cprintf("my priority is %08x\n", sys_check_priority(id));
	cprintf("my priority is now %08x\n", sys_check_priority(id));
	envid_t child = fork();
	if (child == 0) {
		sys_reduce_priority(3);
		waste_cpu_till_boost();
	} else {
		sys_reduce_priority(2);
		cprintf("my priority is now %08x\n",
		        sys_check_priority(sys_getenvid()));
		waste_cpu_till_boost();
	}
	cprintf("i am environment %08x ", sys_getenvid());
	cprintf("exiting with priority %08x\n",
	        sys_check_priority(sys_getenvid()));
}
