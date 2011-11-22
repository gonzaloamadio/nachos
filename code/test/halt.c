/* halt.c
 *	Simple program to test whether running a user program works.
 *	
 *	Just do a "syscall" that shuts down the OS.
 *
 * 	NOTE: for some reason, user programs with global data structures 
 *	sometimes haven't worked in the Nachos environment.  So be careful
 *	out there!  One option is to allocate data structures as 
 * 	automatics within a procedure, but if you do this, you have to
 *	be careful to allocate a big enough stack to hold the automatics!
 */

#include "syscall.h"

int
main()
{
	OpenFileId o, s;
	char buffer[20];
    Create("archivo_nuevo");
    Create("archivo_nuevo_2");
    o = Open("archivo_nuevo");
    Write("Hola Mundo!", 11, o);
    Read(buffer, 17, o);
    s = Open("archivo_nuevo_2");
    Write(buffer, 6, s);
    Halt();
    /* not reached */
}
