// exception.cc 
//	Entry point into the Nachos kernel from user programs.
//	There are two kinds of things that can cause control to
//	transfer back to here from user code:
//
//	syscall -- The user code explicitly requests to call a procedure
//	in the Nachos kernel.  Right now, the only function we support is
//	"Halt".
//
//	exceptions -- The user code does something that the CPU can't handle.
//	For instance, accessing memory that doesn't exist, arithmetic errors,
//	etc.  
//
//	Interrupts (which can also cause control to transfer from user
//	code into the Nachos kernel) are handled elsewhere.
//
// For now, this only handles the Halt() system call.
// Everything else core dumps.
//
// Copyright (c) 1992-1993 The Regents of the University of California.
// All rights reserved.  See copyright.h for copyright notice and limitation 
// of liability and disclaimer of warranty provisions.

#include "copyright.h"
#include "system.h"
#include "syscall.h"
#include "openfile.h"
#include "synchconsole.h"

bool ReadString(int addr, char *buffer);
bool WriteString(int addr, char *buffer);
bool ReadBuffer(int addr, char *buffer, int size);
bool WriteBuffer(int addr, char *buffer, int size);
void UpdateProgramCounter();
void newThreadExec(void* arg);

//----------------------------------------------------------------------
// ExceptionHandler
// 	Entry point into the Nachos kernel.  Called when a user program
//	is executing, and either does a syscall, or generates an addressing
//	or arithmetic exception.
//
// 	For system calls, the following is the calling convention:
//
// 	system call code -- r2
//		arg1 -- r4
//		arg2 -- r5
//		arg3 -- r6
//		arg4 -- r7
//
//	The result of the system call, if any, must be put back into r2. 
//
// And don't forget to increment the pc before returning. (Or else you'll
// loop making the same system call forever!
//
//	"which" is the kind of exception.  The list of possible exceptions 
//	are in machine.h.
//----------------------------------------------------------------------

