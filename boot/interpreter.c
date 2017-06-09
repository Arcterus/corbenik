#include <stdint.h>
#include <stddef.h>

#ifndef LOADER
  #include <stddef.h>        // for size_t, NULL
  #include <stdint.h>        // for uint8_t, uint32_t, uint16_t, uint64_t
  #include <stdlib.h>        // for malloc, free
  #include <string.h>        // for strlen, memcmp, memcpy
  #include <firm/headers.h>  // for firm_section_h, firm_h
  #include <input.h>         // for wait
  #include <malloc.h>        // for memalign
  #include <option.h>        // for get_opt_u32, OPTION_OVERLY_VERBOSE
  #include <std/abort.h>     // for panic
  #include <std/draw.h>      // for stderr
  #include <std/fs.h>        // for crclose, cropen, crsize, crread, crwrite
  #include <std/memory.h>    // for memfind
  #include <structures.h>    // for system_patch, PATH_LOADER_CACHE
#else
  #ifndef PATH_MAX
    #define PATH_MAX 255
    #define _MAX_LFN 255
  #endif

  #include <3ds.h>
  #include <string.h>
  #include <stdio.h>
  #include <stdlib.h>

  #include "patcher.h"
  #include "memory.h"
  #include "logger.h"

  #include <option.h>
  #include <structures.h>
  #include <string.h>
#endif

#define OP_NOP 0x00
#define OP_REL 0x01
#define OP_FIND 0x02
#define OP_BACK 0x03
#define OP_FWD 0x04
#define OP_SET 0x05
#define OP_TEST 0x06
#define OP_JMP 0x07
#define OP_REWIND 0x08
#define OP_AND 0x09
#define OP_OR 0x0A
#define OP_XOR 0x0B
#define OP_NOT 0x0C
#define OP_VER 0x0D
#define OP_CLF 0x0E
#define OP_SEEK 0x0F
#define OP_N3DS 0x10

#define OP_ABORT   0x18
#define OP_ABORTEQ 0x28
#define OP_ABORTNE 0x38
#define OP_ABORTLT 0x48
#define OP_ABORTGT 0x58
#define OP_ABORTF  0x68
#define OP_ABORTNF 0x78

#define OP_JMPEQ 0x17
#define OP_JMPNE 0x27
#define OP_JMPLT 0x37
#define OP_JMPGT 0x47
#define OP_JMPLE 0x57
#define OP_JMPGE 0x67
#define OP_JMPF  0x77
#define OP_JMPNF 0x87

// TODO - Add to python assembler
#define OP_INJECT 0x90

#define OP_STR    0x91

#define OP_NEXT 0xFF

#ifdef LOADER
  #define log(a) logstr(a)
  #define panic(a)                                                                                                                                               \
      {                                                                                                                                                          \
          logstr(a);                                                                                                                                             \
          svcBreak(USERBREAK_ASSERT);                                                                                                                            \
      }
#else
  #define log(a) fprintf(stderr, a)
#endif

struct mode
{
    uint8_t *memory;
    uint32_t size;
};

struct mode modes[21];
int init_bytecode = 0;

#ifndef LOADER
extern int is_n3ds;
static const char hexDigits[] = "0123456789ABCDEF";
#endif

#define DISPATCH() goto *dispatch_table[*code]

