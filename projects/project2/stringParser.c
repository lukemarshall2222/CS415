/**
 * string_parser.c
 * Author: Luke Marshall
 * Created on: 10/15/2024
 * Last modified: 10/15/2024 by Luke Marshall
 * Description: File containing the strFiller function and related helper functions 
 * for the University of Oregon CS 415 Fall term 2024 project 1.
 */

// Included header files:
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "MCP.h"
#include <unistd.h>
#include <fcntl.h>

#define _GNU_SOURCE

int count_token(char* buf, const char* delim)
{
	// check for NULL string:
	if (buf == NULL) {
		return 0;
	}

	// iterate through the tokens and count each one:
	int count = 0;
	char *tkn = strtok_r(buf, delim, &buf);
	while (tkn != NULL) {
		count++;
		tkn = strtok_r(NULL, delim, &buf);
	}
	
	return count;
}

commandLine strFiller(char* buf, const char* delim)
{
	char *str, *strCp, *token;
	// remove any newline:
	str = strtok_r(buf, "\n", &buf);

	// copy string so strtok_r during counting doesn't mess with orignal:
	strCp = strdup(str);

	// find the number of tokens to create the commandLine struct:
	int tokenCount = count_token(strCp, delim);
	free(strCp);

	// declare commandLine struct, allocate memory for cmdList for number of tokens, 
	// and set num_tokens to the number of tokens in buf:
	commandLine tokensAndCount;
	tokensAndCount.cmdList = (char **) malloc(sizeof(char *) * (tokenCount+1));
	// error checking:
    if (tokensAndCount.cmdList == NULL) {
		char error[256];
        snprintf(error, sizeof(error), "Error allocating memory for cmdList array.");
        perror(error);
        exit(EXIT_FAILURE);    
	}

	tokensAndCount.tokenCount = tokenCount;

	// tokenize the string based on the given delimiter:
	if (tokenCount > 0) {
		for (int i = 0; i < tokenCount; i++) {
			token = strtok_r(NULL, delim, &str);
			if (token != NULL) {
				tokensAndCount.cmdList[i] = (char *) malloc(strlen(token)+1);
				// error checking:
				if (tokensAndCount.cmdList[i] == NULL) {
					char error[256];
        			snprintf(error, sizeof(error), "Error allocating memory for individual tokens.");
        			perror(error);
					exit(EXIT_FAILURE);
				}
				// copy the token into place:
				strcpy(tokensAndCount.cmdList[i], token);
			}
		}
	}
	// set final element in array to NULL:
	tokensAndCount.cmdList[tokenCount] = NULL;

	return tokensAndCount;
}

void freeCmdLine(commandLine* command)
{
	// free each token in array:
	for (int i = 0; command->cmdList[i] != NULL; i++) {
		free(command->cmdList[i]);
	}
	// free array and set its value to NULL:
	free(command->cmdList);
	command->cmdList = NULL;
}
