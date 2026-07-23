.section .text.init
.global _start
.type _start, @function

_start:
    la      sp, __stack_top
    andi    sp, sp, -16

    la      t0, __bss_start
    la      t1, __bss_end
1:
    bgeu    t0, t1, 2f
    sd      zero, 0(t0)
    addi    t0, t0, 8
    j       1b
2:
    call    __libc_init_array
    call    main
    call    _exit
3:
    j       3b
.size _start, . - _start
