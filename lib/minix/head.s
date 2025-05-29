.globl _start, _main, _stackpt
.globl begtext, begdata, begbss, _data_org, brksize, sp_limit
.text
begtext:
_start:
    movq _stackpt(%rip), %rsp
    call _main
1:
    jmp 1b
_exit:
    jmp _exit
.data
begdata:
_data_org:
    .quad 0xDADA
    .quad 0,0,0,0,0,0,0
brksize:
    .quad endbss
sp_limit:
    .quad 0
.bss
begbss:
