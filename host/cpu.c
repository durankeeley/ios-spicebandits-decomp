/* cpu.c - Unicorn setup, import trampolines, guest-call plumbing, guest heap */
#include "ra.h"
#include <sys/mman.h>
#include <stdarg.h>
#include <unistd.h>

uint8_t *arena; uc_engine *uc; int g_trace;
#define MAXHLE 2048
static struct { char *name; hle_fn fn; } hle[MAXHLE]; static int nhle;
static int nstub;
static gptr fake_cur = FAKE_BASE;
static gptr next_pc_req;

void hle_log(const char *fmt, ...) { if (!g_trace) return; va_list ap; va_start(ap, fmt); vfprintf(stderr, fmt, ap); va_end(ap); }

u32 reg(int r) { static const int ids[] = {UC_ARM_REG_R0,UC_ARM_REG_R1,UC_ARM_REG_R2,UC_ARM_REG_R3,UC_ARM_REG_R4,UC_ARM_REG_R5,UC_ARM_REG_R6,UC_ARM_REG_R7,UC_ARM_REG_R8,UC_ARM_REG_R9,UC_ARM_REG_R10,UC_ARM_REG_R11,UC_ARM_REG_R12,UC_ARM_REG_SP,UC_ARM_REG_LR,UC_ARM_REG_PC}; u32 v = 0; uc_reg_read(uc, ids[r], &v); return v; }
void setreg(int r, u32 v) { static const int ids[] = {UC_ARM_REG_R0,UC_ARM_REG_R1,UC_ARM_REG_R2,UC_ARM_REG_R3,UC_ARM_REG_R4,UC_ARM_REG_R5,UC_ARM_REG_R6,UC_ARM_REG_R7,UC_ARM_REG_R8,UC_ARM_REG_R9,UC_ARM_REG_R10,UC_ARM_REG_R11,UC_ARM_REG_R12,UC_ARM_REG_SP,UC_ARM_REG_LR,UC_ARM_REG_PC}; uc_reg_write(uc, ids[r], &v); }
u32 arg(int n) { return n < 4 ? reg(n) : rd32(reg(13) + 4 * (n - 4)); }
u64 arg64(int n) { return (u64)arg(n) | ((u64)arg(n + 1) << 32); }
void ret(u32 v) { setreg(0, v); }
void ret64(u64 v) { setreg(0, (u32)v); setreg(1, (u32)(v >> 32)); }
void set_next_pc(gptr pc) { next_pc_req = pc; }

gptr alloc_fake(size_t n) { gptr a = fake_cur; fake_cur += (n + 15) & ~15u; if (fake_cur > FAKE_END) { fprintf(stderr, "fake region exhausted\n"); exit(1); } return a; }

/* ---- trampolines:  movw r12,#idx ; svc #0 ; bx r12   (handler writes r12 = continuation pc) ---- */
static gptr make_stub(int idx) {
    gptr a = TRAP_BASE + idx * 12;
    wr32(a, 0xE300C000u | ((idx & 0xF000) << 4) | (idx & 0xFFF));
    wr32(a + 4, 0xEF000000u);
    wr32(a + 8, 0xE12FFF1Cu);
    return a;
}
static int cur_stub_hle[8192];                       /* stub idx -> hle slot (-1 = unimplemented) */
static char *stub_name[8192];

static gptr find_stub(const char *name) {
    for (int i = 0; i < nstub; i++) if (!strcmp(stub_name[i], name)) return TRAP_BASE + i * 12;
    return 0;
}
void hle_register_table(const HleEntry *t) {
    for (; t->name; t++) { hle[nhle].name = (char *)t->name; hle[nhle].fn = t->fn; nhle++; }
}
gptr hle_stub_for(const char *name) {
    gptr s = find_stub(name); if (s) return s;
    int slot = -1;
    for (int i = nhle - 1; i >= 0; i--) if (!strcmp(hle[i].name, name)) { slot = i; break; }   /* later tables override */
    int idx = nstub++;
    stub_name[idx] = strdup(name); cur_stub_hle[idx] = slot;
    return make_stub(idx);
}
gptr g_msgsend_trampoline;

