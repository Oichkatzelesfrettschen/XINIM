/*<<< WORK-IN-PROGRESS MODERNIZATION HEADER
  This repository is a work in progress to reproduce the
  original MINIX simplicity on modern 32-bit and 64-bit
  ARM and x86/x86_64 hardware using C++17.
>>>*/

#include "../include/stdio.hpp"



/* Get a character from stream */
int getc(FILE *iop)
{
	int ch;

	if ( testflag(iop, (_EOF | _ERR )))
		return (EOF); 

	if ( !testflag(iop, READMODE) ) 
		return (EOF);

	if (--iop->_count <= 0){

		if ( testflag(iop, UNBUFF) )
			iop->_count = read(iop->_fd,&ch,1);
		else 
			iop->_count = read(iop->_fd,iop->_buf,BUFSIZ);

		if (iop->_count <= 0){
			if (iop->_count == 0)
				iop->_flags |= _EOF;
			else 
				iop->_flags |= _ERR;

			return (EOF);
		}
		else 
			iop->_ptr = iop->_buf;
	}

	if (testflag(iop,UNBUFF))
		return (ch & CMASK);
	else
		return (*iop->_ptr++ & CMASK);
}

