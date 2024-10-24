/*
 * string_parser.h
 * Author: Luke Marshall
 * Created on: 10/15/2024
 * Last modified: 10/15/2024 by Luke Marshall
 * Description: File containing declarations of the str_filler function and related helper functions 
 * for the University of Oregon CS 415 Fall term 2024 project 1.
 */

#ifndef STRING_PARSER_H_
#define STRING_PARSER_H_


#define _GNU_SOURCE


typedef struct
{
    char** command_list;
    int num_token;
} command_line;

/**
 * this function returns the number of tokens in the given string
 * based on the delimeter
*/
int count_token(char* buf, const char* delim);

/**
 * This function tokenizes a string based on a specified delimeter, and adds the tokens to an array
 * it returns a command_line struct
*/
command_line str_filler(char* buf, const char* delim);


/**
 * this function safely frees all the memory associated with the given command_line struct.
 */
void free_command_line(command_line* command);


#endif /* STRING_PARSER_H_ */
