/**
 * Original Author: Luke Marshall
 * Last Modified: 10/24/2024
 * Description: Lab 4 main file
 */

// Included headers:
#include<stdio.h>
#include <sys/types.h>
#include<stdlib.h>
#include<unistd.h>
#include<sys/wait.h>

void script_print (pid_t* pid_ary, int size);

int main(int argc, char* argv[])
{
	// check number of args:
	if (argc != 2)
	{
		printf("Usage: ./lab4 <int>\n");
		exit(0);
	}

	// Get the number of processes to create from the arguments:
	int numProcesses = atoi(argv[1]);
	// if it's less than one then just exit:
	if (numProcesses < 1) {
		exit(EXIT_SUCCESS);
	}

	// allocate array for holding child process pids:
	pid_t* childProcessList = (pid_t*) malloc(sizeof(pid_t) * numProcesses);
	if (childProcessList == NULL) {
		perror("Error allocating memory for child pid list");
		exit(EXIT_FAILURE);
	}

	pid_t pid;
	// create and run the given number of child processes:
	char* args[] = {"./iobound", "-seconds", "5", NULL};
	for (int i = 0; i < numProcesses; i++) {
		pid = fork();
		if (pid == 0) {
			if (execvp("./iobound", args) == -1) {
                perror("Error executing call to execvp()");
                exit(EXIT_FAILURE);
            }
		} else if (pid > 0) {
			childProcessList[i] = pid;
		} else {
			perror("Error executing call to fork()");
			free(childProcessList);
			exit(EXIT_FAILURE);
		}
	}

	// wait for all child processes to finish:
    for (int i = 0; i < numProcesses; i++) {
        waitpid(childProcessList[i], NULL, 0);
    }

	// run the script:
	script_print(childProcessList, numProcesses);

	// free the pid array:
	free(childProcessList);

	return 0;
}


void script_print (pid_t* pid_ary, int size)
{
	FILE* fout;
	fout = fopen("top_script.sh", "w");
	fprintf(fout, "#!/bin/bash\ntop");
	for (int i = 0; i < size; i++)
	{
		fprintf(fout, " -p %d", (int)(pid_ary[i]));
	}
	fprintf(fout, "\n");
	fclose(fout);

	char* top_arg[] = {"gnome-terminal", "--", "bash", "top_script.sh", NULL};
	pid_t top_pid;

	top_pid = fork();
	{
		if (top_pid == 0)
		{
			if (execvp(top_arg[0], top_arg) == -1)
			{
				perror("top command: ");
			}
			exit(0);
		}
	}
}


