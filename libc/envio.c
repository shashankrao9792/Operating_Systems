#include "envio.h"
#include "string.h"
#include "stdlib.h"
#include "stdio.h"

int  getenv(char* argv, char *envp[])
{

	int j = 0;
	while(envp[j] != 0) {
		char **tokens = (char**)malloc(sizeof(char*)*10);
		strtok(envp[j], '=', tokens);
		if((strcmp(tokens[0], argv) == 0)) {
			printf("%s  ", tokens[1]);
		}
		j++;
	}
	return 0;
}

//void setenv(char *env_var, char *env_value)
//{
//	int env_var_nbr;
//	env_var_nbr = getenv(env_var);
//	char *str = env_var;
//	str = strcat(str,"=");
//	str = strcat(str,env_value);
//	global_env[env_var_nbr] = str;
//}
