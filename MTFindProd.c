/*
 * Reynolds, Jake
 * CSC 139 Section 5
 * OSs Tested on: Windows 10, Linux
 * Windows 10 Desktop: Intel(R) Core(TM) i7-3820 CPU @ 3.60GHz,  4 cores
 * Windows 10 Laptop:  Intel(R) Core(TM) i5-4200U CPU @ 1.60GHz, 2 cores
 * Linux Machine:
*/

/*
 * Compile options:
 * g++ -O3 MTFindProd.c -o MTFindProd -lpthread
*/
#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <sys/timeb.h>
#include <semaphore.h>
#include <stdbool.h> //CLion complained

#define MAX_SIZE 100000000
#define MAX_THREADS 16
#define RANDOM_SEED 7665
#define MAX_RANDOM_NUMBER 5000
#define NUM_LIMIT 9973

// Global variables
long gRefTime; //For timing
int gData[MAX_SIZE]; //The array that will hold the data

int gThreadCount; //Number of threads
int gDoneThreadCount; //Number of threads that are done at a certain point. Whenever a thread is done, it increments this. Used with the semaphore-based solution
int gThreadProd[MAX_THREADS]; //The modular product for each array division that a single thread is responsible for
bool gThreadDone[MAX_THREADS]; //Is this thread done? Used when the parent is continually checking on child threads

// Semaphores
sem_t completed; //To notify parent that all threads have completed or one of them found a zero
sem_t mutex; //Binary semaphore to protect the shared variable gDoneThreadCount

int SqFindProd(int size); //Sequential FindProduct (no threads) computes the product of all the elements in the array mod NUM_LIMIT
void* ThFindProd(void *param); //Thread FindProduct but without semaphores
void* ThFindProdWithSemaphore(void *param); //Thread FindProduct with semaphores
int ComputeTotalProduct(); //Multiply the division products to compute the total modular product
void InitSharedVars(); //Initializes default values to gThreadDone, gThreadProd, gDoneThreadCount
void GenerateInput(int size, int indexForZero); //Generate the input array
void CalculateIndices(int arraySize, int thrdCnt, int indices[MAX_THREADS][3]); //Calculate the indices to divide the array into T divisions, one division per thread
int GetRand(int min, int max); //Get a random number between min and max

//Debugging Functions
void printIndices(int (*indices)[3]); //Displays indices array. Used for debugging.

//Timing functions
long GetMilliSecondTime(struct timeb timeBuf);
long GetCurrentTime(void);
void SetTime(void);
long GetTime(void);

