.globl send, receive, send_rec, sendrec
SEND=1
RECEIVE=2
BOTH=3
.text
send:
    mov $0, %rax
    mov $SEND, %rdx
    syscall
    ret

receive:
    mov $0, %rax
    mov $RECEIVE, %rdx
    syscall
    ret

sendrec:
send_rec:
    mov $0, %rax
    mov $BOTH, %rdx
    syscall
    ret
