/* extra.c - imports used by the 3rd-party SDK layer of Tower (hashing, UUIDs, GCD, CF helpers, reachability) */
#include "ra.h"
#include <time.h>
#include <unistd.h>
static const char *scstr_ex(gptr o) { return o && o < ARENA_SIZE - 16 ? gstr(rd32(o + 8)) : ""; }
int gformat_va(char *out, size_t cap, const char *fmt, gptr valist);
gptr objc_make_nsstring_global(const char *s);
gptr objc_class_object(const char *name);
const char *objc_describe(gptr o);

/* ---------- SHA-1 / SHA-256 ---------- */
static u32 rol(u32 x, int n) { return (x << n) | (x >> (32 - n)); }
static u32 ror(u32 x, int n) { return (x >> n) | (x << (32 - n)); }
static void sha1(const uint8_t *d, size_t n, uint8_t out[20]) {
    u32 h[5] = {0x67452301, 0xEFCDAB89, 0x98BADCFE, 0x10325476, 0xC3D2E1F0}; size_t tot = ((n + 8) / 64 + 1) * 64; uint8_t *m = calloc(tot, 1); memcpy(m, d, n); m[n] = 0x80;
    u64 bits = (u64)n * 8; for (int i = 0; i < 8; i++) m[tot - 1 - i] = bits >> (8 * i);
    for (size_t o = 0; o < tot; o += 64) { u32 w[80]; for (int i = 0; i < 16; i++) w[i] = m[o + 4 * i] << 24 | m[o + 4 * i + 1] << 16 | m[o + 4 * i + 2] << 8 | m[o + 4 * i + 3];
        for (int i = 16; i < 80; i++) w[i] = rol(w[i - 3] ^ w[i - 8] ^ w[i - 14] ^ w[i - 16], 1);
        u32 a = h[0], b = h[1], c = h[2], dd = h[3], e = h[4];
        for (int i = 0; i < 80; i++) { u32 f, k; if (i < 20) { f = (b & c) | (~b & dd); k = 0x5A827999; } else if (i < 40) { f = b ^ c ^ dd; k = 0x6ED9EBA1; } else if (i < 60) { f = (b & c) | (b & dd) | (c & dd); k = 0x8F1BBCDC; } else { f = b ^ c ^ dd; k = 0xCA62C1D6; }
            u32 t = rol(a, 5) + f + e + k + w[i]; e = dd; dd = c; c = rol(b, 30); b = a; a = t; }
        h[0] += a; h[1] += b; h[2] += c; h[3] += dd; h[4] += e; }
    for (int i = 0; i < 5; i++) for (int j = 0; j < 4; j++) out[4 * i + j] = h[i] >> (24 - 8 * j); free(m);
}
static void sha256(const uint8_t *d, size_t n, uint8_t out[32]) {
    static const u32 K[64] = {0x428a2f98,0x71374491,0xb5c0fbcf,0xe9b5dba5,0x3956c25b,0x59f111f1,0x923f82a4,0xab1c5ed5,0xd807aa98,0x12835b01,0x243185be,0x550c7dc3,0x72be5d74,0x80deb1fe,0x9bdc06a7,0xc19bf174,0xe49b69c1,0xefbe4786,0x0fc19dc6,0x240ca1cc,0x2de92c6f,0x4a7484aa,0x5cb0a9dc,0x76f988da,0x983e5152,0xa831c66d,0xb00327c8,0xbf597fc7,0xc6e00bf3,0xd5a79147,0x06ca6351,0x14292967,0x27b70a85,0x2e1b2138,0x4d2c6dfc,0x53380d13,0x650a7354,0x766a0abb,0x81c2c92e,0x92722c85,0xa2bfe8a1,0xa81a664b,0xc24b8b70,0xc76c51a3,0xd192e819,0xd6990624,0xf40e3585,0x106aa070,0x19a4c116,0x1e376c08,0x2748774c,0x34b0bcb5,0x391c0cb3,0x4ed8aa4a,0x5b9cca4f,0x682e6ff3,0x748f82ee,0x78a5636f,0x84c87814,0x8cc70208,0x90befffa,0xa4506ceb,0xbef9a3f7,0xc67178f2};
    u32 h[8] = {0x6a09e667,0xbb67ae85,0x3c6ef372,0xa54ff53a,0x510e527f,0x9b05688c,0x1f83d9ab,0x5be0cd19}; size_t tot = ((n + 8) / 64 + 1) * 64; uint8_t *m = calloc(tot, 1); memcpy(m, d, n); m[n] = 0x80;
    u64 bits = (u64)n * 8; for (int i = 0; i < 8; i++) m[tot - 1 - i] = bits >> (8 * i);
    for (size_t o = 0; o < tot; o += 64) { u32 w[64]; for (int i = 0; i < 16; i++) w[i] = m[o + 4 * i] << 24 | m[o + 4 * i + 1] << 16 | m[o + 4 * i + 2] << 8 | m[o + 4 * i + 3];
        for (int i = 16; i < 64; i++) { u32 s0 = ror(w[i - 15], 7) ^ ror(w[i - 15], 18) ^ (w[i - 15] >> 3), s1 = ror(w[i - 2], 17) ^ ror(w[i - 2], 19) ^ (w[i - 2] >> 10); w[i] = w[i - 16] + s0 + w[i - 7] + s1; }
        u32 a = h[0], b = h[1], c = h[2], dd = h[3], e = h[4], f = h[5], g = h[6], hh = h[7];
        for (int i = 0; i < 64; i++) { u32 S1 = ror(e, 6) ^ ror(e, 11) ^ ror(e, 25), ch = (e & f) ^ (~e & g), t1 = hh + S1 + ch + K[i] + w[i], S0 = ror(a, 2) ^ ror(a, 13) ^ ror(a, 22), mj = (a & b) ^ (a & c) ^ (b & c), t2 = S0 + mj;
            hh = g; g = f; f = e; e = dd + t1; dd = c; c = b; b = a; a = t1 + t2; }
        h[0] += a; h[1] += b; h[2] += c; h[3] += dd; h[4] += e; h[5] += f; h[6] += g; h[7] += hh; }
    for (int i = 0; i < 8; i++) for (int j = 0; j < 4; j++) out[4 * i + j] = h[i] >> (24 - 8 * j); free(m);
}
static void h_sha1(void) { sha1(H(arg(0)), arg(1), (uint8_t *)H(arg(2))); ret(arg(2)); }
static void h_sha256(void) { sha256(H(arg(0)), arg(1), (uint8_t *)H(arg(2))); ret(arg(2)); }

