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
#include <iostream>

using namespace std;
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
      Thread* newThread = new Thread (threadname,0,0);
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
      Thread* newThread = new Thread (threadname,0,0);
      newThread->Fork (lockTaker, (void*)theLock);
    }
    
    lockTaker((void*)theLock);
}

/*
List<int> buffer;
int tamBuff = 0;
int maxBuff = 5;

int vecesP = 50;
int vecesC = 50;

Lock* lockCV = new Lock("Test Lock CV");
Condition* emptyCV = new Condition("Empty CV", lockCV);
Condition* fullCV = new Condition("Full CV", lockCV);

void productor(void* name)
{
	lockCV->Acquire();
	while (true)
	{
		if (tamBuff == maxBuff)
		{
			fullCV->Wait();
		}
		buffer.Append(1);
		tamBuff++;
		printf("%s (%d)\n", (char*) name, tamBuff);
		if (tamBuff == 1)
		{
			emptyCV->Signal();
		}
	}
}

void consumidor(void* name)
{
	lockCV->Acquire();
	while (true)
	{
		if (tamBuff == 0)
		{
			emptyCV->Wait();
		}
		ASSERT(buffer.Remove() != NULL);
		tamBuff--;
		printf("%s (%d)\n", (char*) name, tamBuff);
		if (tamBuff == maxBuff - 1)
		{
			fullCV->Signal();
		}
	}
}

void CVTest()
{
	DEBUG('t', "Entering CV Test");
	
	for (int i = 1; i <= 3; i++)
	{
		char* threadname = new char[100];
		sprintf(threadname, "Consumidor %d", i);
		Thread* newThread = new Thread(threadname,0);
		newThread->Fork(consumidor, (void*) threadname);
	}
	for (int i = 1; i <= 3; i++)
	{
		char* threadname = new char[100];
		sprintf(threadname, "Productor %d", i);
		Thread* newThread = new Thread(threadname,0);
		newThread->Fork(productor, (void*) threadname);
	}
}
*/

Port *puerto = new Port("Test Port");

void PortTester(void *n)
{
	int destino;
	
	puerto->Receive(&destino);
	
	printf("Este es el secreto: %d\n", destino);
	
}

void PortTest()
{
	puerto->Send(32);
	Thread* newThread = new Thread("PortTester",0,0);
	newThread->Fork(PortTester, (void*) 1);
}

int cont = 0, n_vc = 1;
bool bool_vc = false;
Lock* lock_vc = new Lock("lock vc");
Condition* condition_vc = new Condition("condition vc",lock_vc);


void VCf(void* name) {

  if(!cont) {
    cont++;
    cout << currentThread->getName() << " este es el valor de bool_vc: " << bool_vc << endl;
    cout << "este es el valor de n_vc: " << n_vc << endl;
    n_vc++;
    lock_vc->Acquire();
    cout << "este es el valor de n_vc: " << n_vc << endl;
    if(!bool_vc)
      condition_vc->Wait();

    n_vc++;
    cout << "este es el valor de n_vc: " << n_vc << endl;
    cout << currentThread->getName() << " este es el valor de bool_vc: " << bool_vc << endl;
    return;
  }
  cout << currentThread->getName() << " este es el valor de bool_vc: " << bool_vc << endl;
  currentThread->Yield();
  lock_vc->Acquire();
  
  bool_vc = !bool_vc;
  condition_vc->Signal();

  lock_vc->Release();  
}

void VCTest(){
  DEBUG('t', "Entering VCTest");
  for (int k=1; k<=1; k++) {
    char* threadname = new char[100];
    sprintf(threadname, "Hilo %d", k);
    Thread* newThread = new Thread (threadname,0,0);
    newThread->Fork (VCf, (void*)threadname);
  }
  
  VCf( (void*)"Hilo 0");
}

int count = 0;
Port* port = new Port("port");

void SendReceiveF(void *name) {
 
  int threadName    = (long)name;
  int tmp;

  if(threadName >= 5) {
    port->Send(threadName); 
  } else {
    port->Receive(&tmp);
    cout << currentThread->getName() << " Received value: " << tmp << endl;
  }
}

void
SendReceiveTest()
{
    DEBUG('t', "Entering SendReceiveTest");

    for ( int k=1; k<=9; k++) {
      char* threadname = new char[100];
      sprintf(threadname, "Hilo %d", k);
      Thread* newThread = new Thread (threadname,0,0);
      newThread->Fork (SendReceiveF, (void*)k);
    }
    
    SendReceiveF( (void*)0);
}

int testnum = 1;

void
Joiner(Thread *joinee)
{
  //currentThread->Yield();
  //currentThread->Yield();

  printf("Waiting for the Joinee to finish executing.\n");

  //currentThread->Yield();
  //currentThread->Yield();

  // Note that, in this program, the "joinee" has not finished
  // when the "joiner" calls Join().  You will also need to handle
  // and test for the case when the "joinee" _has_ finished when
  // the "joiner" calls Join().

  currentThread->Join(joinee);

  //currentThread->Yield();
  //currentThread->Yield();

  printf("Joinee has finished executing, we can continue.\n");

 // currentThread->Yield();
  //currentThread->Yield();
}

void
Joinee()
{
  int i;

  for (i = 0; i < 5; i++) {
    printf("Smell the roses.\n");
    currentThread->Yield();
  }

  currentThread->Yield();
  printf("Done smelling the roses!\n");
  currentThread->Yield();
}

void
ForkerThread()
{
  Thread *joiner = new Thread("joiner", 0,4);  // will not be joined
  Thread *joinee = new Thread("joinee", 1,1);  // WILL be joined

  // fork off the two threads and let them do their business
  joiner->Fork((VoidFunctionPtr) Joiner, (void*) joinee);
  joinee->Fork((VoidFunctionPtr) Joinee, 0);

  // this thread is done and can go on its merry way
  printf("Forked off the joiner and joiner threads.\n");
}


