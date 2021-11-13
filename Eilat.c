#define _CRT_SECURE_NO_WARNINGS
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <windows.h>
#include <time.h>

#define MAX_STRING 50
#define SIZE 100
#define MaxVessels 50
#define MinVessels 2
#define MAX_SLEEP_TIME 3000//3 sec.
#define MIN_SLEEP_TIME 5//5 millisec.
#define MAX_WEIGHT 50
#define MIN_WEIGHT 5
#define EPMUTEX "epmutex"

HANDLE ReadHandle, WriteHandle;
DWORD read, write;
CHAR buffer[MAX_STRING];
//Mutexes for Global veriables.
HANDLE GlobalMutex1;
HANDLE GlobalMutex2;
//Mutex for exclusive printing.
HANDLE EPMutex;
//Barrier Mutex.
HANDLE EnterBarrierMutex;
//Barrier semaphore.
HANDLE barrierSem;
HANDLE* CraneSem;
HANDLE* VesSem;

// Vessels Thread's function.
DWORD WINAPI StartEilat(PVOID Param);
// Cranes Thread's function.
DWORD WINAPI StartCrane(PVOID Param);

//Check if the number is prime.
int isPrime(int n);

//Initializes Global variables and mutexes,semaphores.
void initGlobalData(int numOfCranes, int numOfVessels);

//Barriar
void EnterBarrier(int vesID);

//Return a number between lower and upper, and divided by number of vessels.
int RandomNumOfCranes(int lower, int upper, int numOfShips);

//For Random sleep time.
int Random(int max, int min);

//Crane operation.
void CraneWork(int craneID);

//UnloadingQuay.
void UnloadingQuay(int vesID, int index);

//Enter ADT.
void EnterADT(int vesID);

//Exit ADT.
int ExitADT(int vesID);

//Release vessels from Barriar.
void ReleaseShips();

//Prints the time stamp.
void printTime();

//Exclusive print responsable that only one thread at a time can print.
void ExclusivePrint(char *PB);

//Release global data
void FreeGlobalData(int* idArr, HANDLE* vessels, HANDLE* cranes);

//Global variables
int numOfVesselsInBarrier = 0;
int numOfVesselsInADT = 0;
int numOfCranesGlobal = 0;
int numOfShipsGlobal = 0;
int flag = 0;

// a struct to hold the ID and the weight of a spacifit ship.
struct VesselObj {
	int id;
	int weight;
};

// array of vesselObj to hold the ship's id and weight.
//will be initialize by the number of cranes.
// each index in the array will represents a Crane by the Cranes ID-1.
struct VesselObj *vesselObjArr;

