#include "sys/defs.h"
#include "stdio.h"
#include "string.h"
#include "unistd.h"
#include "util.h"
#include "stdlib.h"

int main(int argc, char *argV[], char *envp[])
{
	char * commands = malloc(100);
	long sleep_time_seconds = (long)atoi(argV[1]);
	sleep(sleep_time_seconds,commands);

	return 0;
}
