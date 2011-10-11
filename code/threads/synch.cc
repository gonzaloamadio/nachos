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
//  "debugName" nombre arbitrario del lock.
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
//  El hilo adquiere el lock. Si el lock ya está tomado, mandamos a
//  dormir el hilo que trata de tomarlo a través del semáforo.
//  Además, si el lock está tomado por el hilo owner y un nuevo hilo
//  con mayor prioridad trata de tomar el lock, hacemos inversión de 
//  prioridades. 
//----------------------------------------------------------------------

void Lock::Acquire() 
{
	// Hacemos ASSERT para evitar Acquires anidados.
	ASSERT(!isHeldByCurrentThread());
	
	// Si el hilo dueño tiene menor prioridad, le damos la prioridad del
	// hilo actual y cambiamos su lugar en la multicola del scheduler.
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
	// currentThread se va a dormir.
	sem->P();
	
	// Seteamos el dueño del lock.
	owner = currentThread;
	
	DEBUG('t', "\"%s\" is currently holding \"%s\"\n", owner->getName(), 
		getName());
}

//----------------------------------------------------------------------
// Lock::Release
//  El hilo dueño libera el lock.
//  Si hubo inversión de prioridades, seteamos la prioridad del hilo
//  dueño a la inicial.
//----------------------------------------------------------------------

void Lock::Release()
{
	// Hacemos ASSERT para chequear que el Release lo hace el hilo dueño
	ASSERT(isHeldByCurrentThread());
	DEBUG('t', "\"%s\" has released \"%s\"\n", owner->getName(), 
		getName());
	
	// Si la prioridad inicial del hilo dueño es distinta a la actual,
	// hubo inversión de prioridades y hacemos que la prioridad del hilo
	// vuelva a la inicial. 
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
	// algún hilo que se haya bloqueado con Acquire.
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
//  "debugName" nombre arbitrario de la variable de la condición.
//  "conditionLock" lock que va a usar la variable de condición.
//----------------------------------------------------------------------

Condition::Condition(const char* debugName, Lock* conditionLock)
{
	name = debugName;
	lock = conditionLock;
	semList = new List<Semaphore*>;
}

//----------------------------------------------------------------------
// Condition::~Condition
//  Liberamos la variable de condición. Destruimos la lista de
//  semáforos.
//----------------------------------------------------------------------

Condition::~Condition()
{
	lock = NULL;
	delete semList;
}

//----------------------------------------------------------------------
// Condition::Wait
//  Bloqueamos al dueño del lock con el semáforo creado. Luego al salir
//  de P(), el hilo vuelve a tomar el lock.
//----------------------------------------------------------------------

void Condition::Wait()
{
	Semaphore* sem;
	// ASSERT para chequear que el Wait lo llame el dueño del lock.
	ASSERT(lock->isHeldByCurrentThread());
	
	// Creamos el semáforo para bloquear al dueño de lock.
	sem = new Semaphore("CV Semaphore", 0);
	semList->Append(sem);
	// Liberamos el lock.
	lock->Release();
	// Bloqueamos al dueño de lock y luego liberamos la memoria del sem.
	sem->P();
	delete sem;
	// Al despertarse, el hilo vuelve a tomar el lock.
	lock->Acquire();
}

//----------------------------------------------------------------------
// Condition::Signal
//  Despertamos al hilo del primer semáforo de la lista, mandado a
//  dormir con P() en Wait.
//----------------------------------------------------------------------

void Condition::Signal()
{
	Semaphore* sem;
	ASSERT(lock->isHeldByCurrentThread());
	// Despertamos el hilo del primer semáforo.
	if (!semList->IsEmpty())
	{
		sem = semList->Remove();
		sem->V();
	}
}

//----------------------------------------------------------------------
// Condition::Broadcast
//  Despertamos a todos los hilos de todos los semáforos de la lista,
//  mandados a dormir con P() en Wait.
//----------------------------------------------------------------------

void Condition::Broadcast()
{
	Semaphore* sem;
	ASSERT(lock->isHeldByCurrentThread());
	// Recorremos la lista de semáforos y despertamos a todos los hilos.
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
//  "debugName" nombre arbitrario del puerto.
//----------------------------------------------------------------------

Port::Port(const char *debugName)
{
	name = debugName;
	senders = 0;
	receivers = 0;
	emptyMessage = true;
	// Lock y Variables de Condición
	lock = new Lock("Port Lock");
	senderCondition = new Condition("Port Sender Condition", lock);
	receiverCondition = new Condition("Port Receiver Condition", lock);
}

//----------------------------------------------------------------------
// Port::~Port
//  Liberamos el puerto. Destruimos todas las variables de condición y
//  el lock.
//----------------------------------------------------------------------

Port::~Port()
{
	delete senderCondition;
	delete receiverCondition;
	delete lock;
}

//----------------------------------------------------------------------
// Port::Send
//  Mandamos el mensaje. Adquirimos el lock, avisamos que hay un emisor;
//  si no podemos enviar, esperamos. Cuando sea posible enviar, enviamos
//  y avisamos a un receptor. Luego, liberamos el lock.
//
//  "message" contendrá el mensaje a ser enviado.
//----------------------------------------------------------------------

void Port::Send(int message)
{
	lock->Acquire();
	
	// Incrementamos senders para avisar que hay un emisor.
	senders++;
	// Mientras que no haya receptores o haya un mensaje en el buffer,
	// el emisor espera con senderCondition.
	while (receivers == 0 || !emptyMessage)
		senderCondition->Wait();
	// Salimos del while, entonces podemos enviar.
	// Decrementamos receivers porque el receptor recibirá el mensaje. 
	receivers--;
	// Copiamos el mensaje en el buffer.
	buffer = message;
	// Avisamos que hay un mensaje en el buffer.
	emptyMessage = false;
	// Despertamos a un receptor.
	receiverCondition->Signal();
	
	lock->Release();
}

//----------------------------------------------------------------------
// Port::Receive
//  Recibimos el mensaje. Adquirimos el lock, avisamos que hay un
//  receptor y despertamos a algún emisor; si no podemos recibir,
//  esperamos. Cuando sea posible, recibimos el mensaje, lo copiamos en
//  en message y avisamos a un emisor. Luego, liberamos el lock.
//
//  "*message" contendrá el mensaje recibido.
//----------------------------------------------------------------------

void Port::Receive(int *message)
{
	lock->Acquire();
	
	// Incrementamos receivers para avisar que hay un receptor.
	receivers++;
	// Despertamos a un emisor porque hay un receptor.
	senderCondition->Signal();
	// Mientras que no haya emisores o no haya un mensaje en el buffer,
	// el receptor espera con receiverCondition.
	while (senders == 0 || emptyMessage)
		receiverCondition->Wait();
	// Salimos del while, entonces podemos recibir.
	// Decrementamos senders porque el emisor envió el mensaje.
	senders--;
	// Copiamos el mensaje del buffer en message.
	*message = buffer;
	// Avisamos que el buffer está vacío.
	emptyMessage = true;
	// Despertamos a algún emisor para que siga mandando mensajes.
	senderCondition->Signal();
	
	lock->Release();
}