/*Function name: main.
Description: The Main Process of Eilat Port, responsable for reciveing
the vesseles from Haifa Port and creating each of them thread.
create the cranes threads, and at the end releaseing all handels for them
and closing the process.
Input: none.
Output: none.
Algorithm: recives from Haifa port the number of Vessels, and approve\refuse the transfer.
if approve: recive each Ship and Start each vessels a thread.
Creating the cranes, each Crane will be in the index -1 of it's ID.
the ship threads also will be in index -1 of its ID.
.*/
void main(VOID)
{
	HANDLE *vessels;
	HANDLE *cranes;
	srand(time(0));
	ReadHandle = GetStdHandle(STD_INPUT_HANDLE);
	WriteHandle = GetStdHandle(STD_OUTPUT_HANDLE);
	char printBuffer[SIZE];
	EPMutex = OpenMutex(MUTEX_ALL_ACCESS, FALSE, EPMUTEX);
	int numOfShips = 0;

	/* now read from the pipe */
	if (!ReadFile(ReadHandle, buffer, MAX_STRING, &read, NULL))
	{
		fprintf(stderr, "Eilat Port: Error reading from pipe\n");
	}

	numOfShips = atoi(buffer);
	numOfShipsGlobal = numOfShips;

	sprintf(printBuffer, "Eilat Port: Haifa requests permission to transfer %d Vessels.\n", numOfShips);
	ExclusivePrint(printBuffer);

	//Check if the Vessels number is prime and sends Eilat response.
	int res = isPrime(numOfShips);
	_itoa(res, buffer, 10);
	if (res == 0)
	{
		if (!WriteFile(WriteHandle, buffer, MAX_STRING, &write, NULL))
		{
			fprintf(stderr, "Eilat Port: Error writing to pipe\n");
		}
		else
		{
			sprintf(printBuffer, "Eilat Port: Haifa we approve the transfer request.\n");
			ExclusivePrint(printBuffer);
			Sleep(Random(MAX_SLEEP_TIME, MIN_SLEEP_TIME));
		}
	}
	else
	{
		if (!WriteFile(WriteHandle, buffer, MAX_STRING, &write, NULL))
		{
			fprintf(stderr, "Eilat Port: Error writing to pipe\n");
		}
		else
		{
			sprintf(printBuffer, "Eilat Port: Haifa we deny the transfer request.\n");
			ExclusivePrint(printBuffer);
			Sleep(Random(MAX_SLEEP_TIME, MIN_SLEEP_TIME));
		}
	}

	int *idArr = (int*)malloc(sizeof(int)*numOfShips);
	int index;
	int numOfCranes = RandomNumOfCranes(MinVessels, (numOfShips - 1), numOfShips);

	Sleep(Random(MAX_SLEEP_TIME, MIN_SLEEP_TIME));
	numOfCranesGlobal = numOfCranes;
	initGlobalData(numOfCranes, numOfShips);

	sprintf(printBuffer, "Eilat Port: Number of Cranes: %d.\n", numOfCranes);
	ExclusivePrint(printBuffer);

	//Creating Dynamic array of cranes by the calculated number of cranes.
	cranes = (HANDLE*)malloc(sizeof(HANDLE)*numOfCranes);
	for (int i = 0; i < numOfCranes; i++)
	{
		idArr[i] = (i + 1);
		cranes[i] = CreateThread(NULL, 0, StartCrane, idArr[i], NULL, &idArr[i]);
		sprintf(printBuffer, "Eilat Port: Crane %d Ready for work.\n", i + 1);
		ExclusivePrint(printBuffer);
	}

	//Creating Dynamic array of vessels by the given number of ships.
	vessels = (HANDLE*)malloc(sizeof(HANDLE)*numOfShips);
	for (int i = 0; i < numOfShips; i++)
	{
		if (!(ReadFile(ReadHandle, buffer, MAX_STRING, &read, NULL)))
		{
			fprintf(stderr, "Eilat Port: Error reading from pipe\n");
			exit(1);
		}
		else
		{
			idArr[i] = atoi(buffer);
			index = idArr[i] - 1;
			vessels[index] = CreateThread(NULL, 0, StartEilat, idArr[i], NULL, &idArr[i]);
		}
	}

	WaitForMultipleObjects(numOfShips, vessels, TRUE, INFINITE);

	//Close the Handles.
	for (int i = 0; i < numOfShips; i++)
	{
		CloseHandle(vessels[i]);
	}
	for (int i = 0; i < numOfCranes; i++)
	{
		CloseHandle(CraneSem[i]);
	}
	CloseHandle(barrierSem);
	CloseHandle(EnterBarrierMutex);


	sprintf(printBuffer, "Eilat Port: All Vessel Threads are done.\n");
	ExclusivePrint(printBuffer);
	Sleep(Random(MAX_SLEEP_TIME, MIN_SLEEP_TIME));
	FreeGlobalData(idArr, vessels, cranes);
	sprintf(printBuffer, "Eilat Port: Exiting...\n");
	ExclusivePrint(printBuffer);
	Sleep(Random(MAX_SLEEP_TIME, MIN_SLEEP_TIME));
	CloseHandle(ReadHandle);
	CloseHandle(WriteHandle);
	CloseHandle(EPMutex);
}

/*Function name: StartCrane.
Description: The Thread function.
Input: PVOID Param (CraneID);
Output: none.
Algorithm: each crane starts in WAIT state, untill a ship will release it to work.
and each crane will work few time (depends on the number of vessels and cranes)
.*/
DWORD WINAPI StartCrane(PVOID Param)
{
	int craneID = (int)Param;
	WaitForSingleObject(CraneSem[craneID - 1], INFINITE);
	for (int i = 0; i < (numOfShipsGlobal) / (numOfCranesGlobal); i++)
	{
		CraneWork(craneID);
	}
	return 0;
}

