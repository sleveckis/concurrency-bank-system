#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "string_parser.h"
#include "account.h"
#include <pthread.h>
#include <sys/types.h>
#include <sys/syscall.h>
#include <unistd.h>

#define _GNU_SOURCE

#ifndef SYS_gettid
#error "SYS_gettid unavailable on this system"
#endif

#define gettid() ((pid_t)syscall(SYS_gettid))

typedef struct{
	int verif;
	int index;
}verIn;

/* Part 1 */
void printAccs(int numAccs, account *accList);
void withdraw(int index, account *accList, double amt);
void deposit(int index, account *accList, double amt);
void transfer(int index, account *accList, double amt, char *acc2, int numAccs);
void printBalances(account *accList, int numAccs);
verIn verify(int numAccs, account *accList, char *acc, char *pw);

/* Part 2 */
void* process_transaction(void *arg);
void* update_balance(void *arg);
void setUpAccounts(char *linebuf, size_t *len, FILE *file);
int calcNumTransactions(char *linebuf, size_t *len, FILE *file);
void advanceFilePointerToTransactions(char *linebuf, size_t *len, FILE *file);
void check(int index);

/* Part 2 Global Vars */
account *accList;
int MAX_THREAD = 10;
int numAccs;
int numTrans = 0;
int invalids = 0, deposits = 0, withdraws = 0, transfers = 0, checks = 0;
pthread_t tid[10];

/* Part 3 Global Vars */
int tcount = 0;
pthread_mutex_t lock;
int counter = 0;
int updateCount = 0;
int waiting_thread_count = 0;
pthread_t extra;
int aliveWorkers = 10;

pthread_barrier_t sync_barrier;
pthread_mutex_t mtx = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t condition = PTHREAD_COND_INITIALIZER;


int main(int argc, char *argv[]){
	size_t len = 0;
	char *linebuf = NULL;
	FILE *file = fopen(argv[1], "r");

	/* Part 3: remove old output files */
	for (int i = 0; i < MAX_THREAD; i++){
		char buffer[50];
		snprintf(buffer, sizeof(buffer), "./output/account%d.txt",  i);
		remove(buffer);
	}

	/* Get first line, # of accounts */
	getline(&linebuf, &len, file);
	numAccs = atoi(linebuf);

	accList = (account*)malloc(sizeof(account) * numAccs);
	
	/* Set up account information  - go through the first portion of the file*/
	setUpAccounts(linebuf, &len, file);

	/* Calculate number of transactions  - go through the rest of the file*/
	numTrans = calcNumTransactions(linebuf, &len, file);

	/* Reset file pointer to begining of input file */
	rewind(file);

	/* Move file pointer to be at start of "transactions" portion
	 * of input  file */
	advanceFilePointerToTransactions(linebuf, &len, file);

	/* Save all transactions into a char* array */
	char *allTransactions[numTrans]; 
	memset(allTransactions, 0, numTrans*sizeof(allTransactions[0]));
	for(int i = 0; i < numTrans; i++){
		allTransactions[i] = NULL;
		len = 0;
		getline(&allTransactions[i], &len, file);
	}

	/* Save those into a command_line array */
	command_line cmT[numTrans];
	for(int i = 0; i < numTrans; i++){
		cmT[i] = str_filler(allTransactions[i], " ");
	}


	/* Init locks */
	for(int i = 0; i < MAX_THREAD; i++){
		pthread_mutex_init(&accList[i].ac_lock, NULL);
	}

	/* PART 3 INIT  */
	if(pthread_mutex_init(&lock, NULL) !=0){
		printf("Mutex: 'lock' has failed to init\n");
		//return 1;
	}

	pthread_barrier_init(&sync_barrier, NULL, MAX_THREAD +2);


	/* Processing Thread creation */
	/* Segment transaction array into MAX_THREAD pieces */
	int error = 0;
	for(int i = 0; i < MAX_THREAD; i++){

		int b = i * (numTrans / MAX_THREAD);

		error = pthread_create(&(tid[i]), NULL, process_transaction, (cmT +b));

		if(error !=0){
			printf("Thread can't be created :[%s]\n", strerror(error));
		}
	}

	/* PART 3 THREAD CREATE */
	error = pthread_create(&extra, NULL, &update_balance, NULL);
	if(error != 0)
		printf("Thread no create :[%s]\n", strerror(error));



	/* PART 3 - BARRIER WAIT */
	pthread_barrier_wait(&sync_barrier);




	/* Join all processing threads at termination */
	for (int i = 0; i < MAX_THREAD; i++){
		pthread_join(tid[i], NULL);
	}

	/* PART 3 - JOIN BANKER THREAD */
	void *accUpdateCount_void;
	pthread_join(extra, &accUpdateCount_void);
	int *accUpdateCount = (int*)accUpdateCount_void;

	/* OLD PART 2 CODE BELOW: IGNORE */
	/* Banking thread creation and join*/

	/* old part 2 code 
	pthread_t bank_tid;
	int bank_thread = pthread_create(&bank_tid, NULL, update_balance, NULL);
	if(bank_thread != 0)
		printf("Thread no create :[%s]\n", strerror(error));

	*/

	/* Get return value from update_balance */

	/* old part 2 code 
	void *accUpdateCount_void;
	pthread_join(bank_tid, &accUpdateCount_void);
	int *accUpdateCount = (int*)accUpdateCount_void;
	*/



	/* print em */
	printBalances(accList, numAccs);

	/* Part 1 */
	free(linebuf);
	free(accList);
	fclose(file);

	/* Part 2 */
	for (int i = 0; i < numTrans; i++){
		free(allTransactions[i]);
	}
}


