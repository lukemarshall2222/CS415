#include <stdio.h>
#include <string.h>
#include "command.h"

void main (int argc, char** argv) {
	while (1) {
		printf(">>> ");

		// retrieve user input from the "command line":
		char cmd[80];
		fgets(cmd, 80, stdin);

		// check if the input is valid and perform corresponding operation if it is
		if (cmd[strlen(cmd) - 1] == '\n') {
			cmd[strlen(cmd) - 1 ] = '\0';
		}
		if (strcmp(cmd, "lfcat") != 0 && 
		    strcmp(cmd, "exit") != 0) {
			// invalid input:
			printf("Error: Unrecognized command!\n");
			continue;
		} else if (strcmp(cmd, "lfcat") == 0) {
			lfcat();
			continue;
		} else {
			// input is "exit" command:
			return;
		}
	}
}
