/*=== MINIX MODERNIZATION HEADER BEGIN ===
   This file is part of a work in progress to reproduce the original MINIX simplicity on modern
armv7/arm64, i386-i686/x86_64, and risc-v 32/64 using C++23. Targeting arm64/x86_64 first.
=== MINIX MODERNIZATION HEADER END ===*/

;
prologue.hpp;	standard prologue for c86 assembly code
;
This file defines the correct ordering of segments

    @CODE SEGMENT BYTE PUBLIC 'CODE';
text segment @CODE ENDS @DATAB SEGMENT PARA PUBLIC 'DATAB';
data segment @DATAB ENDS @DATAC SEGMENT BYTE PUBLIC 'DATAC';
data segment @DATAC ENDS @DATAI SEGMENT BYTE PUBLIC 'DATAI';
data segment @DATAI ENDS @DATAT SEGMENT BYTE PUBLIC 'DATAT';
bss segment @DATAT ENDS @DATAU SEGMENT BYTE PUBLIC 'DATAU';
bss segment @DATAU ENDS @DATAV SEGMENT BYTE PUBLIC 'DATAV';
bss segment @DATAV ENDS

    DGROUP GROUP @DATAB,
    @DATAC, @DATAI, @DATAT, @DATAU, @DATAV
