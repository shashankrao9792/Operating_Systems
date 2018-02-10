#include "sys/defs.h"
#include "stdio.h"
#include "string.h"
#include "unistd.h"
#include "stdlib.h"
#include "dirent.h"

int main(int argc, char *argV[], char *envp[])
{
	char* filepath = /*(char*)*/malloc(100);
	int i = 1;
	int fd = -1;
	if(argV[i] == 0) {
		getcwd(&filepath[0]);
		fd = openDirectory(&filepath[0], 0x0, FILE_PERM);
		if(fd == FILE_ERROR) {
			printf("\nNo such file or directory");
			return -1;
		}
		char* p = malloc(500);
		char* b = malloc(500);
		readDirectory(fd, &p[0]);
		strcpy2(b, p, '/');
		printf("\n%s", b);
		closeDirectory(fd);
	}
	else {
		while(argV[i] != 0) {
			fd = -1;
			fd = openDirectory(&(*argV[i]), 0x0, FILE_PERM);
			if(fd == FILE_ERROR) {
				printf("\nNo such file or directory");
				i++;
				continue;
			}
			char* p = /*(char*)*/malloc(500);
			char* b = /*(char*)*/malloc(500);
			readDirectory(fd, &p[0]);
			strcpy2(b, p, '/');
			if(strlen(p) == 0) {
				printf("\n%s\n%s\n", argV[i], b);
			}
			else {
				printf("\n%s:\n%s\n", argV[i], b);
			}
			i++;
			closeDirectory(fd);
		}
	}
	return 0;
}
