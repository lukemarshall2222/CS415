/**
 * File: part2.c
 * Original Author: Luke Marshall
 * Created on: 10/31/2024
 * Last modified: 10/31/2024 by Luke Marshall
 * Description: File containing the code for MCPv1 for part 2 of project 2 in the 
 * University of Oregon Fall CS 415 Operating Systems course.
 */

// Include header files:
#include "MCP.h"
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <fcntl.h>
#include <stdio.h>
#include <sys/wait.h>
#include <signal.h>


int main(int argc, char** argv) {
    // check the number of arguments:
    if (argc != 2) {
        // Error:
        char* buf = "Usage: ./part2 <filename>\n";
        write(STDERR_FILENO, buf, strlen(buf)+1);
        exit(EXIT_FAILURE);
    }

    // open the input file:
    FILE* inputFile = fopen(argv[1], "r");
    // error checking:
    if (inputFile == NULL) {
        // Error:
        char buf[256];
        snprintf(buf, sizeof(buf),  "Error. Failed to open given input file: %s\n", argv[1]);
        write(STDERR_FILENO, buf, strlen(buf)+1);
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
        char* buf = "Error. Failed to allocate memory for the pid array.\n";
		write(STDERR_FILENO, buf, strlen(buf)+1);

        // Cleanup:
        fclose(inputFile);
		exit(EXIT_FAILURE);
	}

    int res;
    commandLine command;
    for (int i = 0; i < cmdCount; i++) {
        // get the full line from the file:
        res = getline(&line, &len, inputFile);
        
        // error checking:
        if (res < 0) {
            // Error:
            char buf[256];
            snprintf(buf, sizeof(buf),  "Error. Failed to retrieve command from line %d\n", i);
            write(STDERR_FILENO, buf, strlen(buf)+1);

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
            char buf[256];
            snprintf(buf, sizeof(buf),  "Error. No command found on line %d\n", i);
            write(STDERR_FILENO, buf, strlen(buf)+1);

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
                if (execvp(command.cmdList[0], command.cmdList) == -1) {
                    // Error:
                    char buf[256];
                    snprintf(buf, sizeof(buf), "Error. Failed in executing call to execvp with command: %s\n", command.cmdList[0]);
                    write(STDERR_FILENO, buf, strlen(buf)+1);

                    // Cleanup:
                    freeCmdLine(&command);
                    free(childProcessList);
                    free(line);
                    fclose(inputFile);
                    exit(EXIT_FAILURE);
                }
            } else { // sigwait failed
                // Error:
                char buf[256];
                snprintf(buf, sizeof(buf), "Error. Failed in executing call to sigwait with command: %s\n", command.cmdList[0]);
                write(STDERR_FILENO, buf, strlen(buf)+1);

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
            char* buf = "Error executing call to fork\n";
            write(STDERR_FILENO, buf, strlen(buf)+1);

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

    // wait for all the children to finish:
    for (int j = 0; j < cmdCount; j++) {
        // signal the process to start:
        if (kill(childProcessList[j], SIGUSR1) < 0) {
            // Error:
            char* buf = "Error. Failed call to kill with start signal.\n";
            write(STDERR_FILENO, buf, strlen(buf)+1);

            // Cleanup:
            free(childProcessList);
            exit(EXIT_FAILURE);
        }
        printf("starting process: %d\n", childProcessList[j]);
    }

    for (int k = 0; k < cmdCount; k++) {
        // signal the process to stop:
        if (kill(childProcessList[k], SIGSTOP) < 0) {
            // Error:
            char* buf = "Error. Failed call to kill with stop signal.\n";
            write(STDERR_FILENO, buf, strlen(buf)+1);

            // Cleanup:
            free(childProcessList);
            exit(EXIT_FAILURE);
        }
        printf("pausing process: %d\n", childProcessList[k]);
    }

    for (int l = 0; l < cmdCount; l++) {
        // signal the process to stop:
        if (kill(childProcessList[l], SIGCONT) < 0) {
            // Error:
            char* buf = "Error. Failed call to kill with continue signal.\n";
            write(STDERR_FILENO, buf, strlen(buf)+1);

            // Cleanup:
            free(childProcessList);
            exit(EXIT_FAILURE);
        }
        printf("continuing process: %d\n", childProcessList[l]);
    }

    for (int m = 0; m < cmdCount; m++) {
        // wait for process to end:
        if (waitpid(childProcessList[m], NULL, 0) < 0) {
            // Error:
            char* buf = "Error. Failed call to wait.\n";
            write(STDERR_FILENO, buf, strlen(buf)+1);

            // Cleanup:
            free(childProcessList);
            exit(EXIT_FAILURE);
        }
        printf("waiting for process: %d\n", childProcessList[m]);
    }
    printf("all done waiting..exiting\n");

    // Cleanup:
    free(childProcessList);
    exit(EXIT_SUCCESS);
}