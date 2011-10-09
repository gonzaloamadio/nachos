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

//----------------------------------------------------------------------
// Lock::Lock
//  Inicializamos el lock.
//  
//  "debugName" nombre arbitrario del lock
//----------------------------------------------------------------------

Lock::Lock(const char* debugName)
{
	name = debugName;
	// Inicializamos el semáforo en 1
	sem = new Semaphore("Lock Semaphore", 1);
	// Inicializamos el dueño en NULL
	owner = NULL;
}

//----------------------------------------------------------------------
// Lock::~Lock
//  Liberamos el lock. Destruimos el semáforo.
//----------------------------------------------------------------------
Lock::~Lock()
{
	delete sem;
}

//----------------------------------------------------------------------
// Lock::Acquire
//  
//----------------------------------------------------------------------
void Lock::Acquire() 
{
	// Hacemos ASSERT para evitar Acquires anidados
	ASSERT(!isHeldByCurrentThread());
	
	if (owner != NULL)
	{
		if (owner->getPriority() < currentThread->getPriority())
		{
			owner->setPriority(currentThread->getPriority());
			scheduler->ChangePriority(owner);
			DEBUG('t', "\"%s\" now has priority \"%d\"\n", 
				owner->getName(), owner->getPriority());
		}
	}
	
	// Hacemos P. Si está libre el currentThread toma el lock. Sino, el
	// currentThread se va a dormir
	sem->P();
	
	// Seteamos el dueño del lock
	owner = currentThread;
	
	DEBUG('t', "\"%s\" is currently holding \"%s\"\n", owner->getName(), 
		getName());
}

//----------------------------------------------------------------------
// Lock::Release
//  
//----------------------------------------------------------------------
void Lock::Release()
{
	// Hacemos ASSERT para chequear que el Release lo hace el hilo dueño
	ASSERT(isHeldByCurrentThread());
	DEBUG('t', "\"%s\" has released \"%s\"\n", owner->getName(), 
		getName());
	
	if (owner->getInitialPriority() != owner->getPriority())
	{
		owner->setPriority(owner->getInitialPriority());
		DEBUG('t', "\"%s\" has returned to its original priority \"%d\"\n", 
			owner->getName(), owner->getPriority());
	}
	
	// Seteamos el dueño en NULL. Lo hacemos antes de liberar el lock,
	// ya que si no lo hacemos puede haber cambio de contexto y
	// ejecutarse luego de que alguien tome el lock. 
	owner = NULL;
	
	// Liberamos el lock. Incrementamos el semáforo y despertamos a
	// algún hilo que se haya bloqueado con Acquire
	sem->V();
}

//----------------------------------------------------------------------
// Lock::isHeldByCurrentThread
//  Devolvemos true si el hilo actual tiene el lock.
//----------------------------------------------------------------------
bool Lock::isHeldByCurrentThread()
{
	return (currentThread == owner);
}

//----------------------------------------------------------------------
// Condition::Condition
//  Inicializamos la variable de condición.
//  
//  "debugName" nombre arbitrario de la variable de la condición
//  "conditionLock" lock que va a usar la variable de condición
//----------------------------------------------------------------------

Condition::Condition(const char* debugName, Lock* conditionLock)
{
	name = debugName;
	lock = conditionLock;
	semList = new List<Semaphore*>;
}
Condition::~Condition()
{
	lock = NULL;
	delete semList;
}
void Condition::Wait()
{
	Semaphore* sem;
	// ASSERT para chequear que el Wait lo llame el dueño del lock
	ASSERT(lock->isHeldByCurrentThread());
	
	// Creamos el semáforo para bloquear al dueño de lock
	sem = new Semaphore("CV Semaphore", 0);
	semList->Append(sem);
	// Liberamos el lock
	lock->Release();
	// Bloqueamos al dueño de lock y luego liberamos la memoria del sem.
	sem->P();
	delete sem;
	// Al despertarse, el hilo vuelve a tomar el lock
	lock->Acquire();
}
void Condition::Signal()
{
	Semaphore* sem;
	ASSERT(lock->isHeldByCurrentThread());
	// Despertamos el hilo del primer semáforo
	if (!semList->IsEmpty())
	{
		sem = semList->Remove();
		sem->V();
	}
}
void Condition::Broadcast()
{
	Semaphore* sem;
	ASSERT(lock->isHeldByCurrentThread());
	// Recorremos la lista de semáforos y despertamos a todos los hilos
	while (!semList->IsEmpty())
	{
		sem = semList->Remove();
		sem->V();
	}
}

//----------------------------------------------------------------------
// Port::Port
//  Inicializamos el puerto.
//  
//  "debugName" nombre arbitrario del puerto
//----------------------------------------------------------------------

Port::Port(const char *debugName)
{
	name = debugName;
	senders = 0;
	receivers = 0;
	lock = new Lock("Port Lock");
	sendCondition = new Condition("Port sendCondition", lock);
	receiveCondition = new Condition("Port receiveCondition", lock);
	emptyMessage = true;
}

//----------------------------------------------------------------------
// Port::~Port
//  Liberamos el puerto. Destruimos todo las VdC y el lock.
//----------------------------------------------------------------------

Port::~Port()
{
	delete sendCondition;
	delete receiveCondition;
	delete lock;
}

void Port::Send(int message)
{
	lock->Acquire();
	senders++;
	while (receivers == 0 || !emptyMessage) // para no sobreescribir un msj existente
		sendCondition->Wait();	
	receivers--;
	theMessage = message;
	emptyMessage = false;
	receiveCondition->Signal();
	lock->Release();
}


void Port::Receive(int *message)
{
	lock->Acquire();
	receivers++;
	sendCondition->Signal();
	while (senders == 0 || emptyMessage)
		receiveCondition->Wait();	
	senders--;
	*message = theMessage;
	emptyMessage = true;
	sendCondition->Signal();
	lock->Release();
}
