/*
 * lab1_skeleton.c
 *
 *  Created on: Nov 25, 2020
 *      Author: Guan, Xin, Monil
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "string_parser.h"

#define _GNU_SOURCE

int main(int argc, char const *argv[])
{
	//checking for command line argument
	if (argc != 2)
	{
		printf("Usage ./lab1.exe intput.txt\n");
	}
	//opening file and assign it to be used as input
	FILE *inFPtr;
	inFPtr = fopen(argv[1], "r");

	//declare line_buffer
	size_t len = 128;
	char* line_buf = malloc(len);

	// command line structs for holding the tokens
	// two types of tokens:
		// large tokens separted by ";" e.g. "pwd ; mkdir test ;" contains tokens "pwd " and " mkdir test "
		// small tokens separated by " " e.g. "mkdir test " contains tokens "mkdir" and "test"
	command_line large_token_buffer; // command_line struct holds each of the large tokens per line
	command_line small_token_buffer; // command_line struct holds each of the small tokens per large token

	int line_num = 0;

	//loop until the file is over
	while (getline(&line_buf, &len, inFPtr) != -1)
	{
		// print the line number:
		printf("Line %d:\n", ++line_num);

		// tokenize the line and store the tokens in command_list of large_token_buffer
		// "large tokens" are partts of the line seperated by ";" (semicolon)
		large_token_buffer = str_filler(line_buf, ";");
		// iterate through each "large token" found in the line
		for (int i = 0; large_token_buffer.command_list[i] != NULL; i++)
		{
			printf("\tLine segment %d: \n", i+1);
			// printf("second segment contents: %s", large_token_buffer.command_list[1]);

			// tokenize "large tokens" and store them in command_list of small_token_buffer
			// "small tokens" are parts of "large tokens" seperated by " "(space)
			small_token_buffer = str_filler(large_token_buffer.command_list[i], " ");

			// iterate through and print each "small token"
			for (int j = 0; small_token_buffer.command_list[j] != NULL; j++)
			{
				printf("\t\tToken %d: %s\n", j+1, small_token_buffer.command_list[j]);
			}

			//free smaller tokens and reset struct space to null
			free_command_line(&small_token_buffer);
			// memset(&small_token_buffer, 0, sizeof(small_token_buffer));
		}

		//free smaller tokens and reset struct space to null
		free_command_line (&large_token_buffer);
		// memset(&large_token_buffer, 0, sizeof(large_token_buffer)); !referncing 
	}
	fclose(inFPtr);
	//free line buffer
	free(line_buf);
}
