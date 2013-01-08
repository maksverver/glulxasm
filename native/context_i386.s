.global context_start
.global context_restart
.global context_save
.global context_restore
.global context_sp

#   +--------------+  <--  stack + size
#   |  return SP   |
#   +--------------+  <--  stack + size -  4
#   |  main arg    |
#   +--------------+  <--  stack + size -  8
#   |  return IP   |
#   +==============+  <--  stack + size - 12
#   | stack frame  |
#   | for main etc |
#   ~              ~
#   |              |
#   +--------------+  <-- stack


context_start:
    pushl   %ebp
    movl    %esp, %ebp

    movl     8(%ebp), %edx      # edx = stack
    addl    12(%ebp), %edx      # edx += size
    movl    16(%ebp), %eax      # eax = &main
    movl    20(%ebp), %ecx      # ecx = arg

    xchgl   %edx, %esp          # stack <--> SP
    pushl   %edx                # return SP
    pushl   %ecx                # arg
    call    *%eax               # main
return_from_context:
    addl    $4, %esp
    popl    %esp
    popl    %ebp
    ret


context_restart:
    pushl   %ebp
    movl    %esp, %ebp

    movl     8(%ebp), %eax      # eax = stack
    addl    12(%ebp), %eax      # eax += size

    movl    %esp, -4(%eax)      # set return SP
    movl    $return_from_context, -12(%eax)     # set return IP

    pushl   20(%ebp)            # arg
    pushl   16(%ebp)            # context ptr
    call    context_restore     # (does not return)

    int     $3                  # we should never get here!
    popl    %ebp
    ret


context_save:
    movl    4(%esp), %edx       # edx = context ptr
    movl    (%esp), %eax        # eax = return IP
    movl    %ebx,    0(%edx)
    movl    %esp,    4(%edx)
    movl    %ebp,    8(%edx)
    movl    %esi,   12(%edx)
    movl    %edi,   16(%edx)
    movl    %eax,   20(%edx)
    xorl    %eax, %eax          # return NULL
    ret


context_restore:
    movl    4(%esp), %edx       # edx = context ptr
    movl    8(%esp), %eax       # eax = arg (value to return from context_save)

    movl     0(%edx), %ebx
    movl     4(%edx), %esp
    movl     8(%edx), %ebp
    movl    12(%edx), %esi
    movl    16(%edx), %edi
    movl    20(%edx), %edx      # edx = saved IP

    movl    %edx, (%esp)        # return to saved Ip
    ret


context_sp:
    movl    %esp, %eax
    ret
