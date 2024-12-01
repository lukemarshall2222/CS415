


// included headers:
#define _XOPEN_SOURCE 700
#include "account.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <stdbool.h>

// defined values:
#define THREAD_COUNT 1

// function declararations:
void *processTransaction(void *arg);
void *updateBalance(void *arg);
void transfer(int index, char* pw, char* destAcct, double amount);
void checkBalance(int index, char* pw);
void deposit(int index, char* pw, double amount);
void withdraw(int index, char* pw, double amount);
int findIndex(char* acct);
int getNumLines(FILE *file);
void updateAcctBalance(int index, double amount);
void countTrackAndCheck(int index, double amount);

// global variables:
account **accounts; // array of account structs
int accountCount; // total number of account structs being tracked
int transactionCount; // number of transactions that have occured, excluding balance checks, throughout all
                      // accounts; reset when reaches 5000
int activeWorkers = THREAD_COUNT;
bool waitForMain = true; // flag to make the worker and bank threads wait for signal from main
bool waitForBank = false; // flag to make the workers wait while the bank does its reward updates

// global mutexes:
pthread_mutex_t transactionCountLock = PTHREAD_MUTEX_INITIALIZER; // lock for accessing the transaction counter
pthread_mutex_t allWaitLock = PTHREAD_MUTEX_INITIALIZER; // lock for accessing the waitFormain flag
pthread_mutex_t activeWorkerThreadsLock = PTHREAD_MUTEX_INITIALIZER;

// global condition variables:
pthread_cond_t allWaitSignal = PTHREAD_COND_INITIALIZER; // to signal when worker and bank threads may start

// global barrier:
pthread_barrier_t allThreadsBarrier; // to sync the worker threads and allow the bank to start
pthread_barrier_t workerThreadsBarrier; // to stop all the worker threads until bank is done updating


