#define _CRT_SECURE_NO_WARNINGS
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <windows.h>
#include <time.h>

#define MAX_STRING 50
#define BUFFER 100
#define MaxVessels 50
#define MAX_SLEEP_TIME 3000 //3 sec.
#define MIN_SLEEP_TIME 5 //5 millisec.
#define EPMUTEX "epmutex"

//Mutex for canal - Haifa to Eilat.
HANDLE mutex;
//Mutex for exclusive printing.
HANDLE EPMutex;
//Pipe form Haifa to Eliat.
HANDLE ReaderToEilat, WriterToEilat;
//Pipe form Eilat to Haifa.
HANDLE ReaderToHaifa, WriterToHaifa;
//Array of semaphores - 1 semphore for each vessel.
HANDLE* sem;
//Global variables
DWORD read, written;

//Vessels leave Haifa Port.
DWORD WINAPI Start(PVOID Param);

//Vessels enter canal.
void GoToEilat(int vesselID);

//Initializes Global variables and mutexes,semaphores.
void initGolbalData(int numOfShips);

//Prints the time stamp.
void printTime();

//Generate random sleep time between MIN_SLEEP_TIME to MAX_SLEEP_TIME.
int Random(int max, int min);

//Exclusive print responsable that only one thread at a time can print.
void ExclusivePrint(char *PB);

