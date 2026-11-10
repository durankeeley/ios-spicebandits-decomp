/* libc.c - libc / libgcc / libstdc++ ABI / misc BSD imports */
#include "ra.h"
#include <stdarg.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <math.h>
#include <time.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/types.h>
#include <dirent.h>
#include <ctype.h>

const char *g_bundle_dir = "."; const char *g_docs_dir = "./save";
static gptr g_errno_addr;
#define SETERR() wr32(g_errno_addr, errno)

const char *host_path(const char *p) {
    static char buf[4096];
    if (!p) return p;
    if (p[0] == '/' && g_bundle_dir) {
        static const char *marks[] = {"/Tower.app/", NULL};
        for (int i = 0; marks[i]; i++) { const char *q = strstr(p, marks[i]); if (q) { snprintf(buf, sizeof buf, "%s/%s", g_bundle_dir, q + strlen(marks[i])); return buf; } }
        const char *q = strstr(p, "/Documents/"); if (q) { snprintf(buf, sizeof buf, "%s/%s", g_docs_dir, q + 11); return buf; }
        q = strstr(p, "/Library/"); if (q) { snprintf(buf, sizeof buf, "%s/Library/%s", g_docs_dir, q + 9); return buf; }
    }
    return p;
}

/* ---------- varargs ---------- */
typedef struct { const u32 *w; int pos; } VA;
static u32 va_u32(VA *v) { return v->w[v->pos++]; }
static u64 va_u64(VA *v) { u64 r; memcpy(&r, v->w + v->pos, 8); v->pos += 2; return r; }   /* Apple ARM ABI: no 8-byte alignment of 64-bit args */
static VA va_from_regs(int first) {      /* args starting at word index `first`: r_first..r3 then stack */
    static u32 buf[64] __attribute__((aligned(8)));
    for (int i = 0; i < 4; i++) buf[i] = reg(i);
    u32 sp = reg(13); for (int i = 0; i < 28; i++) buf[4 + i] = rd32(sp + 4 * i);
    VA v = { buf, first }; return v;
}
static VA va_from_ptr(gptr p) { VA v = { (const u32 *)H(p), 0 }; return v; }

static int gformat(char *out, size_t cap, const char *fmt, VA *va) {
    size_t o = 0;
    #define PUT(s, n) do { size_t _n = (n); for (size_t _i = 0; _i < _n && o + 1 < cap; _i++) out[o++] = (s)[_i]; } while (0)
    while (*fmt) {
        if (*fmt != '%') { if (o + 1 < cap) out[o++] = *fmt; fmt++; continue; }
        char spec[64]; int sl = 0; spec[sl++] = *fmt++;
        if (*fmt == '%') { if (o + 1 < cap) out[o++] = '%'; fmt++; continue; }
        while (*fmt && strchr("-+ #0", *fmt)) spec[sl++] = *fmt++;
        if (*fmt == '*') { sl += snprintf(spec + sl, 12, "%d", (int)va_u32(va)); fmt++; } else while (isdigit((unsigned char)*fmt)) spec[sl++] = *fmt++;
        if (*fmt == '.') { spec[sl++] = *fmt++; if (*fmt == '*') { sl += snprintf(spec + sl, 12, "%d", (int)va_u32(va)); fmt++; } else while (isdigit((unsigned char)*fmt)) spec[sl++] = *fmt++; }
        int ll = 0, h = 0;
        while (*fmt && strchr("hlqjztL", *fmt)) { if (*fmt == 'l' || *fmt == 'q' || *fmt == 'j') ll++; if (*fmt == 'h') h++; fmt++; }
        char c = *fmt ? *fmt++ : 0; char tmp[512];
        switch (c) {
        case 'd': case 'i': { strcpy(spec + sl, "lld"); sl += 3; long long v = ll >= 2 ? (long long)va_u64(va) : (long long)(int)va_u32(va); if (h == 1) v = (short)v; if (h == 2) v = (signed char)v; int n = snprintf(tmp, sizeof tmp, spec, v); PUT(tmp, n); break; }
        case 'u': case 'x': case 'X': case 'o': { spec[sl++] = 'l'; spec[sl++] = 'l'; spec[sl++] = c; spec[sl] = 0; unsigned long long v = ll >= 2 ? va_u64(va) : va_u32(va); if (h == 1) v = (unsigned short)v; if (h == 2) v = (unsigned char)v; int n = snprintf(tmp, sizeof tmp, spec, v); PUT(tmp, n); break; }
        case 'c': { spec[sl++] = 'c'; spec[sl] = 0; int n = snprintf(tmp, sizeof tmp, spec, (int)va_u32(va)); PUT(tmp, n); break; }
        case 's': { spec[sl++] = 's'; spec[sl] = 0; gptr s = va_u32(va); int n = snprintf(tmp, sizeof tmp, spec, s ? gstr(s) : "(null)"); if (n >= (int)sizeof tmp) n = sizeof tmp - 1; PUT(tmp, n); break; }
        case 'p': { int n = snprintf(tmp, sizeof tmp, "0x%x", va_u32(va)); PUT(tmp, n); break; }
        case 'f': case 'F': case 'e': case 'E': case 'g': case 'G': case 'a': { spec[sl++] = c; spec[sl] = 0; u64 b = va_u64(va); double d; memcpy(&d, &b, 8); int n = snprintf(tmp, sizeof tmp, spec, d); PUT(tmp, n); break; }
        case '@': { extern const char *objc_describe(gptr obj); const char *d = objc_describe(va_u32(va)); PUT(d, strlen(d)); break; }
        case 'n': va_u32(va); break;
        default: break;
        }
    }
    if (cap) out[o < cap ? o : cap - 1] = 0;
    return (int)o;
}
int gformat_va(char *out, size_t cap, const char *fmt, gptr valist) { VA v = va_from_ptr(valist); return gformat(out, cap, fmt, &v); }
int gformat_regs(char *out, size_t cap, gptr fmt, int firstvar) { VA v = va_from_regs(firstvar); return gformat(out, cap, gstr(fmt), &v); }