static int prev_in_hook;
#include <time.h>
static double prof_ns[8192]; static u64 prof_n[8192]; static int g_prof;
static inline double now_ns(void) { struct timespec t; clock_gettime(CLOCK_MONOTONIC, &t); return t.tv_sec * 1e9 + t.tv_nsec; }
void prof_report(void) { if (!g_prof) return; double tot = 0; u64 cnt = 0; for (int i = 0; i < nstub; i++) { tot += prof_ns[i]; cnt += prof_n[i]; }
    fprintf(stderr, "[prof] HLE total %.2fs over %llu calls\n", tot / 1e9, (unsigned long long)cnt);
    for (int k = 0; k < 18; k++) { int b = -1; double bv = 0; for (int i = 0; i < nstub; i++) if (prof_ns[i] > bv) { bv = prof_ns[i]; b = i; } if (b < 0) break; fprintf(stderr, "[prof]  %-34s %8.3fs %9llu calls %6.2f us/call\n", stub_name[b], bv / 1e9, (unsigned long long)prof_n[b], bv / 1e3 / (double)prof_n[b]); prof_ns[b] = 0; } }
static void on_intr(uc_engine *u, uint32_t intno, void *ud) {
    (void)u; (void)ud;
    u32 idx = reg(12);
    if (idx == 0xFFFF) { uc_emu_stop(uc); return; }
    if (idx >= (u32)nstub) { fprintf(stderr, "bad trap idx %u (intno %u pc=%08x)\n", idx, intno, reg(15)); uc_emu_stop(uc); return; }
    int slot = cur_stub_hle[idx];
    u32 lr = reg(14);
    next_pc_req = lr;
    if (slot < 0) {
        static int seen[8192];
        if (!seen[idx]) { seen[idx] = 1; fprintf(stderr, "[hle] UNIMPLEMENTED import %s (called from %08x)\n", stub_name[idx], lr); }
        if (getenv("RA_STRICT")) { uc_emu_stop(uc); }
        ret(0);
    } else {
        if (g_trace > 1) fprintf(stderr, "[hle] %s(%08x,%08x,%08x,%08x) lr=%08x\n", stub_name[idx], reg(0), reg(1), reg(2), reg(3), lr);
        double t0 = g_prof ? now_ns() : 0; prev_in_hook = 1; hle[slot].fn(); prev_in_hook = 0; if (g_prof) { prof_ns[idx] += now_ns() - t0; prof_n[idx]++; }
    }
    setreg(12, next_pc_req);
}

static int mem_fault(uc_engine *u, uc_mem_type t, uint64_t addr, int size, int64_t val, void *ud) {
    (void)u; (void)ud; (void)val;
    fprintf(stderr, "[cpu] memory fault type=%d addr=%08llx size=%d pc=%08x lr=%08x sp=%08x\n", t, (unsigned long long)addr, size, reg(15), reg(14), reg(13));
    return 0;
}

static u32 blk_ring[1<<20]; static int blk_pos; static int blk_on;
int g_blkcnt_on; static u32 bc_a[1<<16]; static u32 bc_n[1<<16];
static void on_block(uc_engine *u, uint64_t addr, uint32_t size, void *ud) { (void)u; (void)size; (void)ud;
    if (g_blkcnt_on) { u32 h = ((u32)addr * 2654435761u) >> 16; while (bc_a[h] && bc_a[h] != (u32)addr) h = (h + 1) & 0xffff; bc_a[h] = (u32)addr; bc_n[h]++; return; } blk_ring[blk_pos++ & 0xfffff] = (u32)addr; }
