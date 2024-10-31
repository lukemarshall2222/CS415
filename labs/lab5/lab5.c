#include <stdio.h>
#include <sys/types.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>
#include <signal.h>

void signaler(pid_t* pid_ary, int size, int signal);

int main(int argc, char* argv[])
{
	
	// initialization of pid etc. like lab 4

	// initialize sigset
	sigset_t sigset;
	int sig;

	// create an empty sigset_t
	sigemptyset(&sigset);

	// use sigaddset() to add the SIGUSR1 signal to the set

	// use sigprocmask() to add the signal set in the sigset for blocking
	

	for(int i = 0; i < n; i++)
	{
		if (pid > 0) {
			// same fork as in lab 4
		}

		if(pid == 0)
		{
			// print: Child Process: <pid> - Waiting for SIGUSR1…
			// wait for the signal
			
			// print: Child Process: <pid> - Received signal: SIGUSR1 - Calling exec().
			// call execvp with ./iobound like in lab 4

		}
		if (pid < 0) {
			// fork failed error
		}
	}
	
	// send SIGUSR1 

	// send SIGSTOP 

	// send SIGCONT

	// send SIGINT

	return 0;
}

void signaler(pid_t* pid_ary, int size, int signal)
{
	// sleep for three seconds

	for(int i = 0; i < size; i++)
	{
		// print: Parent process: <pid> - Sending signal: <signal> to child process: <pid>
		// send the signal
	}
}