/* ---------- memory / string ---------- */
static void h_memcpy(void) { memmove(H(arg(0)), H(arg(1)), arg(2)); ret(arg(0)); }
static void h_memset(void) { memset(H(arg(0)), arg(1), arg(2)); ret(arg(0)); }
static void h_strlen(void) { ret(strlen(gstr(arg(0)))); }
static void h_strcmp(void) { ret(strcmp(gstr(arg(0)), gstr(arg(1)))); }
static void h_strncmp(void) { ret(strncmp(gstr(arg(0)), gstr(arg(1)), arg(2))); }
static void h_strcpy(void) { strcpy((char *)H(arg(0)), gstr(arg(1))); ret(arg(0)); }
static void h_strchr(void) { const char *r = strchr(gstr(arg(0)), (int)arg(1)); ret(r ? G(r) : 0); }
static void h_strcspn(void) { ret(strcspn(gstr(arg(0)), gstr(arg(1)))); }
static void h_atoi(void) { ret(atoi(gstr(arg(0)))); }
static void h_malloc(void) { ret(gm_malloc(arg(0))); }
static void h_free(void) { gm_free(arg(0)); }
static void h_realloc(void) { ret(gm_realloc(arg(0), arg(1))); }
static void h_calloc(void) { gptr p = gm_malloc((size_t)arg(0) * arg(1)); memset(H(p), 0, (size_t)arg(0) * arg(1)); ret(p); }
static void h_sprintf(void) { ret(gformat_regs((char *)H(arg(0)), 1 << 20, arg(1), 2)); }
static void h_snprintf(void) { ret(gformat_regs((char *)H(arg(0)), arg(1), arg(2), 3)); }
static void h_vsprintf(void) { VA v = va_from_ptr(arg(2)); ret(gformat((char *)H(arg(0)), 1 << 20, gstr(arg(1)), &v)); }
static void h_vsnprintf(void) { VA v = va_from_ptr(arg(3)); ret(gformat((char *)H(arg(0)), arg(1), gstr(arg(2)), &v)); }
static void h_printf(void) { char b[4096]; gformat_regs(b, sizeof b, arg(0), 1); fputs(b, stdout); ret(strlen(b)); }
static void h_puts(void) { puts(gstr(arg(0))); ret(1); }
static void h_perror(void) { fprintf(stderr, "perror: %s\n", gstr(arg(0))); }
static void h_sscanf(void) {
    /* supports %d %i %u %x %f %lf %s %c with host sscanf by walking the format */
    const char *s = gstr(arg(0)), *f = gstr(arg(1)); VA v = va_from_regs(2); int cnt = 0;
    while (*f) {
        if (isspace((unsigned char)*f)) { while (isspace((unsigned char)*s)) s++; f++; continue; }
        if (*f != '%') { if (*s != *f) break; s++; f++; continue; }
        f++; int lng = 0; while (*f == 'l' || *f == 'h') { if (*f == 'l') lng++; f++; }
        char c = *f++; int n = 0;
        if (c == 'd' || c == 'i' || c == 'u') { int x; if (sscanf(s, c == 'i' ? "%i%n" : "%d%n", &x, &n) < 1) break; wr32(va_u32(&v), x); s += n; cnt++; }
        else if (c == 'x') { unsigned x; if (sscanf(s, "%x%n", &x, &n) < 1) break; wr32(va_u32(&v), x); s += n; cnt++; }
        else if (c == 'f' || c == 'g' || c == 'e') { double x; if (sscanf(s, "%lf%n", &x, &n) < 1) break; gptr p = va_u32(&v); if (lng) memcpy(H(p), &x, 8); else { float y = x; memcpy(H(p), &y, 4); } s += n; cnt++; }
        else if (c == 's') { char tmp[1024]; if (sscanf(s, "%1023s%n", tmp, &n) < 1) break; strcpy((char *)H(va_u32(&v)), tmp); s += n; cnt++; }
        else if (c == 'c') { if (!*s) break; *(char *)H(va_u32(&v)) = *s++; cnt++; }
        else break;
    }
    ret(cnt);
}

