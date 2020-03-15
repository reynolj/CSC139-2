/*
 * Reynolds, Jake
 * CSC 139 Section 5
 * OSs Tested on: Linux
 * Linux CPU: 4 cores, Intel(R) Core(TM) i7-6700 CPU @ 3.40GHz
*/

/*
 * Compile options:
 * g++ -O3 MTFindProd.c -o MTFindProd -lpthread
*/
#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>    // pthread_t, pthread_attr_t attr, pthread_attr_init(), pthread_create() ...
#include <sys/timeb.h>
#include <semaphore.h>  // sem_t, sem_init(), sem_wait(), sem_post() ...
#include <stdbool.h>    // bool, for portability

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

// Calculation functions
int SqFindProd(int size); //Sequential FindProduct (no threads) computes the product of all the elements in the array mod NUM_LIMIT
void* ThFindProd(void *param); //Thread FindProduct but without semaphores
void* ThFindProdWithSemaphore(void *param); //Thread FindProduct with semaphores
int ComputeTotalProduct(); //Multiply the division products to compute the total modular product

// Helper functions
void InitSharedVars(); //Initializes default values to gThreadDone, gThreadProd, gDoneThreadCount
void GenerateInput(int size, int indexForZero); //Generate the input array
void CalculateIndices(int arraySize, int thrdCnt, int indices[MAX_THREADS][3]); //Calculate the indices to divide the array into T divisions, one division per thread
int GetRand(int x, int y); //Get a random number between x and y

//Timing functions
long GetMilliSecondTime(struct timeb timeBuf);
long GetCurrentTime(void);
void SetTime(void);
long GetTime(void);

//Debugging Functions
void printIndices(int (*indices)[3]); //Displays indices array. Used for debugging.
void printProds(); // Displays products of individual divisions

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

    /****************************************Initializing Arrays*******************************************************/
	//Initializing array: gData
    GenerateInput(arraySize, indexForZero);
    //Initializing array: indices
    CalculateIndices(arraySize, gThreadCount, indices);

	/*****************************************Single Thread: Sequential************************************************/
	SetTime();
	prod = SqFindProd(arraySize);
	printf("Sequential multiplication completed in %ld ms. Product = %d\n", GetTime(), prod);

	/****************************************MT: Parent waits for all children*****************************************/
	// Sets default values to variables used by TheFindProd/ThFindProdSemaphore
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
	// Resets shared vars for threads
	InitSharedVars();
	SetTime();
    // Initialize attributes, and create threads
    for (i = 0; i < gThreadCount; i++)
    {
        pthread_attr_init(&attr[i]);
        pthread_create(&tid[i], &attr[i], ThFindProd, (void*) indices[i]);
    }
    // Parent continually check on all child threads
    volatile bool foundZero, allDone; // volatile prevents compiler from optimizing out the busy-wait loop
    do{
        foundZero = false; // for zero in product divisions
        allDone = true; // for completion of all product divisions
        for(i = 0; i < gThreadCount && !foundZero; i++)
        {
            // allDone == true && gThreadDone[0] && gThreadDone[1] && ... && gThreadDone[gThreadCount-1]
            allDone &= gThreadDone[i];
            if (gThreadDone[i])
            {
                if (gThreadProd[i] == 0)
                {
                    // Zero product is found, for loop will stop then do-while loop will stop
                    foundZero = true;
                    // Cancel all threads
                    for (i = 0; i < gThreadCount; i++)
                    {
                        pthread_cancel(tid[i]);
                    }
                }
            }
        }
    }while(!foundZero && !allDone); // loop while no zero product is found, and not all product divisions are done
    prod = ComputeTotalProduct();
	printf("Threaded multiplication with parent continually checking on children completed in %ld ms. Product = %d\n", GetTime(), prod);

	/*********************************************MT: With Semaphores**************************************************/
    // Resets shared vars for threads
    InitSharedVars();
	// Initialize semaphores
    sem_init(&mutex, 0, 1); // initialized as unlocked
    sem_init(&completed, 0, 0); // initialized as locked
    SetTime();
    // Initialize and create threads
    for (i = 0; i < gThreadCount; i++)
    {
        pthread_attr_init(&attr[i]);
        pthread_create(&tid[i], &attr[i], ThFindProdWithSemaphore, (void*) indices[i]);
    }
    // Parent waits on the "completed" semaphore
	sem_wait(&completed);
    // Cancel all threads after calculation
    for (i = 0; i < gThreadCount; i++)
    {
        pthread_cancel(tid[i]);
    }
    prod = ComputeTotalProduct();
	printf("Threaded multiplication with parent waiting on a semaphore completed in %ld ms. Product = %d\n", GetTime(), prod);
	// Clean up
	sem_destroy(&completed);
    sem_destroy(&mutex);
    for (i = 0; i < gThreadCount; i++)
    {
        pthread_attr_destroy(&attr[i]);
    }
}

