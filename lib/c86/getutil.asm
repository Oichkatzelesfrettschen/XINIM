.globl get_base, get_size, get_tot_
.globl endbss
.text
get_base:
    xor %eax, %eax
    ret
get_size:
    mov $endbss, %rax
    ret
get_tot_:
    xor %eax, %eax
    ret
