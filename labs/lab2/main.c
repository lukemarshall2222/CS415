#include <stdio.h>
#include <string.h>
#include "command.h"

void main (int argc, char** argv) {
	while (1) {
		printf(">>> ");
		char cmd[80];
		fgets(cmd, 80, stdin);
		if (cmd[strlen(cmd) - 1] == '\n') {
			cmd[strlen(cmd) - 1 ] = '\0';
		}
		if (strcmp(cmd, "lfcat") != 0 && strcmp(cmd, "exit") != 0) {
			printf("Error: Unrecognized command!\n");
			continue;
		} else if (strcmp(cmd, "lfcat") == 0) {
			lfcat();
			continue;
		} else {
			return;
		}
	}
}