// Function that fills the gData array with random numbers between 1 and MAX_RANDOM_NUMBER
// If indexForZero is valid and non-negative, set the value at that index to zero
void GenerateInput(int size, int indexForZero) {
    int i;
    //Set seed for GetRand()
    srand(RANDOM_SEED);
    //Fill the array with random data from 1, MAX_RANDOM_NUMBER
    for (i = 0; i < size; i++)
    {
        gData[i] = GetRand(1, MAX_RANDOM_NUMBER);
    }
    // If indexForZero is valid and non-negative, set the value at that index to zero
    if (indexForZero != -1)
    {
        gData[indexForZero] = 0;
    }
}

// Function that calculates the indices to divide the array into thrdCnt equal divisions
// For each division i:
// indices[i][0] should be set to the division number i,
// indices[i][1] should be set to the start index
// indices[i][2] should be set to the end index
void CalculateIndices(int arraySize, int thrdCnt, int indices[MAX_THREADS][3]) {
    int partOffset  =   arraySize / thrdCnt;
    int start       =   0;
    int end         =   partOffset-1;
    int i;

    for (i = 0; i < thrdCnt; i++, start += partOffset, end += partOffset)
    {
        indices[i][0] = i;
        indices[i][1] = start;
        indices[i][2] = end;
    }
}

// A regular sequential function to multiply all the elements in gData mod NUM_LIMIT
// if 0 is in gData, it returns 0 immediately
int SqFindProd(int size) {
    int i, prod = 1;
    for (i = 0; i < size ; i++)
    {
        if (gData[i] == 0)
        {
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
void* ThFindProd(void *param) {
	int threadNum   = ((int*)param)[0];
	int start       = ((int*)param)[1];     // Division start
	int end         = ((int*)param)[2];     // Division end
    int prod        = 1;
    int i;

    for (i = start; i <= end ; i++) // Calculate division
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
	// Store the product in the global array: gThreadProd
	gThreadProd[threadNum] = prod;
	// Set state of thread to true in global array: gThreadDone
	gThreadDone[threadNum] = true;
    pthread_exit(NULL);
}

// Thread function that computes the product of all the elements in one division of the array mod NUM_LIMIT
void* ThFindProdWithSemaphore(void *param) {
    int threadNum   = ((int*)param)[0];
    int start       = ((int*)param)[1];     // Division start
    int end         = ((int*)param)[2];     // Division end
    int prod        = 1;
    int i;

    for (i = start; i <= end ; i++) // Calculate division
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
    // Store the product in the global array: gThreadProd
    gThreadProd[threadNum] = prod;
    // If the product value in this division is zero, this function posts the "completed" semaphore and exits the thread
    if (prod == 0) {
        sem_post(&completed);
        pthread_exit(NULL);
    }
    // Protects access to gDoneThreadCount with the "mutex" semaphore
    sem_wait(&mutex);
    // If the product in this division is not zero, this function should increment gDoneThreadCount
    gDoneThreadCount++;
    // If it is the last thread to be done, the "completed" semaphore is posted
    if (gDoneThreadCount == gThreadCount)
    {
        sem_post(&completed);
    }
    sem_post(&mutex);
    pthread_exit(NULL);
}

// Calculates the product of the partitioned products
int ComputeTotalProduct() {
    int i, prod = 1;

	for(i=0; i<gThreadCount; i++)
	{
		prod *= gThreadProd[i];
		prod %= NUM_LIMIT;
	}
	return prod;
}

// Sets default values to vars used by threads
void InitSharedVars() {
	int i;

	for(i=0; i<gThreadCount; i++){
		gThreadDone[i] = false;
		gThreadProd[i] = 1;
	}
	gDoneThreadCount = 0;
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

/********************************************* Unused Debugging Functions**********************************************/
void printIndices(int (*indices)[3]){
    int i;
    printf("%-5s%-10s%-10s\n", "Thd", "Start", "End");
    for (i = 0; i < gThreadCount; i++)
    {
        printf("%-5d%-10d%-10d\n", indices[i][0], indices[i][1], indices[i][2]);
    }
}

void printProds(){
    int i;
    printf("%-5s%-10s\n", "Thd", "Prod");
    for (i = 0; i < gThreadCount; i++)
    {
        printf("%-5d%-10d\n", i, gThreadProd[i]);
    }
}