/*Function name: CraneWork.
Description: The crane unload the quay from the ships.
Input: int craneID;
Output: none.
Algorithm: each Crane will unload the quay of the spacific ship (using vesselObjArr)
after finishing unloading, the crane will release the spacific ship and will be in WAIT state.
.*/
void CraneWork(int craneID)
{
	char printBuffer[SIZE];
	sprintf(printBuffer, "Eilat Port: Crane %d finished unloading %d tons.\n", craneID, vesselObjArr[craneID - 1].weight);
	ExclusivePrint(printBuffer);
	Sleep(Random(MAX_SLEEP_TIME, MIN_SLEEP_TIME));

	if (!ReleaseSemaphore(VesSem[(vesselObjArr[craneID - 1].id) - 1], 1, NULL))
	{
		fprintf(stderr, "Error: release sem.V() %d \n", vesselObjArr[craneID - 1].id);
	}

	sprintf(printBuffer, "Eilat Port Crane %d: Vessel %d, we are done unloading, please exit the dock.\n", craneID, vesselObjArr[craneID - 1].id);
	ExclusivePrint(printBuffer);
	WaitForSingleObject(CraneSem[craneID - 1], INFINITE);
}

/*Function name: StartEilat.
Description: The Thread fucntion. each ship starts here, returns from the unloading and sail back to Haifa.
Input: PVOID Param. (vessel ID)
Output: none.
Algorithm: the ship will enter the Barrier through the function, and checking if the same ship exited from the ADT
if the same ship came back from the ADT, we send the ship back to haifa.
if not the same ship came back from the ADT, send an error message and exit the program.*/
DWORD WINAPI StartEilat(PVOID Param)
{
	WriteHandle = GetStdHandle(STD_OUTPUT_HANDLE);
	char printBuffer[SIZE];
	int vesselID = (int)Param;
	sprintf(printBuffer, "Vessel %d - exiting Canal: Med. Sea ==> Red Sea\n", vesselID);
	ExclusivePrint(printBuffer);
	Sleep(Random(MAX_SLEEP_TIME, MIN_SLEEP_TIME));

	sprintf(printBuffer, "Vessel %d arrived @ Eilat Port\n", vesselID);
	ExclusivePrint(printBuffer);
	Sleep(Random(MAX_SLEEP_TIME, MIN_SLEEP_TIME));

	EnterBarrier(vesselID);
	EnterADT(vesselID);
	int returnVesID = ExitADT(vesselID);

	_itoa(vesselID, buffer, 10);

	if (returnVesID != vesselID)
	{
		printTime();
		fprintf(stderr, "Eilat Port: Error, not the same ship came back from ADT\n");
		exit(1);
	}
	else if (!WriteFile(WriteHandle, buffer, MAX_STRING, &write, NULL))
	{
		fprintf(stderr, "Eilat Port: Error writing to pipe\n");
	}
	sprintf(printBuffer, "Vessel %d - entering Canal: Red Sea ==> Med. Sea.\n", vesselID);
	ExclusivePrint(printBuffer);
	Sleep(Random(MAX_SLEEP_TIME, MIN_SLEEP_TIME));
	return 0;
}

/*Function name: EnterBarrier.
Description: Each ship is enting the Barrier.
Input: int vesID.
Output: none.
Algorithm: taking mutex for using gloval variable
increment the number of vessels in Barrier
checking the conditions to enter the ADT
if the ship cant enter the ADT, the ship in WAIT state
if the ship can enter the ship will release M-1 ships to enter the ADT.*/
void EnterBarrier(int vesID)
{
	char printBuffer[SIZE];
	WaitForSingleObject(EnterBarrierMutex, INFINITE);
	sprintf(printBuffer, "Eilat Port: Vessel %d has entered the Barrier\n", vesID);
	ExclusivePrint(printBuffer);
	Sleep(Random(MAX_SLEEP_TIME, MIN_SLEEP_TIME));
	numOfVesselsInBarrier++;

	if (numOfVesselsInBarrier < numOfCranesGlobal || numOfVesselsInADT != 0)
	{
		if (!ReleaseMutex(EnterBarrierMutex))
		{
			fprintf(stderr, "EnterBarrier :: Unexpected error %d release mutex\n", vesID);
		}

		sprintf(printBuffer, "Vessle %d is in at the barrier\n", vesID);
		ExclusivePrint(printBuffer);
		Sleep(Random(MAX_SLEEP_TIME, MIN_SLEEP_TIME));
		WaitForSingleObject(barrierSem, INFINITE);
	}
	else
	{
		for (int i = 0; i < numOfCranesGlobal - 1; i++)
		{
			ReleaseShips();
		}

		if (!ReleaseMutex(EnterBarrierMutex))
		{
			fprintf(stderr, "EnterBarrier :: Unexpected error %d release mutex\n", vesID);
		}
	}
}

