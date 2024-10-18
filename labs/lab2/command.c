#include <unistd.h>
#include <stdio.h>
#include <dirent.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>

/**
 * funtion takes in a string and an array of strings and returns 1 if the string
 * is in the array and 0 otherwise
 */
int contains(char* element, char* arr[]) {
	for (int i = 0; arr[i] != NULL; i++) {
		if (strcmp(arr[i], element) == 0) {
			return 1;
		}
	}
	return 0;
}

/**
 * function prints a newline character to stdout
*/
void newLine() {
	write(STDOUT_FILENO, "\n", strlen("\n"));
}

void lfcat() {
// High level functionality you need to implement:

	// Get the current directory with getcwd()
	int dirNameLen = 1024;
	char *cwd = malloc(dirNameLen);
	char* res = getcwd(cwd, dirNameLen);
	if (res == NULL) {
		perror("Error getting the current working directory");
		return;
	}
	
	// Open the dir using opendir():
	DIR *dir = opendir(cwd);
	if (dir == NULL) {
		perror("Error opening current working directory");
		return;
	}
	free(cwd);
	
	// use a while loop to read the dir with readdir():
	struct dirent *file;
	FILE *redirect = freopen("output.txt", "w", stdout);
	if (redirect == NULL) {
		perror("Error redirecting stdout to output.txt");
		return;
	}

	while ((file = readdir(dir)) != NULL) {
		// You can debug by printing out the filenames here:
		// printf("File: %s\n", file->d_name);

		/* Option: use an if statement to skip any names that are not readable files 
		(e.g. ".", "..", "main.c", "lab2.exe", "output.txt" */
		char *unreadables[] = {".", "..", "lab2", "main.c", "lab-2-description.pdf", "command.h", 
							  "command.c", "command.o", "main.o", "Makefile", "output.txt", NULL};
		if (contains(file->d_name, unreadables)) {
			// if the file is one of the unreadable files, skip reading it:
			continue;
		}

		// Open the file:
		char* line = NULL;
		size_t size = 0;
		FILE *text = fopen(file->d_name, "r");
		if (text == NULL) {
			perror("Error opening file");
			return;
		}

		char *fileOutput;
		sprintf(fileOutput, "File: %s\n", file->d_name);
		write(STDOUT_FILENO, fileOutput, strlen(fileOutput));
		// Read in each line using getline():
		while (getline(&line, &size, text) != -1) {
			// Write the line to stdout:
			write(STDOUT_FILENO, line, strlen(line));
		}
		// write 80 "-" characters to stdout:
		newLine();
		for (int i = 0; i < 80; i++) {
			write(STDOUT_FILENO, "-", strlen("-"));
		}
		newLine();

		// close the read file and free/null assign your line buffer
		fclose(text);
		free(line);
		line = NULL;
	}

	// close the directory you were reading from using closedir():
	closedir(dir);
	fclose(redirect);
}