int
exec_bytecode(uint8_t *bytecode, uint32_t len, uint16_t ver, int debug)
{
    uint32_t set_mode = 0;

    struct mode *current_mode = &modes[set_mode];

    uint32_t offset = 0, new_offset = 0;

    uint32_t i;

    int eq = 0, gt = 0, lt = 0, found = 0; // Flags.

    static void *dispatch_table[256] = { &&op_err };
    dispatch_table[OP_NOP]     = &&op_nop;
    dispatch_table[OP_REL]     = &&op_rel;
    dispatch_table[OP_FIND]    = &&op_find;
    dispatch_table[OP_BACK]    = &&op_back;
    dispatch_table[OP_FWD]     = &&op_fwd;
    dispatch_table[OP_SET]     = &&op_set;
    dispatch_table[OP_TEST]    = &&op_test;
    dispatch_table[OP_JMP]     = &&op_jmp;
    dispatch_table[OP_REWIND]  = &&op_rewind;
    dispatch_table[OP_AND]     = &&op_and;
    dispatch_table[OP_OR]      = &&op_or;
    dispatch_table[OP_XOR]     = &&op_xor;
    dispatch_table[OP_NOT]     = &&op_not;
    dispatch_table[OP_VER]     = &&op_ver;
    dispatch_table[OP_CLF]     = &&op_clf;
    dispatch_table[OP_SEEK]    = &&op_seek;
    dispatch_table[OP_ABORT]   = &&op_abort;
    dispatch_table[OP_ABORTEQ] = &&op_aborteq;
    dispatch_table[OP_ABORTNE] = &&op_abortne;
    dispatch_table[OP_ABORTLT] = &&op_abortlt;
    dispatch_table[OP_ABORTGT] = &&op_abortgt;
    dispatch_table[OP_ABORTF]  = &&op_abortf;
    dispatch_table[OP_ABORTNF] = &&op_abortnf;
    dispatch_table[OP_JMPEQ]   = &&op_jmpeq;
    dispatch_table[OP_JMPNE]   = &&op_jmpne;
    dispatch_table[OP_JMPLT]   = &&op_jmplt;
    dispatch_table[OP_JMPGT]   = &&op_jmpgt;
    dispatch_table[OP_JMPLE]   = &&op_jmple;
    dispatch_table[OP_JMPF]    = &&op_jmpf;
    dispatch_table[OP_JMPNF]   = &&op_jmpnf;
    dispatch_table[OP_INJECT]  = &&op_inject;
    dispatch_table[OP_STR]     = &&op_str;
    dispatch_table[OP_NEXT]    = &&op_next;

    uint8_t *code = bytecode;
    uint8_t *end = code + len;
    while (code < end && code >= bytecode) {
        DISPATCH();
        op_nop:
            if (debug) {
                log("nop\n");
            }
            code++;
            goto vm_sanity_check;
        op_rel: // Change relativity.
            if (debug) {
#ifdef LOADER
                log("rel\n");
#else
                fprintf(stderr, "rel %hhu\n", code[1]);
#endif
            }
            code++;
            current_mode = &modes[*code];
            set_mode = *code;
            code++;
            goto vm_sanity_check;
        op_find: // Find pattern.
            if (debug) {
#ifdef LOADER
                log("find\n");
#else
                fprintf(stderr, "find %hhu ...\n", code[1]);
#endif
            }
            found = 0;
            new_offset = (size_t)memfind(current_mode->memory + offset, current_mode->size - offset, &code[2], code[1]);
            if ((uint8_t *)new_offset != NULL) {
                // Pattern found, set found state flag
                found = 1;
                offset = new_offset - (size_t)current_mode->memory;
            }
            code += code[1] + 2;
            goto vm_sanity_check;
        op_back:
            if (debug) {
#ifdef LOADER
                log("back\n");
#else
                fprintf(stderr, "back %hhu\n", code[1]);
#endif
            }
            offset -= code[1];
            code += 2;
            goto vm_sanity_check;
        op_fwd:
            if (debug) {
#ifdef LOADER
                log("fwd\n");
#else
                fprintf(stderr, "fwd %u\n", code[1]);
#endif
            }
            offset += code[1];
            code += 2;
            goto vm_sanity_check;
        op_set: // Set data.
            if (debug) {
#ifdef LOADER
                log("set\n");
#else
                fprintf(stderr, "set %u, ...\n", code[1]);
#endif
            }
            memcpy(current_mode->memory + offset, &code[2], code[1]);
            offset += code[1];
            code += code[1] + 2;
            goto vm_sanity_check;
        op_test: // Test data.
            if (debug) {
#ifdef LOADER
                log("test\n");
#else
                fprintf(stderr, "test %u, ...\n", code[1]);
#endif
            }
            eq = memcmp(current_mode->memory + offset, &code[2], code[1]);
            if (eq < 0)
                lt = 1;
            if (eq > 0)
                gt = 1;
            eq = !eq;
            code += code[1] + 2;
            goto vm_sanity_check;
        op_jmp: // Jump to offset.
            code++;
            code = bytecode + (code[0] + (code[1] << 8));
            if (debug) {
#ifdef LOADER
                log("jmp\n");
#else
                fprintf(stderr, "jmp %u\n", code - bytecode);
#endif
            }
            goto vm_sanity_check;
        op_jmpeq: // Jump to offset if equal
            code++;
            if (eq)
                code = bytecode + (code[0] + (code[1] << 8));
            else
                code += 2;
            if (debug) {
#ifdef LOADER
                log("jmpeq\n");
#else
                fprintf(stderr, "jmpeq %u\n", code - bytecode);
#endif
            }
            goto vm_sanity_check;
        op_jmpne: // Jump to offset if not equal
            code++;
            if (!eq)
                code = bytecode + (code[0] + (code[1] << 8));
            else
                code += 2;
            if (debug) {
#ifdef LOADER
                log("jmpne\n");
#else
                fprintf(stderr, "jmpeq %u\n", code - bytecode);
#endif
            }
            goto vm_sanity_check;
        op_jmplt: // Jump to offset if less than
            code++;
            if (lt)
                code = bytecode + (code[0] + (code[1] << 8));
            else
                code += 2;
            if (debug) {
#ifdef LOADER
                log("jmplt\n");
#else
                fprintf(stderr, "jmplt %u\n", code - bytecode);
#endif
            }
            goto vm_sanity_check;
        op_jmpgt: // Jump to offset if greater than
            code++;
            if (gt)
                code = bytecode + (code[0] + (code[1] << 8));
            else
                code += 2;
            if (debug) {
#ifdef LOADER
                log("jmplt\n");
#else
                fprintf(stderr, "jmplt %u\n", code - bytecode);
#endif
            }
            goto vm_sanity_check;
        op_jmple: // Jump to offset if less than or equal
            code++;
            if (lt || eq)
                code = bytecode + (code[0] + (code[1] << 8));
            else
                code += 2;
            if (debug) {
#ifdef LOADER
                log("jmplt\n");
#else
                fprintf(stderr, "jmplt %u\n", code - bytecode);
#endif
            }
            goto vm_sanity_check;
        op_jmpf: // Jump to offset if pattern found
            code++;
            if (found)
                code = bytecode + (code[0] + (code[1] << 8));
            else
                code += 2;
            if (debug) {
#ifdef LOADER
                log("jmplt\n");
#else
                fprintf(stderr, "jmplt %u\n", code - bytecode);
#endif
            }
            goto vm_sanity_check;
        op_jmpnf: // Jump to offset if pattern NOT found
            code++;
            if (!found)
                code = bytecode + (code[0] + (code[1] << 8));
            else
                code += 2;
            if (debug) {
#ifdef LOADER
                log("jmplt\n");
#else
                fprintf(stderr, "jmplt %u\n", code - bytecode);
#endif
            }
            goto vm_sanity_check;
        op_clf: // Clear flags.
            if (debug) {
                log("clf\n");
            }
            code++;
            found = gt = lt = eq = 0;
            goto vm_sanity_check;
        op_rewind:
            if (debug)
                log("rewind\n");
            code++;
            offset = 0;
            goto vm_sanity_check;
        op_and:
            if (debug) {
                log("and\n");
            }
            for (i = 0; i < code[1]; i++) {
                current_mode->memory[offset] &= code[i+2];
            }
            offset += code[1];
            code += code[1] + 2;
            goto vm_sanity_check;
        op_or:
            if (debug) {
                log("or\n");
            }
            for (i = 0; i < code[1]; i++) {
                current_mode->memory[offset] |= code[i+2];
            }
            offset += code[1];
            code += code[1] + 2;
            goto vm_sanity_check;
        op_xor:
            if (debug) {
                log("xor\n");
            }
            for (i = 0; i < code[1]; i++) {
                current_mode->memory[offset] ^= code[i+2];
            }
            offset += code[1];
            code += code[1] + 2;
            goto vm_sanity_check;
        op_not:
            if (debug) {
                log("not\n");
            }
            for (i = 0; i < code[1]; i++) {
                current_mode->memory[offset] = ~current_mode->memory[offset];
            }
            offset += code[1];
            code += 2;
            goto vm_sanity_check;
        op_ver:
            if (debug) {
                log("ver\n");
            }
            code++;
            eq = memcmp(&ver, code, 2);
            if (eq < 0)
                lt = 1;
            if (eq > 0)
                gt = 1;
            eq = !eq;
            code += 2;
            goto vm_sanity_check;
        op_seek: // Jump to offset if greater than or equal
            code++;
            offset = (uint32_t)(code[0] + (code[1] << 8) + (code[2] << 16) + (code[3] << 24));
            if (debug) {
#ifdef LOADER
                log("seek\n");
#else
                fprintf(stderr, "seek %lx\n", offset);
#endif
            }
            code += 4;
            goto vm_sanity_check;
        op_abort:
            code++;
            if (debug)
                log("abort\n");

            panic("abort triggered, halting VM!\n");
            goto vm_sanity_check;
        op_aborteq:
            code++;
            if (debug)
                log("aborteq\n");
            if (eq)
                panic("eq flag set, halting VM!\n");
            goto vm_sanity_check;
        op_abortne:
            code++;
            if (debug)
                log("abortlt\n");
            if (!eq)
                panic("eq flag not set, halting VM!\n");
            goto vm_sanity_check;
        op_abortlt:
            code++;
            if (debug)
                log("abortlt\n");
            if (lt)
                panic("lt flag set, halting VM!\n");
            goto vm_sanity_check;
        op_abortgt:
            code++;
            if (debug)
                log("abortgt\n");
            if (gt)
                panic("gt flag set, halting VM!\n");
            goto vm_sanity_check;
        op_abortf:
            code++;
            if (debug)
                log("abortf\n");
            if (found)
                panic("f flag set, halting VM!\n");
            goto vm_sanity_check;
        op_abortnf:
            code++;
            if (debug)
                log("abortnf\n");
            if (!found)
                panic("f flag is not set, halting VM!\n");
            goto vm_sanity_check;
        op_next:
            if (debug) {
                log("next\n");
            }
            found = gt = lt = eq = 0;

            bytecode = code + 1;
#ifndef LOADER
            set_mode = 0;
            current_mode = &modes[set_mode];
#else
            set_mode = 18;
            current_mode = &modes[set_mode];
#endif
            offset = new_offset = 0;
            code = bytecode;
            goto vm_sanity_check;
        op_inject: // Read in data (from filename)
            if (debug) {
#ifdef LOADER
                log("set\n");
#else
                fprintf(stderr, "set %s\n", &code[1]);
#endif
            }

            char* fname = (char*)&code[1];
#ifdef LOADER
            (void)fname;
            // NYI
#else
            FILE* f = cropen(fname, "r");
            crread(current_mode->memory + offset, 1, crsize(f), f);
            offset += crsize(f);
            code += strlen(fname);
            crclose(f);
#endif
            goto vm_sanity_check;
        op_str:
            ++code;
#ifdef LOADER
            log((char*)code);
            log("\n");
#else
            fprintf(stderr, "%s\n", code);
#endif
            code += strlen((char*)code) + 1;
            goto vm_sanity_check;
        op_err:
#ifndef LOADER
            // Panic; not proper opcode.
            fprintf(stderr, "Invalid opcode. State:\n"
                            "  Relative:  %lu\n"
                            "    Actual:  %lx:%lu\n"
                            "  Memory:    %lx\n"
                            "    Actual:  %lx\n"
                            "  Code Loc:  %lx\n"
                            "    Actual:  %lx\n"
                            "  Opcode:    %hhu\n",
                    (uint32_t)set_mode,
                    (uint32_t)current_mode->memory,
                    (uint32_t)current_mode->size,
                    (uint32_t)offset,
                    (uint32_t)(current_mode->memory + offset),
                    (uint32_t)(code - bytecode),
                    (uint32_t)code,
                    *code);
#endif
            panic("Halting startup.\n");

    vm_sanity_check:
        if (offset > current_mode->size) { // Went out of bounds. Error.
#ifndef LOADER
            fprintf(stderr, " -> %lx", offset);
#endif
            panic("seeked out of bounds\n");
        }

#ifndef LOADER
        if (debug) {
            fprintf(stderr, "l:%d g:%d e:%d f:%d m:%lu o:0x%lx\nc:0x%lx m:0x%lx n:%lx\n",
                lt, gt, eq, found,
                set_mode,
                (uint32_t)offset, (uint32_t)(code - bytecode), (uint32_t)(current_mode->memory + offset), (uint32_t)code);
            wait();
        }
#endif
    }

    return 0;
}

