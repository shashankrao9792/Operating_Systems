#include "sys/defs.h"
#include "stdio.h"
#include "stdlib.h"
#include "envio.h"

int main(int argc, char *argv[], char *envp[])
{
	int i = 1;
	printf("\n");
	while(i < argc) {
		if(argv[i][0] != '$') {
			printf("%s  ", argv[i]);
		}
		else {
			getenv(argv[i], envp);
		}
		i++;
	}
	return 0;
}