/*Function name: ReleaseShips.
Description: release ship to continue
Input: none.
Output: none.
Algorithm: none.*/
void ReleaseShips()
{
	if (!ReleaseSemaphore(barrierSem, 1, NULL))
	{
		fprintf(stderr, "Error: Release Ships sem.V().\n");
	}
}

/*Function name: EnterADT.
Description: Each vessel enter the ADT.
Input: int vessel ID.
Output: none.
Algorithm: taking mutex for using global variable
updating the number of ships in the ADT and the Barrier.
searching for the first empty carne, and set the ID in the vesselObjArr as the vessel's ID
and moving forawrd to UnloadingQuay.
.*/
void EnterADT(int vesID)
{
	WaitForSingleObject(GlobalMutex2, INFINITE);
	numOfVesselsInBarrier--;
	numOfVesselsInADT++;
	char printBuffer[SIZE];
	int index = 0;
	for (index = 0; index < numOfCranesGlobal; index++)
	{
		if (vesselObjArr[index].id == -1)
		{
			vesselObjArr[index].id = vesID;
			sprintf(printBuffer, "Crane %d Works for Vessel %d\n", (index + 1), vesID);
			ExclusivePrint(printBuffer);
			Sleep(Random(MAX_SLEEP_TIME, MIN_SLEEP_TIME));
			break;
		}
	}
	if (!ReleaseMutex(GlobalMutex2))
	{
		fprintf(stderr, "EnterBarrier:: Unexpected error release anotherMutex\n");
	}
	UnloadingQuay(vesID, index);
}

/*Function name: UnloadingQuay.
Description: Each vessel unloading the quay from itself.
Input: int vessel ID, int index,
Output: none.
Algorithm: generate random number of weight of quay.
index represents the index of the spacific carne(the first carne found empty).
release the spacific carne to work, and than WAIT until the carne done.*/
void UnloadingQuay(int vesID, int index)
{
	char printBuffer[SIZE];
	int weight = Random(MAX_WEIGHT, MIN_WEIGHT);
	sprintf(printBuffer, "Vessel %d has %d Tons to Unload.\n", vesID, weight);
	ExclusivePrint(printBuffer);
	Sleep(Random(MAX_SLEEP_TIME, MIN_SLEEP_TIME));

	vesselObjArr[index].weight = weight;

	if (!ReleaseSemaphore(CraneSem[index], 1, NULL))
	{
		fprintf(stderr, "UnloadingQuay:: error %d sem.V()\n", vesID);
	}
	WaitForSingleObject(VesSem[vesID - 1], INFINITE);
	sprintf(printBuffer, "Vessel %d is now empty!\n", vesID);
	ExclusivePrint(printBuffer);
	Sleep(Random(MAX_SLEEP_TIME, MIN_SLEEP_TIME));
}

/*Function name: ExitADT.
Description: Each vessel leaves the ADT. the last ship to exit will release the M ships in the Barrier.
Input: int vessel ID.
Output: returns the vesselID - to cheack the same Ship returned after all the work.
Algorithm: flag - counting the ships that leave the ADT, if flag%numOfCranesGlobal == 0, means its the last ship to leave and will release the M ships.
and change all the IDs in the vesseObjArr to -1 (symbol as empty)
and decrease the numOfVesselsInADT by 1 (each ships)
mutex - for using global variable.*/
int ExitADT(int vesID)
{
	char printBuffer[SIZE];
	sprintf(printBuffer, "Vessel %d leaveing the ADT.\n", vesID);
	ExclusivePrint(printBuffer);
	Sleep(Random(MAX_SLEEP_TIME, MIN_SLEEP_TIME));
	WaitForSingleObject(GlobalMutex1, INFINITE);
	flag++;
	if (numOfVesselsInBarrier >= numOfCranesGlobal && flag%numOfCranesGlobal == 0)
	{
		for (int i = 0; i < numOfCranesGlobal; i++)
		{
			vesselObjArr[i].id = -1;
			ReleaseShips();
		}
	}

	numOfVesselsInADT--;
	if (!ReleaseMutex(GlobalMutex1))
	{
		fprintf(stderr, "EnterBarrier:: Unexpected error release mutex\n");
	}
	return vesID;
}

