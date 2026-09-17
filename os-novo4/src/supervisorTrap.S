.extern _ZN5Riscv20handleSupervisorTrapEv
.align 4
.global _ZN5Riscv14supervisorTrapEv
.type _ZN5Riscv14supervisorTrapEv, @function
_ZN5Riscv14supervisorTrapEv:

    #push svih registara
    addi sp, sp, -256
    .irp index 0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19,20,21,22,23,24,25,26,27,28,29,30,31
    sd x\index, \index * 8(sp)
    .endr
    addi x8, sp, 0 #frame pointer

    call _ZN5Riscv20handleSupervisorTrapEv

    sd x10, 10 * 8(x8) #da se ne pregazi a0

    #da bi presao na narednu instrukciju
    #csrr x5, sepc
    #addi x5, x5, 4
    #csrw sepc, x5

    #pop svih registara
    .irp index 0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19,20,21,22,23,24,25,26,27,28,29,30,31
    ld x\index, \index * 8(sp)
    .endr
    addi sp, sp, 256

    sret

#_ZN5Riscv14supervisorTrapEv umesto supervisorTrap
#_ZN5Riscv20handleSupervisorTrapEv umesto handleSupervisorTrap