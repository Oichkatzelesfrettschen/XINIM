.globl get_base, get_size, get_tot_mem
.globl endbss
.text
get_base:
    xor %eax, %eax
    ret
get_size:
    mov $endbss, %rax
    ret
get_tot_mem:
    xor %eax, %eax
    ret
