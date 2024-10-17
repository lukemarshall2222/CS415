/**
 * string_parser.c
 * Author: Luke Marshall
 * Created on: 10/15/2024
 * Last modified: 10/15/2024 by Luke Marshall
 * Description: File containing the str_filler function and related helper functions 
 * for the University of Oregon CS 415 Fall term 2024 project 1.
 */

// Included header files:
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "string_parser.h"
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

command_line str_filler(char* buf, const char* delim)
{
	char *str, *strCp, *token;
	// remove any newline:
	str = strtok_r(buf, "\n", &buf);

	// copy string so strtok_r during counting doesn't mess with orignal:
	strCp = strdup(str);

	// find the number of tokens to create the command_line struct:
	int num_token = count_token(strCp, delim);
	free(strCp);

	// declare command_line struct, allocate memory for command_list for number of tokens, 
	// and set num_tokens to the number of tokens in buf:
	command_line tokensAndCount;
	tokensAndCount.command_list = (char **) malloc(sizeof(char *) * (num_token+1));
	// error checking:
    if (tokensAndCount.command_list == NULL) {
		char error[256];
        snprintf(error, sizeof(error), "Error allocating memory for command_list array.");
        perror(error);
        exit(EXIT_FAILURE);    
	}

	tokensAndCount.num_token = num_token;

	// tokenize the string based on the given delimiter:
	if (num_token > 0) {
		for (int i = 0; i < num_token; i++) {
			token = strtok_r(NULL, delim, &str);
			if (token != NULL) {
				tokensAndCount.command_list[i] = (char *) malloc(strlen(token)+1);
				// error checking:
				if (tokensAndCount.command_list[i] == NULL) {
					char error[256];
        			snprintf(error, sizeof(error), "Error allocating memory for individual tokens.");
        			perror(error);
					exit(EXIT_FAILURE);
				}
				// copy the token into place:
				strcpy(tokensAndCount.command_list[i], token);
			}
		}
	}
	// set final element in array to NULL:
	tokensAndCount.command_list[num_token] = NULL;

	return tokensAndCount;
}

void free_command_line(command_line* command)
{
	// free each token in array:
	for (int i = 0; command->command_list[i] != NULL; i++) {
		free(command->command_list[i]);
	}
	// free array and set its value to NULL:
	free(command->command_list);
	command->command_list = NULL;
}
