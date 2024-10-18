/**
 * File: command.c
 * Original Author: Luke Marshall
 * Created on: 10/15/2024
 * Last modified: 10/15/2024 by Luke Marshall
 * Description: File containing the functions for executing CLI shell commands
 *  for the University of Oregon CS 415 Fall term 2024 project 1.
 */

// Included header files:
#include <dirent.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <libgen.h>
#include <string.h>
#include <stdio.h>

/**
 * ls
 * lists the directories and files in the current directory
 */
void listDir() {
    // retrieve cwd name:
    char cwd[1024];
    // error checking:
    if (getcwd(cwd, sizeof(cwd)) == NULL) {
        char* error = "Error! Unable to retrieve current working directory name.\n";
        write(STDERR_FILENO, error, strlen(error));
        return;
    }

    // open the cwd:
    DIR* dir = opendir(cwd);
    // error checking:
    if (dir  == NULL) {
        char* error = "Error! Unable to open the current working directory.\n";
        write(STDERR_FILENO, error, strlen(error));
        return;
    }

    struct dirent* entry;
    // iteratively read the names in the directory and write them to output:
    while ((entry = readdir(dir)) != NULL) {
        write(STDOUT_FILENO, entry->d_name, strlen(entry->d_name));
        write(STDOUT_FILENO, " ", 1);
    }
    write(STDOUT_FILENO, "\n", 1);

    closedir(dir);
}

/**
 * pwd
 * Retrieves name of the current working directory
 */
void showCurrentDir() {
    // retrieve cwd name:
    char cwd[1024];
    // error checking:
    if (getcwd(cwd, sizeof(cwd)) == NULL) {
        char* error = "Error! Unable to retrieve current working directory name.\n";
        write(STDERR_FILENO, error, strlen(error));
        return;
    }
    
    // write the name to output:
    write(STDOUT_FILENO, cwd, strlen(cwd));
    write(STDOUT_FILENO, "\n", 1);
}

/**
 * mkdir
 * makes the directory specified by the argument
 * if intermediate directories on a given path don't exist, they will not be created
 * and an error will result
 */
void makeDir(char *dirName) {
    // create the directory wqith full user permissions:
    int res = mkdir(dirName, 0700);
    // error checking:
    if (res < 0) {
        char* error = "Error! Unable to craete the given directory.\n";
        write(STDERR_FILENO, error, strlen(error));
        return;
    }
}

/**
 * chdir
 * changes the pwd to the given directory
 * if given directory doesn't exist will result in error
 */
void changeDir(char *dirName) {
    // change the pwd to the given directory:
    int res = chdir(dirName);
    // error checking:
    if (res < 0) {
        char* error = "Error! Unable to change to given directory.\n";
        write(STDERR_FILENO, error, strlen(error));
        return;
    }
}

/**
 * cp
 * copies the given source file into the given destination
 * if source or destination, or anything in their paths, don't exist, results in error
 */
void copyFile(char *sourcePath, char *destinationPath) {
    struct stat stats;

    // check if source file exists:
    if (stat(sourcePath, &stats) != 0) {
        char* error = "Error! Source file does not exist.\n";
        write(STDERR_FILENO, error, strlen(error));
        return;
    }

    // open the source file:
    int source = open(sourcePath, O_RDONLY);
    // error checking:
    if (source < 0) {
        char* error = "Error! Unable to find/open source file.\n";
        write(STDERR_FILENO, error, strlen(error));
        return;
    }

    // check if the given destination is a directory and if it is, get the name of the 
    // file from the source path and append it to the end of the destination path
    int allocated = 0;
    char* newDest;
    if (stat(destinationPath, &stats) == 0 && S_ISDIR(stats.st_mode)) {
        // find name of file from input source:
        char* fileName = basename(sourcePath);

        // allocate for the new destination path including the file name:
        allocated = 1;
        newDest = (char*) malloc(sizeof(char) * (strlen(destinationPath) + strlen(fileName) + 2));
        if (newDest == NULL) {
            char* error = "Error! Unable to allocate memory.\n";
            write(STDERR_FILENO, error, strlen(error));
            return;
        }

        // append the file name onto the end of the destination:
        strcpy(newDest, destinationPath);
        if (destinationPath[strlen(destinationPath) - 1] != '/') {
            strcat(newDest, "/");
        }
        strcat(newDest, fileName);
    }
    // open destination if exists, create it if doesn't:
    int destination = open(allocated ? newDest : destinationPath, O_WRONLY | O_CREAT | O_TRUNC, 0700);
    // error checking:
    if (destination < 0) {
        char* error = "Error! Unable to open/create destination file.\n";
        write(STDERR_FILENO, error, strlen(error));
        if (allocated) {
            free(newDest);
        }
        return;
    }

    int readAmount, res;
    char buf[1024];
    // read the content of source into destination:
    while ((readAmount = read(source, buf, sizeof(buf))) > 0) {
        res = write(destination, buf, readAmount);
        if (res < 0) {
            char* error = "Error! Unable to write to destination file.\n";
            write(STDERR_FILENO, error, strlen(error));
            if (allocated) {
                free(newDest);
            }
            return;
        }
    }
    if (readAmount < 0) {
        char* error = "Error! Unable to read source file.\n";
        write(STDERR_FILENO, error, strlen(error));
        if (allocated) {
            free(newDest);
        }
        return;
    }

    if (allocated) {
        free(newDest);
    }

    // close the files:
    close(source);
    close(destination);
}

/**
 * rm
 * deletes the given file
 * if fails to delete file returns error
 */
void deleteFile(char *filename) {
    // delete the file:
    int res = remove(filename);
    // error checking:
    if (res < 0) {
        char* error = "Error! Unable to delete the given file.\n";
        write(STDERR_FILENO, error, strlen(error));
        return;
    }
}

/**
 * mv
 * moves the given file from the source to the destination
 * if source or destination, or any intermediraies in the paths, don't exist returns error
 */
void moveFile(char *sourcePath, char *destinationPath) {
    // copy the file to the destination location:
    copyFile(sourcePath, destinationPath);
    // delete the file from the source location:
    deleteFile(sourcePath);
}

void displayFile(char *filename) {
    // open the file:
    int opened = open(filename, O_RDONLY);
    // error checking:
    if (opened < 0) {
        char* error = "Error! Unable to open the given file.\n";
        write(STDERR_FILENO, error, strlen(error));
        return;
    }

    int readAmount, res;
    char buf[1024];
    // read the content of the file:
    while ((readAmount = read(opened, buf, sizeof(buf))) > 0) {
        // write the contents to stdout:
        res = write(STDOUT_FILENO, buf, readAmount);
        // error checking:
        if (res < 0) {
            char* error = "Error! Unable to write to stdout.\n";
            write(STDERR_FILENO, error, strlen(error));
            return;
        }
    }
    // error checking for read:
    if (readAmount < 0) {
        char* error = "Error! Unable to read file.\n";
        write(STDERR_FILENO, error, strlen(error));
        return;
    }
    write(STDOUT_FILENO, "\n", 1);

    // close the file:
    close(opened);
}
