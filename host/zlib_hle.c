/* zlib_hle.c - guest z_stream (32-bit layout) <-> host zlib. The guest's z_stream lives in guest memory; the real z_stream state lives here. */
#include "ra.h"
#include <zlib.h>
typedef struct { gptr g; z_stream z; int inited, deflating; } ZS; static ZS zs[64];
static ZS *zfind(gptr g, int create) { for (int i = 0; i < 64; i++) if (zs[i].inited && zs[i].g == g) return &zs[i]; if (!create) return NULL; for (int i = 0; i < 64; i++) if (!zs[i].inited) { memset(&zs[i], 0, sizeof zs[i]); zs[i].g = g; zs[i].inited = 1; return &zs[i]; } return NULL; }
/* guest z_stream: next_in@0 avail_in@4 total_in@8 next_out@12 avail_out@16 total_out@20 msg@24 state@28 zalloc@32 zfree@36 opaque@40 data_type@44 adler@48 reserved@52 */
static void in_sync(ZS *s) { s->z.next_in = rd32(s->g) ? (Bytef *)H(rd32(s->g)) : NULL; s->z.avail_in = rd32(s->g + 4); s->z.next_out = rd32(s->g + 12) ? (Bytef *)H(rd32(s->g + 12)) : NULL; s->z.avail_out = rd32(s->g + 16); }
static void out_sync(ZS *s) { wr32(s->g, s->z.next_in ? G(s->z.next_in) : 0); wr32(s->g + 4, s->z.avail_in); wr32(s->g + 8, (u32)s->z.total_in); wr32(s->g + 12, s->z.next_out ? G(s->z.next_out) : 0); wr32(s->g + 16, s->z.avail_out); wr32(s->g + 20, (u32)s->z.total_out); wr32(s->g + 24, 0); wr32(s->g + 28, 1); wr32(s->g + 48, (u32)s->z.adler); }
static void h_inflateInit_(void) { ZS *s = zfind(arg(0), 1); if (!s) { ret((u32)-2); return; } ret((u32)inflateInit(&s->z)); }
static void h_inflateInit2_(void) { ZS *s = zfind(arg(0), 1); if (!s) { ret((u32)-2); return; } ret((u32)inflateInit2(&s->z, (int)arg(1))); }
static void h_inflate(void) { ZS *s = zfind(arg(0), 0); if (!s) { ret((u32)-2); return; } in_sync(s); int r = inflate(&s->z, (int)arg(1)); out_sync(s); ret((u32)r); }
static void h_inflateEnd(void) { ZS *s = zfind(arg(0), 0); if (!s) { ret(0); return; } int r = inflateEnd(&s->z); s->inited = 0; ret((u32)r); }
static void h_inflateReset(void) { ZS *s = zfind(arg(0), 0); ret(s ? (u32)inflateReset(&s->z) : (u32)-2); }
static void h_deflateInit_(void) { ZS *s = zfind(arg(0), 1); if (!s) { ret((u32)-2); return; } s->deflating = 1; ret((u32)deflateInit(&s->z, (int)arg(1))); }
static void h_deflateInit2_(void) { ZS *s = zfind(arg(0), 1); if (!s) { ret((u32)-2); return; } s->deflating = 1; ret((u32)deflateInit2(&s->z, (int)arg(1), (int)arg(2), (int)arg(3), (int)arg(4), (int)arg(5))); }
static void h_deflate(void) { ZS *s = zfind(arg(0), 0); if (!s) { ret((u32)-2); return; } in_sync(s); int r = deflate(&s->z, (int)arg(1)); out_sync(s); ret((u32)r); }
static void h_deflateEnd(void) { ZS *s = zfind(arg(0), 0); if (!s) { ret(0); return; } int r = deflateEnd(&s->z); s->inited = 0; ret((u32)r); }
static void h_crc32(void) { ret((u32)crc32(arg(0), arg(1) ? (const Bytef *)H(arg(1)) : NULL, arg(2))); }
static void h_adler32(void) { ret((u32)adler32(arg(0), arg(1) ? (const Bytef *)H(arg(1)) : NULL, arg(2))); }
static void h_uncompress(void) { uLongf dl = rd32(arg(1)); int r = uncompress((Bytef *)H(arg(0)), &dl, (const Bytef *)H(arg(2)), arg(3)); wr32(arg(1), (u32)dl); ret((u32)r); }
static void h_compress(void) { uLongf dl = rd32(arg(1)); int r = compress((Bytef *)H(arg(0)), &dl, (const Bytef *)H(arg(2)), arg(3)); wr32(arg(1), (u32)dl); ret((u32)r); }
static const HleEntry zlib_tab[] = { {"inflateInit_", h_inflateInit_}, {"inflateInit2_", h_inflateInit2_}, {"inflate", h_inflate}, {"inflateEnd", h_inflateEnd}, {"inflateReset", h_inflateReset},
    {"deflateInit_", h_deflateInit_}, {"deflateInit2_", h_deflateInit2_}, {"deflate", h_deflate}, {"deflateEnd", h_deflateEnd}, {"crc32", h_crc32}, {"adler32", h_adler32}, {"uncompress", h_uncompress}, {"compress", h_compress}, {NULL, NULL} };
void zlib_init(void) { hle_register_table(zlib_tab); }
