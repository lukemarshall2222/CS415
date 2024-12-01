#ifndef ACCOUNT_H_
#define ACCOUNT_H_

#include <pthread.h>

typedef struct {
	char acctNum[17];
	char password[9];
    double balance;
    double rewardRate;
    
    double transactionTracker;

    char outFile[64];

    pthread_mutex_t acctLock;
} account;

typedef struct {
    int threadInd;
    char** transactionLines;
    int lineCount;
} threadwork;


typedef struct {
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


#endif /* ACCOUNT_H_ */