/*
 * string_parser.h
 *
 *  Created on: Nov 8, 2020
 *      Author: gguan, Monil
 *
 *	Purpose: The goal of this dynamic helper (command_line) struct is to consistently 
 *			 tokenize strings based on any given delimeter. Following this structure
 *           will help to keep the code clean.
 *
 */

#ifndef STRING_PARSER_H_
#define STRING_PARSER_H_


#define _GNU_SOURCE


typedef struct
{
    char** command_list;
    int num_token;
}command_line;

//this functions returns the number of tokens needed for the string array
//based on the delimeter
int count_token (char* buf, const char* delim);

//This function can tokenize a string to token arrays based on a specified delimeter,
//it returns a command_line struct
command_line str_filler (char* buf, const char* delim);


// this function safely frees all the memory associated with the given command_line struct.
void free_command_line(command_line* command);


#endif /* STRING_PARSER_H_ */
