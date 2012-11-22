#ifndef SYNCHCONSOLE_H
#define SYNCHCONSOLE_H

#include "console.h"
#include "thread.h"
#include "synch.h"

class SynchConsole {

	public:

	SynchConsole(char* in, char* out);
	~SynchConsole();

	const char get();
	void put(char c);

	void writeStr(char *s, int size);

	int readStr(char* buffer, int size);

  
	void readAvailV(){readAvail->V();}
	void writeDoneV(){writeDone->V();}

	private:
		Semaphore* readAvail;
		Semaphore* writeDone;
		Console* console;
		Lock* lock;
};

#endif  //SYNCHCONSOLE_H