int main(int argc, char** argv) {

    // check the usage:
    if (argc != 2) {
        printf("usage: ./bank <input file name");
        exit(EXIT_FAILURE);
    }

    // open the given input file:
    FILE *inputFile = fopen(argv[1], "r");
    // error checking:
    if (inputFile == NULL) {
        perror("Error. Failed to open file");
        exit(EXIT_FAILURE);
    }

    // used for getline() throughout main():
    char* line = NULL;
    size_t len;

    // get the number of accounts from first line of input file:
    getline(&line, &len, inputFile);
    accountCount = strtol(line, NULL, 10);

    // reset the file pointer to the top to get accurate line count, then count the lines
    // and subtract the first one and the ones in the account block
    fseek(inputFile, 0, SEEK_SET);
    int numTransactionLines = getNumLines(inputFile) - (accountCount * 5);
    // get through first line again:
    getline(&line, &len, inputFile);

    // allocate for account struct array:
    accounts = (account**) malloc(sizeof(account*)*accountCount);
    // error checking:
    if (accounts == NULL) {
        perror("Error. Failed to allocate memory for account struct array.");
        exit(EXIT_FAILURE);
    }
    // allocate for each account struct in array:
    for (int i = 0; i < accountCount; i++){
        accounts[i] = (account*) malloc(sizeof(account));
        // error checking:
        if (accounts[i] == NULL) {
            printf("Error. Failed to allocate memory for account struct at index %d.", i);
            exit(EXIT_FAILURE);
        }
        if (pthread_mutex_init(&accounts[i]->acctLock, NULL) != 0) {
            fprintf(stderr, "Error. Failed to init account mutex %d", i);
            exit(EXIT_FAILURE);
        }
    }

    int j = 0;
    commandLine acctNum, pw;
    // iterate over the account information lines per account in the account block:
    while (j < accountCount) {
        // skip over the index:
        getline(&line, &len, inputFile);
        
        // get the account number:
        getline(&line, &len, inputFile);
        acctNum = strFiller(line, "\n");
        memcpy(accounts[j]->acctNum, acctNum.cmdList[0], strlen(line)+1);
        freeCmdLine(&acctNum);
        
        // get the password:
        getline(&line, &len, inputFile);
        pw = strFiller(line, "\n");
        memcpy(accounts[j]->password, pw.cmdList[0], strlen(line)+1);
        freeCmdLine(&pw);
    

        // get the initial balance:
        getline(&line, &len, inputFile);
        accounts[j]->balance = strtod(line, NULL);

        // get the reward rate:
        getline(&line, &len, inputFile);
        accounts[j]->rewardRate = strtod(line, NULL);

        // init the transaction tracker to 0:
        accounts[j]->transactionTracker = 0;

        j++;
    }

    // for debugging account struct initialization:
    // FILE* structs1 = fopen("structs1.txt", "w");
    // for (int i = 0; i < accountCount; i++) {
    //     fprintf(structs1, "acct: %d\n", i);
    //     fprintf(structs1, "acctNum: %s\n", accounts[i]->acctNum);
    //     fprintf(structs1, "pw: %s\n", accounts[i]->password);
    //     fprintf(structs1, "balance: %f\n", accounts[i]->balance);
    //     fprintf(structs1, "rewardRate: %f\n\n", accounts[i]->rewardRate);
    // }
    // fclose(structs1);

    // allocate for the array of threadwork structs:
    threadwork **transactionSets = (threadwork**)malloc(sizeof(threadwork*)*THREAD_COUNT);
    // error checking:
    if (transactionSets == NULL) {
        perror("Error. Failed to allocate memory for threadwork struct array.");
        exit(EXIT_FAILURE);
    }
    // allocate for each threadwork struct and each array of transaction strings:
    int transactionsPerThread = numTransactionLines/THREAD_COUNT;
    int leftOvers = numTransactionLines - (transactionsPerThread*THREAD_COUNT);
    for (int i = 0; i < THREAD_COUNT; i++) {
        transactionSets[i] = (threadwork*)malloc(sizeof(threadwork));
        // error checking:
        if (transactionSets[i] == NULL) {
            printf("Error. Failed to allocate memory for threadwork struct at index %d.", i);
            exit(EXIT_FAILURE);
        }

        transactionSets[i]->lineCount = 0;
        transactionSets[i]->threadInd = i;
        transactionSets[i]->transactionLines = (char**)malloc(sizeof(char*)*(transactionsPerThread+1));
        // error checking:
        if (transactionSets[i]->transactionLines == NULL) {
            fprintf(stderr, "Error. Failed to allocate memory for transaction array in threadwork struct %d.", i);
            exit(EXIT_FAILURE);
        }
    }

    // iterate over the transaction lines in the file, giving the correct amount to each threadwork struct:
    for (int i = 0; i < THREAD_COUNT; i++) {
        for (int j = 0; j < transactionsPerThread; j++) {
            if (getline(&line, &len, inputFile) == -1) break;
            transactionSets[i]->transactionLines[j] = strdup(line);
            transactionSets[i]->lineCount++;
        }
    }

    // take any leftover transaction lines and spread them evenly among the threads:
    if (leftOvers != 0) {
        for (int i = 0; i < leftOvers; i++) {
            if (getline(&line, &len, inputFile) == -1) break;
            transactionSets[i]->transactionLines[transactionSets[i]->lineCount] = strdup(line);
            transactionSets[i]->lineCount++;
        }
    }

    // for debugging threadwork structs:
    // FILE* structs2 = fopen("structs2.txt", "w");
    // for (int i = 0; i < accountCount; i++) {
    //     for (int j = 0; j < transactionSets[i]->lineCount; j++) {
    //         fprintf(structs2, "%s", transactionSets[i]->transactionLines[j]);
    //     }
    // }
    // fclose(structs2);
    
    pthread_t threads[THREAD_COUNT];
    // Create threads
    for (int i = 0; i < THREAD_COUNT; i++) {
        if (pthread_create(&threads[i], NULL, processTransaction, transactionSets[i]) != 0) {
            fprintf(stderr, "Error. Failed to create thread %d", i);
            exit(EXIT_FAILURE);
        }
    }

    // create the bank thread:
    pthread_t bank;
    if (pthread_create(&bank, NULL, updateBalance, NULL) != 0) {
        fprintf(stderr, "Error. Failed to create bank thread");
        exit(EXIT_FAILURE);
    }

    pthread_mutex_lock(&allWaitLock);
    // init the allThreadsBarrier:
    if (pthread_barrier_init(&allThreadsBarrier, NULL, THREAD_COUNT+1) != 0) {
        perror("Error. Failed to init allThreadsBarrier");
        exit(EXIT_FAILURE);
    }
    // init the workerThreadsBarrier:
    if (pthread_barrier_init(&workerThreadsBarrier, NULL, THREAD_COUNT+1) != 0) {
        perror("Error. Failed to init workerThreadsBarrier");
        exit(EXIT_FAILURE);
    }
    // set the flag and signal the threads to continue:
    waitForMain = false;
    pthread_cond_broadcast(&allWaitSignal);
    pthread_mutex_unlock(&allWaitLock);

    // join all threads to make main thread wait:
    for (int i = 0; i < THREAD_COUNT; i++) {
        if (pthread_join(threads[i], NULL) != 0) {
            fprintf(stderr, "Error. thread join failed for thread %d", i);
            exit(EXIT_FAILURE);
        }
    }
    if (pthread_join(bank, NULL) != 0) {
        fprintf(stderr, "Error. thread join failed for bank thread");
        exit(EXIT_FAILURE);
    }
    
    // print end account balances:
    FILE* ro = fopen("real-output.txt", "w");
    for (int k = 0; k < accountCount; k++) {
        fprintf(ro, "%d balance:\t%.2f\n\n", k, accounts[k]->balance);
    }
    fclose(ro);

    // cleanup:
    fclose(inputFile);
    //free memory:
    free(line);
    for (int i = 0; i < accountCount; i++) {
        pthread_mutex_destroy(&accounts[i]->acctLock);
        free(accounts[i]);
    }
    free(accounts);
    for (int i = 0; i < THREAD_COUNT; i++) {
        free(transactionSets[i]->transactionLines);
        free(transactionSets[i]);
    }
    free(transactionSets);
    // destroy mutexes, cond variables, and barriers:
    pthread_mutex_destroy(&transactionCountLock);
    pthread_mutex_destroy(&allWaitLock);
    pthread_mutex_destroy(&activeWorkerThreadsLock);
    pthread_cond_destroy(&allWaitSignal);
    pthread_barrier_destroy(&allThreadsBarrier);
    pthread_barrier_destroy(&workerThreadsBarrier);

    exit(EXIT_SUCCESS);
}


