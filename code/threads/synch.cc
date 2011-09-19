// synch.cc 
//	Routines for synchronizing threads.  Three kinds of
//	synchronization routines are defined here: semaphores, locks 
//   	and condition variables (the implementation of the last two
//	are left to the reader).
//
// Any implementation of a synchronization routine needs some
// primitive atomic operation.  We assume Nachos is running on
// a uniprocessor, and thus atomicity can be provided by
// turning off interrupts.  While interrupts are disabled, no
// context switch can occur, and thus the current thread is guaranteed
// to hold the CPU throughout, until interrupts are reenabled.
//
// Because some of these routines might be called with interrupts
// already disabled (Semaphore::V for one), instead of turning
// on interrupts at the end of the atomic operation, we always simply
// re-set the interrupt state back to its original value (whether
// that be disabled or enabled).
//
// Copyright (c) 1992-1993 The Regents of the University of California.
// All rights reserved.  See copyright.h for copyright notice and limitation 
// of liability and disclaimer of warranty provisions.

#include "copyright.h"
#include "synch.h"
#include "system.h"

//----------------------------------------------------------------------
// Semaphore::Semaphore
// 	Initialize a semaphore, so that it can be used for synchronization.
//
//	"debugName" is an arbitrary name, useful for debugging.
//	"initialValue" is the initial value of the semaphore.
//----------------------------------------------------------------------

Semaphore::Semaphore(const char* debugName, int initialValue)
{
    name = debugName;
    value = initialValue;
    queue = new List<Thread*>;
}

//----------------------------------------------------------------------
// Semaphore::Semaphore
// 	De-allocate semaphore, when no longer needed.  Assume no one
//	is still waiting on the semaphore!
//----------------------------------------------------------------------

Semaphore::~Semaphore()
{
    delete queue;
}

//----------------------------------------------------------------------
// Semaphore::P
// 	Wait until semaphore value > 0, then decrement.  Checking the
//	value and decrementing must be done atomically, so we
//	need to disable interrupts before checking the value.
//
//	Note that Thread::Sleep assumes that interrupts are disabled
//	when it is called.
//----------------------------------------------------------------------

void
Semaphore::P()
{
    IntStatus oldLevel = interrupt->SetLevel(IntOff);	// disable interrupts
    
    while (value == 0) { 			// semaphore not available
    DEBUG('t', "Sent \"%s\" to sleep using \"%s\"\n", currentThread->getName(), getName());
	queue->Append(currentThread);		// so go to sleep
	currentThread->Sleep();
    } 
    value--; 					// semaphore available, 
						// consume its value
    
    interrupt->SetLevel(oldLevel);		// re-enable interrupts
}

//----------------------------------------------------------------------
// Semaphore::V
// 	Increment semaphore value, waking up a waiter if necessary.
//	As with P(), this operation must be atomic, so we need to disable
//	interrupts.  Scheduler::ReadyToRun() assumes that threads
//	are disabled when it is called.
//----------------------------------------------------------------------

void
Semaphore::V()
{
    Thread *thread;
    IntStatus oldLevel = interrupt->SetLevel(IntOff);

    thread = queue->Remove();
    if (thread != NULL)	   // make thread ready, consuming the V immediately
	scheduler->ReadyToRun(thread);
    value++;
    interrupt->SetLevel(oldLevel);
}

// Dummy functions -- so we can compile our later assignments 
// Note -- without a correct implementation of Condition::Wait(), 
// the test case in the network assignment won't work!
Lock::Lock(const char* debugName)
{
	name = debugName; 
	lockSemaphore = new Semaphore("Lock Semaphore", 1); //Iniciamos el semaforo en 1 para que comienze libre
	lockThread = NULL; // dejamos apuntando el thread a NULL para inicializarlo en algo
}
Lock::~Lock()
{
	delete lockSemaphore;
}
void Lock::Acquire() 
{                                     // Lo hacemos con assert para que tire un error apropiado.
	ASSERT(!isHeldByCurrentThread()); //Nos fijamos que no haya acquire anidado, para que no lo vuelva a poner en la cola
	lockSemaphore->P(); // Decrementamos el valor del semaforo, si no es el currentThread , P() lo manda a dormir
	lockThread = currentThread;  // Si viene otro proceso que no es el que tiene el cerrojo, es mandado a dormir y no se ejecuta esta sentencia.
	DEBUG('t', "\"%s\" is currently holding \"%s\"\n", lockThread->getName(), getName());
}
void Lock::Release()
{
	ASSERT(isHeldByCurrentThread());
	DEBUG('t', "\"%s\" has released \"%s\"\n", lockThread->getName(), getName());
	lockThread = NULL;  // Es importante hacer esto antes de V(). Si lo pongo después de liberar el lock
	                    // puede haber cambio de contexto y pisar que thread tiene el lock.
	lockSemaphore->V(); // Si es el hilo actual, lo removemos de la cola, lo dejamos en estado listo con V().
}
bool Lock::isHeldByCurrentThread()
{
	return (currentThread == lockThread);
}

Condition::Condition(const char* debugName, Lock* conditionLock)
{
	name = debugName;
	cvLock = conditionLock;
	cvSemList = new List<Semaphore*>;
}
Condition::~Condition()
{
	cvLock = NULL;
	delete cvSemList;
}
void Condition::Wait()
{
	Semaphore* sem;
	ASSERT(cvLock->isHeldByCurrentThread());
	sem = new Semaphore("CV Semaphore", 0);
	cvSemList->Append(sem);
	cvLock->Release();
	sem->P();
	cvLock->Acquire();
}
void Condition::Signal()
{
	Semaphore* sem;
	ASSERT(cvLock->isHeldByCurrentThread());
	if (!cvSemList->IsEmpty())
	{
		sem = cvSemList->Remove();
		sem->V();
	}
}
void Condition::Broadcast()
{
	Semaphore* sem;
	ASSERT(cvLock->isHeldByCurrentThread());
	while (!cvSemList->IsEmpty())  // Recorremos toda la lista, vamos removiendo los hilos y
	{                            // dejandolos en estado listos para correr. 
		sem = cvSemList->Remove();
		sem->V();
	}
}

Port::Port(const char *debugName)
{
	name = debugName;
	sender = 0;
	receiver = 0;
	srLock = new Lock("Port Lock");
	sendCondition = new Condition("Port sendCondition",srLock);
	receiveCondition = new Condition("Port receiveCondition",srLock);
	emptyMessage = true;
}

Port::~Port()
{
	delete sendCondition;
	delete receiveCondition;
	delete srLock;
}

void Port::Send(int message)
{
	srLock->Acquire();
	sender++;
	while (receiver == 0 || (emptyMessage == false)) // para no sobreescribir un msj existente
		sendCondition->Wait();	
	receiver--;
	theMessage = message;
	emptyMessage = false;
	receiveCondition->Signal();
	srLock->Release();
}


void Port::Receive(int *message)
{
	srLock->Acquire();
	receiver++;
	sendCondition->Signal();
	while (sender == 0 || emptyMessage)
		receiveCondition->Wait();	
	sender--;
	*message = theMessage;
	emptyMessage = true;
	sendCondition->Signal();
	srLock->Release();
}
