/*<<< WORK-IN-PROGRESS MODERNIZATION HEADER
  This repository is a work in progress to reproduce the
  original MINIX simplicity on modern 32-bit and 64-bit
  ARM and x86/x86_64 hardware using C++17.
>>>*/

#include "../include/stdio.h"

ungetc(ch, iop)
int ch;
FILE *iop;
{
	if ( ch < 0  || !testflag(iop,READMODE) || testflag(iop,UNBUFF) ) 
		return( EOF );
	
	if ( iop->_count >= BUFSIZ)
		return(EOF);

	if ( iop->_ptr == iop->_buf)
		iop->_ptr++;

	iop->_count++;
	*--iop->_ptr = ch;
	return(ch);
}
