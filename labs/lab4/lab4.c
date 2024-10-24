#include<stdio.h>
#include <sys/types.h>
#include<stdlib.h>
#include<unistd.h>
#include<sys/wait.h>

void script_print (pid_t* pid_ary, int size);

int main(int argc, char* argv[])
{
	if (argc < 1)
	{
		printf("Wrong number of arguments\n");
		exit (0);
	}

	/*
	*	TODO
	*	#1	declare child process pool
	*	#2 	spawn n new processes
	*		first create the argument needed for the processes
	*		for example "./iobound -seconds 10"
	*	#3	call script_print
	*	#4	wait for children processes to finish
	*	#5	free any dynamic memory
	*/

	// Get the number of processes to create from the arguments:
	int numProcesses = argc == 2 ? 1 : atoi(argv[2]);
	// if it's less than one then just exit:
	if (numProcesses < 1) {
		exit(EXIT_SUCCESS);
	}

	// allocate array for holding child process pids:
	pid_t* childProcessList = (pid_t*) malloc(sizeof(pid_t) * numProcesses);

	pid_t pid;
	// create and run the given number of child processes:
	char* args[] = {"./iobound", "-seconds", "5", NULL};
	for (int i = 0; i < numProcesses; i++) {
		pid = fork();
		if (pid == 0) {
			execvp("./iobound", args);
		} else if (pid > 0) {
			childProcessList[i] = pid;
			waitpid(pid, NULL, 0);
		} else {
			perror("call to fork() failed");
		}
	}

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


