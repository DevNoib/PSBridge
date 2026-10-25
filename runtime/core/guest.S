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
    addiu $sp, $sp, -64
    sd $ra, 56($sp)
    sd $26, 48($sp)
    sd $27, 40($sp)
    sd $gp, 32($sp)

    lw $9, 80($sp)
    lw $10, 84($sp)
    or $8, $7, $0
    or $7, $6, $0
    or $6, $5, $0
    or $5, $4, $0
    or $4, $25, $0

    lui $gp, %hi(_gp)
    addiu $gp, $gp, %lo(_gp)
    jal psbridge_dispatch
    nop

    dsrl32 $3, $2, 0
    dsll32 $2, $2, 0
    dsrl32 $2, $2, 0

    ld $gp, 32($sp)
    ld $27, 40($sp)
    ld $26, 48($sp)
    ld $ra, 56($sp)
    addiu $sp, $sp, 64
    jr $ra
    nop

    .end psbridge_import

    .globl psbridge_resolve_guest
    .ent psbridge_resolve_guest
psbridge_resolve_guest:
    addiu $sp, $sp, -32
    sw $1, 0($sp)
    sw $2, 4($sp)
    sw $3, 8($sp)
    sw $4, 12($sp)
    sw $5, 16($sp)
    sw $6, 20($sp)

    lui $5, %hi(guest_info)
    addiu $5, $5, %lo(guest_info)
    lw $4, 0($5)
    sltu $1, $25, $4
    bne $1, $0, guest_resolve_native
    nop

    lw $6, 8($5)
    addu $6, $4, $6
    sltu $1, $25, $6
    beq $1, $0, guest_resolve_native
    nop

    subu $25, $25, $4
    lw $6, 4($5)
    addu $25, $25, $6

guest_resolve_native:
    lui $1, 0x0200
    sltu $2, $25, $1
    bne $2, $0, guest_resolve_done
    nop

    or $2, $0, $0
    lui $3, %hi(guest_map_count)
    lw $3, %lo(guest_map_count)($3)
    lui $5, %hi(guest_map)
    addiu $5, $5, %lo(guest_map)

guest_resolve_loop:
    sltu $1, $2, $3
    beq $1, $0, guest_resolve_done
    nop

    addu $4, $2, $3
    srl $4, $4, 1
    sll $6, $4, 3
    addu $6, $5, $6
    lw $1, 0($6)
    sltu $1, $25, $1
    bne $1, $0, guest_resolve_lower
    nop

    lw $1, 0($6)
    sltu $1, $1, $25
    bne $1, $0, guest_resolve_higher
    nop

    lw $25, 4($6)
    b guest_resolve_done
    nop

guest_resolve_lower:
    or $3, $4, $0
    b guest_resolve_loop
    nop

guest_resolve_higher:
    addiu $2, $4, 1
    b guest_resolve_loop
    nop

guest_resolve_done:
    lw $1, 0($sp)
    lw $2, 4($sp)
    lw $3, 8($sp)
    lw $4, 12($sp)
    lw $5, 16($sp)
    lw $6, 20($sp)
    addiu $sp, $sp, 32
    jr $ra
    nop
    .end psbridge_resolve_guest

    .globl psbridge_call_guest
    .ent psbridge_call_guest
psbridge_call_guest:
    addiu $sp, $sp, -32
    sw $ra, 28($sp)
    sw $gp, 24($sp)
    sw $26, 20($sp)

    or $12, $4, $0
    or $4, $5, $0

    lui $8, %hi(guest_info)
    addiu $8, $8, %lo(guest_info)
    lw $9, 0($8)
    lw $10, 4($8)
    lw $gp, 16($8)

    beq $gp, $0, guest_call_gp_ready
    nop

    subu $gp, $gp, $10
    addu $gp, $gp, $9

guest_call_gp_ready:
    or $25, $12, $0
    jal psbridge_resolve_guest
    nop
    or $12, $25, $0

    jalr $ra, $12
    nop

    lw $gp, 24($sp)
    lw $26, 20($sp)
    lw $ra, 28($sp)
    addiu $sp, $sp, 32
    jr $ra
    nop
    .end psbridge_call_guest

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
    or $25, $12, $0
    jal psbridge_resolve_guest
    nop
    or $12, $25, $0

    jalr $ra, $12
    nop

    lw $gp, 24($sp)
    lw $26, 20($sp)
    lw $ra, 28($sp)
    addiu $sp, $sp, 32
    jr $ra
    nop
    .end psbridge_call_thread
