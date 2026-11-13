/* ra.h - shared declarations for the iOS host (Unicorn ARM + HLE of Apple APIs). */
#ifndef RA_H
#define RA_H
#include <stdint.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unicorn/unicorn.h>

typedef uint32_t u32; typedef int32_t s32; typedef uint64_t u64; typedef int64_t s64;
typedef uint32_t gptr;                       /* guest pointer */

/* ---- guest memory map ---- */
#define ARENA_SIZE   0x60000000u             /* 1.5 GB guest address space, lazily committed (the host never frees ObjC objects, so a long session leaks; 224 MB ran out) */
#define FAKE_BASE    0x08000000u             /* host-created objects, class stubs, data symbols */
#define FAKE_END     0x0c000000u
#define TRAP_BASE    0x0c000000u             /* import trampolines (12 bytes each) */
#define TRAP_END     0x0c100000u
#define HEAP_BASE    0x10000000u
#define HEAP_END     0x5e000000u
#define STACK_TOP    0x5fff0000u
#define STACK_SIZE   0x00800000u
#define RET_TRAP     (TRAP_END - 16)         /* host->guest call return address */

extern uint8_t *arena;
extern uc_engine *uc;
static inline void *H(gptr a) { return arena + a; }               /* guest -> host pointer */
static inline gptr  G(const void *p) { return (gptr)((const uint8_t *)p - arena); }
static inline u32  rd32(gptr a) { u32 v; memcpy(&v, arena + a, 4); return v; }
static inline void wr32(gptr a, u32 v) { memcpy(arena + a, &v, 4); }
static inline const char *gstr(gptr a) { return a ? (const char *)(arena + a) : "(null)"; }

/* ---- CPU / calling convention ---- */
u32  reg(int r);                             /* r0..r15 */
void setreg(int r, u32 v);
u32  arg(int n);                             /* nth 32-bit argument (r0-r3 then stack) */
u64  arg64(int n);                           /* 64-bit arg at word index n (must be even) */
void ret(u32 v);
void ret64(u64 v);
u32  call_guest(gptr fn, int nargs, const u32 *args);
extern int g_trace;                          /* RA_TRACE=1 */

/* ---- HLE registry ---- */
typedef void (*hle_fn)(void);
typedef struct { const char *name; hle_fn fn; } HleEntry;
gptr  hle_stub_for(const char *name);        /* trampoline address for an imported function */
void  hle_register_table(const HleEntry *t);
gptr  alloc_fake(size_t n);                  /* zeroed, 16-aligned block in the fake region */
extern gptr g_msgsend_trampoline;
void  set_next_pc(gptr pc);                  /* handler requests control transfer instead of `return` */

/* ---- heap ---- */
gptr  gm_malloc(size_t n);
void  gm_free(gptr p);
gptr  gm_realloc(gptr p, size_t n);
gptr  gm_strdup(const char *s);

/* ---- loader ---- */
typedef struct { char name[17]; u32 addr, size; } Section;
extern Section g_sections[96]; extern int g_nsections;
int   macho_load(const char *path);
extern u32 g_entry;
Section *find_section(const char *name, int nth);

/* ---- modules ---- */
void libc_init(void);
void objc_init(void);
void gles_init(void);
void audio_init(void);
void sdl_init(int w, int h);
int  objc_run_app(void);                     /* UIApplicationMain loop; returns exit code */
extern const char *g_bundle_dir, *g_docs_dir, *g_levels_dir;
extern int g_screen_w, g_screen_h;
const char *host_path(const char *guest);    /* map guest paths -> host paths */
void  hle_log(const char *fmt, ...);
#endif