void
ExceptionHandler(ExceptionType which)
{
	// En el registro r2 está el código de la system call
    int type = machine->ReadRegister(2); 
    int arg1 = machine->ReadRegister(4);
    int arg2 = machine->ReadRegister(5);
    int arg3 = machine->ReadRegister(6);
    //int arg4 = machine->ReadRegister(7);
	
	char buffer[256];
	OpenFile *op;
	
    if ((which == SyscallException)) {
		switch (type) {
			    case SC_Halt:
						DEBUG('a', "Shutdown, initiated by user program.\n");
						interrupt->Halt();
						break;
				// void Exit(int status);
				case SC_Exit:
						DEBUG('a', "Thread \"%s\" exited with status %d", currentThread->getName(), arg1);
						currentThread->setExitStatus(arg1);
						currentThread->Finish();
						break;
				// SpaceId Exec(char *name);
				case SC_Exec:
						if (!ReadString(arg1, buffer))
						{
							DEBUG('a', "Could not read the string in user space in syscall Exec\n"); 
							machine->WriteRegister(2, -1);
							break;
						}
						
						OpenFile *executable;
						executable = fileSystem->Open(buffer);
						if (executable == NULL)
						{
							DEBUG('a', "Could not open the executable \"%s\"\n", buffer); 
							machine->WriteRegister(2, -1);
							break;
						}
						
						AddrSpace *space;
						space = new AddrSpace(executable);
						
						Thread *thread;
						thread = new Thread(buffer, 1, 0);
						thread->space = space;
						thread->Fork(newThreadExec, (void*) 0);
						
						machine->WriteRegister(2, (SpaceId) thread);
						break;
						
				// int Join(SpaceId id);
				case SC_Join:
						int st;
						st = currentThread->Join((Thread*) arg1);
						machine->WriteRegister(2, st);
						break;
				
				// void Create(char *name);
				case SC_Create:
						if (ReadString(arg1, buffer))
						{
							fileSystem->Create(buffer, 0);
							DEBUG('a', "Created a new file called \"%s\".\n", buffer);
						}
						else
							DEBUG('a', "Could not read the string in user space in syscall Create\n"); 
						break;
				
				// OpenFileId Open(char *name);
				case SC_Open:
						if (!ReadString(arg1, buffer))
						{
							DEBUG('a', "Could not read the string in user space in syscall Open\n"); 
							machine->WriteRegister(2, -1);
							break;
						}
						
						op = fileSystem->Open(buffer);
						if (op == NULL)
						{
							DEBUG('a', "Could not open the file \"%s\"\n", buffer); 
							machine->WriteRegister(2, -1);
							break;
						}
						
						OpenFileId fd;
						fd = currentThread->createFD(op);
						DEBUG('a', "Opened the file called \"%s\" with file descriptor \"%d\".\n", buffer, fd);
						machine->WriteRegister(2, fd);
						break;
						
				// void Write(char *buffer, int size, OpenFileId id);
				case SC_Write:
						if (arg3 == ConsoleInput)
						{
							printf("Invalid File Descriptor\n");
							DEBUG('a', "Can't write in the input.\n");
							break;
						}
						
						if (ReadBuffer(arg1, buffer, arg2))
						{						
							if (arg3 == ConsoleOutput)
							{
								synchConsole->writeStr(buffer, arg2);
								DEBUG('a', "Wrote \"%s\" to the console.\n", buffer);
							}
							if (arg3 >= 2)
							{
								op = currentThread->getFD(arg3);
								if (op != NULL)
								{
									op->Write(buffer, arg2);
									DEBUG('a', "Wrote \"%s\" in the file with file descriptor \"%d\".\n", buffer, arg3);
								}
								else
									DEBUG('a', "There is no file descriptor with number \"%d\"\n", arg3);
							}
						}
						else
							DEBUG('a', "Could not read the buffer in syscall Write\n"); 
						break;
						
				// int Read(char *buffer, int size, OpenFileId id);
				case SC_Read:
						if (arg3 == ConsoleOutput)
						{
							printf("Invalid File Descriptor\n");
							DEBUG('a', "Can't read the output.\n");
							machine->WriteRegister(2, -1);
						}
						
						if (arg3 == ConsoleInput)
						{
							int readBytes;
							readBytes = synchConsole->readStr(buffer, arg2);
							if (!WriteBuffer(arg1, buffer, arg2))
							{
								DEBUG('a', "Could not write string to user space in syscall Read\n");
								machine->WriteRegister(2, -1);
								break;
							}
							DEBUG('a', "Read \"%s\" with length %d from the console.\n", buffer, readBytes);
							machine->WriteRegister(2, readBytes);
						}
						
						if (arg3 >= 2)
						{
							op = currentThread->getFD(arg3);
							if (op == NULL)
							{
								DEBUG('a', "There is no file descriptor with number \"%d\"\n", arg3);
								machine->WriteRegister(2, -1);
								break;
							}
							
							int readBytes;
							readBytes = op->Read(buffer, arg2);
							if (WriteBuffer(arg1, buffer, readBytes))
							{
								DEBUG('a', "Read \"%s\" from the file with file descriptor \"%d\".\n", buffer, arg3);
								machine->WriteRegister(2, readBytes);
							}
							else
							{
								DEBUG('a', "Could not write string to user space in syscall Read\n");
								machine->WriteRegister(2, -1);
							}
						}
						break;
						
				// void Close(OpenFileId id);
				case SC_Close:
						currentThread->removeFD(arg1);
						DEBUG('a', "Closed the file with file descriptor \"%d\".\n",  arg1);
						break;
						
				default: break;
		}
		UpdateProgramCounter();
    } else {
	printf("Unexpected user mode exception %d %d\n", which, type);
	ASSERT(false);
    }
    
    
}

// Hacer funciones para leer string, leer buffer, y escribir ambas

bool ReadString(int addr, char *buffer)
{
	int i = 0;
	do
	{
		if (!machine->ReadMem(addr + i, 1, (int*) &buffer[i]))
			return false;
	} while (buffer[i++] != '\0');
	return true;
}

bool WriteString(int addr, char *buffer)
{
	int i = 0;
	do
	{
		if (!machine->WriteMem(addr + i, 1, (int) buffer[i]))
			return false;
	} while (buffer[i++] != '\0');
	return true;
}

bool ReadBuffer(int addr, char *buffer, int size)
{
	int i = 0;
	while (i < size)
	{
		if (!machine->ReadMem(addr + i, 1, (int*) &buffer[i]))
			return false;
		i++;
	}
	return true;
}

bool WriteBuffer(int addr, char *buffer, int size)
{
	int i = 0;
	while (i < size)
	{
		if (!machine->WriteMem(addr + i, 1, (int) buffer[i]))
			return false;
		i++;
	}
	return true;
}

void UpdateProgramCounter()
{
	int pc;
	pc = machine->ReadRegister(PCReg);
	machine->WriteRegister(PrevPCReg, pc);
	pc = machine->ReadRegister(NextPCReg);
	machine->WriteRegister(PCReg, pc);
	pc += 4;
	machine->WriteRegister(NextPCReg, pc);
}

void newThreadExec(void* arg)
{
	currentThread->space->InitRegisters();
	currentThread->space->RestoreState();
	machine->Run();
}