/*Function name: isPrime.
Description: check if the given number is prime.
Input: int num.
Output: 0 if the number is not prime, 1 if the number is prime.
Algorithm: .*/
int isPrime(int n)
{
	for (int i = 2; i <= (n / 2); i++)
	{
		if (n%i == 0)
		{
			return 0;//not prime
		}
	}
	return 1;//prime
}

/*Function name: RandomNumOfCranes.
Description: Generate random values between lower and upper and divided by numOfShips.
Input: int lower,int upper, int numOfShips (the divided by).
Output: random int between values.
Algorithm: uses rand to generate the number.*/
int RandomNumOfCranes(int lower, int upper, int numOfShips)
{
	int num;
	while (TRUE)
	{
		num = (rand() % (upper - lower + 1)) + lower;
		if (numOfShips%num == 0)
		{
			return num;
		}
	}
}

/*Function name: initGolbalData.
Description: the function initializes the global mutexs for the Barriar,
and the semaphores for each Crane and vessel.
Input: int the number of vesseles and cranes.
Output: none.
Algorithm: create mutexs and check the creation,same thing for semaphores.*/
void initGlobalData(int numOfCranes, int numOfVessels)
{
	GlobalMutex1 = CreateMutex(NULL, FALSE, NULL);
	GlobalMutex2 = CreateMutex(NULL, FALSE, NULL);
	EnterBarrierMutex = CreateMutex(NULL, FALSE, NULL);
	barrierSem = CreateSemaphore(NULL, 0, numOfCranes, NULL);
	vesselObjArr = (struct VesselObj*)malloc(sizeof(struct VesselObj)*numOfCranesGlobal);

	CraneSem = (HANDLE*)malloc(sizeof(HANDLE)*numOfCranes);
	for (int i = 0; i < numOfCranes; i++)
	{
		CraneSem[i] = CreateSemaphore(NULL, 0, 1, NULL);
		vesselObjArr[i].id = -1;
	}

	VesSem = (HANDLE*)malloc(sizeof(HANDLE)*numOfVessels);
	for (int i = 0; i < numOfVessels; i++)
	{
		VesSem[i] = CreateSemaphore(NULL, 0, 1, NULL);
	}
}

/*Function name: Random.
Description: Generate random values between min and max(for sleep time).
Input: int max,int min.
Output: random int between values.
Algorithm: uses rand to generate the number.*/
int Random(int max, int min)
{
	return ((rand() % (max - min + 1)) + min);
}

/*Function name: printTime.
Description : the function creates the time stamp
for all major prints.
Input: none.
Output: none.
Algorithm: uses time.h library to get current time
from the computer by: hours,minutes,secondes.*/
void printTime()
{
	int hours, minutes, seconds;
	time_t now;
	time(&now);
	struct tm *local = localtime(&now);
	hours = local->tm_hour;
	minutes = local->tm_min;
	seconds = local->tm_sec;
	fprintf(stderr, "[%02d:%02d:%02d]", hours, minutes, seconds);
}

/*Function name: ExclusivePrint.
Description: the function is responsable that only one
print can happen at a time, if 2 threads want to print simultanusly
the first to grab the mutex will print.
for all major prints.
Input: PB- buffer that the prints are written to.
Output: none..*/
void ExclusivePrint(char *PB)
{
	WaitForSingleObject(EPMutex, INFINITE);
	printTime();
	fprintf(stderr, "%s", PB);
	if (!ReleaseMutex(EPMutex))
	{
		printf("Eilat Port: Error EPMutex releas\n");
	}
}

void FreeGlobalData(int* idArr,HANDLE* vessels, HANDLE* cranes)
{
	free(vessels);
	free(idArr);
	free(cranes);
	free(CraneSem);
	free(VesSem);
	free(vesselObjArr);
}