/**
 * function called by worker threads to handle the specified transaction
*/
void *processTransaction(void* arg) { // a.k.a. processThreadWork(threadwork *tw);
    // wait on main to signal:
    pthread_mutex_lock(&allWaitLock);
    while (waitForMain) {
        pthread_cond_wait(&allWaitSignal, &allWaitLock);
    }
    pthread_mutex_unlock(&allWaitLock);

    // begin actual execution:
    threadwork *tw = (threadwork*) arg;

    commandLine transaction;
    int index;
    for (int i = 0; i < tw->lineCount; i++) {
        transaction = strFiller(tw->transactionLines[i], " ");
        // find index of account struct:
        index = findIndex(transaction.cmdList[1]);
        if (index < 0) return NULL;

        if (transaction.cmdList[0][0] == 'T') { // transfer
            transfer(
                     index, 
                     transaction.cmdList[2], 
                     transaction.cmdList[3], 
                     strtod(transaction.cmdList[4], NULL)
                    );
        } else if (transaction.cmdList[0][0] == 'C') { // balance check
            checkBalance(
                         index, 
                         transaction.cmdList[2]
                        );
        } else if (transaction.cmdList[0][0] == 'D') { // deposite
            deposit(
                    index,
                    transaction.cmdList[2],
                    strtod(transaction.cmdList[3], NULL)
                   );
        } else if (transaction.cmdList[0][0] == 'W') { // withdrawl
            withdraw(
                     index,
                     transaction.cmdList[2],
                     strtod(transaction.cmdList[3], NULL)
                    );
        } else { // unrecognized transaction type
            fprintf(
                    stderr,
                    "Error. Unknown transaction type: %s\n", 
                    transaction.cmdList[0]
                   );
        }
    }

    // mark another worker thread as complete:
    pthread_mutex_lock(&activeWorkerThreadsLock);
    printf("terminating worker thread\n");
    activeWorkers--;
    pthread_mutex_unlock(&activeWorkerThreadsLock);

    return NULL;
}

/**
 * function takes the index where source account located in accounts array, a password, a destination account number,
 * and an amount. subtracts the amount from the source account and adds it to its transaction tracker and the destination
 * account balance
*/
void transfer(int index, char* pw, char* destAcct, double amount) {
    // printf("transfer: %f from %s to %s\n", amount, accounts[index]->acctNum, destAcct);

    // check password:
    if (strcmp(accounts[index]->password, pw) != 0) {
        return;
    }
    
    // subtract amount from source account blance and add it to its tracker:
    updateAcctBalance(index, -amount);

    // update and check the global counter, update the account transaction tracker amount:
    countTrackAndCheck(index, amount);
    
    // add amount to destination account balance:
    int destInd = findIndex(destAcct);
    updateAcctBalance(destInd, amount);

    return;
}

/**
 * function takes the index where account located in accounts array and a password
 * prints the amount to the out file of the account
*/
void checkBalance(int index, char* pw) {
    // password check:
    if (strcmp(accounts[index]->password, pw) != 0) {
        return;
    }
    // printf("balance: %f\n", accounts[index]->balance);
    return;
}

/**
 * function takes the index where account located in accounts array, a password,
 * and an amount. adds the amount to its transaction tracker and the account balance
*/
void deposit(int index, char* pw, double amount) {
    // printf("deposit: %f in %s\n", amount, accounts[index]->acctNum);

    // password check:
    if (strcmp(accounts[index]->password, pw) != 0) {
        return;
    }

    // add amount to source account blance and its tracker:
    updateAcctBalance(index, amount);
    countTrackAndCheck(index, amount);

    return;
}

