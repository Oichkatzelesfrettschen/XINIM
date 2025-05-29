#include "../include/stdio.h"
#include "../include/lib.h"

fclose(fp)
FILE *fp;
{
	register int i;

	for (i=0; i<NFILES; i++)
		if (fp == _io_table[i]) {
			_io_table[i] = 0;
			break;
		}
	if (i >= NFILES)
		return(EOF);
	fflush(fp);
	close(fp->_fd);
        if ( testflag(fp,IOMYBUF) && fp->_buf )
                safe_free( fp->_buf );
        safe_free(fp);
	return(NULL);
}