static FILE *getf(gptr p);
static FILE *getf_fwd(gptr p) { return getf(p); }
static void h_strstr(void) { const char *r = strstr(gstr(arg(0)), gstr(arg(1))); ret(r ? G(r) : 0); }
static void h_strcasestr(void) { const char *r = strcasestr(gstr(arg(0)), gstr(arg(1))); ret(r ? G(r) : 0); }
static void h_strncasecmp(void) { ret((u32)strncasecmp(gstr(arg(0)), gstr(arg(1)), arg(2))); }
static void h_strcasecmp(void) { ret((u32)strcasecmp(gstr(arg(0)), gstr(arg(1)))); }
static void h_strncpy(void) { strncpy((char *)H(arg(0)), gstr(arg(1)), arg(2)); ret(arg(0)); }
static void h_strcat(void) { strcat((char *)H(arg(0)), gstr(arg(1))); ret(arg(0)); }
static void h_strncat(void) { strncat((char *)H(arg(0)), gstr(arg(1)), arg(2)); ret(arg(0)); }
static void h_strrchr(void) { const char *r = strrchr(gstr(arg(0)), (int)arg(1)); ret(r ? G(r) : 0); }
static void h_strdup(void) { ret(gm_strdup(gstr(arg(0)))); }
static void h_memcmp(void) { ret((u32)memcmp(H(arg(0)), H(arg(1)), arg(2))); }
static void h_memchr(void) { void *r = memchr(H(arg(0)), (int)arg(1), arg(2)); ret(r ? G(r) : 0); }
static void h_strtol(void) { char *e; long v = strtol(gstr(arg(0)), &e, (int)arg(2)); if (arg(1)) wr32(arg(1), G(e)); ret((u32)v); }
static void h_strtoul(void) { char *e; unsigned long v = strtoul(gstr(arg(0)), &e, (int)arg(2)); if (arg(1)) wr32(arg(1), G(e)); ret((u32)v); }
static void h_strtod(void) { char *e; double v = strtod(gstr(arg(0)), &e); if (arg(1)) wr32(arg(1), G(e)); u64 b; memcpy(&b, &v, 8); ret64(b); }
static void h_atof(void) { double v = atof(gstr(arg(0))); u64 b; memcpy(&b, &v, 8); ret64(b); }
static void h_atol(void) { ret((u32)atol(gstr(arg(0)))); }
static void h_abs(void) { int v = (int)arg(0); ret((u32)(v < 0 ? -v : v)); }
static void h_random(void) { ret((u32)random()); }
static void h_srandomdev(void) { srandom(time(NULL)); }
static void h_getpid(void) { ret(1234); }
static void h_sleep(void) { sleep(arg(0)); ret(0); }
static void h_pthread_zero(void) { ret(0); }
static void h_fputs(void) { FILE *f = getf_fwd(arg(1)); if (f) fputs(gstr(arg(0)), f); ret(1); }
static void h_unlink(void) { ret((u32)unlink(host_path(gstr(arg(0))))); }
static void h_access(void) { ret((u32)access(host_path(gstr(arg(0))), (int)arg(1))); }
static void h_rename(void) { char a[4096]; snprintf(a, sizeof a, "%s", host_path(gstr(arg(0)))); ret((u32)rename(a, host_path(gstr(arg(1))))); }
static void h_getcwd(void) { if (!getcwd((char *)H(arg(0)), arg(1))) { ret(0); return; } ret(arg(0)); }
static void h_localtime(void) { time_t t = (time_t)(s32)rd32(arg(0)); struct tm *tm = localtime(&t); static gptr buf; if (!buf) buf = gm_malloc(64); int *w = (int *)H(buf); w[0]=tm->tm_sec; w[1]=tm->tm_min; w[2]=tm->tm_hour; w[3]=tm->tm_mday; w[4]=tm->tm_mon; w[5]=tm->tm_year; w[6]=tm->tm_wday; w[7]=tm->tm_yday; w[8]=tm->tm_isdst; w[9]=0; w[10]=0; ret(buf); }
static void h_mktime(void) { int *w = (int *)H(arg(0)); struct tm tm = {0}; tm.tm_sec=w[0]; tm.tm_min=w[1]; tm.tm_hour=w[2]; tm.tm_mday=w[3]; tm.tm_mon=w[4]; tm.tm_year=w[5]; tm.tm_isdst=-1; ret((u32)mktime(&tm)); }
static void h_qsort(void) { /* comparator runs in the guest through a nested engine: simple insertion sort over guest memory */
    gptr base = arg(0); u32 n = arg(1), sz = arg(2), cmp = arg(3); if (sz == 0 || sz > 4096) return; char *tmp = malloc(sz);
    for (u32 i = 1; i < n; i++) { memcpy(tmp, H(base + i * sz), sz); gptr tg = gm_malloc(sz); memcpy(H(tg), tmp, sz); u32 j = i;
        while (j > 0) { u32 a[2] = { base + (j - 1) * sz, tg }; if ((s32)call_guest(cmp, 2, a) <= 0) break; memcpy(H(base + j * sz), H(base + (j - 1) * sz), sz); j--; }
        memcpy(H(base + j * sz), H(tg), sz); gm_free(tg); }
    free(tmp); }
