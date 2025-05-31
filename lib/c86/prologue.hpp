/*<<< WORK-IN-PROGRESS MODERNIZATION HEADER
  This repository is a work in progress to reproduce the
  original MINIX simplicity on modern 32-bit and 64-bit
  ARM and x86/x86_64 hardware using C++17.
>>>*/

;
prologue.h;	standard prologue for c86 assembly code
;
This file defines the correct ordering of segments

    @CODE SEGMENT BYTE 'CODE';
text segment @CODE ENDS @DATAB SEGMENT PARA 'DATAB';
data segment @DATAB ENDS @DATAC SEGMENT BYTE 'DATAC';
data segment @DATAC ENDS @DATAI SEGMENT BYTE 'DATAI';
data segment @DATAI ENDS @DATAT SEGMENT BYTE 'DATAT';
bss segment @DATAT ENDS @DATAU SEGMENT BYTE 'DATAU';
bss segment @DATAU ENDS @DATAV SEGMENT BYTE 'DATAV';
bss segment @DATAV ENDS

    DGROUP GROUP @DATAB,
    @DATAC, @DATAI, @DATAT, @DATAU, @DATAV
