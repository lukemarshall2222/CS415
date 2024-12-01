


// included headers:
#include "account.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

// function declararations:
void *processTransaction(void *arg);
void *updateBalance(void *arg);
void transfer(int index, char* pw, char* destAcct, double amount);
void checkBalance(int index, char* pw);
void deposit(int index, char* pw, double amount);
void withdraw(int index, char* pw, double amount);
int findIndex(char* acct);

// global variables:
account **accounts; // array of account structs
int accountCount; // total number of account structs being tracked
int transactionCount; // number of transactions that have occured, excluding balance checks, throughout all
                      // accounts; reset when reaches 5000


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
    // for (int i = 0; i < accountCount; i++) {
    //     printf("acct: %d\n", i);
    //     printf("acctNum: %s\n", accounts[i]->acctNum);
    //     printf("pw: %s\n", accounts[i]->password);
    //     printf("balance: %f\n", accounts[i]->balance);
    //     printf("rewardRate: %f\n\n", accounts[i]->rewardRate);
    // }
    
    commandLine transaction;
    // iterate over the transaction lines in the file:
    while (getline(&line, &len, inputFile) != -1) {
        transaction = strFiller(line, " ");
        processTransaction(&transaction);

        // cleanup:
        freeCmdLine(&transaction);

        // update the account balances according to their reward rates when transaction count reaches 5000:
        if (transactionCount == 5000) updateBalance(NULL);
    }

    // print end account balances:
    FILE* ro = fopen("real-output.txt", "w");
    for (int k = 0; k < accountCount; k++) {
        fprintf(ro, "%d balance:\t%.2f\n\n", k, accounts[k]->balance);
    }
    fclose(ro);

    // cleanup:
    free(line);
    fclose(inputFile);
    for (int i = 0; i < accountCount; i++) {
        free(accounts[i]);
    }
    free(accounts);

    exit(EXIT_SUCCESS);
}


/**
 * function called by worker threads to handle the specified transaction
*/
void *processTransaction(void *arg) {
    commandLine *transaction = (commandLine*) arg;
    // find index of account struct:
    int index = findIndex(transaction->cmdList[1]);
    if (index < 0) return NULL;

    if (transaction->cmdList[0][0] == 'T') { // tranfer
        transfer(
                 index, 
                 transaction->cmdList[2], 
                 transaction->cmdList[3], 
                 strtod(transaction->cmdList[4], NULL)
                );
    } else if (transaction->cmdList[0][0] == 'C') { // balance check
        checkBalance(
                     index, 
                     transaction->cmdList[2]
                    );
    } else if (transaction->cmdList[0][0] == 'D') { // deposite
        deposit(
                index,
                transaction->cmdList[2],
                strtod(transaction->cmdList[3], NULL)
               );
    } else if (transaction->cmdList[0][0] == 'W') { // withdrawl
        withdraw(
                 index,
                 transaction->cmdList[2],
                 strtod(transaction->cmdList[3], NULL)
                );
    } else { // unrecognized transaction type
        fprintf(
                stderr,
                "Error. Unknown transaction type: %s\n", 
                transaction->cmdList[0]
               );
    }

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
    accounts[index]->balance -= amount;
    accounts[index]->transactionTracker += amount;
    
    // add amount to destination account balance:
    int destInd = findIndex(destAcct);
    accounts[destInd]->balance += amount;

    transactionCount++;
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
    accounts[index]->balance += amount;
    accounts[index]->transactionTracker += amount;

    transactionCount++;
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
    accounts[index]->balance -= amount;
    accounts[index]->transactionTracker += amount;

    transactionCount++;
    return;
}

/**
 * adds the product of the reward rate and the total dollar amount of transactions according to the tracker
 * to the balance of each corresponding account
*/
void *updateBalance(void *arg) {

    // add to each account balance according to its reward rate:
    account *acct;
    for (int i = 0; i < accountCount; i++) {
        acct = accounts[i];
        acct->balance += (acct->rewardRate * acct->transactionTracker);
        accounts[i]->transactionTracker = 0;
    }

    // reset transaction count:
    transactionCount = 0;
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