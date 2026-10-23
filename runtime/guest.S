    .set noreorder
    .set noat
    .text

    .globl psbridge_start
    .ent psbridge_start
psbridge_start:
    addiu $sp, $sp, -32
    sw $ra, 28($sp)
    sw $gp, 24($sp)

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
    lw $ra, 28($sp)
    addiu $sp, $sp, 32
    jr $ra
    nop
    .end psbridge_start

    .globl psbridge_import
    .ent psbridge_import
psbridge_import:
    addiu $sp, $sp, -32
    sw $ra, 28($sp)
    sw $gp, 24($sp)
    or $4, $25, $0

    lui $gp, %hi(_gp)
    addiu $gp, $gp, %lo(_gp)
    jal psbridge_dispatch
    nop

    lw $gp, 24($sp)
    lw $ra, 28($sp)
    addiu $sp, $sp, 32
    jr $ra
    nop
    .end psbridge_import
