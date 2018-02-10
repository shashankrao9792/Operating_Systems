#include "sys/defs.h"
#include "user_syscall.h"
#include "string.h"
#include "stdio.h"
#include "unistd.h"
#include "util.h"
#include "stdlib.h"

int  main(int argc, char *argv[], char *envp[])
{	
	while(1){
		printf("Hello");
		yield();
	}
}
