/**
 * File: main.c
 * Original Author: Luke Marshall
 * Created on: 10/15/2024
 * Last modified: 10/15/2024 by Luke Marshall
 * Description: File containing the main function for the University of Oregon CS 415
 * Fall term 2024 project 1.
 */

// Included header files:
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "string_parser.h"
#include "command.h"

// Function declarations:
void FileMode(char* fileName);
int executeCmd(char*** cmd);
void interactiveMode();
int tokenizeAndExecuteCmds(char* line);


int main(int argc, char** argv) {
    if (argc > 1) {
        // check that the flag is -f and that a third argument is given:
        if (strcmp(argv[1], "-f") != 0 || argc != 3) {
            // if not, report as error:
            printf("Usage of file mode: pseudo-shell -f <filename>\n");
            exit(EXIT_FAILURE);
        }
        FileMode(argv[2]);
    } else {
        interactiveMode();
    }

    return 0;
}

/**
 * args: a file name
 * Takes in a file name and reads the file, executing the commands on each line
 */
void FileMode(char* fileName) {

    // -------------------- open input file ----------------------------------------------

    // open file and assign it to be used as input:
	FILE* inputFile = fopen(fileName, "r");
    // error checking:
    if (inputFile == NULL) {
        char error[256];
        snprintf(error, sizeof(error), "Error opening input file: %s", fileName);
        perror(error);
        exit(EXIT_FAILURE);
    }

    char* outputFileName = "output.txt";
    // redirect stdout to output file:
    freopen(outputFileName, "w", stdout);
    freopen(outputFileName, "w", stderr);

    // ------ read input file linewise, execute the commands in it, and write results to output -------

	char* lineBuf = NULL;
    size_t maxLineLen = 0;
    int exitFlag = 0;
	//loop until EOF, tokenizing and executing each command:
	while (getline(&lineBuf, &maxLineLen, inputFile) != -1) {
        exitFlag = tokenizeAndExecuteCmds(lineBuf);

        if (exitFlag) break;
	}

    //free line buffer
	free(lineBuf);

    // -------------------- close input file ----------------------------------------------

    // close the input file:
    int closed = fclose(inputFile);
    // error checking:
    if (closed < 0) {
        char error[256];
        snprintf(error, sizeof(error), "Error closing input file: %s", fileName);
        perror(error);
        exit(EXIT_FAILURE);
    }

    // close the output file:
    fclose(stdout);
    fclose(stderr);
}

/**
 * Creates a loop asking for input and accepting commands one line at a time
 * and executing them, outpuitting to stdout
 */
void interactiveMode() {
    char* lineBuf = NULL;
    int exitFlag = 0;
    while (1) {
        printf(">>> ");

        // read the command line given:
        size_t maxLineLen = 0;
        ssize_t lineLen = getline(&lineBuf, &maxLineLen, stdin);
        // error checking:
        if (lineLen < 0) {
            char error[256];
            snprintf(error, sizeof(error), "Error getting the line from input.");
            perror(error);
            exit(EXIT_FAILURE);
        }

        // parse the input and execute it:
        exitFlag = tokenizeAndExecuteCmds(lineBuf);

        // free the automatically allocated line buffer:
        free(lineBuf);

        // check the exit flag:
        if (exitFlag) break;

    }

    //free line buffer
	// free(lineBuf);
}

/**
 * Topkenizes the given line of commands and executes each command in turn, outputting
 * to the output for the given file descriptor
 */
int tokenizeAndExecuteCmds(char* line) {
    /** two types of tokens:
	  *	 large tokens separted by ";" e.g. "pwd ; mkdir test ;" contains tokens "pwd " and " mkdir test "
      *  Each large token represents the entirety of a command including any args
      * 
	  *	 small tokens separated by " " (space) e.g. "mkdir test " contains tokens "mkdir" and "test"
      *  Each small token represents a portion of a command, either the command one of args if included
      */

    // command line structs for holding the tokens:
	command_line largeTokenBuffer; // holds each of the large tokens per line, 
                                     // separated by any ; on the line
	command_line smallTokenBuffer; // holds each of the small tokens per large token, 
                                     // separated by space within the large tokens

                                     
    // tokenize the line and store the tokens in command_list of largeTokenBuffer:
    largeTokenBuffer = str_filler(line, ";");
    if (largeTokenBuffer.num_token == 0) {
        return 0;
    }
    // command_list of largeTokenBuffer now holds at least one command and any args,
    // possibly multiple commands, as an array

    // iterate through each large token found in the line
    int exitFlag = 0;
    for (int i = 0; largeTokenBuffer.command_list[i] != NULL; i++) {

        // tokenize large tokens and store them in command_list of smallTokenBuffer:
        smallTokenBuffer = str_filler(largeTokenBuffer.command_list[i], " ");
        if (smallTokenBuffer.num_token == 0) {
            continue;
        }
        // command_list of smallTokenBuffer now holds at least a single command, 
        // possibly a command and its args, as an array
        
        // execute the command:
        exitFlag = executeCmd(&(smallTokenBuffer.command_list));

        //free small token command_line:
        free_command_line(&smallTokenBuffer);

        // check for exit flag:
        if (exitFlag) {
            break;
        }
    }

    //free large token command_line:
    free_command_line(&largeTokenBuffer);

    return exitFlag;
}

int executeCmd(char*** cmd) {
    char** command = *cmd;
    char* keyword = command[0];
    int exitFlag = 0;
    if (strcmp(keyword, "ls") == 0) { // ls
        if (command[1] != NULL) {
            printf("Error! Unsupported parameters for command: ls\n");
        } else {
            listDir();
        }
    } else if (strcmp(keyword, "pwd") == 0) { // pwd
        if (command[1] != NULL) {
            printf("Error! Unsupported parameters for command: pwd\n");
        } else {
            showCurrentDir();
        }
    } else if (strcmp(keyword, "mkdir") == 0) { // mkdir
        if (command[1] == NULL || command[2] != NULL) {
            printf("Error! Unsupported parameters for command: mkdir\n");
        } else {
            makeDir(command[1]);
        }
    } else if (strcmp(keyword, "cd") == 0) { // cd
        if (command[1] == NULL || command[2] != NULL) {
            printf("Error! Unsupported parameters for command: cd\n");
        } else {
            changeDir(command[1]);
        }
    } else if (strcmp(keyword, "cp") == 0) { // cp
        if (command[1] == NULL || command[2] == NULL || command[3] != NULL) {
            printf("Error! Unsupported parameters for command: cp\n");
        } else {
            copyFile(command[1], command[2]);
        }
    } else if (strcmp(keyword, "mv") == 0) { // mv
        if (command[1] == NULL || command[2] == NULL || command[3] != NULL) {
            printf("Error! Unsupported parameters for command: mv\n");
        } else {
            moveFile(command[1], command[2]);
        }
    } else if (strcmp(keyword, "rm") == 0) { // rm
        if (command[1] == NULL || command[2] != NULL) {
            printf("Error! Unsupported parameters for command: rm\n");
        } else {
            deleteFile(command[1]);
        }
    } else if (strcmp(keyword, "cat") == 0) { // cat
        if (command[1] == NULL || command[2] != NULL) {
            printf("Error! Unsupported parameters for command: cat\n");
        } else {
            displayFile(command[1]);
        }
    } else if (strcmp(keyword, "exit") == 0) { // exit
        exitFlag = 1;
    } else { // anything else
        printf("Error! Unrecognized command: %s\n", keyword);
    }
    return exitFlag;    
}