void blkcnt_dump(void) { for (int k = 0; k < 3000; k++) { int b = -1; u32 bv = 0; for (int i = 0; i < 65536; i++) if (bc_n[i] > bv) { bv = bc_n[i]; b = i; } if (b < 0) break; fprintf(stderr, "[blkcnt] %08x %u\n", bc_a[b], bv); bc_n[b] = 0; } }
void dump_blocks(int n) { (void)n; int i = 1; while (i < (1 << 20) && blk_ring[(blk_pos - i) & 0xfffff] >= 0x08000000 && blk_ring[(blk_pos - i) & 0xfffff] < 0x0c000100) i++;   /* skip the run through zeroed memory */
    fprintf(stderr, "[blocks] before the run (oldest first):"); for (int k = i + 40; k >= i; k--) fprintf(stderr, " %x", blk_ring[(blk_pos - k) & 0xfffff]); fprintf(stderr, "\n"); }
static void on_watch(uc_engine *u, uc_mem_type t, uint64_t addr, int size, int64_t val, void *ud) {
    (void)u; (void)ud; fprintf(stderr, "[watch] %s %08llx size=%d val=%08llx pc=%08x lr=%08x\n", t == UC_MEM_WRITE ? "W" : "R", (unsigned long long)addr, size, (unsigned long long)val, reg(15), reg(14));
}
static void dump_str(const char *tag, u32 a) {
    if (a < 0x1000 || a >= ARENA_SIZE - 64) return; const unsigned char *p = arena + a; int n = 0;
    while (n < 80 && p[n] >= 32 && p[n] < 127) n++;
    if (n >= 3) fprintf(stderr, "   %s -> \"%.*s\"\n", tag, n, p);
}
static u32 xw_addr[8]; static int nxw; static uc_engine *engines_dbg[16];
static void arm_extra_watch(uc_engine *e);
/* Level-start glue. The engine's loader runs on a pthread; on iOS the thread's completion reaches the main loop through the scene
 * transition (sub_c64cc(engine, success, multi)).  We capture the engine when its game object is created and, once the loader thread's
 * done flag (game+0x78) is set, deliver that success notification from the main loop. */
static u32 g_engine_obj; static int g_level_done;
static void on_engine_cap(uc_engine *u, uint64_t addr, uint32_t size, void *ud) { (void)u; (void)addr; (void)size; (void)ud; if (!g_engine_obj) g_engine_obj = reg(0); }
/* Threads are started a few frames after pthread_create (not inside it): the engine's loader reports completion by posting a scene-transition
 * request, and that request is dropped unless the transition is idle (kind 3) - it is, on a device, because loading takes seconds. */
void cpu_thread_start_now(gptr fn, u32 arg);
typedef struct { gptr fn; u32 arg; int due; } PendThr;
static PendThr pend[16]; static int npend, g_frames;
/* A game created after the EAGLView was un-hidden never gets -resumeGameRender (the device un-hides the view when the level starts), so it stays
 * paused (game+0x2f4/0x2f5).  Once the level is ready (game+0x18) resume it the way resumeGameRender does: sub_d4dbc(game,1); sub_d4e10(game,1). */