/* ---------- CoreFoundation bits ---------- */
static void h_CFUUIDCreate(void) { gptr o = gm_malloc(16); wr32(o, 0); wr32(o + 4, (u32)rand()); wr32(o + 8, (u32)rand()); ret(o); }
static void h_CFUUIDCreateString(void) { char b[64]; snprintf(b, sizeof b, "%08X-%04X-4%03X-A%03X-%08X%04X", (u32)rand(), rand() & 0xFFFF, rand() & 0xFFF, rand() & 0xFFF, (u32)rand(), rand() & 0xFFFF); ret(objc_make_nsstring_global(b)); }
static void h_CFStringGetFastestEncoding(void) { ret(0x08000100); }
static void h_CFStringGetCStringPtr(void) { ret(rd32(arg(0) + 8)); }
static void h_CFStringGetCString(void) { const char *s = gstr(rd32(arg(0) + 8)); u32 cap = arg(2); if (strlen(s) + 1 > cap) { ret(0); return; } strcpy((char *)H(arg(1)), s); ret(1); }
static void h_CFStringGetLength(void) { ret(rd32(arg(0) + 12)); }
static void h_NSLogv(void) { char b[8192]; gformat_va(b, sizeof b, gstr(rd32(arg(0) + 8)), arg(1)); fprintf(stderr, "[NSLog] %s\n", b); }
static void h_zero(void) { ret(0); }
static void h_one(void) { ret(1); }
static void h_noop(void) { }
static void h_sysctl(void) { gptr oldlenp = arg(3); if (oldlenp) wr32(oldlenp, 0); ret((u32)-1); }   /* e.g. NET_RT_IFLIST (MAC lookup): not available */
static void h_uname(void) { gptr p = arg(0); memset(H(p), 0, 5 * 256); strcpy((char *)H(p), "Darwin"); strcpy((char *)H(p + 256), "localhost"); strcpy((char *)H(p + 512), "11.0.0"); strcpy((char *)H(p + 768), "Darwin Kernel Version 11.0.0"); strcpy((char *)H(p + 1024), "iPad2,1"); ret(0); }
/* ---- blocks runtime helpers (flags: 3 object, 7 block, 8 byref, 0x80 weak) ---- */
static gptr block_copy(gptr b);
static void h_block_object_assign(void) {
    gptr dst = arg(0), src = arg(1); int fl = arg(2) & 0x7f;
    if (fl == 8) {                                           /* __block variable: move the byref struct to the heap once */
        if (!src) { wr32(dst, 0); return; }
        gptr fwd = rd32(src + 4);
        if (fwd == src) { u32 sz = rd32(src + 12); if (sz < 24 || sz > 4096) sz = 24; gptr c = gm_malloc(sz); memcpy(H(c), H(src), sz); wr32(c + 4, c); wr32(src + 4, c); wr32(c + 8, rd32(c + 8) | (1u << 24)); fwd = c; }
        wr32(dst, fwd); return;
    }
    if (fl == 7) { wr32(dst, block_copy(src)); return; }     /* captured block */
    wr32(dst, src);                                          /* object: no refcounting in the host */
}
static void h_block_object_dispose(void) { }
/* ---- dirent (Darwin legacy layout: u32 ino, u16 reclen, u8 type, u8 namlen, char name[256]) ---- */
#include <dirent.h>
static DIR *dtab[16]; static gptr dbuf[16];
static void h_opendir(void) { DIR *d = opendir(host_path(gstr(arg(0)))); if (!d) { ret(0); return; } for (int i = 0; i < 16; i++) if (!dtab[i]) { dtab[i] = d; dbuf[i] = gm_malloc(300); ret(0xD1D00000u + i); return; } closedir(d); ret(0); }
static void h_readdir(void) { int i = arg(0) - 0xD1D00000u; if (i < 0 || i >= 16 || !dtab[i]) { ret(0); return; } struct dirent *e = readdir(dtab[i]); if (!e) { ret(0); return; }
    gptr b = dbuf[i]; memset(H(b), 0, 300); wr32(b, (u32)e->d_ino); *(uint16_t *)H(b + 4) = 12 + 256; *(uint8_t *)H(b + 6) = e->d_type == DT_DIR ? 4 : 8; *(uint8_t *)H(b + 7) = (uint8_t)strlen(e->d_name); strncpy((char *)H(b + 8), e->d_name, 255); ret(b); }