int main(int argc, char *argv[]){

	pthread_t tid[MAX_THREADS];
	pthread_attr_t attr[MAX_THREADS];
	int indices[MAX_THREADS][3];
	int i, indexForZero, arraySize, prod;

	/************************************Code for parsing and checking command-line arguments *************************/
	if(argc != 4){
		fprintf(stderr, "Invalid number of arguments!\n");
		exit(-1);
	}
	if((arraySize = atoi(argv[1])) <= 0 || arraySize > MAX_SIZE){
		fprintf(stderr, "Invalid Array Size\n");
		exit(-1);
	}
	gThreadCount = atoi(argv[2]);
	if(gThreadCount > MAX_THREADS || gThreadCount <=0){
		fprintf(stderr, "Invalid Thread Count\n");
		exit(-1);
	}
	indexForZero = atoi(argv[3]);
	if(indexForZero < -1 || indexForZero >= arraySize){
		fprintf(stderr, "Invalid index for zero!\n");
		exit(-1);
	}

	//Initializing arrays: gData and indices
    GenerateInput(arraySize, indexForZero);
    CalculateIndices(arraySize, gThreadCount, indices);

    //Used for debugging
    //printIndices(indices);

	/*****************************************Single Thread: Sequential************************************************/
	SetTime();
	prod = SqFindProd(arraySize);
	printf("Sequential multiplication completed in %ld ms. Product = %d\n", GetTime(), prod);

	/*****************************************MT: Parent waits for all children****************************************/
	InitSharedVars();
	SetTime();

    // Initialize attributes, and create threads
    for (i = 0; i < gThreadCount; i++)
    {
        // Initialize thread attributes
        pthread_attr_init(&attr[i]);
        // Create threads
        pthread_create(&tid[i], &attr[i], ThFindProd, (void*) indices[i]);
    }

    // Parent waits for all threads using pthread_join
    for (i = 0; i < gThreadCount; i++)
    {
        pthread_join(tid[i], NULL);
    }

    prod = ComputeTotalProduct();
	printf("Threaded multiplication with parent waiting for all children completed in %ld ms. Product = %d\n", GetTime(), prod);

	/*************************************MT: Parent checks on children using busy-waiting loop************************/
	InitSharedVars();
	SetTime();

    // Initialize attributes, and create threads
    for (i = 0; i < gThreadCount; i++)
    {
        // Initialize thread attributes
        pthread_attr_init(&attr[i]);
        // Create threads
        pthread_create(&tid[i], &attr[i], ThFindProd, (void*) indices[i]);
    }

    // Parent continually check on all child threads
    volatile bool foundZero, allDone;
    do{
        foundZero = false;
        allDone = true;
        for(i = 0; i < gThreadCount && !foundZero; i++)
        {
            // Condition that checks if all threads are done
            // allDone == true && gThreadDone[0] && gThreadDone[1] && ... && gThreadDone[gThreadCount-1]
            allDone &= gThreadDone[i];
            if (gThreadDone[i])
            {
                //Cancel all threads if zero product is found
                if (gThreadProd[i] == 0)
                {
                    foundZero = true;
                    for (i = 0; i < gThreadCount; i++)
                    {
                        pthread_cancel(tid[i]);
                    }
                }
                else
                {
                    pthread_join(tid[i], NULL);
                }
            }
        }
    }while(!foundZero && !allDone);

    prod = ComputeTotalProduct();
	printf("Threaded multiplication with parent continually checking on children completed in %ld ms. Product = %d\n", GetTime(), prod);

	/*********************************************MT: With Semaphores**************************************************/

	InitSharedVars();
	// Initialize semaphores
    sem_init(&mutex, 0, 1);
    sem_init(&completed, 0, 1);
    SetTime();
    // Initialize and create threads
    for (i = 0; i < gThreadCount; i++)
    {
        // Initialize thread attributes
        pthread_attr_init(&attr[i]);
        // Create threads
        pthread_create(&tid[i], &attr[i], ThFindProdWithSemaphore, (void*) indices[i]);
    }
    // Parent waits on the "completed" semaphore
	sem_wait(&completed);
    for (i = 0; i < gThreadCount; i++)
    {
        pthread_join(tid[i], NULL);
    }
    prod = ComputeTotalProduct();
	printf("Threaded multiplication with parent waiting on a semaphore completed in %ld ms. Product = %d\n", GetTime(), prod);
}

void printIndices(int (*indices)[3]){
    int i;
    printf("%-5s%-10s%-10s\n", "Thd", "Start", "End");
    for (i = 0; i < gThreadCount; i++)
    {
        printf("%-5d%-10d%-10d\n", indices[i][0], indices[i][1], indices[i][2]);
    }
}

// A regular sequential function to multiply all the elements in gData mod NUM_LIMIT
// REMEMBER TO MOD BY NUM_LIMIT AFTER EACH MULTIPLICATION TO PREVENT YOUR PRODUCT VARIABLE FROM OVERFLOWING
int SqFindProd(int size) {
    int i, prod = 1;

    for (i = 0; i < size ; i++)
    {
        if (gData[i] == 0)
        {
            //"Your single thread multiplier must terminate as soon as a zero is found"
            // prod = 0
            return 0;
        }
        else
        {
            prod = (prod * gData[i]) % NUM_LIMIT;
        }
    }
    return prod;
}

// Thread function that computes the product of all the elements in one division of the array mod NUM_LIMIT
// REMEMBER TO MOD BY NUM_LIMIT AFTER EACH MULTIPLICATION TO PREVENT YOUR PRODUCT VARIABLE FROM OVERFLOWING
// When it is done, this function should store the product in gThreadProd[threadNum] and set gThreadDone[threadNum] to true
void* ThFindProd(void *param) {
	int threadNum   = ((int*)param)[0];
	int start       = ((int*)param)[1];
	int end         = ((int*)param)[2];
	int prod        = 1;
    int i;

	for (i = start; i <= end; i++)
    {
        if (gData[i] == 0)
        {
            prod = 0;
            break;
        }
        else
        {
            prod = (prod * gData[i]) % NUM_LIMIT;
        }
    }
	gThreadProd[threadNum] = prod;
	gThreadDone[threadNum] = true;
	return NULL;
}

