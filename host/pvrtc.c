/* pvrtc.c - PVRTC v1 (2bpp / 4bpp) software decoder -> RGBA8 (desktop GPUs lack PVRTC) */
#include "ra.h"
static uint32_t morton(uint32_t x, uint32_t y) { uint32_t r = 0; for (int i = 0; i < 16; i++) { r |= ((y >> i) & 1) << (2 * i); r |= ((x >> i) & 1) << (2 * i + 1); } return r; }
/* sizes must be power of two (PVRTC1 requirement) */
static void unpack(uint16_t c, int *r, int *g, int *b, int *a, int isB) {
    if (c & 0x8000) { /* opaque: RGB555 (A) / RGB555 (B) */
        *a = 255 << 0; *r = (c >> 10) & 31; *g = (c >> 5) & 31; *b = isB ? (c & 31) : (c & 30) | ((c & 30) >> 4);
        if (isB) { *r = (*r << 3) | (*r >> 2); *g = (*g << 3) | (*g >> 2); *b = (*b << 3) | (*b >> 2); } else { *r = (*r << 3) | (*r >> 2); *g = (*g << 3) | (*g >> 2); *b = (*b << 3) | (*b >> 2); }
        if (isB) { *b = (c & 31); *b = (*b << 3) | (*b >> 2); } else { int bb = (c & 30) >> 1; *b = (bb << 4) | (bb); *b = ((c & 0x1E) << 3) | ((c & 0x1E) >> 1); }
        *a = 255;
    } else { /* translucent: RGB444 + A3 */
        int aa = (c >> 12) & 7; *a = (aa << 5) | (aa << 2) | (aa >> 1);
        *r = ((c >> 8) & 15); *g = ((c >> 4) & 15); *b = c & (isB ? 15 : 14); *r = (*r << 4) | *r; *g = (*g << 4) | *g; *b = isB ? (*b << 4) | *b : ((*b << 4) | (*b << 1) | (*b >> 3));
    }
}
void pvrtc_decode(const uint8_t *src, int w, int h, int two, uint8_t *dst) {
    int bw = two ? 8 : 4, bh = 4; int nbx = w / bw, nby = h / bh; if (nbx < 2) nbx = 2; if (nby < 2) nby = 2;
    const uint32_t *blk = (const uint32_t *)src;
    for (int y = 0; y < h; y++) for (int x = 0; x < w; x++) {
        /* bilinear-interpolated colour A/B from the 4 neighbouring blocks (wrap) */
        int bx = (x - bw / 2 + w) % w, by = (y - bh / 2 + h) % h;
        int bx0 = (bx / bw) % nbx, by0 = (by / bh) % nby, bx1 = (bx0 + 1) % nbx, by1 = (by0 + 1) % nby;
        int fx = ((x + bw / 2) % bw), fy = ((y + bh / 2) % bh);
        int idx[4][2] = {{bx0, by0}, {bx1, by0}, {bx0, by1}, {bx1, by1}};
        int wgt[4] = {(bw - fx) * (bh - fy), fx * (bh - fy), (bw - fx) * fy, fx * fy};
        int sa[4] = {0}, sb[4] = {0}; /* r g b a accumulators */
        long A[4] = {0}, B[4] = {0};
        for (int k = 0; k < 4; k++) {
            uint32_t m = morton(idx[k][0], idx[k][1]); const uint32_t *p = blk + 2 * m; uint32_t col = p[1];
            int ar, ag, ab, aa, br, bg, bb, ba; unpack(col & 0xFFFF, &ar, &ag, &ab, &aa, 0); unpack(col >> 16, &br, &bg, &bb, &ba, 1);
            A[0] += ar * wgt[k]; A[1] += ag * wgt[k]; A[2] += ab * wgt[k]; A[3] += aa * wgt[k]; B[0] += br * wgt[k]; B[1] += bg * wgt[k]; B[2] += bb * wgt[k]; B[3] += ba * wgt[k];
        } (void)sa; (void)sb;
        int tot = bw * bh;
        /* modulation of the pixel's own block */
        int cbx = (x / bw) % nbx, cby = (y / bh) % nby; uint32_t m = morton(cbx, cby); uint32_t mod = blk[2 * m];
        int lx = x % bw, ly = y % bh; int mv;
        if (!two) { mv = (mod >> ((ly * 4 + lx) * 2)) & 3; int wB = mv == 0 ? 0 : mv == 1 ? 3 : mv == 2 ? 5 : 8; int wA = 8 - wB;
            for (int c = 0; c < 4; c++) dst[(y * w + x) * 4 + c] = (uint8_t)(((A[c] / tot) * wA + (B[c] / tot) * wB) / 8); }
        else { mv = (mod >> ((ly * 8 + lx) * 1 % 32)) & 1; int wB = mv ? 8 : 0; int wA = 8 - wB;
            for (int c = 0; c < 4; c++) dst[(y * w + x) * 4 + c] = (uint8_t)(((A[c] / tot) * wA + (B[c] / tot) * wB) / 8); }
    }
}
