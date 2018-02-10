#include "stdlib.h"
#include <sys/defs.h>
#include "envio.h"
#include "stdio.h"

void _start(void) {
	uint64_t *rsp = 0;
	int argc;
	char **argv = NULL;
	char **envp = NULL;
	__asm__ __volatile__("movq %%rsp, %0;" : "=a" (rsp)	 : :  "cc","memory");

	uint64_t heapAddress1;
	heapAddress1 = *(uint64_t*)((char **)rsp + 0x1);
	argc = (int)(heapAddress1);

	uint64_t heapAddress2;
	heapAddress2 = *(uint64_t*)((char **)rsp + 0x2);
	argv = (char**)(heapAddress2);

	uint64_t heapAddress3;
	heapAddress3 = *(uint64_t*)((char **)rsp + 0x3);
	envp = (char**)(heapAddress3);

	int ret_value;
	ret_value = main(argc, argv, envp);
	exit(ret_value);
}