static u32 g_resumed_game;
static void game_resume_poll(void) {
    if (!g_engine_obj) return; u32 game = rd32(g_engine_obj + 0x10); if (!game || game == g_resumed_game) return;
    if (!arena[game + 0x18] || !arena[game + 0x2f5]) return;
    g_resumed_game = game; u32 a[2] = { game, 1 }; fprintf(stderr, "[engine] resuming game %08x\n", game); call_guest(0xd4dbc | 1, 2, a); call_guest(0xd4e10 | 1, 2, a);
}
void engine_poll(void) {
    g_frames++; game_resume_poll();
    for (int i = 0; i < npend; i++) {
        if (!pend[i].fn || g_frames < pend[i].due) continue;
        if (g_engine_obj && g_frames < pend[i].due + 600) { gptr cmd = rd32(g_engine_obj + 0x14); if (cmd && rd32(cmd) != 3) continue; }
        PendThr t = pend[i]; pend[i].fn = 0; { extern void offline_post_configs(void); offline_post_configs(); }   /* level configs (settings, grantsunlock) before the loader thread reads them */ fprintf(stderr, "[thread] starting deferred fn=%08x at frame %d\n", t.fn, g_frames); cpu_thread_start_now(t.fn, t.arg);
    }
}
static void on_break(uc_engine *u, uint64_t addr, uint32_t size, void *ud) {
    { const char *pk = getenv("RA_POKE");   /* "brkaddr:reg:off:byte" - write a byte at [reg+off] each time brkaddr runs */
      unsigned long ba; int rg; long of; int v; if (pk && sscanf(pk, "%lx:%d:%lx:%d", &ba, &rg, &of, &v) == 4 && (ba & ~1ul) == addr) { ((unsigned char *)arena)[reg(rg) + of] = (unsigned char)v; fprintf(stderr, "[poke] %08x=%d\n", reg(rg) + of, v); return; } }
    { const char *wb = getenv("RA_WATCH_BRK");   /* "brkaddr:reg:off" - on the first hit of brkaddr, watch writes to [reg+off] on every engine */
      unsigned long ba; int rg; long of; static int once; if (!once && wb && sscanf(wb, "%lx:%d:%lx", &ba, &rg, &of) == 3 && (ba & ~1ul) == addr) { once = 1; xw_addr[nxw++] = reg(rg) + of; fprintf(stderr, "[xwatch] %08x\n", xw_addr[0]); for (int i = 0; i < 16; i++) if (engines_dbg[i]) arm_extra_watch(engines_dbg[i]); } }
    (void)u; (void)size; (void)ud;
    fprintf(stderr, "[break %08llx] r0=%08x r1=%08x r2=%08x r3=%08x r4=%08x r5=%08x sp=%08x lr=%08x\n", (unsigned long long)addr,
            reg(0), reg(1), reg(2), reg(3), reg(4), reg(5), reg(13), reg(14));
    dump_str("r0", reg(0)); dump_str("r1", reg(1)); dump_str("r2", reg(2));
    fprintf(stderr, "   r6=%08x r7=%08x r8=%08x r9=%08x r10=%08x r11=%08x r12=%08x  [r8]=%08x\n", reg(6), reg(7), reg(8), reg(9), reg(10), reg(11), reg(12), reg(8) < ARENA_SIZE - 4 ? rd32(reg(8)) : 0);
    const char *dm = getenv("RA_DUMPREG");   /* e.g. "10:0x60:0x100" = dump guest words at r10+0x60..+0x100 */
    if (dm) { int r = atoi(dm); unsigned lo = 0, hi = 0x40; const char *c = strchr(dm, ':'); if (c) { lo = strtoul(c + 1, (char **)&c, 0); if (c && *c == ':') hi = strtoul(c + 1, NULL, 0); }
        gptr b = reg(r); if (b && b < ARENA_SIZE - hi) { fprintf(stderr, "   dump r%d=%08x:", r, b); for (unsigned o = lo; o < hi; o += 4) fprintf(stderr, " +%x=%x", o, rd32(b + o)); fprintf(stderr, "\n"); } }
    const char *dp = getenv("RA_DUMPPTR");   /* "reg:off:lo:hi" = dump words of the object pointed to by [reg+off] */
    if (dp) { int r; unsigned off, lo, hi; if (sscanf(dp, "%d:%x:%x:%x", &r, &off, &lo, &hi) == 4) { gptr q = reg(r) + off; if (q < ARENA_SIZE - 8) { gptr b = rd32(q); if (b && b < ARENA_SIZE - hi) { fprintf(stderr, "   dumpptr %08x:", b); for (unsigned o = lo; o < hi; o += 4) fprintf(stderr, " +%x=%x", o, rd32(b + o)); fprintf(stderr, "\n"); } } } }
}
/* capture r0 each time guest code reaches `addr` (used to find the engine's input object) */
static struct { gptr addr; gptr *dst; } caps[8]; static int ncaps;
static void on_capture(uc_engine *u, uint64_t addr, uint32_t size, void *ud) { (void)u; (void)size; (void)ud; for (int i = 0; i < ncaps; i++) if (caps[i].addr == (gptr)addr) *caps[i].dst = reg(0); }
void cpu_capture_r0(gptr addr, gptr *dst) { if (ncaps >= 8) return; caps[ncaps].addr = addr & ~1u; caps[ncaps].dst = dst; ncaps++; uc_hook h; uc_hook_add(uc, &h, UC_HOOK_CODE, on_capture, NULL, addr & ~1u, addr & ~1u); }
static void vfp_enable(void) {
    u32 v = 0; uc_reg_read(uc, UC_ARM_REG_C1_C0_2, &v); v |= 0xF << 20; uc_reg_write(uc, UC_ARM_REG_C1_C0_2, &v);
    u32 fpexc = 0x40000000; uc_reg_write(uc, UC_ARM_REG_FPEXC, &fpexc);
}