/* ---------- threads: run the start routine synchronously on a nested engine (the emulator is single threaded) ---------- */
static void h_pthread_create(void) { gptr tid = arg(0), fn = arg(2), a = arg(3); if (tid) wr32(tid, 0x7D000001); u32 args[1] = { a }; hle_log("[thread] create fn=%08x -> running synchronously\n", fn); call_guest(fn, 1, args); ret(0); }
static void h_pthread_self(void) { ret(0x7D000001); }
static void h_NSSelectorFromString(void) { extern const char *objc_describe(gptr); ret(gm_strdup(objc_describe(arg(0)))); }
/* ---------- stdio ---------- */
#define MAXF 256
static FILE *ftab[MAXF]; static gptr fblk[MAXF];
static int fnew(FILE *f) { for (int i = 3; i < MAXF; i++) if (!ftab[i]) { ftab[i] = f; fblk[i] = alloc_fake(16); wr32(fblk[i], i); wr32(fblk[i] + 4, 0xF11E0000); return i; } return -1; }
gptr stdio_slot(int which) {
    static gptr slots[3];
    if (!slots[which]) { gptr b = alloc_fake(16); wr32(b, which); wr32(b + 4, 0xF11E0000); ftab[which] = which == 0 ? stdin : which == 1 ? stdout : stderr; slots[which] = alloc_fake(16); wr32(slots[which], b); }
    return slots[which];
}
static FILE *getf(gptr p) { if (!p) return NULL; u32 i = rd32(p); if (rd32(p + 4) != 0xF11E0000 || i >= MAXF) return NULL; return ftab[i]; }
static void h_fopen(void) {
    const char *p = host_path(gstr(arg(0)));
    FILE *f = fopen(p, gstr(arg(1))); if (!f) { SETERR(); hle_log("[fopen] %s FAILED\n", p); ret(0); return; }
    hle_log("[fopen] %s\n", p);
    int i = fnew(f); ret(i < 0 ? 0 : fblk[i]);
}
static void h_fclose(void) { FILE *f = getf(arg(0)); if (f) { u32 i = rd32(arg(0)); fclose(f); ftab[i] = NULL; wr32(arg(0) + 4, 0); } ret(0); }
static void h_fread(void) { FILE *f = getf(arg(3)); ret(f ? fread(H(arg(0)), arg(1), arg(2), f) : 0); }
static void h_fwrite(void) { FILE *f = getf(arg(3)); ret(f ? fwrite(H(arg(0)), arg(1), arg(2), f) : 0); }
static void h_fseek(void) { FILE *f = getf(arg(0)); ret(f ? fseek(f, (s32)arg(1), arg(2)) : -1); }
static void h_ftell(void) { FILE *f = getf(arg(0)); ret(f ? ftell(f) : -1); }
static void h_fflush(void) { FILE *f = getf(arg(0)); if (f) fflush(f); ret(0); }
static void h_fprintf(void) { FILE *f = getf(arg(0)); char b[8192]; gformat_regs(b, sizeof b, arg(1), 2); if (f) fputs(b, f); ret(strlen(b)); }
static void h_remove(void) { ret(remove(host_path(gstr(arg(0))))); }
static void h_mkdir(void) { ret(mkdir(host_path(gstr(arg(0))), arg(1))); }
static int conv_oflags(int o) { int r = o & 3; if (o & 0x4) r |= O_NONBLOCK; if (o & 0x8) r |= O_APPEND; if (o & 0x200) r |= O_CREAT; if (o & 0x400) r |= O_TRUNC; if (o & 0x800) r |= O_EXCL; return r; }
void dump_blocks(int n); void guest_backtrace(void);
static void h_open(void) { const char *p = host_path(gstr(arg(0))); int fd = open(p, conv_oflags(arg(1)), arg(2)); if (fd < 0) SETERR(); hle_log("[open] %s -> %d\n", p, fd); if (getenv("RA_BLOCKS_ON_OPEN") && strstr(p, getenv("RA_BLOCKS_ON_OPEN"))) { fprintf(stderr, "[open %s]\n", p); dump_blocks(120); guest_backtrace(); } ret(fd); }
static void h_close(void) { ret(close(arg(0))); }
static void h_read(void) { int r = read(arg(0), H(arg(1)), arg(2)); hle_log("[read] fd=%d n=%u -> %d\n", arg(0), arg(2), r); ret(r); }
static void h_write(void) { ret(write(arg(0), H(arg(1)), arg(2))); }
static void h_lseek(void) { s64 off = (s64)arg64(1); s64 r = lseek(arg(0), off, arg(3)); hle_log("[lseek] fd=%d off=%lld whence=%d -> %lld\n", arg(0), (long long)off, arg(3), (long long)r); ret64(r); }
static void fill_stat(gptr p, const struct stat *st) {   /* Darwin struct stat with 64-bit inode (iOS SDK), size @60 */
    memset(H(p), 0, 108);
    wr32(p, (u32)st->st_dev); *(uint16_t *)H(p + 4) = (uint16_t)st->st_mode; *(uint16_t *)H(p + 6) = (uint16_t)st->st_nlink;
    *(u64 *)H(p + 8) = (u64)st->st_ino; wr32(p + 16, st->st_uid); wr32(p + 20, st->st_gid);
    wr32(p + 28, (u32)st->st_atime); wr32(p + 36, (u32)st->st_mtime); wr32(p + 44, (u32)st->st_ctime); wr32(p + 52, (u32)st->st_ctime);
    *(u64 *)H(p + 60) = (u64)st->st_size; *(u64 *)H(p + 68) = (u64)st->st_blocks; wr32(p + 76, 4096);
}
static void h_fstat(void) { struct stat st; int r = fstat(arg(0), &st); if (!r) fill_stat(arg(1), &st); ret(r); }
static void h_stat(void) { struct stat st; int r = stat(host_path(gstr(arg(0))), &st); if (!r) fill_stat(arg(1), &st); else SETERR(); ret(r); }
static void h_fcntl(void) { ret(0); }

