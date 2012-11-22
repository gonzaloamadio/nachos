#include "synchconsole.h"

void readCallback(void* arg)
{
	((SynchConsole*) arg)->readAvailV();
}
void writeCallback(void* arg)
{
	((SynchConsole*) arg)->writeDoneV();
}

SynchConsole::SynchConsole(char* in, char* out)
{
	console = new Console(in,out, (VoidFunctionPtr) readCallback,
				(VoidFunctionPtr) writeCallback,
				(void*) this);

	readAvail = new Semaphore("Read Semaphore SynchConsole", 0);
	writeDone = new Semaphore("Write Semaphore SynchConsole", 0);
	lock = new Lock("Lock SynchConsole");
}

SynchConsole::~SynchConsole()
{
	delete lock;
	delete writeDone;
	delete readAvail;
	delete console;
}

void SynchConsole::put(char c)
{
	lock->Acquire();
	console->PutChar(c);
	writeDone->P();
	lock->Release();
}

const char SynchConsole::get()
{
	lock->Acquire();
	readAvail->P();
	char c = console->GetChar();
	lock->Release();
	return c;
}

int SynchConsole::readStr(char* buffer, int size)
{
	int i;
	for (i = 0; i < size; i++)
	{
		if ((buffer[i] = get()) == '\n')
			break;
	}
	buffer[++i] = '\0';
	return i;
}

void SynchConsole::writeStr(char *s, int size)
{
	for (int i = 0; i < size; i++)
	{
		put(s[i]);
		if (s[i] == '\0') break;
	}
}
