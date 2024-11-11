/**
 * File: part5.c
 * Original Author: Luke Marshall
 * Created on: 11/10/2024
 * Last modified: 11/10/2024 by Luke Marshall
 * Description: File containing the code for MCPv5 for part 5 of project 2 in the 
 * University of Oregon Fall CS 415 Operating Systems course.
 */

// definitions:
#define CPU_BOUND_RATIO 20
#define IO_BOUND_RATIO 10

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
void printProcessInfo(pid_t pid);
void adjustProcessTimeSlice(int childIndex, unsigned long utime, unsigned long stime, 
                            unsigned long syscr, unsigned long syscw);

// Global Variables:
int currRunningChild = 0;
int numChildren = 0;
int numRunningChildren = 0;
pid_t* globalChildProcessList;
int* globalChildTrackerList;
int* timeSliceList;
char reason[20];



int main(int argc, char** argv) {
    // check the number of arguments:
    if (argc < 2 || argc > 3) {
        // Error:
        char* buf = "Usage: ./part2 <filename> <time slice?>\n";
        write(STDERR_FILENO, buf, strlen(buf)+1);
        exit(EXIT_FAILURE);
    }

    // set the number of seconds used as time slice:
    int timeSlice = (argc == 3) ? atoi(argv[2]) : 1;

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

    // allocate for an array of time slices:
    int* timeSlices = (int*) malloc(sizeof(int) * cmdCount);
     if (timeSlices == NULL) {
        // Error:
        perror("Error. Failed to allocate memory for the tracking array");

        // Cleanup:
        fclose(inputFile);
        free(childProcessList);
        free(childTrackerList);
		exit(EXIT_FAILURE);
	}
    // set all values in the array to 1:
    for (int k = 0; k < cmdCount; k++) {
        timeSlices[k] = timeSlice;
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
            free(childTrackerList);
            free(timeSlices);
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
            free(childTrackerList);
            free(timeSlices);
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
                    perror("Error. Failed in executing call to execvp");

                    // Cleanup:
                    freeCmdLine(&command);
                    free(childProcessList);
                    free(childTrackerList);
                    free(timeSlices);
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
                free(childTrackerList);
                free(timeSlices);
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
            free(childTrackerList);
            free(timeSlices);
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
        if (kill(childProcessList[j], SIGUSR1) < 0) {
            // Error:
            perror("Error. Failed call to kill with start signal");

            // Cleanup:
            free(childProcessList);
            free(childTrackerList);
            free(timeSlices);
            exit(EXIT_FAILURE);
        }
        sleep(0.5);
    }

    for (int k = 0; k < cmdCount; k++) {
        // signal the process to stop:
        if (kill(childProcessList[k], SIGSTOP) < 0) {
            // Error:
            perror("Error. Failed call to kill with stop signal");

            // Cleanup:
            free(childProcessList);
            free(childTrackerList);
            free(timeSlices);
            exit(EXIT_FAILURE);
        }
    }

    // set global variables for use in alarm handler:
    numChildren = cmdCount;
    numRunningChildren = cmdCount;
    globalChildProcessList = childProcessList;
    globalChildTrackerList = childTrackerList;
    timeSliceList = timeSlices;

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

        usleep(10000); 
    }

    // cleanup:
    free(globalChildProcessList);
    free(globalChildTrackerList);
    free(timeSliceList);
    // printf("Finished executing all child processes...exiting\n");
    exit(EXIT_SUCCESS);
}

void alarmHandler(int sig) {
    // Stop the currently running process
    kill(globalChildProcessList[currRunningChild], SIGSTOP);

    // print the process info:
    printProcessInfo(currRunningChild);

    // Find the next process that is still running
    int i = (currRunningChild + 1) % numChildren;
    while (!globalChildTrackerList[i]) {
        i = (i + 1) % numChildren;
    }

    // Start the next process
    currRunningChild = i;
    kill(globalChildProcessList[currRunningChild], SIGCONT);
    
    // Set the next alarm
    alarm(timeSliceList[currRunningChild]);
}