/* ---------- time / misc ---------- */
static void h_gettimeofday(void) { struct timeval tv; gettimeofday(&tv, NULL); if (arg(0)) { wr32(arg(0), tv.tv_sec); wr32(arg(0) + 4, tv.tv_usec); } ret(0); }
static void h_time(void) { u32 t = time(NULL); if (arg(0)) wr32(arg(0), t); ret(t); }
/* mach time: nanosecond clock, timebase 1/1. The engine's animation clock reads this; returning 0 froze every GL animation. */
static u64 mono_ns(void) { struct timespec ts; clock_gettime(CLOCK_MONOTONIC, &ts); return (u64)ts.tv_sec * 1000000000ull + ts.tv_nsec; }
static void h_mach_absolute_time(void) { ret64(getenv("RA_NOMACH") ? 0 : mono_ns()); }
static void h_mach_timebase_info(void) { if (arg(0)) { wr32(arg(0), 1); wr32(arg(0) + 4, 1); } ret(0); }
static void h_mach_wait_until(void) { u64 t = arg64(0), n = mono_ns(); if (t > n) { struct timespec d = { (time_t)((t - n) / 1000000000ull), (long)((t - n) % 1000000000ull) }; nanosleep(&d, NULL); } ret(0); }
static void h_floatundidf(void) { double v = (double)arg64(0); u64 b; memcpy(&b, &v, 8); ret64(b); }
static void h_fixunsdfdi(void) { u64 b = arg64(0); double v; memcpy(&v, &b, 8); ret64(v <= 0 ? 0 : (u64)v); }
static void h_usleep(void) { usleep(arg(0)); ret(0); }
static void h_arc4random(void) { ret(((u32)rand() << 16) ^ (u32)rand()); }
static void h_error(void) { ret(g_errno_addr); }
static void h_getenv(void) { ret(0); }
void guest_backtrace(void) {
    u32 sp = reg(13); fprintf(stderr, "  lr=%08x  stack-scan:", reg(14));
    for (int i = 0; i < 96; i++) { u32 w = rd32(sp + 4 * i); if (w >= 0x2000 && w < 0xe0800) fprintf(stderr, " %x", w); }
    fprintf(stderr, "\n");
}
static void h_exit(void) { fprintf(stderr, "[guest] exit(%d) devtype@103c9c=%u\n", (int)arg(0), rd32(0x103c9c)); guest_backtrace(); exit(arg(0)); }
static void h_zero(void) { ret(0); }
static void h_neg1(void) { ret((u32)-1); }
static void h_gethostname(void) { strcpy((char *)H(arg(0)), "localhost"); ret(0); }
static void h_sysctlbyname(void) {
    const char *n = gstr(arg(0)); gptr oldp = arg(1), oldlenp = arg(2);
    const char *sv = NULL; int iv = -1;
    if (!strcmp(n, "hw.machine")) sv = "iPad2,1"; else if (!strcmp(n, "hw.model")) sv = "K93AP"; else if (!strcmp(n, "kern.osversion")) sv = "8C148";
    else if (!strcmp(n, "kern.hostname")) sv = "localhost"; else if (!strcmp(n, "hw.ncpu") || !strcmp(n, "hw.activecpu")) iv = 2;
    else if (!strcmp(n, "hw.physmem") || !strcmp(n, "hw.memsize") || !strcmp(n, "hw.usermem")) iv = 512 << 20; else if (!strcmp(n, "hw.cpufrequency")) iv = 1000000000;
    else if (!strcmp(n, "hw.busfrequency")) iv = 250000000; else if (!strcmp(n, "hw.pagesize")) iv = 4096;
    if (!sv && iv < 0) { if (oldlenp) wr32(oldlenp, 0); errno = ENOENT; ret((u32)-1); return; }
    u32 need = sv ? strlen(sv) + 1 : 4;
    if (oldlenp) { if (oldp && rd32(oldlenp) < need) { wr32(oldlenp, need); ret((u32)-1); return; } wr32(oldlenp, need); }
    if (oldp) { if (sv) strcpy((char *)H(oldp), sv); else wr32(oldp, iv); }
    ret(0);
}
static void h_getgid(void) { ret(501); }