// Thread function that computes the product of all the elements in one division of the array mod NUM_LIMIT
// REMEMBER TO MOD BY NUM_LIMIT AFTER EACH MULTIPLICATION TO PREVENT YOUR PRODUCT VARIABLE FROM OVERFLOWING
// When it is done, this function should store the product in gThreadProd[threadNum]
// If the product value in this division is zero, this function should post the "completed" semaphore
// If the product value in this division is not zero, this function should increment gDoneThreadCount and
// post the "completed" semaphore if it is the last thread to be done
// Don't forget to protect access to gDoneThreadCount with the "mutex" semaphore
void* ThFindProdWithSemaphore(void *param) {
    int threadNum   = ((int*)param)[0];
    int start       = ((int*)param)[1];
    int end         = ((int*)param)[2];
    int prod        = 1;
    int i;

    for (i = start; i <= end; i++)
    {
        if (gData[i] == 0)
        {
            prod = 0;
            break;
        }
        else
        {
            prod = (prod * gData[i]) % NUM_LIMIT;
        }
    }
    gThreadProd[threadNum] = prod;

    if (prod == 0) {
        sem_post(&completed);
    }

    sem_wait(&mutex);
    if (++gDoneThreadCount == gThreadCount-1)
    {
        sem_post(&completed);
    }
    sem_post(&mutex);
    return NULL;
}

int ComputeTotalProduct() {
    int i, prod = 1;

	for(i=0; i<gThreadCount; i++)
	{
		prod *= gThreadProd[i];
		prod %= NUM_LIMIT;
	}

	return prod;
}

void InitSharedVars() {
	int i;

	for(i=0; i<gThreadCount; i++){
		gThreadDone[i] = false;
		gThreadProd[i] = 1;
	}
	gDoneThreadCount = 0;
}

// Function that fills the gData array with random numbers between 1 and MAX_RANDOM_NUMBER
// If indexForZero is valid and non-negative, set the value at that index to zero
void GenerateInput(int size, int indexForZero) {
	int i;

	//Set seed for GetRand()
    srand(RANDOM_SEED);

    //Fill the array with random data from 1, 5000
    for (i = 0; i < size; i++)
    {
        if (i == indexForZero)
        {
            gData[i] = 0;
        }
        else
        {
            gData[i] = GetRand(1, MAX_RANDOM_NUMBER);
        }
    }
}

// Function that calculates the indices to divide the array into thrdCnt equal divisions
// For each division i:
// indices[i][0] should be set to the division number i,
// indices[i][1] should be set to the start index
// indices[i][2] should be set to the end index
void CalculateIndices(int arraySize, int thrdCnt, int indices[MAX_THREADS][3]) {
    int i           =   0;
    int partOffset  =   arraySize / thrdCnt;
    int start       =   0;
    int end         =   partOffset-1;

    for (/*NULL*/; i < thrdCnt; i++, start += partOffset, end += partOffset)
    {
        indices[i][0] = i;
        indices[i][1] = start;
        indices[i][2] = end;
    }
}

// Get a random number in the range [x, y]
int GetRand(int x, int y) {
    int r = rand();
    r = x + r % (y-x+1);
    return r;
}

long GetMilliSecondTime(struct timeb timeBuf){
	long mliScndTime;
	mliScndTime = timeBuf.time;
	mliScndTime *= 1000;
	mliScndTime += timeBuf.millitm;
	return mliScndTime;
}

long GetCurrentTime(void){
	long crntTime=0;
	struct timeb timeBuf;
	ftime(&timeBuf);
	crntTime = GetMilliSecondTime(timeBuf);
	return crntTime;
}

void SetTime(void){
	gRefTime = GetCurrentTime();
}

long GetTime(void){
	long crntTime = GetCurrentTime();
	return (crntTime - gRefTime);
}