/*Function name: main.
Description: the main responeable for creation of the child process
and communication pipes, after that creates vessels threads if Eilat approved
transfer and whan all vessels saild they WAIT.
when the vessels starts to return, they exit WAIT and finish there voyage back
to haifa, after all vessels are back ,closing there threads and closing all handels
and waiting or child process to finish, when finished closing parent process and finish
program.
Input: reading enterd value.
Output: 0 - end of program run.*/
int main(int argc, char *argv[])
{
	int numOfShips = atoi(argv[1]);
	srand(time(0));
	initGolbalData(numOfShips);

	//Check validation of entered number of vessels.
	if (numOfShips < 2 || numOfShips > 50)
	{
		printTime();
		printf("Invalid vessles number(valid number between 2-50)!\n");
		exit(1);
	}
	printTime();
	printf("Number of Vessels entered: %d Vessels\n", numOfShips);

	STARTUPINFO si;
	PROCESS_INFORMATION pi;
	SECURITY_ATTRIBUTES sa = { sizeof(SECURITY_ATTRIBUTES), NULL,TRUE };
	ZeroMemory(&pi, sizeof(pi));
	TCHAR ProcessName[256];
	size_t convertedChars = 0;
	char command[MAX_STRING];
	char buffer[MAX_STRING];
	char printBuffer[BUFFER];

	/* Create the pipe from Haifa to Eliat */
	if (!CreatePipe(&ReaderToEilat, &WriterToEilat, &sa, 0)) {
		fprintf(stderr, "Create Pipe Failed\n");
		return 1;
	}

	/* Create the pipe from Eliat to Haifa */
	if (!CreatePipe(&ReaderToHaifa, &WriterToHaifa, &sa, 0)) {
		fprintf(stderr, "Create Pipe Failed\n");
		return 1;
	}

	/* Establish the START_INFO structure for Eliat process */
	GetStartupInfo(&si);
	si.hStdError = GetStdHandle(STD_ERROR_HANDLE);

	/* Redirect the standard input to the read end of the pipe */
	si.hStdOutput = WriterToHaifa;
	si.hStdInput = ReaderToEilat;
	si.dwFlags = STARTF_USESTDHANDLES;

	sprintf(command, "Eilat.exe");
	mbstowcs_s(&convertedChars, ProcessName, MAX_STRING, command, _TRUNCATE);

	/* create the child process */
	if (!CreateProcess(NULL,
		ProcessName,
		NULL,
		NULL,
		TRUE, /* inherit handles */
		0,
		NULL,
		NULL,
		&si,
		&pi))
	{
		fprintf(stderr, "Process Creation Failed\n");
		return -1;
	}
	//Close Unused ends of pipes.
	CloseHandle(ReaderToEilat);
	CloseHandle(WriterToHaifa);

	/* Haifa now wants to write to the pipe */
	if (!WriteFile(WriterToEilat, argv[1], MAX_STRING, &written, NULL))
	{
		printTime();
		fprintf(stderr, "Haifa Port: Error writing to pipe.\n");
	}
	sprintf(printBuffer, "Haifa Port: Sending transfer request to Eilat Port.\n", numOfShips);
	ExclusivePrint(printBuffer);
	Sleep(Random(MAX_SLEEP_TIME, MIN_SLEEP_TIME));

	/* now read from the pipe */
	if (ReadFile(ReaderToHaifa, buffer, MAX_STRING, &read, NULL))
	{
		int num = atoi(buffer); //4.1 + 3.1
		if (num == 1)
		{
			sprintf(printBuffer, "Haifa Port: Eilat Denied the transfer request!!!\n");
			ExclusivePrint(printBuffer);
			exit(1);
		}
		else
		{
			sprintf(printBuffer, "Haifa Port: Eilat Approved the transfer request - lets go.\n");
			ExclusivePrint(printBuffer);
		}
	}
	else
	{
		printTime();
		fprintf(stderr, "Haifa Port: Error reading from pipe.\n");
		exit(1);
	}

	//Array that holds the vessles threads.
	HANDLE* vessels = (HANDLE*)malloc(sizeof(HANDLE)*numOfShips);
	int vesID[MaxVessels];
	//Creating the threads for the vessles.
	for (int i = 0; i < numOfShips; i++)
	{
		vesID[i] = i + 1;
		vessels[i] = CreateThread(NULL, 0, Start, vesID[i], NULL, &vesID[i]);
		if (vessels[i] == NULL)
		{
			fprintf(stderr, "Error happend while creating vessles threads\n");
			exit(1);
		}
	}

	for (int i = 0; i < numOfShips; i++)
	{
		if (ReadFile(ReaderToHaifa, buffer, MAX_STRING, &read, NULL))
		{
			if (!ReleaseSemaphore(sem[atoi(buffer) - 1], 1, NULL))
			{
				fprintf(stderr, "Semaphore release sem[%d] faild\n", atoi(buffer));
			}
			else
			{
				sprintf(printBuffer, "Vessel %d - exiting Canal: Red Sea ==> Med. Sea\n", atoi(buffer));
				ExclusivePrint(printBuffer);
				Sleep(Random(MAX_SLEEP_TIME, MIN_SLEEP_TIME));
			}
			sprintf(printBuffer, "Vessel %d - done sailing @ Haifa Port\n", atoi(buffer));
			ExclusivePrint(printBuffer);
			Sleep(Random(MAX_SLEEP_TIME, MIN_SLEEP_TIME));
		}
		else
		{
			fprintf(stderr, "Haifa: Error reading from pipe.\n");
		}

	}

	//Waiting for all vessels to return.
	WaitForMultipleObjects(numOfShips, vessels, TRUE, INFINITE);

	CloseHandle(mutex);
	CloseHandle(WriterToEilat);
	CloseHandle(ReaderToHaifa);

	for (int i = 0; i < numOfShips; i++)
	{
		CloseHandle(vessels[i]);
	}

	sprintf(printBuffer, "Haifa Port: All Vessel Threads are Done\n");
	ExclusivePrint(printBuffer);
	Sleep(Random(MAX_SLEEP_TIME, MIN_SLEEP_TIME));

	//Waiting for Eilat to exit.
	WaitForSingleObject(pi.hProcess, INFINITE);

	printTime();
	printf("Haifa Port: Exiting...\n");
	CloseHandle(EPMutex);
	CloseHandle(pi.hProcess);
	CloseHandle(pi.hThread);
	free(sem);
	system("pause");
	return 0;
}

