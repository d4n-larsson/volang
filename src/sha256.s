# SHA-256 (FIPS 180-4) for package content hashes (spec 007 research R8). One hash at a time:
# sha256_init, sha256_update (any number of times), sha256_final; or sha256 for one buffer.
    .include "defs.inc"

    .section .rodata
    .align 3
sha_k:
    .word 0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5, 0x3956c25b, 0x59f111f1, 0x923f82a4, 0xab1c5ed5
    .word 0xd807aa98, 0x12835b01, 0x243185be, 0x550c7dc3, 0x72be5d74, 0x80deb1fe, 0x9bdc06a7, 0xc19bf174
    .word 0xe49b69c1, 0xefbe4786, 0x0fc19dc6, 0x240ca1cc, 0x2de92c6f, 0x4a7484aa, 0x5cb0a9dc, 0x76f988da
    .word 0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7, 0xc6e00bf3, 0xd5a79147, 0x06ca6351, 0x14292967
    .word 0x27b70a85, 0x2e1b2138, 0x4d2c6dfc, 0x53380d13, 0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85
    .word 0xa2bfe8a1, 0xa81a664b, 0xc24b8b70, 0xc76c51a3, 0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070
    .word 0x19a4c116, 0x1e376c08, 0x2748774c, 0x34b0bcb5, 0x391c0cb3, 0x4ed8aa4a, 0x5b9cca4f, 0x682e6ff3
    .word 0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208, 0x90befffa, 0xa4506ceb, 0xbef9a3f7, 0xc67178f2
sha_iv:
    .word 0x6a09e667, 0xbb67ae85, 0x3c6ef372, 0xa54ff53a, 0x510e527f, 0x9b05688c, 0x1f83d9ab, 0x5be0cd19

    .bss
    .align 3
sha_h:        .space 32
sha_buf:      .space 64
sha_buffered: .space 8
sha_total:    .space 8
sha_w:        .space 256

    .text
# void sha256_init(void)
    .globl sha256_init
sha256_init:
    la t0, sha_iv
    la t1, sha_h
    li t2, 8
1:
    lw t3, 0(t0)
    sw t3, 0(t1)
    addi t0, t0, 4
    addi t1, t1, 4
    addi t2, t2, -1
    bnez t2, 1b
    la t0, sha_buffered
    sd zero, 0(t0)
    la t0, sha_total
    sd zero, 0(t0)
    ret

# void sha256_block(void)
# Compresses sha_buf into sha_h.
    .globl sha256_block
sha256_block:
    addi sp, sp, -80
    sd s0, 72(sp)
    sd s1, 64(sp)
    sd s2, 56(sp)
    sd s3, 48(sp)
    sd s4, 40(sp)
    sd s5, 32(sp)
    sd s6, 24(sp)
    sd s7, 16(sp)
    sd s8, 8(sp)
    sd s9, 0(sp)
    la t0, sha_buf              # W[0..15]: big-endian words
    la t1, sha_w
    li t2, 16
1:
    lbu t3, 0(t0)
    lbu t4, 1(t0)
    lbu t5, 2(t0)
    lbu t6, 3(t0)
    slli t3, t3, 24
    slli t4, t4, 16
    slli t5, t5, 8
    or t3, t3, t4
    or t3, t3, t5
    or t3, t3, t6
    sw t3, 0(t1)
    addi t0, t0, 4
    addi t1, t1, 4
    addi t2, t2, -1
    bnez t2, 1b
    la s9, sha_w                # W[16..63]
    li s0, 16
2:
    slli t0, s0, 2
    add t0, s9, t0              # &W[t]
    lw a0, -60(t0)              # W[t-15]
    srliw a1, a0, 7
    slliw t1, a0, 25
    or a1, a1, t1
    srliw t2, a0, 18
    slliw t1, a0, 14
    or t2, t2, t1
    xor a1, a1, t2
    srliw t2, a0, 3
    xor a1, a1, t2
    lw a0, -8(t0)               # W[t-2]
    srliw a2, a0, 17
    slliw t1, a0, 15
    or a2, a2, t1
    srliw t2, a0, 19
    slliw t1, a0, 13
    or t2, t2, t1
    xor a2, a2, t2
    srliw t2, a0, 10
    xor a2, a2, t2
    lw a3, -64(t0)              # W[t-16]
    lw a4, -28(t0)              # W[t-7]
    addw a1, a1, a2
    addw a1, a1, a3
    addw a1, a1, a4
    sw a1, 0(t0)
    addi s0, s0, 1
    li t1, 64
    blt s0, t1, 2b
    la t0, sha_h                # a..h = s1..s8
    lw s1, 0(t0)
    lw s2, 4(t0)
    lw s3, 8(t0)
    lw s4, 12(t0)
    lw s5, 16(t0)
    lw s6, 20(t0)
    lw s7, 24(t0)
    lw s8, 28(t0)
    li s0, 0
