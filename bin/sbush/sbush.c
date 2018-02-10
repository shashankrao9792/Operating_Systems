#include "sys/defs.h"
#include "string.h"
#include "stdio.h"
#include "unistd.h"
#include "util.h"
#include "stdlib.h"


void welcome_msg() {
	printf("\n\n ****************** Welcome to SBUnix Operating System ******************\n\n");
}


int  main(int argc, char *argv[], char *envp[])
{	
	clrscr();
	welcome_msg();

	char *prompt = "\n>>> ";
	char *command = malloc(100);
	char* temp = malloc(100);

	while(1) {
		printf("%s", prompt);
		scanf("%s", (uint64_t)command);
		shebang(command, temp, envp);
	}
	return 0;
}