/**
 * function takes the index where source account located in accounts array, a password, and an amount. 
 * subtracts the amount from the account and adds it to its transaction tracker
*/
void withdraw(int index, char* pw, double amount) {
    // printf("withdraw: %f from %s\n", amount, accounts[index]->acctNum);

    // check password:
    if (strcmp(accounts[index]->password, pw) != 0) {
        return;
    }

    // subtract amount from source account blance and add it to its tracker:
    updateAcctBalance(index, -amount);
    countTrackAndCheck(index, amount);

    return;
}

/**
 * locks the account, makes an update based on amount passed in,
 * then unlocks it
*/
void updateAcctBalance(int index, double amount) {
    pthread_mutex_lock(&accounts[index]->acctLock);
    accounts[index]->balance += amount;
    pthread_mutex_unlock(&accounts[index]->acctLock);
}

/**
 * locks the global transaction counter, increments it
 * locks the given account, adds to its transaction tracker, unlocks account
 * checks if counter is at 5000, if it is: locks each account in turn, calculates its reward and adds it to global reward tracker,
 *                                          unlocks account, and resets counter
 * unlocks global transaction counter
*/
void countTrackAndCheck(int index, double amount) {
    // lock the transaction counter:
    pthread_mutex_lock(&transactionCountLock);
    // check if it has reached 5000:
    if (transactionCount == 5000) {
        // unlock it, call the barrier to sync with all other threads
        pthread_mutex_unlock(&transactionCountLock);
        pthread_barrier_wait(&allThreadsBarrier);
        // when last thread calls barrier, all threads will continue
        // call the workers barrier to stop all the workers:
        pthread_barrier_wait(&workerThreadsBarrier);
        // bank calls worker barrier after completing its work to continue their execution
        // reaquire the lock:
        pthread_mutex_lock(&transactionCountLock);
    }
    // update the transaction counter:
    transactionCount++;
    // lock the account and update its tracker according to the previous transaction:
    pthread_mutex_lock(&accounts[index]->acctLock);
    accounts[index]->transactionTracker += amount;
    pthread_mutex_unlock(&accounts[index]->acctLock);
    pthread_mutex_unlock(&transactionCountLock);
}

/**
 * adds the product of the reward rate and the total dollar amount of transactions according to the tracker
 * to the balance of each corresponding account
*/
void *updateBalance(void *arg) {
    // wait on main to signal:
    pthread_mutex_lock(&allWaitLock);
    while (waitForMain) {
        pthread_cond_wait(&allWaitSignal, &allWaitLock);
    }
    pthread_mutex_unlock(&allWaitLock);

    while (true) {

        pthread_mutex_lock(&activeWorkerThreadsLock);
        if (activeWorkers < 1) {
            pthread_mutex_unlock(&activeWorkerThreadsLock);
            break;
        }
        pthread_mutex_unlock(&activeWorkerThreadsLock);

        // wait for workers to all reach the transaction counter check and see that it's 5000:
        pthread_barrier_wait(&allThreadsBarrier);
        // bank will continue but workers hit another barrier
        
        // calculate and apply the reward to each account:
        double reward;
        for (int i = 0; i < accountCount; i++) {
            pthread_mutex_lock(&accounts[i]->acctLock);
            reward = accounts[i]->transactionTracker * accounts[i]->rewardRate;
            accounts[i]->balance += reward;
            FILE *fd = fopen("out.txt", "w");
            fprintf(fd, "applied reward\n");
            fclose(fd);
            pthread_mutex_unlock(&accounts[i]->acctLock);
        }

        // reset the global transaction counter:
        pthread_mutex_lock(&transactionCountLock);
        transactionCount = 0;
        pthread_mutex_unlock(&transactionCountLock);

        // restart the worker threads:
        pthread_barrier_wait(&workerThreadsBarrier);
    }

    return NULL;
}

/**
 * Finds and returns the index of the accounts array where the given account struct is stored
 * returns the index if it exists in the array and -1 otherwise
*/
int findIndex(char* acct) {
    int index = -1;
    for (int i = 0; i < accountCount; i++) {
        if (strcmp(accounts[i]->acctNum, acct) == 0) { // strings are the same
            index = i;
            break;
        }
    }

    // error checking:
    if (index < 0) {
        fprintf(
                stderr, 
                "Error. could not find account %s in account struct array.\n", 
                acct
               );
       return -1;
    }

    return index;
}

// returns number of lines in a file
int getNumLines(FILE* file) {
    int lineCount = 0;
    char ch;
    // count the newlines:
    while ((ch = fgetc(file)) != EOF) {
        if (ch == '\n') {
            lineCount++;
        }
    }

    // reset the file pointer tot the start:
    fseek(file, 0, SEEK_SET);

    return lineCount;
}
