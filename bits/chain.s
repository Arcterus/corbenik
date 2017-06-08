// This is a tiny chainloader. It expects r0 to be the
// code to copy, r1 to be the code's size, and r2 to indicate
// whether or not the payload is a FIRM.

// As long as GCC follows standard calling conventions, you
// can call it from C once in memory like:

//   void copy(uint8_t* data, uint32_t size, uint32_t isFirm)

// This means NO need to use fatfs with the chainloader since the
// caller itself handles the disk read.

// The code below is also all PC-relative, meaning you can actually
// run the chainloader from anywhere (as long as it is aligned to
// instruction boundaries/the chainloader isn't overwritten/the
// code isn't copied wrong over itself)

.syntax unified
.section .text
copy:
    // Check if FIRM
    cmp r2, #1
    bne bin_copy

    // Save ARM11 and ARM9 entrypoints
    mov r3, r0           // Save payload start
    ldr r4, [r0, #8]     // ARM11 Entry
    ldr r5, [r0, #12]    // ARM9 Entry

    add r0, r0, #0x40

    // Need to copy FIRM sections into place
    mov r2, #4
    section_copy:
        ldr r6, [r0]
        add r7, r3, r6      // Get start of section (firm start + offset)
        ldr r8, [r0, #4]    // Get destination address
        ldr r1, [r0, #8]    // Get size of section

        memcpy:
            cmp r1, #0
            beq memcpy_end

            ldrb r6, [r7], #1
            strb r6, [r8], #1

            sub r1, r1, #1
            b memcpy
        memcpy_end:

        add r0, r0, #0x30

        sub r2, r2, #1
        cmp r2, #0
        bne section_copy

    mov r2, #1    // Restore FIRM check

    b boot

bin_copy:
    // Must be BIN
    ldr r3, value
    add r1, r0, r1

    inner:
        cmp r0, r1
        ldrbne r2, [r0], #1
        strbne r2, [r3, #1]!
        bne inner

boot:
    // Flush caches

    mov r6, r2       // Save FIRM check data

    // ICache
    mov r0, #0
    mcr p15, 0, r0, c7, c5, 0

    // DCache
    mov r2, #0
    mov r1, r2
    flush_dcache:
        mov r0, #0
        mov r3, r2, lsl #30
        flush_cache_inner_loop:
            orr r12, r3, r0, lsl#5
            mcr p15, 0, r12, c7, c14, 2  // clean and flush dcache entry (index and segment)
            add r0, #1
            cmp r0, #0x20
            bcc flush_cache_inner_loop
        add r2, #1
        cmp r2, #4
        bcc flush_dcache

    mov r0, #0
    mcr p15, 0, r0, c7, c10, 4 // drain write buffer before jump

    // Reload argc and argv.
    ldr r0, argc
    ldr r1, argv

    // Check if FIRM (again...)
    cmp r6, #1
    beq firmboot

    // Actually boot (BIN) payload.
    ldr r3, offset
    bx r3

firmboot:
    // Move ARM11 entrypoint saved earlier into place
    mov r2, #0x20000000
    str r4, [r2, #-4]

    // Boot using ARM9 entrypoint saved earlier
    bx r5

.align 4

value:  .int 0x23efffff
offset: .int 0x23f00000
argc:   .ascii "ARGC"
argv:   .ascii "ARGV"
