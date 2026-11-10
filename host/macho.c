/* macho.c - load an armv7 Mach-O into the arena and resolve dyld binds to HLE stubs / fake data */
#include "ra.h"

Section g_sections[96]; int g_nsections; u32 g_entry;
gptr resolve_symbol(const char *name);              /* symbols.c */

Section *find_section(const char *name, int nth) {
    for (int i = 0; i < g_nsections; i++) if (!strcmp(g_sections[i].name, name) && nth-- == 0) return &g_sections[i];
    return NULL;
}

static u32 be32(const uint8_t *p) { return (u32)p[0] << 24 | p[1] << 16 | p[2] << 8 | p[3]; }
static u32 uleb(const uint8_t **p) { u64 r = 0; int s = 0; uint8_t b; do { b = *(*p)++; if (s < 64) r |= (u64)(b & 0x7f) << s; s += 7; } while (b & 0x80); return (u32)r; }   /* 64-bit decode, truncated: negative deltas wrap mod 2^32 */
static s32 sleb(const uint8_t **p) { s32 r = 0; int s = 0; uint8_t b; do { b = *(*p)++; r |= (s32)(b & 0x7f) << s; s += 7; } while (b & 0x80); if (s < 32 && (b & 0x40)) r |= -(1 << s); return r; }

static u32 seg_addr[16]; static int nseg;

static int nbinds;
static void run_binds(const uint8_t *p, const uint8_t *end, int lazy) {
    const char *sym = ""; u32 addr = 0; s32 addend = 0; (void)lazy;
    while (p < end) {
        uint8_t op = *p++; int imm = op & 0xF; op &= 0xF0;
        switch (op) {
        case 0x00: break;                                  /* DONE (lazy streams contain many) */
        case 0x10: case 0x30: break;                       /* dylib ordinal imm / special */
        case 0x20: uleb(&p); break;
        case 0x40: sym = (const char *)p; while (*p) p++; p++; break;
        case 0x50: break;                                  /* type */
        case 0x60: addend = sleb(&p); break;
        case 0x70: addr = seg_addr[imm] + uleb(&p); break;
        case 0x80: addr += uleb(&p); break;
        case 0x90: case 0xA0: case 0xB0: { nbinds++;
            if (getenv("RA_DEBUG_BINDS") && (addr == 0x462fbc || addr == 0x462fcc)) fprintf(stderr, "[bind] %08x <- %s\n", addr, sym);
            wr32(addr, resolve_symbol(sym) + addend);
            addr += 4;
            if (op == 0xA0) addr += uleb(&p); else if (op == 0xB0) addr += imm * 4;
            break; }
        case 0xC0: { u32 cnt = uleb(&p), skip = uleb(&p); for (u32 i = 0; i < cnt; i++) { wr32(addr, resolve_symbol(sym) + addend); addr += 4 + skip; } break; }
        default: fprintf(stderr, "bad bind opcode %02x\n", op); return;
        }
    }
}

int macho_load(const char *path) {
    FILE *f = fopen(path, "rb"); if (!f) { perror(path); return -1; }
    fseek(f, 0, SEEK_END); long n = ftell(f); fseek(f, 0, SEEK_SET);
    uint8_t *d = malloc(n); if (fread(d, 1, n, f) != (size_t)n) { fclose(f); return -1; } fclose(f);
    u32 base = 0;
    if (be32(d) == 0xCAFEBABE) {                           /* fat: choose armv7 (cpusubtype 9) */
        u32 na = be32(d + 4); base = 0;
        for (u32 i = 0; i < na; i++) { const uint8_t *a = d + 8 + 20 * i; if (be32(a) == 12 && be32(a + 4) == 9) base = be32(a + 8); }
        if (!base) { fprintf(stderr, "no armv7 slice\n"); return -1; }
    }
    const uint8_t *m = d + base;
    u32 magic; memcpy(&magic, m, 4);
    if (magic != 0xFEEDFACE) { fprintf(stderr, "not a 32-bit Mach-O\n"); return -1; }
    u32 ncmds; memcpy(&ncmds, m + 16, 4);
    const uint8_t *lc = m + 28;
    u32 bind_off = 0, bind_sz = 0, lazy_off = 0, lazy_sz = 0;
    for (u32 i = 0; i < ncmds; i++) {
        u32 cmd, sz; memcpy(&cmd, lc, 4); memcpy(&sz, lc + 4, 4);
        if (cmd == 1) {                                    /* LC_SEGMENT */
            char segname[17] = {0}; memcpy(segname, lc + 8, 16);
            u32 vmaddr, vmsize, fileoff, filesize, nsects;
            memcpy(&vmaddr, lc + 24, 4); memcpy(&vmsize, lc + 28, 4); memcpy(&fileoff, lc + 32, 4); memcpy(&filesize, lc + 36, 4); memcpy(&nsects, lc + 48, 4);
            seg_addr[nseg++] = vmaddr;
            if (filesize && vmaddr + vmsize < ARENA_SIZE) memcpy(arena + vmaddr, m + fileoff, filesize);
            const uint8_t *s = lc + 56;
            for (u32 k = 0; k < nsects; k++, s += 68) {
                Section *sc = &g_sections[g_nsections++]; memcpy(sc->name, s, 16); sc->name[16] = 0;
                memcpy(&sc->addr, s + 32, 4); memcpy(&sc->size, s + 36, 4);
            }
        } else if (cmd == 0x80000022 || cmd == 0x22) {     /* LC_DYLD_INFO(_ONLY) */
            memcpy(&bind_off, lc + 16, 4); memcpy(&bind_sz, lc + 20, 4); memcpy(&lazy_off, lc + 32, 4); memcpy(&lazy_sz, lc + 36, 4);
        } else if (cmd == 5) {                             /* LC_UNIXTHREAD: pc is word 15 of the ARM state */
            memcpy(&g_entry, lc + 16 + 15 * 4, 4);
        }
        lc += sz;
    }
    if (getenv("RA_DEBUG_PEEK")) fprintf(stderr, "[macho] bind_off=%u size=%u lazy_off=%u size=%u nseg=%d seg1=%x seg2=%x\n", bind_off, bind_sz, lazy_off, lazy_sz, nseg, seg_addr[1], seg_addr[2]);
    if (bind_sz) run_binds(m + bind_off, m + bind_off + bind_sz, 0);
    if (lazy_sz) run_binds(m + lazy_off, m + lazy_off + lazy_sz, 1);
    if (getenv("RA_DEBUG_BINDS")) fprintf(stderr, "[macho] %d single binds\n", nbinds);
    free(d);
    return 0;
}