void* process_transaction(void *arg){

	/* New for part 3*/
	printf("Working thread: %d created but waiting\n", gettid());

	pthread_barrier_wait(&sync_barrier);

	printf("Working thread: %d now working \n", gettid());

	command_line *allTrans = (command_line*)arg;
	/* End new section */

	for(int i = 0; i < (numTrans/10); i++){

		command_line transaction = allTrans[i];

		verIn ver = verify(numAccs, accList, transaction.command_list[1], transaction.command_list[2]);	


		if(strcmp("D", transaction.command_list[0]) == 0 && ver.verif){
			deposit(ver.index, accList, atof(transaction.command_list[3]));
			deposits++;

		} else if(strcmp("W", transaction.command_list[0]) == 0 && ver.verif){
			withdraw(ver.index, accList, atof(transaction.command_list[3]));
			withdraws++;

		} else if(strcmp("T", transaction.command_list[0]) ==0 && ver.verif){
			transfer(ver.index, accList, atof(transaction.command_list[4]), transaction.command_list[3], numAccs);
			transfers++;

		} else if(strcmp("C", transaction.command_list[0]) ==0 && ver.verif){
			check(ver.index);
			checks++;

		} else if(!ver.verif){
			invalids++;

		} else {
			printf("Extreme error\n");
		}

		free_command_line(&transaction);
		memset(&transaction, 0, 0);

		/* Begin new part 3 section */
		if(tcount >= 5000){
			pthread_mutex_lock(&mtx);

			waiting_thread_count++;
			pthread_cond_wait(&condition, &mtx);

			pthread_mutex_unlock(&mtx);
		}
		/* End new part 3 section */


		
	}

	aliveWorkers--;
	printf("Worker thread %d done \n", gettid());
	pthread_exit(NULL);
}

void* update_balance(void *arg){

	/* PART 3 */
	printf("Periodic Thread: %d created but waiting\n", gettid());
	pthread_barrier_wait(&sync_barrier);
	printf("Periodic Thread: %d started working\n", gettid());

	/* This is the "how many times each account's been updated" array */
	static int r[10] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0};

	/* PART 3 */
	while(1){
		if(waiting_thread_count >= aliveWorkers ){	

			pthread_mutex_lock(&mtx);

			for(int i = 0; i < numAccs; i++){
				pthread_mutex_lock(&accList[i].ac_lock);
				accList[i].balance += (accList[i].reward_rate * accList[i].transaction_tracter);
				accList[i].transaction_tracter = 0;
				FILE *fp;
				char buffer[50];
				snprintf(buffer, sizeof(buffer), "./output/account%d.txt", i);
				fp = fopen(buffer, "ab");
				fprintf(fp, "Current balance: %.2f\n", accList[i].balance);
				fclose(fp);

				r[i]++;
				if (i == 0)
					printf("Total updates: %d\n", r[0]);
				pthread_mutex_unlock(&accList[i].ac_lock);
			}

			waiting_thread_count = 0;
			tcount = 0;
			updateCount++;
			pthread_cond_broadcast(&condition);

			pthread_mutex_unlock(&mtx);

		}
		/* exit after enough iterations */
		if(updateCount >=((numTrans-checks-invalids)/5000) ){
			break;
		}


	}
	

	return r;

}