/*Function name: Start.
Description: after the thread creation for each vessel this method start
the vessel voyage from Haifa Port,also takes the mutex for the vessel that
sails and enters the canal.
Input: Param which holds the number of vesseles enterd.
Output: none - returns to main when done.
Algorithm: printing the vessel start sail indication and grab the cana mutex
that only one vessel can pass at a time.*/
DWORD WINAPI Start(PVOID Param)
{
	char printBuffer[MAX_STRING];
	int vesselID = (int)Param;
	sprintf(printBuffer, "Vessel %d - starts sailing @ Haifa Port\n", vesselID);
	ExclusivePrint(printBuffer);
	Sleep(Random(MAX_SLEEP_TIME, MIN_SLEEP_TIME));
	GoToEilat(vesselID);
	return 0;
}

/*Function name: GoToEilat.
Description: this function send each vessel(after his thread creation) to the canal
and write to the pipe(Haifa writes to Eilat) the vessel ID that pass in the canal.
Input: int the number of vesseles enterd.
Output: none.
Algorithm: responsable for printing the canal indication(on entrence) and writing to the pipe,also check
that the writing was good and release the mutex on the canal that the current vessel took.*/
void GoToEilat(int vesselID)
{
	char printBuffer[MAX_STRING];
	char vesID[MAX_STRING];
	_itoa(vesselID, vesID, 10);
	sprintf(printBuffer, "Vessel %d - entring Canal: Med. Sea ==> Red Sea\n", vesselID);
	ExclusivePrint(printBuffer);
	Sleep(Random(MAX_SLEEP_TIME, MIN_SLEEP_TIME));
	//mutex.P.
	WaitForSingleObject(mutex, INFINITE);
	if (!WriteFile(WriterToEilat, vesID, MAX_STRING, &written, NULL))
	{
		printTime();
		fprintf(stderr, "Haifa Port: Error writing to pipe, Vessel %d did problems!\n", vesselID);
		exit(1);
	}
	if (!ReleaseMutex(mutex))
	{
		printTime();
		fprintf(stderr, "Haifa Port: Unexpected error mutex.V()\n");
	}
	WaitForSingleObject(sem[vesselID - 1], INFINITE);
}

/*Function name: initGolbalData.
Description: the function initializes the mutex for the canal
and the semaphores for each vessel.
Input: int the number of vesseles enterd.
Output: none.
Algorithm: create mutex and check the creation,same thing for semaphore for each vessele.*/
void initGolbalData(int numOfShips)
{
	//Creating Canal Mutex.
	mutex = CreateMutex(NULL, FALSE, NULL);

	if (mutex == NULL)
	{
		printTime();
		fprintf(stderr, "Mutex creation faild\n");
		exit(1);
	}

	EPMutex = CreateMutex(NULL, FALSE, EPMUTEX);
	if (EPMutex == NULL)
	{
		printTime();
		fprintf(stderr, "EPMutex creation faild\n");
		exit(1);
	}
	sem = (HANDLE*)malloc(sizeof(HANDLE)*numOfShips);
	//Creating Shemaphore for each Vessel.
	for (int i = 0; i < numOfShips; i++)
	{
		sem[i] = CreateSemaphore(NULL, 0, 1, NULL);
		if (sem[i] == NULL)
		{
			printTime();
			fprintf(stderr, "Semaphore creation sem[%d] faild\n", i);
			exit(1);
		}
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
Description: the function creates the time stamp
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
	printf("[%02d:%02d:%02d]", hours, minutes, seconds);
}

/*Function name: ExclusivePrint.
Description: the function is responsable that only one
print can happen at a time, if 2 threads want to print simultanusly
the first to grab the mutex will print.
for all major prints.
Input: PB- buffer that the prints are written to.
Output: none..*/
void ExclusivePrint(char* PB)
{
	WaitForSingleObject(EPMutex, INFINITE);
	printTime();
	printf("%s", PB);
	if (!ReleaseMutex(EPMutex))
	{
		printf("Haifa Port: Error EPMutex releas\n");
	}
}