static void h_closedir(void) { int i = arg(0) - 0xD1D00000u; if (i >= 0 && i < 16 && dtab[i]) { closedir(dtab[i]); dtab[i] = NULL; gm_free(dbuf[i]); } ret(0); }
/* ---- OpenAL: silent stubs with valid handles (real mixer comes with the audio milestone) ---- */
static u32 al_next = 1;
static void h_alc_open(void) { ret(1); } static void h_alc_ctx(void) { ret(1); }
static void h_al_gen(void) { u32 n = arg(0); for (u32 i = 0; i < n; i++) wr32(arg(1) + 4 * i, al_next++); }
static void h_al_getsourcei(void) { if (arg(2)) wr32(arg(2), 0x1014); }   /* AL_STOPPED */
static void h_al_getbufferi(void) { if (arg(2)) wr32(arg(2), 0); }
static gptr cls_URLx;
static gptr make_url(const char *path) { if (!cls_URLx) cls_URLx = objc_class_object("NSURL"); gptr o = gm_malloc(32); wr32(o, cls_URLx); wr32(o + 8, gm_strdup(path)); return o; }
static void h_CFRetain(void) { ret(arg(0)); }
static void h_CFURLFromFSRep(void) { char b[1024]; u32 n = arg(2); if (n > 1023) n = 1023; memcpy(b, H(arg(1)), n); b[n] = 0; ret(make_url(b)); }
static void h_CFURLWithFSPath(void) { ret(make_url(scstr_ex(arg(1)))); }
static void h_CFURLGetFSRep(void) { gptr u = arg(0); const char *p = gstr(rd32(u + 8)); u32 mx = arg(3); if (strlen(p) + 1 > mx) { ret(0); return; } strcpy((char *)H(arg(2)), p); ret(1); }
static void h_CFURLWithString(void) { ret(make_url(scstr_ex(arg(1)))); }
static void h_srandom(void) { srand(arg(0)); }