/* ---------- compiler-rt / libgcc ---------- */
static void h_divsi3(void) { s32 b = arg(1); ret(b ? (s32)arg(0) / b : 0); }
static void h_modsi3(void) { s32 b = arg(1); ret(b ? (s32)arg(0) % b : 0); }
static void h_udivsi3(void) { u32 b = arg(1); ret(b ? arg(0) / b : 0); }
static void h_umodsi3(void) { u32 b = arg(1); ret(b ? arg(0) % b : 0); }
static void h_divdi3(void) { s64 a = arg64(0), b = arg64(2); ret64(b ? a / b : 0); }
static void h_moddi3(void) { s64 a = arg64(0), b = arg64(2); ret64(b ? a % b : 0); }
static void h_udivdi3(void) { u64 a = arg64(0), b = arg64(2); ret64(b ? a / b : 0); }
static void h_umoddi3(void) { u64 a = arg64(0), b = arg64(2); ret64(b ? a % b : 0); }
static void h_muldi3(void) { ret64(arg64(0) * arg64(2)); }
static void h_muldf3vfp(void) { u64 a = arg64(0), b = arg64(2); double x, y; memcpy(&x, &a, 8); memcpy(&y, &b, 8); x *= y; memcpy(&a, &x, 8); ret64(a); }
static void h_fixdfsivfp(void) { u64 a = arg64(0); double x; memcpy(&x, &a, 8); ret((u32)(s32)x); }
static void h_fixsfsivfp(void) { u32 a = arg(0); float x; memcpy(&x, &a, 4); ret((u32)(s32)x); }
static void h_subsf3vfp(void) { u32 a = arg(0), b = arg(1); float x, y; memcpy(&x, &a, 4); memcpy(&y, &b, 4); x -= y; memcpy(&a, &x, 4); ret(a); }
/* libm that guest may import */
#define MATH1(nm, fn) static void h_##nm(void) { u64 a = arg64(0); double x; memcpy(&x, &a, 8); x = fn(x); memcpy(&a, &x, 8); ret64(a); }
MATH1(sin, sin) MATH1(cos, cos) MATH1(sqrt, sqrt) MATH1(floor, floor) MATH1(ceil, ceil) MATH1(tan, tan) MATH1(atan, atan) MATH1(fabs, fabs) MATH1(exp, exp) MATH1(log, log)
#define MATH1F(nm, fn) static void h_##nm##f(void) { u32 a = arg(0); float x; memcpy(&x, &a, 4); x = fn(x); memcpy(&a, &x, 4); ret(a); }
MATH1F(sin, sinf) MATH1F(cos, cosf) MATH1F(sqrt, sqrtf) MATH1F(floor, floorf) MATH1F(ceil, ceilf) MATH1F(tan, tanf) MATH1F(atan, atanf) MATH1F(fabs, fabsf)
MATH1F(round, roundf) MATH1F(trunc, truncf) MATH1F(log, logf) MATH1F(exp, expf) MATH1F(asin, asinf) MATH1F(acos, acosf) MATH1F(log2, log2f) MATH1F(sinh, sinhf) MATH1F(cosh, coshf)
MATH1(round, round) MATH1(trunc, trunc) MATH1(asin, asin) MATH1(acos, acos) MATH1(log10, log10) MATH1(log2, log2)
static void h_fmodf(void) { u32 a = arg(0), b = arg(1); float x, y; memcpy(&x, &a, 4); memcpy(&y, &b, 4); x = fmodf(x, y); memcpy(&a, &x, 4); ret(a); }
static void h_lroundf(void) { u32 a = arg(0); float x; memcpy(&x, &a, 4); ret((u32)lroundf(x)); }
static void h_atan2f(void) { u32 a = arg(0), b = arg(1); float x, y; memcpy(&x, &a, 4); memcpy(&y, &b, 4); x = atan2f(x, y); memcpy(&a, &x, 4); ret(a); }
static void h_powf(void) { u32 a = arg(0), b = arg(1); float x, y; memcpy(&x, &a, 4); memcpy(&y, &b, 4); x = powf(x, y); memcpy(&a, &x, 4); ret(a); }
static void h_pow(void) { u64 a = arg64(0), b = arg64(2); double x, y; memcpy(&x, &a, 8); memcpy(&y, &b, 8); x = pow(x, y); memcpy(&a, &x, 8); ret64(a); }

/* ---------- C++ ABI ---------- */
static void h_guard_acquire(void) { gptr g = arg(0); if (*(char *)H(g)) { ret(0); } else ret(1); }
static void h_guard_release(void) { *(char *)H(arg(0)) = 1; }
static void h_alloc_exc(void) { ret(gm_malloc(arg(0) + 128) + 128); }
static void h_throw(void) { fprintf(stderr, "[guest] C++ exception thrown (type %08x) - exceptions unsupported, aborting\n", arg(1)); fflush(stderr); abort(); }
static void h_pure(void) { fprintf(stderr, "[guest] pure virtual call\n"); abort(); }
static void h_sjlj_resume(void) { fprintf(stderr, "[guest] _Unwind_SjLj_Resume reached (exception unwinding) lr=%08x\n", reg(14)); abort(); }
static void h_noop(void) { }

