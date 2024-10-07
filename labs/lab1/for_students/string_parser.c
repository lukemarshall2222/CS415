/*
 * string_parser.c
 *
 *  Created on: Nov 25, 2020
 *      Author: gguan, Monil
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "string_parser.h"

#define _GNU_SOURCE

int count_token (char* buf, const char* delim)
{
	//TODO：
	/*
	*	#1.	Check for NULL string
	*	#2.	iterate through the tokenized string and count the tokens
	*		Cases to watch out for
	*			a.	string starts with delimeter
	*			b. 	string ends with delimeter
	*			c.	account NULL for the last token
	*	#3. return the number of tokens (note not number of delimeters)
	*/

	// check for NULL string
	if (buf == NULL) {
		return 0;
	}

	// iterate through the tokens and count each one
	int count = 0;
	char *tkn = strtok_r(buf, delim, &buf);
	while (tkn != NULL) {
		count++;
		tkn = strtok_r(NULL, delim, &buf);
	}
	
	return count;
}

command_line str_filler (char* buf, const char* delim)
{
	//TODO：
	/*
	*	#1.	create command_line variable to be filled and returned
	*	#2.	count the number of tokens with count_token function, set num_token. 
    *           one can use strtok_r to remove the \n at the end of the line.
	*	#3. malloc memory for token array inside command_line variable
	*			based on the number of tokens.
	*	#4.	use function strtok_r to find out the tokens 
    *   #5. malloc each index of the array with the length of tokens,
	*			fill command_list array with tokens, and fill last spot with NULL.
	*	#6. return the struct.
	*/

	char *str, *strcopy, *token;
	// remove any newline:
	str = strtok_r(buf, "\n", &buf);
	strcopy = strdup(str);

	// find the number of tokens to create the command_line struct
	int numTokens = count_token(strcopy, delim);

	// declare command_line struct, allocate memory for command_list for number of tokens, 
	// and set num_tokens to the number of tokens in buf
	command_line tokensAndCount;
	tokensAndCount.command_list = (char **) malloc(sizeof(char *) * (numTokens+1));

	// Check if mem allocation successful:
    if (tokensAndCount.command_list == NULL) {
        exit(EXIT_FAILURE);
    }

	tokensAndCount.num_token = numTokens;

	if (numTokens > 0) {
		token = strtok_r(str, delim, &str);
		for (int i = 0; i < numTokens; i++) {
			if (token != NULL) {
				tokensAndCount.command_list[i] = (char *) malloc(strlen(token)+1);
				// check if mem allocation successful:
				if (tokensAndCount.command_list[i] == NULL) {
					exit(EXIT_FAILURE);
				}
				strcpy(tokensAndCount.command_list[i], token);
				token = strtok_r(NULL, delim, &str);
			}
		}
	}
	tokensAndCount.command_list[numTokens] = NULL;

	free(strcopy);

	return tokensAndCount;
}

void free_command_line(command_line* command)
{
	//TODO：
	/*
	*	#1.	free the array base num_token
	*/

	for (int i = 0; i < command->num_token; i++) {
		free(command->command_list[i]);
	}
	free(command->command_list);
}
