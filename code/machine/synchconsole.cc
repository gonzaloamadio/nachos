#include "synchconsole.h"

//----------------------------------------------------------------------
// ConsoleInterruptHandlers
// 	Wake up the thread that requested the I/O.
//----------------------------------------------------------------------

void readCallback(int arg) {((SynchConsole*)arg)->readAvailV();}
void writeCallback(int arg){((SynchConsole*)arg)->writeDoneV();}

SynchConsole::SynchConsole(char* in, char* out){

  console   = new Console(in,out,
			  readCallback,
			  writeCallback,
			  (int)this);

  readAvail = new Semaphore("Read Semaphore SynchConsole",0);
  writeDone = new Semaphore("Write Semaphore SynchConsole",0);
  lock      = new Lock("Lock SynchConsole");
}

SynchConsole::~SynchConsole(){

  delete lock;
  delete writeDone;
  delete readAvail;
  delete console;
}

void SynchConsole::put(char c){
  lock->Acquire();
  console->PutChar(c);
  writeDone->P();
  lock->Release();
}

const char SynchConsole::get(){
  lock->Acquire();
  readAvail->P();
  char c = console->GetChar();
  lock->Release();
  return c;
}

void SynchConsole::readStr(char* buffer, int size){
    for (int i = 0; i < size ; i++)
      buffer[i] = get();
}

void SynchConsole::writeStr(char *s, int size){
    for (int i = 0; i < size; i++)
      put(s[i]);
  }