#ifdef LOADER
int
execb(uint64_t tid, uint16_t ver, EXHEADER_prog_addrs* shared)
{
#else
int
execb(uint64_t tid, firm_h* firm_patch)
{
    uint16_t ver = 0; // FIXME - Provide native_firm version
#endif
    uint32_t patch_len;
#ifdef LOADER
    char cache_path[] = PATH_LOADER_CACHE "/0000000000000000";

    hexdump_titleid(tid, cache_path);

    static uint8_t *patch_mem;

    Handle file;
    u32 total;

    // Open file.
    if (!R_SUCCEEDED(fileOpen(&file, ARCHIVE_SDMC, cache_path, FS_OPEN_READ))) {
        // Failed to open.
        return 0; // No patches.
    }

    log("  patch: ");
    log(cache_path);
    log("\n");

    u64 file_size;

    if (!R_SUCCEEDED(FSFILE_GetSize(file, &file_size))) {
        FSFILE_Close(file); // Read to memory.

        return 1;
    }

    patch_mem = malloc(file_size);
    if (patch_mem == NULL) {
        log("  out of memory on loading patch\n");

        FSFILE_Close(file); // Read to memory.

        return 1;
    }

    // Read file.
    if (!R_SUCCEEDED(FSFILE_Read(file, &total, 0, patch_mem, file_size))) {
        FSFILE_Close(file); // Read to memory.

        // Failed to read.
        return 1;
    }

    FSFILE_Close(file); // Done reading in.

    // Set memory.
    modes[0].memory = (uint8_t*)shared->text_addr;
    modes[0].size   = shared->total_size << 12;

    // Set memory.
    modes[1].memory = (uint8_t*)shared->text_addr;
    modes[1].size   = shared->text_size << 12;

    // Set memory.
    modes[2].memory = (uint8_t*)shared->data_addr;
    modes[2].size   = shared->data_size << 12;

    // Set memory.
    modes[3].memory = (uint8_t*)shared->ro_addr;
    modes[3].size   = shared->ro_size << 12;

    log("  exec\n");

    patch_len = file_size;
#else
    // The WHOLE firm.
    modes[0].memory = (uint8_t *)firm_patch;
    modes[0].size   = firm_patch->section[0].size + firm_patch->section[1].size + sizeof(firm_h) +
                      firm_patch->section[2].size + firm_patch->section[3].size;

    // FIRM Sect 0
    modes[1].memory = (uint8_t *)firm_patch + firm_patch->section[0].offset;
    modes[1].size   = firm_patch->section[0].size;

    // FIRM Sect 1
    modes[2].memory = (uint8_t *)firm_patch + firm_patch->section[1].offset;
    modes[2].size   = firm_patch->section[1].size;

    // FIRM Sect 2
    modes[3].memory = (uint8_t *)firm_patch + firm_patch->section[2].offset;
    modes[3].size   = firm_patch->section[2].size;

    // FIRM Sect 3
    modes[4].memory = (uint8_t *)firm_patch + firm_patch->section[3].offset;
    modes[4].size   = firm_patch->section[3].size;

    char cache_path[] = PATH_LOADER_CACHE "/0000000000000000";

    uint64_t title = tid;

    uint32_t tlen = strlen(cache_path) - 1;
    int j = 16;
    while (j--) {
        cache_path[tlen--] = hexDigits[title & 0xF];
        title >>= 4;
    }

    // Read patch to scrap memory.

    FILE *f = cropen(cache_path, "r");
    if (!f) {
        // File wasn't found. The user didn't enable anything.
        return 0;
    }

    patch_len = crsize(f);

    uint8_t* patch_mem = memalign(16, patch_len);

    crread(patch_mem, 1, patch_len, f);
    crclose(f);
#endif

    int debug = 0;
#ifdef LOADER
    if (config.options[OPTION_OVERLY_VERBOSE]) {
#else
    if (get_opt_u32(OPTION_OVERLY_VERBOSE)) {
#endif
        debug = 1;
    }

    int r = exec_bytecode(patch_mem, patch_len, ver, debug);

    free(patch_mem);

    return r;
}

#ifndef LOADER

int cache_patch(const char *filename) {
    uint16_t ver = 0; // FIXME - Provide native_firm version
    uint32_t patch_len;

    struct system_patch *patch;
    uint8_t *patch_mem;
    // Read patch to scrap memory.

    FILE *f = cropen(filename, "r");
    if (!f) {
        // File wasn't found. The user didn't enable anything.
        return 0;
    }

    uint32_t len = crsize(f);

    uint8_t* patch_loc = malloc(len);

    crread(patch_loc, 1, len, f);
    crclose(f);

    patch = (struct system_patch*)patch_loc;

    // Make sure various bits are correct.
    if (memcmp(patch->magic, "AIDA", 4)) {
        // Incorrect magic.
        return 1;
    }

    fprintf(stderr, "Cache: %s\n", patch->name);

    patch_mem = (uint8_t *)patch + sizeof(struct system_patch) + (patch->depends * 8) + (patch->titles * 8);
    patch_len = patch->size;

    if (patch->titles != 0) {
        // Not an error, per se, but it means this patch is meant for loader, not us.
        // Patches intended for use during boot will always be applied to zero titles.
        // We should generate a cache for loader in a file intended for titleid.
        uint8_t *title_buf = (uint8_t *)patch + sizeof(struct system_patch);

        fprintf(stderr, "  Version: %u\n", patch->version);

        for (uint32_t i = 0; i < patch->titles; i++) {
            char cache_path[] = PATH_LOADER_CACHE "/0000000000000000";

            uint64_t title = 0;
            memcpy(&title, &title_buf[i * 8], 8);

            uint32_t tlen = strlen(cache_path) - 1;
            int j = 16;
            while (j--) {
                cache_path[tlen--] = hexDigits[title & 0xF];
                title >>= 4;
            }

            fprintf(stderr, "  cache: %s\n", &cache_path[strlen(cache_path) - 16]);

            size_t len_nam = 4 + strlen(patch->name);
            char *write = malloc(len_nam);
            write[0] = OP_NEXT;
            write[1] = OP_STR;
            memcpy(write+2, patch->name, len_nam - 4);
            write[len_nam - 2] = 0;
            write[len_nam - 1] = OP_NEXT;

            FILE *cache = cropen(cache_path, "w");
            crseek(cache, 0, SEEK_END);
            crwrite(write, 1, len_nam, cache);
            crwrite(patch_mem, 1, patch_len, cache);
            crclose(cache);
            // Add to cache.
        }
    }

    return 0;
}

#endif
