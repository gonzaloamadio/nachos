// scheduler.cc 
//	Routines to choose the next thread to run, and to dispatch to
//	that thread.
//
// 	These routines assume that interrupts are already disabled.
//	If interrupts are disabled, we can assume mutual exclusion
//	(since we are on a uniprocessor).
//
// 	NOTE: We can't use Locks to provide mutual exclusion here, since
// 	if we needed to wait for a lock, and the lock was busy, we would 
//	end up calling FindNextToRun(), and that would put us in an 
//	infinite loop.
//
// 	Very simple implementation -- no priorities, straight FIFO.
//	Might need to be improved in later assignments.
//
// Copyright (c) 1992-1993 The Regents of the University of California.
// All rights reserved.  See copyright.h for copyright notice and limitation 
// of liability and disclaimer of warranty provisions.

#include "copyright.h"
#include "scheduler.h"
#include "system.h"

//----------------------------------------------------------------------
// Scheduler::Scheduler
// 	Initialize the list of ready but not running threads to empty.
//----------------------------------------------------------------------

Scheduler::Scheduler()
{ 
    for (int i = 0; i < MAX_PRIORITY; i++)
		readyList[i] = new List<Thread*>;
} 

//----------------------------------------------------------------------
// Scheduler::~Scheduler
// 	De-allocate the list of ready threads.
//----------------------------------------------------------------------

Scheduler::~Scheduler()
{ 
    for (int i = 0; i < MAX_PRIORITY; i++)
		delete readyList[i];
} 

//----------------------------------------------------------------------
// Scheduler::ReadyToRun
// 	Mark a thread as ready, but not running.
//	Put it on the ready list, for later scheduling onto the CPU.
//
//	"thread" is the thread to be put on the ready list.
//----------------------------------------------------------------------

void
Scheduler::ReadyToRun (Thread *thread)
{
    DEBUG('t', "Putting thread %s on ready list.\n", thread->getName());

	thread->setStatus(READY);
	
	// Obtenemos la prioridad del hilo
	int priority = thread->getPriority();
    // Verificamos que la prioridad sea válida.
    ASSERT(0 <= priority && priority < MAX_PRIORITY);
    // Insertamos el hilo en la cola correspondiente a su prioridad.
    readyList[priority]->Append(thread);
}

//----------------------------------------------------------------------
// Scheduler::FindNextToRun
// 	Return the next thread to be scheduled onto the CPU.
//	If there are no ready threads, return NULL.
// Side effect:
//	Thread is removed from the ready list.
//----------------------------------------------------------------------

Thread *
Scheduler::FindNextToRun ()
{
	for (int i = (MAX_PRIORITY - 1); i >= 0; i--)
	{
		if (!readyList[i]->IsEmpty())
		{
			// Devolvemos el hilo con mayor prioridad.
			return readyList[i]->Remove();
		}
	}
	return NULL;
}

//----------------------------------------------------------------------
// Scheduler::Run
// 	Dispatch the CPU to nextThread.  Save the state of the old thread,
//	and load the state of the new thread, by calling the machine
//	dependent context switch routine, SWITCH.
//
//      Note: we assume the state of the previously running thread has
//	already been changed from running to blocked or ready (depending).
// Side effect:
//	The global variable currentThread becomes nextThread.
//
//	"nextThread" is the thread to be put into the CPU.
//----------------------------------------------------------------------

void
Scheduler::Run (Thread *nextThread)
{
    Thread *oldThread = currentThread;
    
#ifdef USER_PROGRAM			// ignore until running user programs 
    if (currentThread->space != NULL) {	// if this thread is a user program,
        currentThread->SaveUserState(); // save the user's CPU registers
	currentThread->space->SaveState();
    }
#endif
    
    oldThread->CheckOverflow();		    // check if the old thread
					    // had an undetected stack overflow

    currentThread = nextThread;		    // switch to the next thread
    currentThread->setStatus(RUNNING);      // nextThread is now running
    
    DEBUG('t', "Switching from thread \"%s\" to thread \"%s\"\n",
	  oldThread->getName(), nextThread->getName());
    
    // This is a machine-dependent assembly language routine defined 
    // in switch.s.  You may have to think
    // a bit to figure out what happens after this, both from the point
    // of view of the thread and from the perspective of the "outside world".

    SWITCH(oldThread, nextThread);
    
    DEBUG('t', "Now in thread \"%s\"\n", currentThread->getName());

    // If the old thread gave up the processor because it was finishing,
    // we need to delete its carcass.  Note we cannot delete the thread
    // before now (for example, in Thread::Finish()), because up to this
    // point, we were still running on the old thread's stack!
    if (threadToBeDestroyed != NULL) {
        delete threadToBeDestroyed;
	threadToBeDestroyed = NULL;
    }
    
#ifdef USER_PROGRAM
    if (currentThread->space != NULL) {		// if there is an address space
        currentThread->RestoreUserState();     // to restore, do it.
	currentThread->space->RestoreState();
    }
#endif
}

//----------------------------------------------------------------------
// Scheduler::Print
// 	Print the scheduler state -- in other words, the contents of
//	the ready list.  For debugging.
//----------------------------------------------------------------------

static void
ThreadPrint (Thread* t) {
  t->Print();
}

void
Scheduler::Print()
{
    printf("Ready list contents:\n");
    for (int i = 0; i < MAX_PRIORITY; i++)
    {
		if (!readyList[i]->IsEmpty())
		{
			printf("Threads with priority %d: ", i);
			readyList[i]->Apply(ThreadPrint);
			printf("\n");
		}
	}
}

//----------------------------------------------------------------------
// Scheduler::ChangePriority
// 	Cambiamos de cola al thread de acuerdo a su prioridad.
//----------------------------------------------------------------------

void
Scheduler::ChangePriority(Thread* thread)
{
	Thread *first, *temp;
	
	// Buscamos el thread dentro de cada lista; al encontrarlo,
	// lo sacamos.
	for (int i = 0; i < MAX_PRIORITY; i++)
	{
		if (!readyList[i]->IsEmpty())
		{
			first = readyList[i]->Remove();
			readyList[i]->Append(first);
			do
			{
				temp = readyList[i]->Remove();
				if (thread != temp)
				{
					readyList[i]->Append(temp);
				}
			} while (temp != first);
		}
	}
	
	// Obtenemos y verificamos la prioridad de thread.
	int priority = thread->getPriority();
	ASSERT(0 <= priority && priority < MAX_PRIORITY);
	// Insertamos thread en la cola correspondiente a su prioridad.
	readyList[priority]->Append(thread);
}
