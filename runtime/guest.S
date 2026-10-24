    .set noreorder
    .set noat
    .text

    .globl psbridge_start
    .ent psbridge_start
psbridge_start:
    addiu $sp, $sp, -32
    sw $ra, 28($sp)
    sw $gp, 24($sp)
    sw $26, 20($sp)

    lui $26, %hi(main_kernel_work)
    addiu $26, $26, %lo(main_kernel_work)

    lui $8, %hi(guest_info)
    addiu $8, $8, %lo(guest_info)
    lw $9, 0($8)
    lw $10, 4($8)
    lw $gp, 16($8)
    beq $gp, $0, guest_gp_ready
    nop
    subu $gp, $gp, $10
    addu $gp, $gp, $9

guest_gp_ready:
    or $4, $0, $0
    or $5, $0, $0
    jal guest_entry
    nop

    lw $gp, 24($sp)
    lw $26, 20($sp)
    lw $ra, 28($sp)
    addiu $sp, $sp, 32
    jr $ra
    nop
    .end psbridge_start

    .globl psbridge_import
    .ent psbridge_import


psbridge_import:
    addiu $sp, $sp, -48
    sd $ra, 40($sp)
    sd $26, 32($sp)
    sd $27, 24($sp)
    sd $gp, 16($sp)

    or $10, $9, $0
    or $9, $8, $0
    or $8, $7, $0
    or $7, $6, $0
    or $6, $5, $0
    or $5, $4, $0
    or $4, $25, $0

    lui $gp, %hi (_gp)
    addiu $gp, $gp, %lo(_gp)
    jal psbridge_dispatch
    nop

    dsrl32 $3, $2, 0
    dsll $3, $2, 0
    dsrl32 $3, $2, 0

    ld $gp, 16($sp)
    ld $27, 24($sp)
    ld $26, 32($sp)
    ld $ra, 40($sp)
    addiu $sp, $sp, 48
    jr $ra
    nop

    .end psbridge_import
    .globl psbridge_call_thread
    .ent psbridge_call_thread
psbridge_call_thread:
    addiu $sp, $sp, -32
    sw $ra, 28($sp)
    sw $gp, 24($sp)
    sw $26, 20($sp)

    or $12, $4, $0
    or $26, $7, $0
    or $4, $5, $0
    or $5, $6, $0

    lui $8, %hi(guest_info)
    addiu $8, $8, %lo(guest_info)
    lw $9, 0($8)
    lw $10, 4($8)
    lw $gp, 16($8)

    beq $gp, $0, thread_gp_ready
    nop
    
    subu $gp, $gp, $10
    addu $gp, $gp, $9

thread_gp_ready:
    jalr $ra, $12
    nop

    lw $gp, 24($sp)
    lw $26, 20($sp)
    lw $ra, 28($sp)
    addiu $sp, $sp, 32
    jr $ra
    nop
    .end psbridge_call_thread

