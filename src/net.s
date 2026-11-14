# Plain HTTP for std.http (spec 007 US5, research R5): __http_exchange(host, port, request).
    .include "defs.inc"

    .section .rodata
m_no_net: .string "network access is not available"

    .text
# void *io_http_exchange(void *host, long port, void *request)
# result[str]: the raw response, or err with the reason.
    .globl io_http_exchange
io_http_exchange:
    addi sp, sp, -16
    sd ra, 8(sp)
    la a0, m_no_net
    call str_from_cstr
    mv a1, a0
    li a0, 1
    call make_result
    ld ra, 8(sp)
    addi sp, sp, 16
    ret