void printProcessInfo(int childIndex) {
    char filename[64];
    pid_t pid = globalChildProcessList[childIndex];

    // ---------- stat file ------------------------------------------

    // make stat filename for the given pid:
    snprintf(filename, sizeof(filename), "/proc/%d/stat", pid);

    // Open the stat file
    FILE* statFile = fopen(filename, "r");
    if (statFile == NULL) {
        perror("Error. failed call to fopen to open /proc<pid>/stat");
        return;
    }

    char* line = NULL;
    size_t len = 0;
    // Read line in /stat:
    getline(&line, &len, statFile);
    fclose(statFile);

    // Parse the line:
    char name[64];
    sscanf(line, "%*d (%255[^)])", name);

    int PID, PPID, tmp2, tmp3, tmp4, tmp5;
    unsigned int tmp6;
    unsigned long tmp7, tmp8, tmp9, tmp10, utime, stime;

    // Parse up to the required fields, then capture utime and stime
    sscanf(line, "%d %*s %*c %d %d %d %d %d %u %lu %lu %lu %lu %lu %lu",
           &PID, &PPID, &tmp2, &tmp3, &tmp4, &tmp5, &tmp6, &tmp7, &tmp8,
           &tmp9, &tmp10, &utime, &stime);

    free(line);

    
    // -------------- io file --------------------------------------

    // make io filename for the given pid:
    snprintf(filename, sizeof(filename), "/proc/%d/io", pid);

    // Open the io file:
    FILE* ioFile = fopen(filename, "r");
    if (ioFile == NULL) {
        perror("Error. failed call to fopen to open /proc/<pid>/io");
        return;
    }

    // get lines 3 and 4 from the /proc/<pid>/io file:
    line = NULL;
    len = 0;
    unsigned long syscr, syscw;
    while (getline(&line, &len, ioFile) != -1) {
        if (strstr(line, "syscr:") != NULL) {
            sscanf(line, "%*s %lu", &syscr);
        } else if (strstr(line, "syscw:") != NULL) {
            sscanf(line, "%*s %lu", &syscw);
        }
    }
    fclose(ioFile);
    free(line);


    // // -------------- status file --------------------------------------

    // make io filename for the given pid:
    snprintf(filename, sizeof(filename), "/proc/%d/status", pid);

    // Open the io file:
    FILE* statusFile = fopen(filename, "r");
    if (statusFile == NULL) {
        perror("Error. failed call to fopen to open /proc/<pid>/status");
        return;
    }

    line = NULL;
    len = 0;
    unsigned long vmRSS;
    while (getline(&line, &len, statusFile) != -1) {
        if (strstr(line, "VmRSS") != NULL) {
            sscanf(line, "%*s %lu", &vmRSS);
            break;
        }
    }
    fclose(statusFile);
    free(line);

    // ------------- adjust the time slice -----------------------------

    adjustProcessTimeSlice(childIndex, utime, stime, syscr, syscw);


    // ------------ printing table --------------------------------------

    // print table column headers:
    printf("%-8s %-12s %-8s %-10s %-10s %-10s %-12s %-18s\n", 
           "PID", "Name", "PPID", "CPU Time", "IO Calls", "Mem Size", "Time Slice", "Reason");

    // print the row for the process:
    printf("%-8d %-12s %-8d %-10lu %-10lu %-10lu %-12d %-18s\n", 
            PID, name, PPID, (utime + stime), (syscr + syscw), vmRSS,
            timeSliceList[currRunningChild], reason);

}

void adjustProcessTimeSlice(int childIndex, unsigned long utime, unsigned long stime, 
                            unsigned long syscr, unsigned long syscw) {

    unsigned long totalIO = syscr + syscw;
    unsigned long totalCPUTime = utime + stime;

    unsigned long ratio = totalCPUTime/(totalIO+1);

    if (ratio/timeSliceList[childIndex] > CPU_BOUND_RATIO) {
        timeSliceList[childIndex] *= 2;
        strcpy(reason, "CPU High");
    } else if (ratio < IO_BOUND_RATIO) {
        timeSliceList[childIndex] /= 2;
        if (timeSliceList[childIndex] < 1) timeSliceList[childIndex] = 1;
        strcpy(reason, "IO High");
    } else {
        strcpy(reason, "Balanced CPU-IO");
    }
}