// threadtest.cc 
//	Simple test case for the threads assignment.
//
//	Create several threads, and have them context switch
//	back and forth between themselves by calling Thread::Yield, 
//	to illustrate the inner workings of the thread system.
//
// Copyright (c) 1992-1993 The Regents of the University of California.
// All rights reserved.  See copyright.h for copyright notice and limitation 
// of liability and disclaimer of warranty provisions.
//
// Parts from Copyright (c) 2007-2009 Universidad de Las Palmas de Gran Canaria
//

#include "copyright.h"
#include "system.h"
#include "synch.h" // * para probar los hilos

//----------------------------------------------------------------------
// SimpleThread
// 	Loop 10 times, yielding the CPU to another ready thread 
//	each iteration.
//
//	"name" points to a string with a thread name, just for
//      debugging purposes.
//----------------------------------------------------------------------

void
SimpleThread(void* name)
{
    // Reinterpret arg "name" as a string
    char* threadName = (char*)name;
    
    // If the lines dealing with interrupts are commented,
    // the code will behave incorrectly, because
    // printf execution may cause race conditions.
    for (int num = 0; num < 10; num++) {
        //IntStatus oldLevel = interrupt->SetLevel(IntOff);
	printf("*** thread %s looped %d times\n", threadName, num);
	//interrupt->SetLevel(oldLevel);
        currentThread->Yield();
    }
    //IntStatus oldLevel = interrupt->SetLevel(IntOff);
    printf(">>> Thread %s has finished\n", threadName);
    //interrupt->SetLevel(oldLevel);
}

//----------------------------------------------------------------------
// ThreadTest
// 	Set up a ping-pong between several threads, by launching
//	ten threads which call SimpleThread, and finally calling 
//	SimpleThread ourselves.
//----------------------------------------------------------------------

void
ThreadTest()
{
    DEBUG('t', "Entering SimpleTest");

    for ( int k=1; k<=10; k++) {
      char* threadname = new char[100];
      sprintf(threadname, "Hilo %d", k);
      Thread* newThread = new Thread (threadname);
      newThread->Fork (SimpleThread, (void*)threadname);
    }
    
    SimpleThread( (void*)"Hilo 0");
}

int varTest = 0;


void
lockTaker(void* lock)
{
	Lock* tLock = (Lock*) lock;
	
	tLock->Acquire();
	printf("Lock acquired\n");
	for (int i = 1; i <= 10; i++)
	{
		if (i == 5) currentThread->Yield();
		varTest += 1;
	}
	printf("varTest: %d\n", varTest);
	printf("Lock released\n");
	tLock->Release();
	
}

// * para probar locks y variables de condicion
void
LockTest()
{
    DEBUG('t', "Entering Lock Test");
    
	Lock* theLock = new Lock("Test Lock");
    for ( int k=1; k<=4; k++) {
      char* threadname = new char[100];
      sprintf(threadname, "HiloLock %d", k);
      Thread* newThread = new Thread (threadname);
      newThread->Fork (lockTaker, (void*)theLock);
    }
    
    lockTaker((void*)theLock);
}

List<int> buffer;
int tamBuff = 0;
int maxBuff = 5;

int vecesP = 11;
int vecesC = 11;

Lock* lockCV = new Lock("Test Lock CV");
Condition* emptyCV = new Condition("Empty CV", lockCV);
Condition* fullCV = new Condition("Full CV", lockCV);

void productor(void* n)
{
	lockCV->Acquire();
	while (vecesP--)
	{
		if (tamBuff == maxBuff)
		{
			fullCV->Wait();
		}
		buffer.Append(1);
		tamBuff++;
		printf("Productor (%d)\n", vecesP);
		currentThread->Yield();
		if (tamBuff == 1)
		{
			emptyCV->Signal();
		}
	}
}

void consumidor(void* n)
{
	lockCV->Acquire();
	while (vecesC--)
	{
		if (tamBuff == 0)
		{
			emptyCV->Wait();
		}
		buffer.Remove();
		tamBuff--;
		printf("Consumidor (%d)\n", vecesC);
		if (tamBuff == maxBuff - 1)
		{
			fullCV->Signal();
		}
	}
}

void CVTest()
{
	DEBUG('t', "Entering CV Test");
	
	Thread* newThread = new Thread("Consumidor");
	newThread->Fork(consumidor, (void*) 1);

	newThread = new Thread("Productor");
	newThread->Fork(productor, (void*) 1);
}