#define MAXDEPTH 16
static uc_engine *engines[MAXDEPTH]; static int depth;
static void arm_extra_watch(uc_engine *e) { for (int i = 0; i < nxw; i++) { uc_hook hw; uc_hook_add(e, &hw, getenv("RA_WATCH_READ") ? (UC_HOOK_MEM_WRITE | UC_HOOK_MEM_READ) : UC_HOOK_MEM_WRITE, on_watch, NULL, xw_addr[i], xw_addr[i] + 3); } }
static void add_debug_hooks(uc_engine *uc) {
    for (int i = 0; i < 16; i++) if (!engines_dbg[i]) { engines_dbg[i] = uc; break; }
    arm_extra_watch(uc);
    { uc_hook hc; uc_hook_add(uc, &hc, UC_HOOK_CODE, on_engine_cap, NULL, 0xf643c, 0xf643c); }
    const char *wt = getenv("RA_WATCH");            /* comma-separated hex addrs: log writes */
    while (wt && *wt) { char *e; unsigned long a = strtoul(wt, &e, 16); if (e == wt) break; uc_hook hw; uc_hook_add(uc, &hw, UC_HOOK_MEM_WRITE, on_watch, NULL, a, a + 3); wt = *e ? e + 1 : e; }
    if (getenv("RA_BLOCKS")) { blk_on = 1; uc_hook hbk; uc_hook_add(uc, &hbk, UC_HOOK_BLOCK, on_block, NULL, 1, 0); }
    const char *brk = getenv("RA_BREAK");           /* comma-separated hex addrs; thumb bit ignored */
    while (brk && *brk) { char *e; unsigned long a = strtoul(brk, &e, 16); if (e == brk) break; uc_hook hb; uc_hook_add(uc, &hb, UC_HOOK_CODE, on_break, NULL, a & ~1ul, a & ~1ul); brk = *e ? e + 1 : e; }
}
static uc_engine *make_engine(int idx) {
    uc_engine *e; uc_err er = uc_open(UC_ARCH_ARM, UC_MODE_ARM, &e);
    if (er) { fprintf(stderr, "uc_open: %s\n", uc_strerror(er)); exit(1); }
    uc_ctl_set_cpu_model(e, UC_CPU_ARM_CORTEX_A9);
    er = uc_mem_map_ptr(e, 0, ARENA_SIZE, UC_PROT_ALL, arena);     /* every engine shares the one guest memory */
    if (er) { fprintf(stderr, "uc_mem_map_ptr: %s\n", uc_strerror(er)); exit(1); }
    uc_engine *save = uc; uc = e; vfp_enable(); uc = save;
    uc_hook h; uc_hook_add(e, &h, UC_HOOK_INTR, on_intr, NULL, 1, 0);
    uc_hook h2; uc_hook_add(e, &h2, UC_HOOK_MEM_READ_UNMAPPED | UC_HOOK_MEM_WRITE_UNMAPPED | UC_HOOK_MEM_FETCH_UNMAPPED, mem_fault, NULL, 1, 0);
    u32 tls = 0; if (uc) uc_reg_read(uc, UC_ARM_REG_C13_C0_3, &tls); if (tls) uc_reg_write(e, UC_ARM_REG_C13_C0_3, &tls);
    add_debug_hooks(e);
    (void)idx; return e;
}
void cpu_init(void) {
    g_prof = getenv("RA_PROF") != NULL;
    arena = mmap(NULL, ARENA_SIZE, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS | MAP_NORESERVE, -1, 0);
    if (arena == MAP_FAILED) { perror("mmap arena"); exit(1); }
    uc = NULL; engines[0] = make_engine(0); uc = engines[0];
    wr32(RET_TRAP, 0xE30FCFFFu); wr32(RET_TRAP + 4, 0xEF000000u);
    u32 sp = STACK_TOP; uc_reg_write(uc, UC_ARM_REG_SP, &sp);
    u32 tls = alloc_fake(256); uc_reg_write(uc, UC_ARM_REG_C13_C0_3, &tls);
    for (int i = 1; i < MAXDEPTH; i++) engines[i] = NULL;
}