/* ---------- ctype ---------- */
gptr runelocale_addr(void) {
    static gptr a;
    if (a) return a;
    a = alloc_fake(8 + 32 + 4 * 3 + 4 * 256 * 3 + 64);
    memcpy(H(a), "RuneMagA", 8);
    u32 *rt = (u32 *)H(a + 52);
    for (int c = 0; c < 256; c++) {
        u32 f = 0;
        if (c < 128) {
            if (isalpha(c)) f |= 0x100; if (iscntrl(c)) f |= 0x200; if (isdigit(c)) f |= 0x400; if (isgraph(c)) f |= 0x800; if (islower(c)) f |= 0x1000;
            if (ispunct(c)) f |= 0x2000; if (isspace(c)) f |= 0x4000; if (isupper(c)) f |= 0x8000; if (isxdigit(c)) f |= 0x10000; if (c == ' ' || c == '\t') f |= 0x20000;
            if (isprint(c)) f |= 0x40000; if (c == ' ' || c == '\t') f |= 0x80000; f |= 0x100000 * 0; if (isdigit(c)) f |= (c - '0') | 0x400; if (isxdigit(c) && !isdigit(c)) f |= (tolower(c) - 'a' + 10);
            if (isalnum(c)) f |= 0x100 * 0; if (isdigit(c) || isalpha(c)) f |= 0x0; if (isalnum(c)) f |= 0x0; if (c >= 32 && c < 127) f |= 0x40000;
            if (isalpha(c) || isdigit(c)) f |= 0x0;
            if (isalnum(c)) f |= 0x100 * 0;
        }
        rt[c] = f;
        ((u32 *)H(a + 52 + 1024))[c] = c < 128 ? tolower(c) : c;
        ((u32 *)H(a + 52 + 2048))[c] = c < 128 ? toupper(c) : c;
    }
    return a;
}
static void h_maskrune(void) { u32 c = arg(0), m = arg(1); if (c < 256) ret(((u32 *)H(runelocale_addr() + 52))[c] & m); else ret(0); }
static void h_tolower(void) { u32 c = arg(0); ret(c < 128 ? tolower(c) : c); }
static void h_toupper(void) { u32 c = arg(0); ret(c < 128 ? toupper(c) : c); }

static void h_md5(void) { ret(0); }    /* CC_MD5 : filled in by audio/net modules if needed */