/* ---------- GCD: blocks are queued and run from the main loop (host cannot nest emulation) ---------- */
typedef struct { gptr block; double due; } Job;
static Job jobs[1024]; static int njobs;
static gptr s_mainq, s_globalq;
/* Block ABI: {isa, flags, reserved, invoke, descriptor{reserved, size,...}}; stack blocks must be copied to the heap */
static gptr block_copy(gptr b) {
    if (!b || b >= ARENA_SIZE - 32) return b;
    u32 flags = rd32(b + 4); if (flags & (1u << 28)) return b;               /* BLOCK_IS_GLOBAL */
    gptr d = rd32(b + 16); u32 sz = (d && d < ARENA_SIZE - 16) ? rd32(d + 4) : 20; if (sz < 20 || sz > 4096) sz = 20;
    gptr c = gm_malloc(sz); memcpy(H(c), H(b), sz);
    if ((flags & (1u << 25)) && d) { gptr copyfn = rd32(d + 8); if (copyfn) { u32 a[2] = { c, b }; call_guest(copyfn, 2, a); } }   /* retain/copy captured objects, blocks, __block vars */
    return c;
}
static void enqueue(gptr block, double delay) {
    if (g_trace) fprintf(stderr, "[gcd] enqueue block=%08x isa=%08x flags=%08x invoke=%08x desc=%08x lr=%08x\n", block, rd32(block), rd32(block + 4), rd32(block + 12), rd32(block + 16), reg(14));
    block = block_copy(block);
    if (njobs < 1024 && block) { jobs[njobs].block = block; jobs[njobs].due = (double)clock() / CLOCKS_PER_SEC + delay; njobs++; }
}
static void h_get_main_queue(void) { if (!s_mainq) s_mainq = alloc_fake(16); ret(s_mainq); }
static void h_get_global_queue(void) { if (!s_globalq) s_globalq = alloc_fake(16); ret(s_globalq); }
static void h_dispatch_async(void) { enqueue(arg(1), 0); }
static void h_dispatch_sync(void) { gptr blk = arg(1); u32 a[1] = { blk }; call_guest(rd32(blk + 12), 1, a); }
static void h_dispatch_once(void) { gptr pred = arg(0), blk = arg(1); if (rd32(pred)) return; wr32(pred, ~0u); u32 a[1] = { blk }; call_guest(rd32(blk + 12), 1, a); }   /* synchronous, via a nested engine */
static void h_dispatch_after(void) { u64 when = arg64(0); double d = (double)when / 1e9; if (when == 0 || d > 1e6) d = 0; enqueue(arg(3), d > 0 && d < 3600 ? d : 0); }   /* dispatch_time_t is relative-ish; approximate */
static void h_queue_create(void) { ret(alloc_fake(16)); }
static const HleEntry extra_tab[] = {
    {"CC_SHA1", h_sha1}, {"CC_SHA256", h_sha256}, {"CFUUIDCreate", h_CFUUIDCreate}, {"CFUUIDCreateString", h_CFUUIDCreateString},
    {"CFStringGetFastestEncoding", h_CFStringGetFastestEncoding}, {"CFStringGetCStringPtr", h_CFStringGetCStringPtr}, {"CFStringGetCString", h_CFStringGetCString}, {"CFStringGetLength", h_CFStringGetLength},
    {"NSLogv", h_NSLogv}, {"if_nametoindex", h_one}, {"SCNetworkReachabilityCreateWithName", h_one}, {"SCNetworkReachabilitySetCallback", h_one}, {"SCNetworkReachabilityScheduleWithRunLoop", h_one},
    {"setxattr", h_zero}, {"CFRetain", h_CFRetain}, {"CFURLCreateFromFileSystemRepresentation", h_CFURLFromFSRep}, {"CFURLCreateWithFileSystemPath", h_CFURLWithFSPath}, {"CFURLGetFileSystemRepresentation", h_CFURLGetFSRep}, {"CFURLCreateWithString", h_CFURLWithString}, {"sysctl", h_sysctl}, {"uname", h_uname}, {"CFStreamCreatePairWithSocketToHost", h_noop}, {"srandom", h_srandom}, {"srand", h_srandom},
    {"dispatch_get_main_queue", h_get_main_queue}, {"dispatch_get_global_queue", h_get_global_queue}, {"dispatch_get_current_queue", h_get_main_queue},
    {"dispatch_async", h_dispatch_async}, {"dispatch_sync", h_dispatch_sync}, {"dispatch_once", h_dispatch_once}, {"dispatch_after", h_dispatch_after},
    {"dispatch_queue_create", h_queue_create}, {"dispatch_release", h_noop}, {"dispatch_retain", h_noop},
    {"_Block_copy", h_zero}, {"_Block_release", h_noop}, {"_Block_object_assign", h_block_object_assign}, {"_Block_object_dispose", h_block_object_dispose},
    {"opendir", h_opendir}, {"readdir", h_readdir}, {"closedir", h_closedir},
    {"alcOpenDevice", h_alc_open}, {"alcCreateContext", h_alc_ctx}, {"alcGetCurrentContext", h_alc_ctx}, {"alcMakeContextCurrent", h_one}, {"alcProcessContext", h_noop}, {"alcSuspendContext", h_noop}, {"alcDestroyContext", h_noop}, {"alcCloseDevice", h_one}, {"alcGetError", h_zero},
    {"alGenSources", h_al_gen}, {"alGenBuffers", h_al_gen}, {"alDeleteSources", h_noop}, {"alDeleteBuffers", h_noop}, {"alGetError", h_zero}, {"alGetSourcei", h_al_getsourcei}, {"alGetBufferi", h_al_getbufferi},
    {"alSourcei", h_noop}, {"alSourcef", h_noop}, {"alSourcefv", h_noop}, {"alSource3f", h_noop}, {"alSourcePlay", h_noop}, {"alSourceStop", h_noop}, {"alSourcePause", h_noop}, {"alSourceRewind", h_noop}, {"alSourceQueueBuffers", h_noop}, {"alSourceUnqueueBuffers", h_noop},
    {"alListenerfv", h_noop}, {"alListenerf", h_noop}, {"alListener3f", h_noop}, {"alDistanceModel", h_noop}, {"alBufferData", h_noop}, {"alIsBuffer", h_one}, {"alIsSource", h_one},
    {NULL, NULL}
};
static void h_block_copy(void) { ret(block_copy(arg(0))); }
/* run due GCD jobs from the host main loop; returns number executed */
int extra_drain_dispatch(void) {
    int ran = 0; double now = (double)clock() / CLOCKS_PER_SEC;
    for (int i = 0; i < njobs; i++) if (jobs[i].block && jobs[i].due <= now) {
        gptr b = jobs[i].block; jobs[i].block = 0; u32 a[1] = { b }; call_guest(rd32(b + 12), 1, a); ran++;
    }
    int w = 0; for (int i = 0; i < njobs; i++) if (jobs[i].block) jobs[w++] = jobs[i]; njobs = w;
    return ran;
}
void extra_init(void) { hle_register_table(extra_tab); static const HleEntry t2[] = {{"_Block_copy", h_block_copy}, {NULL, NULL}}; hle_register_table(t2); }