/* ---- cooperative threads: a pthread runs on its own stack until it blocks in pthread_cond_wait, then its CPU context is saved;
 * pthread_cond_signal/broadcast resumes every parked thread (they run to their next block, nested, inside the signaller) ---- */
typedef struct { uc_context *ctx; int parked, running, alive; gptr stack; } Thr;
static Thr thr[16]; static int nthr, cur_thr = -1, park_req;
int cpu_in_thread(void) { return cur_thr >= 0; }
void host_cpu_stop(void) { if (cur_thr >= 0) park_req = 1; uc_emu_stop(uc); }
static void thread_go(int ti, gptr fn, u32 arg);
void cpu_thread_start_now(gptr fn, u32 arg);
void cpu_thread_start(gptr fn, u32 arg) { if (npend < 16) { pend[npend].fn = fn; pend[npend].arg = arg; pend[npend].due = g_frames + 20; npend++; } }
void cpu_thread_start_now(gptr fn, u32 arg) {
    if (nthr >= 16) return; int ti = nthr++; thr[ti].alive = 1; thr[ti].stack = gm_malloc(1 << 19); thread_go(ti, fn, arg); }
void cpu_thread_wake(void) { for (int i = 0; i < nthr; i++) if (thr[i].alive && thr[i].parked && !thr[i].running) thread_go(i, 0, 0); }
static void thread_go(int ti, gptr fn, u32 arg) {
    Thr *t = &thr[ti];
    if (depth + 1 >= MAXDEPTH) return;
    uc_engine *prev = uc; int d = depth; if (!engines[d + 1]) engines[d + 1] = make_engine(d + 1);
    uc_engine *eng = engines[d + 1]; int was_hook = prev_in_hook; gptr saved_next = next_pc_req; int saved_thr = cur_thr;
    uc = eng; depth++; prev_in_hook = 0; cur_thr = ti; t->running = 1; park_req = 0; u32 start;
    if (fn) { setreg(0, arg); setreg(13, (t->stack + (1 << 19) - 64) & ~7u); setreg(14, RET_TRAP); start = fn; }
    else { uc_context_restore(eng, t->ctx); start = reg(15); t->parked = 0; }
    uc_err e = uc_emu_start(uc, start, RET_TRAP, 0, 0);
    if (e) fprintf(stderr, "[thread] emu error %s pc=%08x\n", uc_strerror(e), reg(15));
    if (park_req) { if (!t->ctx) uc_context_alloc(eng, &t->ctx); uc_context_save(eng, t->ctx); t->parked = 1; } else t->alive = 0;
    park_req = 0; t->running = 0; cur_thr = saved_thr; depth--; uc = prev; prev_in_hook = was_hook; next_pc_req = saved_next; }

/* Call guest code. Re-entrant: a nested call (from inside an HLE handler) runs on a second Unicorn engine that shares the same
 * guest memory, starting below the caller's stack pointer.  `uc` always points at the engine currently executing. */