static const HleEntry libc_tab[] = {
    {"memcpy", h_memcpy}, {"memmove", h_memcpy}, {"memset", h_memset}, {"strlen", h_strlen}, {"strcmp", h_strcmp}, {"strncmp", h_strncmp}, {"strcpy", h_strcpy},
    {"strchr", h_strchr}, {"strcspn", h_strcspn}, {"atoi", h_atoi}, {"malloc", h_malloc}, {"free", h_free}, {"realloc", h_realloc}, {"calloc", h_calloc},
    {"sprintf", h_sprintf}, {"snprintf", h_snprintf}, {"vsprintf", h_vsprintf}, {"vsnprintf", h_vsnprintf}, {"printf", h_printf}, {"puts", h_puts}, {"perror", h_perror}, {"sscanf", h_sscanf},
    {"fopen", h_fopen}, {"fclose", h_fclose}, {"fread", h_fread}, {"fwrite", h_fwrite}, {"fseek", h_fseek}, {"ftell", h_ftell}, {"fflush", h_fflush}, {"fprintf", h_fprintf},
    {"remove", h_remove}, {"mkdir", h_mkdir}, {"open", h_open}, {"close", h_close}, {"read", h_read}, {"write", h_write}, {"lseek", h_lseek}, {"fstat", h_fstat}, {"stat", h_stat}, {"fcntl", h_fcntl},
    {"gettimeofday", h_gettimeofday}, {"time", h_time}, {"usleep", h_usleep}, {"arc4random", h_arc4random}, {"__error", h_error}, {"getenv", h_getenv}, {"setenv", h_zero}, {"exit", h_exit},
    {"gethostname", h_gethostname}, {"getgid", h_getgid}, {"sysctlbyname", h_sysctlbyname}, {"mach_host_self", h_zero}, {"mach_absolute_time", h_mach_absolute_time}, {"mach_timebase_info", h_mach_timebase_info}, {"mach_wait_until", h_mach_wait_until}, {"__floatundidf", h_floatundidf}, {"__fixunsdfdi", h_fixunsdfdi}, {"host_page_size", h_zero}, {"host_statistics", h_neg1},
    {"socket", h_neg1}, {"connect", h_neg1}, {"bind", h_neg1}, {"listen", h_neg1}, {"accept", h_neg1}, {"select", h_neg1}, {"send", h_neg1}, {"recv", h_neg1}, {"sendto", h_neg1}, {"recvfrom", h_neg1},
    {"setsockopt", h_neg1}, {"shutdown", h_neg1}, {"gethostbyname", h_zero}, {"getifaddrs", h_neg1}, {"freeifaddrs", h_zero}, {"inet_ntoa", h_zero}, {"inet_pton", h_neg1},
    {"__divsi3", h_divsi3}, {"__modsi3", h_modsi3}, {"__udivsi3", h_udivsi3}, {"__umodsi3", h_umodsi3}, {"__divdi3", h_divdi3}, {"__moddi3", h_moddi3}, {"__udivdi3", h_udivdi3}, {"__umoddi3", h_umoddi3},
    {"__muldi3", h_muldi3}, {"__muldf3vfp", h_muldf3vfp}, {"__fixdfsivfp", h_fixdfsivfp}, {"__fixsfsivfp", h_fixsfsivfp}, {"__subsf3vfp", h_subsf3vfp},
    {"sin", h_sin}, {"cos", h_cos}, {"sqrt", h_sqrt}, {"floor", h_floor}, {"ceil", h_ceil}, {"tan", h_tan}, {"atan", h_atan}, {"fabs", h_fabs}, {"exp", h_exp}, {"log", h_log}, {"pow", h_pow},
    {"sinf", h_sinf}, {"cosf", h_cosf}, {"sqrtf", h_sqrtf}, {"floorf", h_floorf}, {"ceilf", h_ceilf}, {"tanf", h_tanf}, {"atanf", h_atanf}, {"fabsf", h_fabsf}, {"atan2f", h_atan2f}, {"roundf", h_roundf}, {"truncf", h_truncf}, {"logf", h_logf}, {"expf", h_expf}, {"asinf", h_asinf}, {"acosf", h_acosf}, {"log2f", h_log2f}, {"sinhf", h_sinhf}, {"coshf", h_coshf}, {"round", h_round}, {"trunc", h_trunc}, {"asin", h_asin}, {"acos", h_acos}, {"log10", h_log10}, {"log2", h_log2}, {"fmodf", h_fmodf}, {"lroundf", h_lroundf}, {"powf", h_powf},
    {"__cxa_guard_acquire", h_guard_acquire}, {"__cxa_guard_release", h_guard_release}, {"__cxa_allocate_exception", h_alloc_exc}, {"__cxa_throw", h_throw}, {"__cxa_pure_virtual", h_pure},
    {"_Unwind_SjLj_Register", h_noop}, {"_Unwind_SjLj_Unregister", h_noop}, {"_Unwind_SjLj_Resume", h_sjlj_resume}, {"__gxx_personality_sj0", h_zero}, {"__cxa_begin_catch", h_zero}, {"__cxa_end_catch", h_noop},
    {"pthread_create", h_pthread_create}, {"pthread_join", h_pthread_zero}, {"pthread_detach", h_pthread_zero}, {"pthread_self", h_pthread_self}, {"pthread_attr_init", h_pthread_zero}, {"pthread_attr_setdetachstate", h_pthread_zero}, {"pthread_attr_destroy", h_pthread_zero}, {"pthread_cond_init", h_pthread_zero}, {"pthread_cond_signal", h_pthread_zero}, {"pthread_cond_broadcast", h_pthread_zero}, {"pthread_mutexattr_init", h_pthread_zero}, {"pthread_mutexattr_settype", h_pthread_zero}, {"NSSelectorFromString", h_NSSelectorFromString},
    {"strstr", h_strstr}, {"strcasestr", h_strcasestr}, {"strncasecmp", h_strncasecmp}, {"strcasecmp", h_strcasecmp}, {"strncpy", h_strncpy}, {"strcat", h_strcat}, {"strncat", h_strncat}, {"strrchr", h_strrchr}, {"strdup", h_strdup},
    {"memcmp", h_memcmp}, {"memchr", h_memchr}, {"strtol", h_strtol}, {"strtoul", h_strtoul}, {"strtod", h_strtod}, {"atof", h_atof}, {"atol", h_atol}, {"abs", h_abs}, {"labs", h_abs}, {"random", h_random}, {"srandomdev", h_srandomdev},
    {"getpid", h_getpid}, {"sleep", h_sleep}, {"pthread_mutex_init", h_pthread_zero}, {"pthread_mutex_lock", h_pthread_zero}, {"pthread_mutex_unlock", h_pthread_zero}, {"pthread_mutex_destroy", h_pthread_zero},
    {"unlink", h_unlink}, {"access", h_access}, {"rename", h_rename}, {"getcwd", h_getcwd}, {"localtime", h_localtime}, {"mktime", h_mktime}, {"qsort", h_qsort},
    {"__maskrune", h_maskrune}, {"__tolower", h_tolower}, {"__toupper", h_toupper}, {"CC_MD5", h_md5},
    {NULL, NULL}
};
void libc_init(void) { g_errno_addr = alloc_fake(16); hle_register_table(libc_tab); stdio_slot(0); stdio_slot(1); stdio_slot(2); }