void printAccs(int numAccs, account *accList){
	for(int i = 0; i < numAccs;i++){
		printf("%d acc: %s ", i, accList[i].account_number);
		printf("pw: %s ", accList[i].password);
		printf("bal: %f ", accList[i].balance);
		printf("rw rt: %f ", accList[i].balance);
		printf("tt: %f", accList[i].transaction_tracter);
		printf("hwfdflsj");
		printf("\n");
	}

}

verIn verify(int numAccs, account *accList, char *acc, char *pw){
	verIn ret;
	ret.verif = 0;
	for(int i = 0; i < numAccs; i++){
		if(strcmp(accList[i].account_number, acc) == 0 && strcmp(accList[i].password, pw) ==0){
			ret.index = i;
			ret.verif = 1;
		}
	}
	return ret;
}

void deposit(int index, account *accList, double amt){
	pthread_mutex_lock(&accList[index].ac_lock);
	accList[index].balance += amt;
	accList[index].transaction_tracter += amt;
	pthread_mutex_unlock(&accList[index].ac_lock);

	pthread_mutex_lock(&lock);
	tcount++;
	pthread_mutex_unlock(&lock);
}

void withdraw(int index, account *accList, double amt){
	
	pthread_mutex_lock(&accList[index].ac_lock);
	accList[index].balance -= amt;
	accList[index].transaction_tracter += amt;
	pthread_mutex_unlock(&accList[index].ac_lock);

	pthread_mutex_lock(&lock);
	tcount++;
	pthread_mutex_unlock(&lock);

}

void check(int index){
	/*
	FILE *fp;
	char buffer[50];
	snprintf(buffer, sizeof(buffer), "account%d_balance.txt", index);
	fp = fopen(buffer, "ab");
	pthread_mutex_lock(&accList[index].ac_lock);
	fprintf(fp, "Current balance: %.2f\n", accList[index].balance);
	pthread_mutex_unlock(&accList[index].ac_lock);
	fclose(fp);
	*/
}

void transfer(int index, account *accList, double amt, char *acc2, int numAccs){

	pthread_mutex_lock(&accList[index].ac_lock);
	accList[index].balance -= amt;
	accList[index].transaction_tracter += amt;
	pthread_mutex_unlock(&accList[index].ac_lock);

	for(int i = 0; i < numAccs; i++){

		if(strcmp(accList[i].account_number, acc2) == 0){

			pthread_mutex_lock(&accList[i].ac_lock);
			accList[i].balance += amt;
			pthread_mutex_unlock(&accList[i].ac_lock);

		}
	}
	pthread_mutex_lock(&lock);
	tcount++;
	pthread_mutex_unlock(&lock);

}


void printBalances(account *accList, int numAccs){
	FILE *fp;
	fp = fopen("./output/output.txt", "w");
	for(int i = 0; i < numAccs; i++){
		fprintf(fp, "%d ", i);
		fprintf(fp, "balance:\t");
		fprintf(fp, "%.2f\n\n", accList[i].balance);
	}
	fclose(fp);
}

void setUpAccounts(char *linebuf, size_t *len, FILE *file){
	for(int i = 0; i < numAccs; i++){
		/* skip the "index" line */
		getline(&linebuf, len, file);

		getline(&linebuf, len, file);
		linebuf[strcspn(linebuf, "\n")] = 0;
		strcpy(accList[i].account_number, linebuf);

		getline(&linebuf, len, file);
		linebuf[strcspn(linebuf, "\n")] = 0;
		strcpy(accList[i].password, linebuf);

		getline(&linebuf, len, file);
		accList[i].balance = atof(linebuf);

		getline(&linebuf, len, file);
		accList[i].reward_rate = atof(linebuf);

		accList[i].transaction_tracter = 0;
	}
}

int calcNumTransactions(char *linebuf, size_t *len, FILE *file){
	int numTrans = 0;

	while(getline(&linebuf, len, file)!= -1){
		numTrans++;
	}
	return numTrans;

}


/* Kind of hacky, but this gets our file pointer back to the start
 * of the transactions portion of our input file */
void advanceFilePointerToTransactions(char *linebuf, size_t *len, FILE *file){
	for(int i = 0; i < numAccs; i++){
		getline(&linebuf, len, file);
		getline(&linebuf, len, file);
		getline(&linebuf, len, file);
		getline(&linebuf, len, file);
		getline(&linebuf, len, file);

	}

	/* We need one more line for some reason */
	getline(&linebuf, len, file);
}