u32 call_guest(gptr fn, int nargs, const u32 *args) {
    if (depth + 1 >= MAXDEPTH) { fprintf(stderr, "[cpu] guest call nesting too deep fn=%08x\n", fn); return 0; }
    uc_engine *prev = uc; int d = depth;
    uc_engine *eng = prev;
    u32 sp = reg(13);
    if (d > 0 || getenv("RA_FORCE_NESTED")) { }
    if (prev_in_hook) {                              /* we are inside an HLE handler: use the next engine */
        if (!engines[d + 1]) engines[d + 1] = make_engine(d + 1);
        eng = engines[d + 1]; sp -= 256;
    }
    sp &= ~7u;
    int extra = nargs > 4 ? nargs - 4 : 0;
    sp -= extra * 4; sp &= ~7u;
    uc = eng; if (eng != prev) depth++;
    int was_hook = prev_in_hook; prev_in_hook = 0; gptr saved_next_pc = next_pc_req;   /* handlers of the nested call clobber this global */
    for (int i = 0; i < nargs; i++) { if (i < 4) setreg(i, args[i]); else wr32(sp + 4 * (i - 4), args[i]); }
    setreg(13, sp); setreg(14, RET_TRAP);
    if (g_trace > 2) fprintf(stderr, "[call] depth=%d eng=%d fn=%08x sp=%08x args=%08x,%08x,%08x\n", depth, eng == prev ? 0 : 1, fn, sp, nargs > 0 ? args[0] : 0, nargs > 1 ? args[1] : 0, nargs > 2 ? args[2] : 0);
    uc_err e = uc_emu_start(uc, fn, RET_TRAP, 0, 0);
    if (e) { fprintf(stderr, "[cpu] emu error: %s pc=%08x lr=%08x (call fn=%08x a0=%08x a1=%08x depth=%d)\n", uc_strerror(e), reg(15), reg(14), fn, nargs > 0 ? args[0] : 0, nargs > 1 ? args[1] : 0, depth); if (blk_on) { dump_blocks(0); exit(1); } }
    if (g_trace > 2) fprintf(stderr, "[call] return depth=%d r0=%08x pc=%08x\n", depth, reg(0), reg(15));
    u32 r = reg(0);
    if (eng == prev) setreg(13, sp + extra * 4);
    if (eng != prev) depth--;
    uc = prev; prev_in_hook = was_hook; next_pc_req = saved_next_pc;
    return r;
}

/* ---- guest heap: segregated power-of-two free lists, 16-byte header ---- */
static gptr heap_cur = HEAP_BASE; static gptr freel[32];
static int cls_of(size_t n) { int c = 4; while (((size_t)1 << c) < n) c++; return c; }
gptr gm_malloc(size_t n) {
    if (!n) n = 1; int c = cls_of(n);
    gptr p = freel[c];
    if (p) { freel[c] = rd32(p); }
    else { size_t sz = (size_t)1 << c; if (heap_cur + 16 + sz > HEAP_END) { fprintf(stderr, "guest heap exhausted (req %zu, lr=%08x)\n", n, reg(14)); return 0; } p = heap_cur + 16; heap_cur += 16 + sz; }
    wr32(p - 4, c); memset(H(p), 0, (size_t)1 << c); return p;   /* zeroed: ObjC alloc and calloc rely on it */
}
void gm_free(gptr p) { if (p < HEAP_BASE + 16 || p >= heap_cur) return; int c = rd32(p - 4); if (c < 4 || c > 30) return; wr32(p, freel[c]); freel[c] = p; }
gptr gm_realloc(gptr p, size_t n) { if (p < HEAP_BASE + 16 || p >= heap_cur) return gm_malloc(n); int c = rd32(p - 4); size_t old = (size_t)1 << c; if (n <= old) return p; gptr q = gm_malloc(n); memcpy(H(q), H(p), old); gm_free(p); return q; }
gptr gm_strdup(const char *s) { size_t n = strlen(s) + 1; gptr p = gm_malloc(n); memcpy(H(p), s, n); return p; }
