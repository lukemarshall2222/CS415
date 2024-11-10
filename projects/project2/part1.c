/**
 * File: part1.c
 * Original Author: Luke Marshall
 * Created on: 10/28/2024
 * Last modified: 10/28/2024 by Luke Marshall
 * Description: File containing the code for MCPv1 for part 1 of project 2 in the 
 * Univversity of Oregon Fall CS 415 Operating Systems course.
 */

// Included header files:
#include "MCP.h"
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <fcntl.h>
#include <stdio.h>
#include <sys/wait.h>


int main(int argc, char** argv) {
    // check the number of arguments:
    if (argc != 2) {
        // Error:
        char* buf = "Usage: ./part1 <filename>\n";
        write(STDERR_FILENO, buf, strlen(buf)+1);
        exit(EXIT_FAILURE);
    }

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
    free(line);
    len = 0;

    // allocate for array of child pids:
    pid_t* childProcessList = (pid_t*) malloc(sizeof(pid_t) * cmdCount);
    // error checking:
    if (childProcessList == NULL) {
        // Error:
        perror("Error. Failed to allocate memory for the pid array.");

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

    // wait for all the children to finish:
    for (int j = 0; j < cmdCount; j++) {
        printf("waiting for process %d\n", childProcessList[j]);
        res = waitpid(childProcessList[j], NULL, 0);
        // error checking:
        if (res < 0) {
            // Error:
            perror("Error. Failed call to wait");

            // Cleanup:
            free(childProcessList);
            exit(EXIT_FAILURE);
        }
    }

    // Cleanup:
    free(childProcessList);
    printf("Finished executing all child processes...exiting\n");
    exit(EXIT_SUCCESS);
}