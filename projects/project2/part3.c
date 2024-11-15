/**
 * File: part3.c
 * Original Author: Luke Marshall
 * Created on: 11/10/2024
 * Last modified: 11/10/2024 by Luke Marshall
 * Description: File containing the code for MCPv3 for part 3 of project 2 in the 
 * University of Oregon Fall CS 415 Operating Systems course.
 */

// Included header files:
#include "MCP.h"
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <fcntl.h>
#include <stdio.h>
#include <sys/wait.h>
#include <signal.h>

// function declarations:
void alarmHandler(int sig);

// Global Variables:
int currRunningChild = 0;
int numChildren = 0;
int numRunningChildren = 0;
pid_t* globalChildProcessList;
int* globalChildTrackerList;
int timeSlice = 1;


int main(int argc, char** argv) {
    // check the number of arguments:
    if (argc < 2 || argc > 3) {
        // Error:
        char* buf = "Usage: ./part2 <filename>\n";
        write(STDERR_FILENO, buf, strlen(buf)+1);
        exit(EXIT_FAILURE);
    }

    // set the number of seconds used as time slice:
    timeSlice = (argc == 3) ? atoi(argv[2]) : 1;

    // set up the signal:
    signal(SIGALRM, alarmHandler);

    // open the input file:
    FILE* inputFile = fopen(argv[1], "r");
    // error checking:
    if (inputFile == NULL) {
        // Error:
        perror("Error. Failed to open given input file");
        exit(EXIT_FAILURE);
    }

    // count the number of lines in input file to find the number of commands:
    char *line = NULL;
    size_t len = 0;
    int cmdCount = 0;
    while (getline(&line, &len, inputFile) != -1) {
        cmdCount++;
    }
    // reset for next iteration through the lines:
    fseek(inputFile, 0, SEEK_SET);
    if (line != NULL) free(line);
    len = 0;

    // allocate for array of child pids:
    pid_t* childProcessList = (pid_t*) malloc(sizeof(pid_t) * cmdCount);
    // error checking:
    if (childProcessList == NULL) {
        // Error:
        perror("Error. Failed to allocate memory for the child pid array");

        // Cleanup:
        fclose(inputFile);
		exit(EXIT_FAILURE);
	}

    // allocate for an array of child tracking ints:
    int* childTrackerList = (int*) malloc(sizeof(int) * cmdCount);
     if (childProcessList == NULL) {
        // Error:
        perror("Error. Failed to allocate memory for the tracking array");

        // Cleanup:
        fclose(inputFile);
        free(childProcessList);
		exit(EXIT_FAILURE);
	}
    // set all values in the array to 1:
    for (int k = 0; k < cmdCount; k++) {
        childTrackerList[k] = 1;
    }

    int res;
    commandLine command;
    for (int i = 0; i < cmdCount; i++) {
        // get the full line from the file:
        res = getline(&line, &len, inputFile);
        
        // error checking:
        if (res < 0) {
            // Error:
            perror("Error. Failed to retrieve command");

            // Cleanup:
            free(childProcessList);
            free(line);
            fclose(inputFile);
            exit(EXIT_FAILURE);
        }

        // tokenize and store the string as command and its args:
        command = strFiller(line, " ");
        // error checking:
        if (command.tokenCount == 0) {
            // Error:
            perror("Error. No command found");

            // Cleanup:
            freeCmdLine(&command);
            free(childProcessList);
            free(line);
            fclose(inputFile);
            continue;
        }

        pid_t pid = fork();
		if (pid == 0) { // child is executing:
            // make it wait and give control back to MCP:
            sigset_t sigSet;
            sigemptyset(&sigSet);
            sigaddset(&sigSet, SIGUSR1);
            sigprocmask(SIG_BLOCK, &sigSet, NULL);
            int sig;
            if (sigwait(&sigSet, &sig) == 0) { 
                printf("calling command %s in child process %d\n", command.cmdList[0], getpid());
                if (execvp(command.cmdList[0], command.cmdList) == -1) {
                    // Error:
                    perror("Error. Failed in executing call to execvp");

                    // Cleanup:
                    freeCmdLine(&command);
                    free(childProcessList);
                    free(line);
                    fclose(inputFile);
                    exit(EXIT_FAILURE);
                }
            } else { // sigwait failed
                // Error:
                perror("Error. Failed in executing call to sigwait");

                // Cleanup:
                freeCmdLine(&command);
                free(childProcessList);
                free(line);
                fclose(inputFile);
                exit(EXIT_FAILURE);
            }
        } else if (pid > 0) { // parent is executing:
            childProcessList[i] = pid;
        } else { // error checking:
            // Error:
            perror("Error executing call to fork");

            // Cleanup:
            freeCmdLine(&command);
            free(childProcessList);
            free(line);
            fclose(inputFile);
            exit(EXIT_FAILURE);
        }
        // Cleanup:
        freeCmdLine(&command);
    }

    // Cleanup:
    free(line);
    fclose(inputFile);

    // signal all children to restart:
    for (int j = 0; j < cmdCount; j++) {
        // signal the process to start:
        printf("beginning child process %d\n", childProcessList[j]);
        if (kill(childProcessList[j], SIGUSR1) < 0) {
            // Error:
            perror("Error. Failed call to kill with start signal");

            // Cleanup:
            free(childProcessList);
            exit(EXIT_FAILURE);
        }
        sleep(0.5);
    }

    for (int k = 0; k < cmdCount; k++) {
        // signal the process to stop:
        printf("stopping child process %d\n", childProcessList[k]);
        if (kill(childProcessList[k], SIGSTOP) < 0) {
            // Error:
            perror("Error. Failed call to kill with stop signal");

            // Cleanup:
            free(childProcessList);
            exit(EXIT_FAILURE);
        }
    }

    // set global variables for use in alarm handler:
    numChildren = cmdCount;
    numRunningChildren = cmdCount;
    globalChildProcessList = childProcessList;
    globalChildTrackerList = childTrackerList;

    // start the first child process and set its alarm:
    kill(childProcessList[0], SIGCONT);
    alarm(timeSlice);

    int status;
    pid_t pid;
    // loop while there are child processes still able to execute:
    while (numRunningChildren > 0) {
        // non-blocking wait to be able to check the status of the currently-running process:
        pid = waitpid(-1, &status, WNOHANG);

        if (pid > 0) { // status of the executing child process changed
            if (WIFEXITED(status)) { // check if the status change was a full exit
                for (int i = 0; i < numChildren; i++) {
                    // if it was: find the process in the list, mark it as complete, and 
                    // subtract 1 from the overall number of child processes executing:
                    if (globalChildProcessList[i] == pid) {
                        numRunningChildren--;
                        globalChildTrackerList[i] = 0; 
                        break;
                    }
                }
            }
        }
    }

    // cleanup:
    free(globalChildProcessList);
    free(globalChildTrackerList);
    printf("Finished executing all child processes...exiting\n");
    exit(EXIT_SUCCESS);
}

void alarmHandler(int sig) {
    // Stop the currently running process
    printf("alarm signaled, stopping child process %d\n", globalChildProcessList[currRunningChild]);
    kill(globalChildProcessList[currRunningChild], SIGSTOP);

    // Find the next process that is still running
    int i = (currRunningChild + 1) % numChildren;
    while (!globalChildTrackerList[i]) {
        i = (i + 1) % numChildren;
    }

    // Start the next process
    currRunningChild = i;
    printf("begin time slice, starting child process %d\n", globalChildProcessList[currRunningChild]);
    kill(globalChildProcessList[currRunningChild], SIGCONT);
    
    // Set the next alarm
    alarm(timeSlice);
}
