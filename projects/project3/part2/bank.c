


// included headers:
#include "account.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

// defined values:
#define THREAD_COUNT 10

// function declararations:
void *processTransaction(void *arg);
void *updateBalance(void *arg);
void transfer(int index, char* pw, char* destAcct, double amount);
void checkBalance(int index, char* pw);
void deposit(int index, char* pw, double amount);
void withdraw(int index, char* pw, double amount);
int indexOf(char* acct);
int getNumLines(FILE *file);
void updateAcctBalance(int index, double amount);
void countTrackAndCheck(int index, double amount);

// global variables:
account **accounts; // array of account structs
int accountCount; // total number of account structs being tracked
int transactionCount; // number of transactions that have occured, excluding balance checks, throughout all
                      // accounts; reset when reaches 5000
double *rewardTracker;
int activeWorkerThreads = THREAD_COUNT;
int pipeFD[2];
int balanceChecks = 0;

// global mutexes and cond variables:
pthread_mutex_t transactionCountLock = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t activeWorkerThreadsLock = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t pipeLock = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t balanceChecksLock = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t workerSignal = PTHREAD_COND_INITIALIZER;


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

    // take any leftover transaction lines and give them to the threads:
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

    // allocate for the reward tracker:
    rewardTracker = (double*)malloc(sizeof(double)*accountCount);
    
    pipe(pipeFD);
    pid_t pid = fork();

    if (pid < 0) {
        perror("Error. Failed to fork process");
        exit(EXIT_FAILURE);
    } else if (pid == 0 ) { // child process
        close(pipeFD[1]);
        char buf[256];
        ssize_t amountRead;
        FILE *file;
        file = fopen("Ledger.txt", "w");
        fclose(file);
        while ((amountRead = read(pipeFD[0], buf, sizeof(buf)-1)) > 0) {
            buf[amountRead] = '\0';
            file = fopen("Ledger.txt", "a");
            fprintf(file, "%s", buf);
            fclose(file);
        }
    } else { // parent/main process
        pthread_t threads[THREAD_COUNT];
        // Create threads
        for (int i = 0; i < THREAD_COUNT; i++) {
            if (pthread_create(&threads[i], NULL, processTransaction, transactionSets[i]) != 0) {
                fprintf(stderr, "Error. Failed to create thread %d", i);
                exit(EXIT_FAILURE);
            }
        }

        pthread_t bank;
        if (pthread_create(&bank, NULL, updateBalance, NULL) != 0) {
            fprintf(stderr, "Error. Failed to create bank thread");
            exit(EXIT_FAILURE);
        }

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

        close(pipeFD[1]);
        

        // print end account balances:
        FILE* ro = fopen("output.txt", "w");
        for (int k = 0; k < accountCount; k++) {
            fprintf(ro, "%d balance:\t%.2f\n\n", k, accounts[k]->balance);
        }
        fclose(ro);
    }

    // cleanup:
    free(line);
    fclose(inputFile);
    for (int i = 0; i < accountCount; i++) {
        pthread_mutex_destroy(&accounts[i]->acctLock);
        free(accounts[i]);
    }
    free(accounts);
    for (int i = 0; i < THREAD_COUNT; i++) {
        for (int j = 0; j < transactionSets[i]->lineCount; j++) {
            free(transactionSets[i]->transactionLines[j]);
        }
        free(transactionSets[i]->transactionLines);
        free(transactionSets[i]);
    }
    free(transactionSets);
    free(rewardTracker);
    pthread_mutex_destroy(&transactionCountLock);
    pthread_mutex_destroy(&activeWorkerThreadsLock);
    pthread_mutex_destroy(&pipeLock);
    pthread_mutex_destroy(&balanceChecksLock);
    pthread_cond_destroy(&workerSignal);

    exit(EXIT_SUCCESS);
}


/**
 * function called by worker threads to handle the specified transaction
*/
void *processTransaction(void* arg) { // a.k.a. processThreadWork(threadwork *tw);
    threadwork *tw = (threadwork*) arg;

    commandLine transaction;
    int index;
    for (int i = 0; i < tw->lineCount; i++) {
        transaction = strFiller(tw->transactionLines[i], " ");
        // find index of account struct:
        index = indexOf(transaction.cmdList[1]);
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

        freeCmdLine(&transaction);
    }

    // threak workload is done, decrement the active threads count:
    pthread_mutex_lock(&activeWorkerThreadsLock);
    activeWorkerThreads--;
    if (activeWorkerThreads == 0) {
        pthread_cond_signal(&workerSignal);
    }
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
    int destInd = indexOf(destAcct);
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

    // get the local time:
    time_t currTime = time(NULL);
    struct tm *time = localtime(&currTime);
    char *timeBuf = asctime(time);

    pthread_mutex_lock(&balanceChecksLock);
    if (balanceChecks % 500 == 0) {
        char buf[256];
        sprintf(buf, "Worker checked the balance of account %s. Balance is %.2f. Check occured at %s", 
                    accounts[index]->acctNum, accounts[index]->balance, timeBuf);
        pthread_mutex_lock(&pipeLock);
        write(pipeFD[1], buf, strlen(buf));
        pthread_mutex_unlock(&pipeLock);
        balanceChecks = 0;
    }
    balanceChecks++;
    pthread_mutex_unlock(&balanceChecksLock);
    
    // fprintf("balance: %f\n", accounts[index]->balance);
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
    pthread_mutex_lock(&transactionCountLock);
    transactionCount++;

    pthread_mutex_lock(&accounts[index]->acctLock);
    accounts[index]->transactionTracker += amount;
    pthread_mutex_unlock(&accounts[index]->acctLock);

    if (transactionCount == 5000) {
        for (int i = 0; i < accountCount; i++) {
            pthread_mutex_lock(&accounts[i]->acctLock);
            printf("applying reward\n");
            rewardTracker[i] += accounts[i]->transactionTracker * accounts[i]->rewardRate;
            accounts[i]->transactionTracker = 0;
            pthread_mutex_unlock(&accounts[i]->acctLock);
        }
        transactionCount = 0;
        printf("continuing exection\n");
    }
    pthread_mutex_unlock(&transactionCountLock);
}

/**
 * adds the product of the reward rate and the total dollar amount of transactions according to the tracker
 * to the balance of each corresponding account
*/
void *updateBalance(void *arg) {
    pthread_mutex_lock(&activeWorkerThreadsLock);
    while (activeWorkerThreads > 0) {
        pthread_cond_wait(&workerSignal, &activeWorkerThreadsLock);
    }
    pthread_mutex_unlock(&activeWorkerThreadsLock);

    // add to each account balance according to its tracked reward:
    char buf[256];
    time_t currTime; 
    char *timeBuf;
    for (int i = 0; i < accountCount; i++) {
        printf("applying final reward\n");
        updateAcctBalance(i, rewardTracker[i]);

        // get the local time:
        currTime = time(NULL);
        timeBuf = asctime(localtime(&currTime));

        sprintf(buf, "Applied interest to account %s. New Balance: $%.2f. Time of Update: %s", 
                        accounts[i]->acctNum, accounts[i]->balance, timeBuf);
        pthread_mutex_lock(&pipeLock);
        write(pipeFD[1], buf, strlen(buf));
        pthread_mutex_unlock(&pipeLock);
    }

    return NULL;
}

/**
 * Finds and returns the index of the accounts array where the given account struct is stored
 * returns the index if it exists in the array and -1 otherwise
*/
int indexOf(char* acct) {
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
