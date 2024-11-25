/*
 * MCP.h
 * Author: Luke Marshall
 * Created on: 10/28/2024
 * Last modified: 10/28/2024 by Luke Marshall
 * Description: File containing  function and struct declarations of the for MCPv1 for 
 * part 1 of project 2 in the University of Oregon Fall CS 415 Operating Systems course.
 * Includes infrastructure for parsing the input commands from the input file.
 */

#ifndef STRING_PARSER_H_
#define STRING_PARSER_H_


#define _GNU_SOURCE


typedef struct
{
    char** cmdList;
    int tokenCount;
} commandLine;

/**
 * this function returns the number of tokens in the given string
 * based on the delimeter
*/
int countTokens(char* buf, const char* delim);

/**
 * This function tokenizes a string based on a specified delimeter, and adds the tokens to an array
 * it returns a commandLine struct
*/
commandLine strFiller(char* buf, const char* delim);


/**
 * this function safely frees all the memory associated with the given commandLine struct.
 */
void freeCmdLine(commandLine* command);


#endif /* STRING_PARSER_H_ */