3:
    srliw a1, s5, 6
    slliw t1, s5, 26
    or a1, a1, t1
    srliw t2, s5, 11
    slliw t1, s5, 21
    or t2, t2, t1
    xor a1, a1, t2
    srliw t2, s5, 25
    slliw t1, s5, 7
    or t2, t2, t1
    xor a1, a1, t2
    and a2, s5, s6              # ch
    not a3, s5
    and a3, a3, s7
    xor a2, a2, a3
    slli t0, s0, 2
    la t3, sha_k
    add t3, t3, t0
    lw t3, 0(t3)
    add t4, s9, t0
    lw t4, 0(t4)
    addw a4, s8, a1             # temp1
    addw a4, a4, a2
    addw a4, a4, t3
    addw a4, a4, t4
    srliw a1, s1, 2
    slliw t1, s1, 30
    or a1, a1, t1
    srliw t2, s1, 13
    slliw t1, s1, 19
    or t2, t2, t1
    xor a1, a1, t2
    srliw t2, s1, 22
    slliw t1, s1, 10
    or t2, t2, t1
    xor a1, a1, t2
    and a2, s1, s2              # maj
    and a3, s1, s3
    xor a2, a2, a3
    and a3, s2, s3
    xor a2, a2, a3
    addw a5, a1, a2             # temp2
    mv s8, s7
    mv s7, s6
    mv s6, s5
    addw s5, s4, a4
    mv s4, s3
    mv s3, s2
    mv s2, s1
    addw s1, a4, a5
    addi s0, s0, 1
    li t1, 64
    blt s0, t1, 3b
    la t0, sha_h
    lw t1, 0(t0)
    addw t1, t1, s1
    sw t1, 0(t0)
    lw t1, 4(t0)
    addw t1, t1, s2
    sw t1, 4(t0)
    lw t1, 8(t0)
    addw t1, t1, s3
    sw t1, 8(t0)
    lw t1, 12(t0)
    addw t1, t1, s4
    sw t1, 12(t0)
    lw t1, 16(t0)
    addw t1, t1, s5
    sw t1, 16(t0)
    lw t1, 20(t0)
    addw t1, t1, s6
    sw t1, 20(t0)
    lw t1, 24(t0)
    addw t1, t1, s7
    sw t1, 24(t0)
    lw t1, 28(t0)
    addw t1, t1, s8
    sw t1, 28(t0)
    ld s0, 72(sp)
    ld s1, 64(sp)
    ld s2, 56(sp)
    ld s3, 48(sp)
    ld s4, 40(sp)
    ld s5, 32(sp)
    ld s6, 24(sp)
    ld s7, 16(sp)
    ld s8, 8(sp)
    ld s9, 0(sp)
    addi sp, sp, 80
    ret

# void sha256_byte(long b)
# Appends one byte (sha256_update and the padding).
    .globl sha256_byte
sha256_byte:
    addi sp, sp, -16
    sd ra, 8(sp)
    la t0, sha_buffered
    ld t1, 0(t0)
    la t2, sha_buf
    add t2, t2, t1
    sb a0, 0(t2)
    addi t1, t1, 1
    sd t1, 0(t0)
    li t2, 64
    bne t1, t2, 9f
    sd zero, 0(t0)
    call sha256_block
9:
    ld ra, 8(sp)
    addi sp, sp, 16
    ret

# void sha256_update(char *p, long n)
    .globl sha256_update
sha256_update:
    addi sp, sp, -32
    sd ra, 24(sp)
    sd s0, 16(sp)
    sd s1, 8(sp)
    mv s0, a0
    mv s1, a1
    la t0, sha_total
    ld t1, 0(t0)
    add t1, t1, s1
    sd t1, 0(t0)
1:
    blez s1, 9f
    lbu a0, 0(s0)
    call sha256_byte
    addi s0, s0, 1
    addi s1, s1, -1
    j 1b
9:
    ld ra, 24(sp)
    ld s0, 16(sp)
    ld s1, 8(sp)
    addi sp, sp, 32
    ret

# void sha256_final(char *out)
# Pads, and writes the 32-byte digest to out.
    .globl sha256_final
sha256_final:
    addi sp, sp, -32
    sd ra, 24(sp)
    sd s0, 16(sp)
    sd s1, 8(sp)
    mv s0, a0
    la t0, sha_total
    ld s1, 0(t0)
    slli s1, s1, 3              # the length in bits
    li a0, 0x80
    call sha256_byte
1:
    la t0, sha_buffered
    ld t0, 0(t0)
    li t1, 56
    beq t0, t1, 2f
    li a0, 0
    call sha256_byte
    j 1b
2:
    li t0, 56                   # the length, big-endian
3:
    srl a0, s1, t0
    andi a0, a0, 255
    sd t0, 0(sp)
    call sha256_byte
    ld t0, 0(sp)
    addi t0, t0, -8
    bgez t0, 3b
    la t0, sha_h                # the digest, big-endian
    li t1, 0
4:
    lw t2, 0(t0)
    srliw t3, t2, 24
    sb t3, 0(s0)
    srliw t3, t2, 16
    sb t3, 1(s0)
    srliw t3, t2, 8
    sb t3, 2(s0)
    sb t2, 3(s0)
    addi t0, t0, 4
    addi s0, s0, 4
    addi t1, t1, 1
    li t2, 8
    blt t1, t2, 4b
    ld ra, 24(sp)
    ld s0, 16(sp)
    ld s1, 8(sp)
    addi sp, sp, 32
    ret

# void sha256(char *p, long n, char *out)
    .globl sha256
sha256:
    addi sp, sp, -32
    sd ra, 24(sp)
    sd s0, 16(sp)
    sd s1, 8(sp)
    sd s2, 0(sp)
    mv s0, a0
    mv s1, a1
    mv s2, a2
    call sha256_init
    mv a0, s0
    mv a1, s1
    call sha256_update
    mv a0, s2
    call sha256_final
    ld ra, 24(sp)
    ld s0, 16(sp)
    ld s1, 8(sp)
    ld s2, 0(sp)
    addi sp, sp, 32
    ret
