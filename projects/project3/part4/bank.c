


// included headers:
#define _XOPEN_SOURCE 700
#include "account.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <stdbool.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <unistd.h>
#include <signal.h>


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
bool checkPassword(int index, char* pw);
account *copyAccts();
void updateSavings(int signo);
void terminateChild(int signo);

// global variables:
account **accounts; // array of account structs
int accountCount; // total number of account structs being tracked
int transactionCount; // number of transactions that have occured, excluding balance checks, throughout all
                      // accounts; reset when reaches 5000
int activeWorkerThreads = THREAD_COUNT;
int waitingWorkers = 0;
account *savingsAccts;
volatile sig_atomic_t parentFinished = 0;
int childPID;

// global mutexes:
pthread_mutex_t transactionCountLock = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t activeWorkerThreadsLock = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t waitingWorkerThreadsLock = PTHREAD_MUTEX_INITIALIZER;

// global cond variables:
pthread_cond_t workerSignal = PTHREAD_COND_INITIALIZER;
pthread_cond_t bankBeginSignal = PTHREAD_COND_INITIALIZER;
pthread_cond_t bankWorkingSignal = PTHREAD_COND_INITIALIZER;

// global barriers:
pthread_barrier_t mainBarrier;

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
    FILE *file;
    char buf[100];
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

        // init the output file:
        sprintf(buf, "account%d.txt", j);
        memcpy(accounts[j]->outFile, buf, (size_t)strlen(buf)+1);
        sprintf(buf, "Output/%s", accounts[j]->outFile);
        file = fopen(buf, "w");
        fprintf(file, "account %d:\n", j);
        fclose(file);

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

    int fd = open("/dev/zero", O_RDWR);
    if (fd < 1) {
        perror("Error. Open of shared file descriptor failed");
        exit(EXIT_FAILURE);
    }

    account *accountsForSavings = mmap(NULL, accountCount*sizeof(account), PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    // place the account structs in shared memory:
    account *acctCopies = copyAccts();
    memcpy(accountsForSavings, acctCopies, sizeof(account)*accountCount);
    free(acctCopies);

    sigset_t set;
    sigemptyset(&set);
    sigaddset(&set, SIGUSR2);
    pthread_sigmask(SIG_BLOCK, &set, NULL);


    // error checking:
    if (accountsForSavings == MAP_FAILED) {
        perror("Error. Failed to create shared memory");
        exit(EXIT_FAILURE);
    }

    pid_t pid = fork();
    // error checking:
    if (pid < 0) {
        perror("Error. failed to create child process");
        exit(EXIT_FAILURE);
    } else if (pid == 0) { // child process
        // copy the accounts locally:
        savingsAccts = (account*)malloc(sizeof(account)*accountCount);
        memcpy(savingsAccts, accountsForSavings, sizeof(account)*accountCount);

        // readjust the accounts for savings:
        for (int i = 0; i < accountCount; i++) {
            savingsAccts[i].balance *= 0.2;
            savingsAccts[i].rewardRate = 0.02;
        }

        // set up signals:

        struct sigaction sig;
        sig.sa_handler = updateSavings;
        sigemptyset(&sig.sa_mask);
        sig.sa_flags = 0;
        sigaction(SIGUSR1, &sig, NULL);
        
        sig.sa_handler = terminateChild;
        sigaction(SIGTERM, &sig, NULL);

        // init all the savings files:
        char buf[100];
        FILE *file;
        for (int i = 0; i < accountCount; i++) {
            sprintf(buf, "savings/%s", savingsAccts[i].outFile);
            file = fopen(buf, "w");
            fprintf(file, "account: %d\n", i);
            fprintf(file, "Current Savings Balance  %.2f\n", savingsAccts[i].balance);
            fclose(file);
        }

        // signal parent to start:
        kill(getppid(), SIGUSR2);

        while (parentFinished != 1) {
            pause();
        }
        free(savingsAccts);
        printf("child terminating\n");
    } else { // parent/main process
        childPID = pid;

        // wait for child to signal ready:
        int sig;
        sigwait(&set, &sig);

        // create the barrier:
        pthread_barrier_init(&mainBarrier, NULL, THREAD_COUNT+2);
        
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

        // signal the rest of the threads to continue:
        pthread_barrier_wait(&mainBarrier);
        printf("signaling workers and bank to start\n");

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

        // signal child to terminate:
        kill(childPID, SIGTERM);
        
        // print end account balances:
        FILE* out = fopen("output.txt", "w");
        for (int k = 0; k < accountCount; k++) {
            fprintf(out, "%d balance:\t%.2f\n\n", k, accounts[k]->balance);
        }
        fclose(out);
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
    pthread_mutex_destroy(&transactionCountLock);
    pthread_mutex_destroy(&activeWorkerThreadsLock);
    pthread_mutex_destroy(&waitingWorkerThreadsLock);
    pthread_cond_destroy(&workerSignal);
    pthread_cond_destroy(&bankBeginSignal);
    pthread_barrier_destroy(&mainBarrier);
    munmap(accountsForSavings, sizeof(account)*accountCount);

    exit(EXIT_SUCCESS);
}

void updateSavings(int signo) {
    char buf[100]; 
    FILE *savingsFile;
    for (int i = 0; i < accountCount; i++) {
        printf("updating savings account %d\n", i);
        savingsAccts[i].balance += savingsAccts[i].balance * savingsAccts[i].rewardRate;
        sprintf(buf, "savings/%s", savingsAccts[i].outFile);
        savingsFile = fopen(buf, "a");
        fprintf(savingsFile, "Current Savings Balance  %.2f\n", savingsAccts[i].balance);
        fclose(savingsFile);
    }
    
}

void terminateChild(int signo) {
    printf("parent signaling to terminate child\n");
    parentFinished = 1;
}


/**
 * function called by worker threads to handle the specified transaction
*/
void *processTransaction(void* arg) { // a.k.a. processThreadWork(threadwork *tw);
    // wait for main to signal to continue:
    printf("worker waiting for signal from main\n");
    pthread_barrier_wait(&mainBarrier);

    threadwork *tw = (threadwork*) arg;
    printf("signal received, beginning worker %d execution\n", tw->threadInd);

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
    pthread_mutex_lock(&waitingWorkerThreadsLock);
    activeWorkerThreads--;
    if (activeWorkerThreads == 0 || activeWorkerThreads == waitingWorkers) {
        pthread_cond_signal(&bankBeginSignal);
    }
    pthread_mutex_unlock(&waitingWorkerThreadsLock);
    pthread_mutex_unlock(&activeWorkerThreadsLock);
    printf("terminating worker thread %d\n", tw->threadInd);
    pthread_exit(NULL);
}

/**
 * function takes the index where source account located in accounts array, a password, a destination account number,
 * and an amount. subtracts the amount from the source account and adds it to its transaction tracker and the destination
 * account balance
*/
void transfer(int index, char* pw, char* destAcct, double amount) {
    // printf("transfer: %f from %s to %s\n", amount, accounts[index]->acctNum, destAcct);

    // check password:
    if (!checkPassword(index, pw)) {
        // fprintf(stderr, "invalid password, transaction rejected.\n");
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
    if (!checkPassword(index, pw)) {
        // fprintf(stderr, "invalid password, transaction rejected.\n");
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
    if (!checkPassword(index, pw)) {
        // fprintf(stderr, "invalid password, transaction rejected.\n");
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
    if (!checkPassword(index, pw)) {
        // fprintf(stderr, "invalid password, transaction rejected.\n");
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
    // check if time for reward:
    pthread_mutex_lock(&transactionCountLock);
    if (transactionCount == 5000) {
        // check if current thread is last to get to this point:
        pthread_mutex_lock(&activeWorkerThreadsLock);
        pthread_mutex_lock(&waitingWorkerThreadsLock);
        waitingWorkers++;
        if (activeWorkerThreads == waitingWorkers) {
            // reset the waiting workers, signal the bank to begin, and wait for its signal back to continue:
            waitingWorkers = 0;
            pthread_mutex_unlock(&waitingWorkerThreadsLock);
            pthread_mutex_unlock(&activeWorkerThreadsLock);
            pthread_cond_signal(&bankBeginSignal);
            pthread_cond_wait(&bankWorkingSignal, &transactionCountLock);
        } else {
            pthread_mutex_unlock(&waitingWorkerThreadsLock);
            pthread_mutex_unlock(&activeWorkerThreadsLock);
            pthread_cond_wait(&bankWorkingSignal, &transactionCountLock);
        }
    }
    transactionCount++;
    pthread_mutex_unlock(&transactionCountLock);

    pthread_mutex_lock(&accounts[index]->acctLock);
    accounts[index]->transactionTracker += amount;
    pthread_mutex_unlock(&accounts[index]->acctLock);
}

/**
 * adds the product of the reward rate and the total dollar amount of transactions according to the tracker
 * to the balance of each corresponding account
*/
void *updateBalance(void *arg) {
    // wait for main to signal to continue:
    printf("bank waiting for signal from main\n");
    pthread_barrier_wait(&mainBarrier);
    printf("signal received, bank beginning execution\n");

    while (true) {
        pthread_mutex_lock(&activeWorkerThreadsLock);
        // wait for signal from worker to begin:
        pthread_cond_wait(&bankBeginSignal, &activeWorkerThreadsLock);

        // --- transactions hit 5000 ---

        // signal the savings bank:
        kill(childPID, SIGUSR1);

        // reset the trasaction count
        pthread_mutex_lock(&transactionCountLock);
        transactionCount = 0;
        pthread_mutex_unlock(&transactionCountLock);

        // calculate and apply all the rewards:
        double reward;
        FILE *fd;
        char buf[100];
        for (int i = 0; i < accountCount; i++) {
            pthread_mutex_lock(&accounts[i]->acctLock);
            reward = accounts[i]->transactionTracker * accounts[i]->rewardRate;
            printf("applying reward\n");
            accounts[i]->balance += reward;
            accounts[i]->transactionTracker = 0;

            // write the balance update to the corresponding file:
            sprintf(buf, "Output/%s", accounts[i]->outFile);
            fd = fopen(buf, "a");
            fprintf(fd, "Current Balance:\t\t\t%.2f\n", accounts[i]->balance);
            fclose(fd);
            pthread_mutex_unlock(&accounts[i]->acctLock);
        }

        printf("broadcasting restart signal\n");
        pthread_cond_broadcast(&bankWorkingSignal);

        // --- no more active workers ---

        // if no workers active, terminate:
        if (activeWorkerThreads < 1) {
            pthread_mutex_unlock(&activeWorkerThreadsLock);
            break;
        }
        pthread_mutex_unlock(&activeWorkerThreadsLock);

        // back to top to wait for next bank begin signal
    }
    printf("bank thread terminating\n");
    pthread_exit(NULL);
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
    while (!feof(file)) {
        ch = fgetc(file);
        if (ch == '\n') {
            lineCount++;
        }
    }

    // reset the file pointer tot the start:
    fseek(file, 0, SEEK_SET);

    return lineCount;
}

bool checkPassword(int index, char* pw) {
    bool correct;
    pthread_mutex_lock(&accounts[index]->acctLock);
    correct = strcmp(accounts[index]->password, pw) == 0;
    pthread_mutex_unlock(&accounts[index]->acctLock);
    return correct;
}

account *copyAccts() {
    account *accountArr = (account*)malloc(sizeof(account)*accountCount);
    for (int i = 0; i < accountCount; i++) {
        memcpy(&accountArr[i], accounts[i], sizeof(account));
    }
    return accountArr;
}
