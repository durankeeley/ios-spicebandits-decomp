/* uikit.inc - a small UIKit: view tree, images (incl. Apple CgBI PNGs), text, NIBArchive loader, hit-testing, compositor.
 * Included into objc.c (shares its helpers: new_string, array_*, send_guest, find_gcls_name, ...). */
#include <SDL2/SDL_ttf.h>
#include <SDL2/SDL_image.h>
#include <zlib.h>
#include <GL/gl.h>
#include <dirent.h>

/* ------------------------------------------------------------------ images ------------------------------------------------------------------ */
typedef struct { char name[200]; GLuint tex; uint8_t *rgba; int w, h; } ImageRec;
static ImageRec images[4096]; static int nimg;
static gptr cls_UIImage, cls_UIColor, cls_UIFont, cls_NSCoder, cls_UIView;

static uint32_t be32p(const uint8_t *p) { return (uint32_t)p[0] << 24 | p[1] << 16 | p[2] << 8 | p[3]; }
static int paeth(int a, int b, int c) { int p = a + b - c, pa = abs(p - a), pb = abs(p - b), pc = abs(p - c); return (pa <= pb && pa <= pc) ? a : (pb <= pc ? b : c); }

/* PNG decoder that also understands Apple's CgBI variant (raw deflate, BGRA, premultiplied). Returns malloc'd RGBA8. */
static uint8_t *png_decode(const uint8_t *d, size_t n, int *ow, int *oh) {
    if (n < 33 || memcmp(d, "\x89PNG\r\n\x1a\n", 8)) return NULL;
    size_t o = 8; int w = 0, h = 0, depth = 0, ctype = 0, cgbi = 0, interlace = 0; uint8_t *idat = NULL; size_t idl = 0; uint8_t pal[768] = {0}; uint8_t trns[256]; memset(trns, 255, sizeof trns);
    while (o + 12 <= n) {
        uint32_t len = be32p(d + o); const uint8_t *t = d + o + 4, *c = d + o + 8; if (o + 12 + len > n) break;
        if (!memcmp(t, "CgBI", 4)) cgbi = 1;
        else if (!memcmp(t, "IHDR", 4)) { w = be32p(c); h = be32p(c + 4); depth = c[8]; ctype = c[9]; interlace = c[12]; }
        else if (!memcmp(t, "PLTE", 4)) memcpy(pal, c, len > 768 ? 768 : len);
        else if (!memcmp(t, "tRNS", 4) && ctype == 3) memcpy(trns, c, len > 256 ? 256 : len);
        else if (!memcmp(t, "IDAT", 4)) { idat = realloc(idat, idl + len); memcpy(idat + idl, c, len); idl += len; }
        else if (!memcmp(t, "IEND", 4)) break;
        o += 12 + len;
    }
    if (!w || !h || depth != 8 || interlace) { free(idat); return NULL; }
    int ch = ctype == 6 ? 4 : ctype == 2 ? 3 : ctype == 4 ? 2 : 1; size_t stride = (size_t)w * ch, rawlen = (stride + 1) * h;
    uint8_t *raw = malloc(rawlen); z_stream z; memset(&z, 0, sizeof z);
    if (inflateInit2(&z, cgbi ? -15 : 15) != Z_OK) { free(idat); free(raw); return NULL; }
    z.next_in = idat; z.avail_in = idl; z.next_out = raw; z.avail_out = rawlen; inflate(&z, Z_FINISH); inflateEnd(&z); free(idat);
    uint8_t *px = calloc((size_t)w * h, ch), *out = malloc((size_t)w * h * 4);
    for (int y = 0; y < h; y++) {
        uint8_t *row = px + (size_t)y * stride, *prev = y ? row - stride : NULL; const uint8_t *src = raw + (size_t)y * (stride + 1) + 1; int f = raw[(size_t)y * (stride + 1)];
        for (size_t x = 0; x < stride; x++) {
            int a = x >= (size_t)ch ? row[x - ch] : 0, b = prev ? prev[x] : 0, c = (prev && x >= (size_t)ch) ? prev[x - ch] : 0, v = src[x];
            switch (f) { case 1: v += a; break; case 2: v += b; break; case 3: v += (a + b) / 2; break; case 4: v += paeth(a, b, c); break; }
            row[x] = (uint8_t)v;
        }
    }
    free(raw);
    for (int i = 0; i < w * h; i++) {
        uint8_t r, g, b, a = 255; const uint8_t *s = px + (size_t)i * ch;
        if (ctype == 6) { r = s[0]; g = s[1]; b = s[2]; a = s[3]; } else if (ctype == 2) { r = s[0]; g = s[1]; b = s[2]; }
        else if (ctype == 4) { r = g = b = s[0]; a = s[1]; } else if (ctype == 3) { r = pal[s[0] * 3]; g = pal[s[0] * 3 + 1]; b = pal[s[0] * 3 + 2]; a = trns[s[0]]; } else { r = g = b = s[0]; }
        if (cgbi) { uint8_t t = r; r = b; b = t; if (a && a < 255) { r = r * 255 / a > 255 ? 255 : r * 255 / a; g = g * 255 / a > 255 ? 255 : g * 255 / a; b = b * 255 / a > 255 ? 255 : b * 255 / a; } }
        out[i * 4] = r; out[i * 4 + 1] = g; out[i * 4 + 2] = b; out[i * 4 + 3] = a;
    }
    free(px); *ow = w; *oh = h; return out;
}

static uint8_t *load_rgba(const char *path, int *w, int *h) {
    FILE *f = fopen(path, "rb"); if (!f) return NULL; fseek(f, 0, SEEK_END); long n = ftell(f); fseek(f, 0, SEEK_SET);
    uint8_t *d = malloc(n); if (fread(d, 1, n, f) != (size_t)n) { fclose(f); free(d); return NULL; } fclose(f);
    uint8_t *r = png_decode(d, n, w, h);
    if (!r) {                                               /* JPEG etc. via SDL_image */
        SDL_RWops *rw = SDL_RWFromConstMem(d, (int)n); SDL_Surface *s = IMG_Load_RW(rw, 1);
        if (s) { SDL_Surface *c = SDL_ConvertSurfaceFormat(s, SDL_PIXELFORMAT_ABGR8888, 0); SDL_FreeSurface(s); if (c) { *w = c->w; *h = c->h; r = malloc((size_t)c->w * c->h * 4); for (int y = 0; y < c->h; y++) memcpy(r + (size_t)y * c->w * 4, (uint8_t *)c->pixels + (size_t)y * c->pitch, c->w * 4); SDL_FreeSurface(c); } }
    }
    free(d); return r;
}
static gptr new_image(int idx) { gptr o = new_obj(cls_UIImage); wr32(o + 4, idx + 1); wr32(o + 8, images[idx].w); wr32(o + 12, images[idx].h); return o; }
static gptr image_named(const char *name) {
    if (!name || !*name) return 0;
    for (int i = 0; i < nimg; i++) if (!strcmp(images[i].name, name)) return new_image(i);
    static const char *ext[] = {"", ".png", ".jpg", ".jpeg", NULL}; static const char *dirs[] = {"", "gui/", "sfx/", NULL}; char path[1024]; uint8_t *px = NULL; int w = 0, h = 0;
    for (int d = 0; dirs[d] && !px; d++) for (int e = 0; ext[e] && !px; e++) { snprintf(path, sizeof path, "%s/%s%s%s", g_bundle_dir, dirs[d], name, ext[e]); struct stat st; if (!stat(path, &st) && S_ISREG(st.st_mode)) px = load_rgba(path, &w, &h); }
    if (!px || nimg >= 4096) { hle_log("[ui] image not found: %s\n", name); return 0; }
    if (getenv("RA_IMGLOG")) fprintf(stderr, "[img] %s\n", name); strncpy(images[nimg].name, name, 199); images[nimg].rgba = px; images[nimg].w = w; images[nimg].h = h; return new_image(nimg++);
}
static gptr image_from_path(const char *path) {
    for (int i = 0; i < nimg; i++) if (!strcmp(images[i].name, path)) return new_image(i);
    int w, h; uint8_t *px = load_rgba(path, &w, &h); if (!px || nimg >= 4096) return 0;
    if (getenv("RA_IMGLOG")) fprintf(stderr, "[img] %s\n", path); strncpy(images[nimg].name, path, 199); images[nimg].rgba = px; images[nimg].w = w; images[nimg].h = h; return new_image(nimg++);
}
static ImageRec *img_of(gptr o) { if (!o || o >= ARENA_SIZE - 16 || rd32(o) != cls_UIImage) return NULL; u32 i = rd32(o + 4); return i && i <= (u32)nimg ? &images[i - 1] : NULL; }
static GLuint image_tex(ImageRec *im) {
    if (!im) return 0;
    if (!im->tex && im->rgba) { glGenTextures(1, &im->tex); glBindTexture(GL_TEXTURE_2D, im->tex); glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, im->w, im->h, 0, GL_RGBA, GL_UNSIGNED_BYTE, im->rgba);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR); glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, 0x812F); glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, 0x812F); }
    return im->tex;
}

/* ------------------------------------------------------------------ colors / fonts ------------------------------------------------------------------ */
static gptr new_color(float r, float g, float b, float a) { gptr o = new_obj(cls_UIColor); wr32(o + 4, w_of(r)); wr32(o + 8, w_of(g)); wr32(o + 12, w_of(b)); wr32(o + 16 - 16 + 0, rd32(o)); /* isa kept */ wr32(gm_malloc(16), 0); gptr ex = gm_malloc(16); wr32(ex, w_of(a)); wr32(o, cls_UIColor); (void)ex; return o; }
/* UIColor layout (24 bytes): {isa, r, g, b, a} - allocate 32 */
static gptr color_obj(float r, float g, float b, float a) { gptr o = gm_malloc(32); wr32(o, cls_UIColor); wr32(o + 4, w_of(r)); wr32(o + 8, w_of(g)); wr32(o + 12, w_of(b)); wr32(o + 16, w_of(a)); return o; }
static void color_get(gptr c, float out[4]) { if (c && c < ARENA_SIZE - 32 && rd32(c) == cls_UIColor) { for (int i = 0; i < 4; i++) out[i] = f_of(rd32(c + 4 + 4 * i)); } else { out[0] = out[1] = out[2] = 0; out[3] = 0; } }
static gptr font_obj(const char *name, float size) { gptr o = gm_malloc(32); wr32(o, cls_UIFont); wr32(o + 4, gm_strdup(name ? name : "Helvetica")); wr32(o + 8, w_of(size)); return o; }

static TTF_Font *font_for(const char *name, float size) {
    static struct { char key[200]; TTF_Font *f; } cache[64]; static int nc, inited;
    if (!inited) { TTF_Init(); inited = 1; }
    char key[200]; snprintf(key, sizeof key, "%s@%d", name ? name : "", (int)(size + 0.5f));
    for (int i = 0; i < nc; i++) if (!strcmp(cache[i].key, key)) return cache[i].f;
    /* resolve: bundle fonts/ by (case-insensitive, space/dash-insensitive) name, else Liberation Sans */
    char path[1024] = "", want[128] = ""; int bold = name && (strcasestr(name, "bold") != NULL), ital = name && (strcasestr(name, "italic") || strcasestr(name, "oblique"));
    for (const char *p = name ? name : ""; *p && strlen(want) < 120; p++) if (*p != ' ' && *p != '-' && *p != '_') want[strlen(want)] = tolower(*p);
    char dir[1024]; snprintf(dir, sizeof dir, "%s/fonts", g_bundle_dir); DIR *dp = opendir(dir); struct dirent *de; int best = 0;
    while (dp && (de = readdir(dp))) {
        char norm[256] = ""; for (const char *p = de->d_name; *p && *p != '.' && strlen(norm) < 250; p++) if (*p != ' ' && *p != '-' && *p != '_') norm[strlen(norm)] = tolower(*p);
        int score = 0; if (!strncmp(want, norm, strlen(norm)) && *norm) score = (int)strlen(norm); if (!strcmp(want, norm)) score = 1000;
        if (score > best) { best = score; snprintf(path, sizeof path, "%s/%s", dir, de->d_name); }
    }
    if (dp) closedir(dp);
    if (!*path || best < 4) snprintf(path, sizeof path, "/usr/share/fonts/liberation/Liberation%s-%s.ttf", "Sans", bold ? (ital ? "BoldItalic" : "Bold") : (ital ? "Italic" : "Regular"));
    TTF_Font *f = TTF_OpenFont(path, size < 4 ? 4 : (int)(size + 0.5f)); if (!f) f = TTF_OpenFont("/usr/share/fonts/TTF/DejaVuSans.ttf", (int)(size + 0.5f));
    if (nc < 64) { strcpy(cache[nc].key, key); cache[nc++].f = f; }
    return f;
}

typedef struct { char key[400]; GLuint tex; int w, h; unsigned stamp; } TextTex;
static TextTex ttex[1024]; static int nttex; static unsigned tstamp;
static GLuint text_texture(const char *text, const char *fname, float fsize, const float col[4], int wrapw, int *ow, int *oh) {
    if (!text || !*text) return 0;
    char key[400]; snprintf(key, sizeof key, "%s|%s|%.1f|%02x%02x%02x|%d", text, fname ? fname : "", fsize, (int)(col[0] * 255), (int)(col[1] * 255), (int)(col[2] * 255), wrapw);
    for (int i = 0; i < nttex; i++) if (!strcmp(ttex[i].key, key)) { ttex[i].stamp = ++tstamp; *ow = ttex[i].w; *oh = ttex[i].h; return ttex[i].tex; }
    TTF_Font *f = font_for(fname, fsize); if (!f) return 0;
    SDL_Color c = {(Uint8)(col[0] * 255), (Uint8)(col[1] * 255), (Uint8)(col[2] * 255), 255};
    SDL_Surface *s = wrapw > 0 ? TTF_RenderUTF8_Blended_Wrapped(f, text, c, wrapw) : TTF_RenderUTF8_Blended(f, text, c); if (!s) return 0;
    SDL_Surface *r = SDL_ConvertSurfaceFormat(s, SDL_PIXELFORMAT_ABGR8888, 0); SDL_FreeSurface(s); if (!r) return 0;
    int slot = -1; if (nttex < 1024) slot = nttex++; else { unsigned best = ~0u; for (int i = 0; i < 1024; i++) if (ttex[i].stamp < best) { best = ttex[i].stamp; slot = i; } glDeleteTextures(1, &ttex[slot].tex); }
    glGenTextures(1, &ttex[slot].tex); glBindTexture(GL_TEXTURE_2D, ttex[slot].tex); glPixelStorei(GL_UNPACK_ROW_LENGTH, r->pitch / 4);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, r->w, r->h, 0, GL_RGBA, GL_UNSIGNED_BYTE, r->pixels); glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR); glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    strncpy(ttex[slot].key, key, 399); ttex[slot].w = r->w; ttex[slot].h = r->h; ttex[slot].stamp = ++tstamp; *ow = r->w; *oh = r->h; SDL_FreeSurface(r); return ttex[slot].tex;
}

/* ------------------------------------------------------------------ views ------------------------------------------------------------------ */
enum { K_VIEW, K_IMAGEVIEW, K_LABEL, K_BUTTON, K_SCROLL, K_WINDOW, K_SPINNER, K_TEXTFIELD };
typedef struct Action { gptr target; char sel[64]; int ev; } Action;
typedef struct View {
    gptr obj; struct View *parent; struct View **kids; int nk, ck; int kind;
    float bx, by, bw, bh, cx, cy; float ta, tb, tc_, td, ttx, tty; float alpha; int hidden, tag, interact, clips, hasbg, is_gl, highlighted, selected, disabled, enabled_set;
    float bg[4]; gptr image, himage; char *text; char font[64]; float fsize, tcol[4]; int align, lines;
    gptr bimg[4], bbg[4]; char *btitle[4]; float btc[4][4]; Action act[8]; int nact; float sx, sy, cw, ch; gptr gr_target[4]; char gr_sel[4][64]; int ngr; gptr delegate; gptr layer; int dirty; int mask; int noautosub;
} View;
static View *vtab[8192]; static int nvt;
static View *g_window;                 /* key window */
static View *view_lookup(gptr obj) { for (int i = 0; i < nvt; i++) if (vtab[i]->obj == obj) return vtab[i]; return NULL; }
static View *view_of(gptr obj) {
    if (!obj || obj >= ARENA_SIZE - 16) return NULL; View *v = view_lookup(obj); if (v) return v;
    if (nvt >= 8192) return NULL;
    v = calloc(1, sizeof *v); v->obj = obj; v->alpha = 1; v->interact = 1; v->ta = v->td = 1; v->fsize = 17; v->bw = v->bh = 0; strcpy(v->font, "Helvetica"); v->tcol[3] = 1; v->kind = K_VIEW;
    v->bx = v->by = 0; v->bw = 320; v->bh = 480; v->cx = 160; v->cy = 240; vtab[nvt++] = v;
    gptr cls = rd32(obj); const char *cn = is_fake_class(cls) ? fake_name(cls) : ""; GCls *g = find_gcls(cls); for (int guard = 0; g && guard < 16; guard++) { cn = gstr(0); GCls *sg = find_gcls(g->super); if (!sg) { cn = is_fake_class(g->super) ? fake_name(g->super) : ""; break; } g = sg; }
    if (!strcmp(cn, "UIImageView")) v->kind = K_IMAGEVIEW; else if (!strcmp(cn, "UILabel")) v->kind = K_LABEL; else if (strstr(cn, "Button")) v->kind = K_BUTTON; else if (!strcmp(cn, "UIScrollView")) v->kind = K_SCROLL;
    else if (!strcmp(cn, "UIWindow")) v->kind = K_WINDOW; else if (!strcmp(cn, "UIActivityIndicatorView")) v->kind = K_SPINNER; else if (!strcmp(cn, "UITextField")) v->kind = K_TEXTFIELD;
    return v;
}
static void view_remove(View *v) { View *p = v->parent; if (!p) return; for (int i = 0; i < p->nk; i++) if (p->kids[i] == v) { memmove(p->kids + i, p->kids + i + 1, (p->nk - i - 1) * sizeof(View *)); p->nk--; break; } v->parent = NULL; }
static void view_insert(View *p, View *v, int at) {
    if (!p || !v || p == v) return; view_remove(v);
    if (p->nk >= p->ck) { p->ck = p->ck ? p->ck * 2 : 8; p->kids = realloc(p->kids, p->ck * sizeof(View *)); }
    if (at < 0 || at > p->nk) at = p->nk; memmove(p->kids + at + 1, p->kids + at, (p->nk - at) * sizeof(View *)); p->kids[at] = v; p->nk++; v->parent = p; v->dirty = 1; p->dirty = 1;
}
static void v_frame(View *v, float *x, float *y, float *w, float *h) { *w = v->bw; *h = v->bh; *x = v->cx - v->bw / 2; *y = v->cy - v->bh / 2; }
static void v_set_frame(View *v, float x, float y, float w, float h);
/* UIViewAutoresizing: 1 flexible left margin, 2 width, 4 right margin, 8 top margin, 16 height, 32 bottom margin */
static void autoresize_axis(int mask_lo, int mask_sz, int mask_hi, float pos, float size, float oldP, float delta, float *npos, float *nsize) {
    float lo = pos, sz = size, hi = oldP - pos - size; float parts[3] = { (mask_lo ? lo : 0), (mask_sz ? sz : 0), (mask_hi ? hi : 0) }; float tot = parts[0] + parts[1] + parts[2]; int cnt = !!mask_lo + !!mask_sz + !!mask_hi;
    *npos = pos; *nsize = size; if (!cnt) return;
    float f0, f1; if (tot > 0.001f) { f0 = delta * parts[0] / tot; f1 = delta * parts[1] / tot; } else { f0 = mask_lo ? delta / cnt : 0; f1 = mask_sz ? delta / cnt : 0; }
    *npos = pos + f0; *nsize = size + f1;
}
static const char *v_clsname(gptr o);
/* TutorialView sizes its bubble from the measured text and relies on its nib children following (the nib stores autoresizesSubviews=NO). */
static int in_tutorial(View *v) { for (; v; v = v->parent) if (v->obj && !strcmp(v_clsname(v->obj), "TutorialView")) return 1; return 0; }
static void v_set_frame(View *v, float x, float y, float w, float h) {
    float ow = v->bw, oh = v->bh; if (ow != w || oh != h) v->dirty = 1;
    v->bw = w; v->bh = h; v->cx = x + w / 2; v->cy = y + h / 2;
    if ((ow != w || oh != h) && v->nk && (!v->noautosub || in_tutorial(v)) && ow > 0 && oh > 0)
        for (int i = 0; i < v->nk; i++) { View *c = v->kids[i]; if (!c->mask) continue; float cx0 = c->cx - c->bw / 2, cy0 = c->cy - c->bh / 2, nx, nw, ny, nh;
            autoresize_axis(c->mask & 1, c->mask & 2, c->mask & 4, cx0, c->bw, ow, w - ow, &nx, &nw); autoresize_axis(c->mask & 8, c->mask & 16, c->mask & 32, cy0, c->bh, oh, h - oh, &ny, &nh);
            if (nw < 0) nw = 0; if (nh < 0) nh = 0; v_set_frame(c, nx, ny, nw, nh); }
}
static const char *v_clsname(gptr o) { gptr c = rd32(o); GCls *g = find_gcls(c); return g ? g->name : (is_fake_class(c) ? fake_name(c) : "?"); }
static int g_gl_view_marked;
int g_spice_view_loaded;   /* StoreKit answers are held back until the Spice shop screen exists to receive them */

/* image content size in points */
static void image_size(gptr img, float *w, float *h) { ImageRec *im = img_of(img); *w = im ? im->w : 0; *h = im ? im->h : 0; }

/* ------------------------------------------------------------------ compositor ------------------------------------------------------------------ */
typedef struct { float a, b, c, d, tx, ty; } Mat;
static Mat mat_mul(Mat m, Mat n) { /* m applied after n: result = m * n  (point -> n -> m) */ Mat r; r.a = m.a * n.a + m.c * n.b; r.b = m.b * n.a + m.d * n.b; r.c = m.a * n.c + m.c * n.d; r.d = m.b * n.c + m.d * n.d; r.tx = m.a * n.tx + m.c * n.ty + m.tx; r.ty = m.b * n.tx + m.d * n.ty + m.ty; return r; }
static void mat_pt(Mat m, float x, float y, float *ox, float *oy) { *ox = m.a * x + m.c * y + m.tx; *oy = m.b * x + m.d * y + m.ty; }
extern GLuint sdl_fbo_tex(void); extern int g_buf_w, g_buf_h;
static int g_scr_w = 1024, g_scr_h = 768;

static void quad(Mat m, float x0, float y0, float x1, float y1, float u0, float v0, float u1, float v1) {
    float px[4], py[4]; mat_pt(m, x0, y0, &px[0], &py[0]); mat_pt(m, x1, y0, &px[1], &py[1]); mat_pt(m, x1, y1, &px[2], &py[2]); mat_pt(m, x0, y1, &px[3], &py[3]);
    glBegin(GL_QUADS); glTexCoord2f(u0, v0); glVertex2f(px[0], py[0]); glTexCoord2f(u1, v0); glVertex2f(px[1], py[1]); glTexCoord2f(u1, v1); glVertex2f(px[2], py[2]); glTexCoord2f(u0, v1); glVertex2f(px[3], py[3]); glEnd();
}
static void draw_image(Mat m, gptr img, float x, float y, float w, float h, float alpha, int mode) {
    ImageRec *im = img_of(img); if (!im) return; GLuint t = image_tex(im); if (!t) return;
    float iw = im->w, ih = im->h, dx = x, dy = y, dw = w, dh = h;
    if (mode == 1 || mode == 2) { float s = mode == 1 ? fminf(w / iw, h / ih) : fmaxf(w / iw, h / ih); dw = iw * s; dh = ih * s; dx = x + (w - dw) / 2; dy = y + (h - dh) / 2; }   /* AspectFit / AspectFill */
    else if (mode >= 4) { dw = iw; dh = ih; dx = x + (w - iw) / 2; dy = y + (h - ih) / 2; if (mode == 5) dy = y; if (mode == 6) dy = y + h - ih; }
    glBindTexture(GL_TEXTURE_2D, t); glEnable(GL_TEXTURE_2D); glColor4f(1, 1, 1, alpha); quad(m, dx, dy, dx + dw, dy + dh, 0, 0, 1, 1);
}
static void draw_text(Mat m, const char *text, const char *font, float size, const float col[4], float x, float y, float w, float h, int align, int lines, float alpha) {
    int tw = 0, th = 0; GLuint t = text_texture(text, font, size, col, lines != 1 ? (int)w : 0, &tw, &th); if (!t) return;
    float dx = x; if (align == 1) dx = x + (w - tw) / 2; else if (align == 2) dx = x + w - tw; float dy = y + (h - th) / 2; if (lines != 1 && th > h) dy = y;
    glBindTexture(GL_TEXTURE_2D, t); glEnable(GL_TEXTURE_2D); glColor4f(1, 1, 1, col[3] * alpha); quad(m, dx, dy, dx + tw, dy + th, 0, 0, 1, 1);
}
static int has_levelup_note(View *v, int depth) {
    if (v->text && !strncmp(v->text, "You will level up", 17)) return 1;
    if (depth > 0) for (int i = 0; i < v->nk; i++) if (has_levelup_note(v->kids[i], depth - 1)) return 1;
    return 0; }
static void draw_view(View *v, Mat parent, float alpha) {
    if (v->hidden || v->alpha < 0.01f) return;
    /* offline "You will level up when you get online!" bubble (nib placeholder box + stretched label): hide the small container holding it */
    if (v->nk <= 3 && has_levelup_note(v, 1)) return;
    /* developer leftovers authored visible in the nibs (hidden on device by code we do not reproduce): system rounded-rect debug buttons, download-progress placeholders */
    if (v->kind == K_BUTTON && v->obj && !strcmp(v_clsname(v->obj), "UIRoundedRectButton")) return;
    if (v->text && !strncmp(v->text, "progress:", 9)) return;
    /* Spice shop: no store -> no sale stickers, struck-through old prices, bundle note or "must be online" notice */
    if (v->text && (!strncmp(v->text, "You must be online", 18) || !strncmp(v->text, "+ parrot and pipe", 17))) return;
    if (v->text && !strcmp(v->text, "\"Spice must flow!\"")) return;   /* nib placeholder status, hidden unless the player set one */
    if (v->text && v->text[0] == '$' && v->tag >= 17 && v->tag <= 47 && v->tag % 10 == 7) return;
    if (v->kind == K_IMAGEVIEW && v->image && img_of(v->image) && (strstr(img_of(v->image)->name, "sale_") || strstr(img_of(v->image)->name, "spiceshop_parrot"))) return;
    float a = alpha * v->alpha, x, y, w, h; v_frame(v, &x, &y, &w, &h);
    Mat T = {1, 0, 0, 1, v->cx, v->cy}, X = {v->ta, v->tb, v->tc_, v->td, v->ttx, v->tty}, B = {1, 0, 0, 1, -v->bw / 2 - 0, -v->bh / 2 - 0};
    Mat m = mat_mul(parent, mat_mul(T, mat_mul(X, B)));      /* parent * translate(center) * transform * translate(-size/2) */
    int scissor = 0;
    if (v->clips && v->ta == 1 && v->td == 1 && v->tb == 0 && v->tc_ == 0) { float sx0, sy0, sx1, sy1; mat_pt(m, 0, 0, &sx0, &sy0); mat_pt(m, v->bw, v->bh, &sx1, &sy1);
        glEnable(GL_SCISSOR_TEST); glScissor((int)fminf(sx0, sx1), (int)(g_scr_h - fmaxf(sy0, sy1)), (int)fabsf(sx1 - sx0) + 1, (int)fabsf(sy1 - sy0) + 1); scissor = 1; }
    glDisable(GL_TEXTURE_2D);
    if (v->hasbg && v->bg[3] > 0.003f) { glColor4f(v->bg[0], v->bg[1], v->bg[2], v->bg[3] * a); quad(m, 0, 0, v->bw, v->bh, 0, 0, 1, 1); }
    if (v->is_gl && !getenv("RA_NOGLQUAD")) { glBindTexture(GL_TEXTURE_2D, sdl_fbo_tex()); glEnable(GL_TEXTURE_2D); glColor4f(1, 1, 1, a); quad(m, 0, 0, v->bw, v->bh, 0, 1, 1, 0); }
    else if (v->kind == K_IMAGEVIEW) { draw_image(m, v->highlighted && v->himage ? v->himage : v->image, 0, 0, v->bw, v->bh, a, v->enabled_set /* contentMode stored in align slot */ ? 0 : 0); }
    else if (v->kind == K_BUTTON) {
        int st = v->disabled ? 2 : v->highlighted ? 1 : v->selected ? 3 : 0; gptr bg = v->bbg[st] ? v->bbg[st] : v->bbg[0]; gptr im = v->bimg[st] ? v->bimg[st] : v->bimg[0];
        if (bg) draw_image(m, bg, 0, 0, v->bw, v->bh, a, 0);
        if (im) { float iw, ih; image_size(im, &iw, &ih); draw_image(m, im, (v->bw - iw) / 2, (v->bh - ih) / 2, iw, ih, a, 0); }
        const char *title = v->btitle[st] ? v->btitle[st] : v->btitle[0];
        if (title && *title) { const float *c = (v->btc[st][3] > 0) ? v->btc[st] : v->btc[0]; float col[4] = {c[0], c[1], c[2], c[3] > 0 ? c[3] : 1}; draw_text(m, title, v->font, v->fsize, col, 0, 0, v->bw, v->bh, 1, 1, a); }
    } else if (v->kind == K_LABEL && v->text && *v->text) draw_text(m, v->text, v->font, v->fsize, v->tcol, 0, 0, v->bw, v->bh, v->align, v->lines, a);
    Mat cm = m; if (v->kind == K_SCROLL) { Mat o = {1, 0, 0, 1, -v->sx, -v->sy}; cm = mat_mul(m, o); }
    for (int i = 0; i < v->nk; i++) draw_view(v->kids[i], cm, a);
    if (scissor) glDisable(GL_SCISSOR_TEST);
}
static void dump_view(View *v, int d) {
    float x, y, w, h; v_frame(v, &x, &y, &w, &h);
    fprintf(stderr, "%*s%s kind=%d (%.0f,%.0f %.0fx%.0f) a=%.2f%s%s%s tag=%d kids=%d%s%s m=%d%s\n", d * 2, "", v_clsname(v->obj), v->kind, x, y, w, h, v->alpha, v->hidden ? " HIDDEN" : "", v->is_gl ? " GL" : "", v->hasbg ? " bg" : "", v->tag, v->nk,
            v->kind == K_IMAGEVIEW && v->image ? " img" : "", v->text ? " text" : "", v->mask, v->noautosub ? " NOAUTOSUB" : "");
    if (getenv("RA_UIDUMP_TEXT")) { if (v->text) fprintf(stderr, "%*s   text=\"%.60s\"\n", d * 2, "", v->text); if (v->kind == K_IMAGEVIEW && v->image && img_of(v->image)) fprintf(stderr, "%*s   image=%s\n", d * 2, "", img_of(v->image)->name); }
    for (int i = 0; i < v->nk && d < 14; i++) dump_view(v->kids[i], d + 1);
}
void uikit_dump(void) { fprintf(stderr, "[ui] window=%p views=%d\n", (void *)g_window, nvt); if (g_window) dump_view(g_window, 0); }
/* UIKit lays views out before they are first shown (this is when EAGLView sizes its renderbuffer). */
static int g_in_layout;
static void layout_tree(View *v, int depth) {
    if (depth > 24) return;
    if (v->dirty && !v->hidden) { v->dirty = 0; if (guest_responds(v->obj, "layoutSubviews")) { g_in_layout++; send_guest(v->obj, "layoutSubviews", 0, NULL); g_in_layout--; hle_log("[ui] layoutSubviews %s\n", v_clsname(v->obj)); } }
    for (int i = 0; i < v->nk; i++) layout_tree(v->kids[i], depth + 1);
}
/* A GL view authored in the nib at portrait size (768x1024), with full-screen portrait containers above it, must fill the landscape window. */
static void fix_gl_chain(View *v) {
    if (v->is_gl) { for (View *p = v; p && p != g_window; p = p->parent) { View *par = p->parent; if (par && par->bw >= par->bh && p->bw < p->bh && fabsf(p->bw - 768) < 1 && fabsf(p->bh - 1024) < 1) v_set_frame(p, 0, 0, par->bw, par->bh); } }
    for (int i = 0; i < v->nk; i++) fix_gl_chain(v->kids[i]);
}
/* views authored full-screen in portrait (768x1024) fill their landscape parent */
static void fix_portrait(View *v) {
    if (!v->hidden && v->parent && v->parent->bw > v->parent->bh && fabsf(v->bw - 768) < 1 && fabsf(v->bh - 1024) < 1) v_set_frame(v, 0, 0, v->parent->bw, v->parent->bh);
    /* a 768-wide portrait page inside the 1024-wide landscape parent is centred (shop, career, top/bottom bars) */
    { float fx, fy, fw, fh; v_frame(v, &fx, &fy, &fw, &fh); if (v->parent && fabsf(v->parent->bw - 1024) < 1 && v->parent->bh < 1000 && fabsf(fw - 768) < 1 && fabsf(fx) < 1 && fh > 60) v_set_frame(v, 128, fy, fw, fh); }
    /* a page whose backdrop overhangs by 128 px on both sides is a 768-wide page centred in the landscape window */
    { float fx, fy, fw, fh; v_frame(v, &fx, &fy, &fw, &fh); if (v->parent && fabsf(fx) < 1 && fabsf(fw - 1024) < 1 && fabsf(v->parent->bw - 1024) < 1 && v->nk <= 6) for (int i = 0; i < v->nk; i++) { float kx, ky, kw, kh; v_frame(v->kids[i], &kx, &ky, &kw, &kh); if (fabsf(kx + 128) < 1 && fabsf(kw - 1024) < 1 && v->kids[i]->kind == K_IMAGEVIEW) { v->bw = 768; v->cx = 128 + 384; break; } } }
    /* bottom-anchored in the 1024-high portrait nib (e.g. the balance/upgrades/rank bar): pull it up to the landscape bottom edge */
    { float fx, fy, fw, fh; v_frame(v, &fx, &fy, &fw, &fh); if (v->parent && v->parent->bw >= v->parent->bh && fabsf(v->parent->bh - 768) < 1 && fw <= 780 && fabsf(fy + fh - 1024) < 1.5f && fh < 200) v_set_frame(v, fx, v->parent->bh - fh, fw, fh); }
    for (int i = 0; i < v->nk; i++) fix_portrait(v->kids[i]);
}
void uikit_layout(void) { if (g_window && !g_in_layout) { fix_gl_chain(g_window); fix_portrait(g_window); layout_tree(g_window, 0); } }
void uikit_composite(void) {
    if (!g_window) return;
    glPushAttrib(GL_ALL_ATTRIB_BITS); glPushClientAttrib(GL_CLIENT_ALL_ATTRIB_BITS);
    glMatrixMode(GL_PROJECTION); glPushMatrix(); glLoadIdentity(); glOrtho(0, g_scr_w, g_scr_h, 0, -1, 1);
    glMatrixMode(GL_MODELVIEW); glPushMatrix(); glLoadIdentity(); glMatrixMode(GL_TEXTURE); glPushMatrix(); glLoadIdentity(); glMatrixMode(GL_MODELVIEW);
    glActiveTexture(GL_TEXTURE0); glDisable(GL_DEPTH_TEST); glDisable(GL_CULL_FACE); glDisable(GL_ALPHA_TEST); glDisable(GL_LIGHTING); glDisable(GL_STENCIL_TEST);
    glDisableClientState(GL_COLOR_ARRAY); glDisableClientState(GL_NORMAL_ARRAY); glDisableClientState(GL_TEXTURE_COORD_ARRAY); glDisableClientState(GL_VERTEX_ARRAY);
    glEnable(GL_BLEND); glBlendFunc(GL_ONE_MINUS_SRC_ALPHA == 0 ? GL_SRC_ALPHA : GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA); glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE); glColorMask(1, 1, 1, 1); glDepthMask(0);
    glClearColor(0, 0, 0, 1); glClear(GL_COLOR_BUFFER_BIT);
    Mat id = {1, 0, 0, 1, 0, 0}; for (int i = 0; i < g_window->nk; i++) draw_view(g_window->kids[i], id, 1);
    glMatrixMode(GL_PROJECTION); glPopMatrix(); glMatrixMode(GL_TEXTURE); glPopMatrix(); glMatrixMode(GL_MODELVIEW); glPopMatrix();
    glPopClientAttrib(); glPopAttrib();
}

/* ------------------------------------------------------------------ NIBArchive ------------------------------------------------------------------ */
typedef struct { int key; int type; union { int64_t i; double f; struct { const uint8_t *p; int n; } d; uint32_t ref; } u; } NV;
typedef struct { int cls, first, n; } NObj;
typedef struct { uint8_t *data; NObj *objs; int nobj; NV *vals; int nval; char **keys; int nkeys; char **cls; int ncls; gptr *made; gptr owner; } Nib;
static uint32_t nib_varint(const uint8_t *d, size_t *o) { uint32_t r = 0; int sh = 0; for (;;) { uint8_t b = d[(*o)++]; r |= (uint32_t)(b & 0x7f) << sh; sh += 7; if (b & 0x80) return r; } }
static Nib *nib_reg[16384]; static int nnib_reg;   /* one entry per nib load; the Upgrades yard alone loads 31 (the old 256 ran out after ~4 flips between it and the tech shop -> blank page) */
static u32 nib_id(Nib *nb) { for (int i = 0; i < nnib_reg; i++) if (nib_reg[i] == nb) return i + 1; if (nnib_reg < 16384) { nib_reg[nnib_reg] = nb; return ++nnib_reg; } return 0; }
static Nib *nib_by_id(u32 id) { return id && id <= (u32)nnib_reg ? nib_reg[id - 1] : NULL; }
static Nib *nib_parse(const char *path) {
    FILE *f = fopen(path, "rb"); if (!f) return NULL; fseek(f, 0, SEEK_END); long n = ftell(f); fseek(f, 0, SEEK_SET); uint8_t *d = malloc(n); if (fread(d, 1, n, f) != (size_t)n) { fclose(f); return NULL; } fclose(f);
    if (n < 50 || memcmp(d, "NIBArchive", 10)) { free(d); return NULL; }
    uint32_t h[10]; memcpy(h, d + 10, 40); Nib *nb = calloc(1, sizeof *nb); nb->data = d; nb->nobj = h[2]; nb->nkeys = h[4]; nb->nval = h[6]; nb->ncls = h[8];
    size_t o = h[9]; nb->cls = calloc(nb->ncls, sizeof(char *)); for (int i = 0; i < nb->ncls; i++) { uint32_t ln = nib_varint(d, &o), ex = nib_varint(d, &o); o += 4 * ex; nb->cls[i] = strndup((char *)d + o, ln - 1); o += ln; }
    o = h[5]; nb->keys = calloc(nb->nkeys, sizeof(char *)); for (int i = 0; i < nb->nkeys; i++) { uint32_t ln = nib_varint(d, &o); nb->keys[i] = strndup((char *)d + o, ln); o += ln; }
    o = h[7]; nb->vals = calloc(nb->nval, sizeof(NV));
    for (int i = 0; i < nb->nval; i++) { NV *v = &nb->vals[i]; v->key = nib_varint(d, &o); v->type = d[o++];
        switch (v->type) { case 0: v->u.i = (int8_t)d[o]; o += 1; break; case 1: { int16_t x; memcpy(&x, d + o, 2); v->u.i = x; o += 2; break; } case 2: { int32_t x; memcpy(&x, d + o, 4); v->u.i = x; o += 4; break; }
            case 3: { int64_t x; memcpy(&x, d + o, 8); v->u.i = x; o += 8; break; } case 4: v->u.i = 1; break; case 5: v->u.i = 0; break; case 6: { float x; memcpy(&x, d + o, 4); v->u.f = x; o += 4; break; }
            case 7: { double x; memcpy(&x, d + o, 8); v->u.f = x; o += 8; break; } case 8: { uint32_t ln = nib_varint(d, &o); v->u.d.p = d + o; v->u.d.n = ln; o += ln; break; } case 9: break; case 10: memcpy(&v->u.ref, d + o, 4); o += 4; break; } }
    o = h[3]; nb->objs = calloc(nb->nobj, sizeof(NObj)); for (int i = 0; i < nb->nobj; i++) { nb->objs[i].cls = nib_varint(d, &o); nb->objs[i].first = nib_varint(d, &o); nb->objs[i].n = nib_varint(d, &o); }
    nb->made = calloc(nb->nobj, sizeof(gptr)); return nb;
}
static NV *nv(Nib *nb, int obj, const char *key) { NObj *o = &nb->objs[obj]; for (int i = 0; i < o->n; i++) { NV *v = &nb->vals[o->first + i]; if (!strcmp(nb->keys[v->key], key)) return v; } return NULL; }
static const char *nclass(Nib *nb, int obj) { return nb->cls[nb->objs[obj].cls]; }
static int nv_int(Nib *nb, int obj, const char *k, int def) { NV *v = nv(nb, obj, k); return v && v->type <= 5 ? (int)v->u.i : (v && (v->type == 6 || v->type == 7) ? (int)v->u.f : def); }
static float nv_float(Nib *nb, int obj, const char *k, float def) { NV *v = nv(nb, obj, k); return v ? ((v->type == 6 || v->type == 7) ? (float)v->u.f : (v->type <= 5 ? (float)v->u.i : def)) : def; }
static int nv_floats(Nib *nb, int obj, const char *k, float *out, int n) { NV *v = nv(nb, obj, k); if (!v || v->type != 8 || v->u.d.n < 1 + 4 * n) return 0; memcpy(out, v->u.d.p + 1, 4 * n); return 1; }
static const char *nv_string(Nib *nb, int obj, const char *k) { NV *v = nv(nb, obj, k); if (!v) return NULL; if (v->type == 10) { NV *b = nv(nb, v->u.ref, "NS.bytes"); if (b && b->type == 8) { static char buf[16][512]; static int bi; char *r = buf[bi++ & 15]; int n = b->u.d.n < 511 ? b->u.d.n : 511; memcpy(r, b->u.d.p, n); r[n] = 0; return r; } } return NULL; }
static int nv_ref(Nib *nb, int obj, const char *k) { NV *v = nv(nb, obj, k); return v && v->type == 10 ? (int)v->u.ref : -1; }
static gptr nib_make(Nib *nb, int idx);
static gptr nib_ivar_set(gptr obj, const char *name, gptr val);

static void nib_apply_view(Nib *nb, int idx, gptr obj) {
    View *v = view_of(obj); if (!v) return; float f[4];
    if (nv_floats(nb, idx, "UIBounds", f, 4)) { v->bx = f[0]; v->by = f[1]; v->bw = f[2]; v->bh = f[3]; }
    if (nv_floats(nb, idx, "UICenter", f, 2)) { v->cx = f[0]; v->cy = f[1]; }
    v->mask = nv_int(nb, idx, "UIAutoresizingMask", 0); v->noautosub = !nv_int(nb, idx, "UIAutoresizeSubviews", 1); v->alpha = nv_float(nb, idx, "UIAlpha", 1); v->hidden = nv_int(nb, idx, "UIHidden", 0); v->tag = nv_int(nb, idx, "UITag", 0); v->interact = !nv_int(nb, idx, "UIUserInteractionDisabled", 0); v->clips = nv_int(nb, idx, "UIClipsToBounds", 0);
    int bc = nv_ref(nb, idx, "UIBackgroundColor"); if (bc >= 0) { gptr c = nib_make(nb, bc); if (c) { color_get(c, v->bg); v->hasbg = 1; } }
    int fo = nv_ref(nb, idx, "UIFont"); if (fo >= 0) { const char *fn = nv_string(nb, fo, "UIFontName"); if (!fn) fn = nv_string(nb, fo, "NSName"); strncpy(v->font, fn ? fn : "Helvetica", 63); v->fsize = nv_float(nb, fo, "UIFontPointSize", 17); }
    if (v->kind == K_IMAGEVIEW) { int im = nv_ref(nb, idx, "UIImage"); if (im >= 0) v->image = nib_make(nb, im); int hi = nv_ref(nb, idx, "UIHighlightedImage"); if (hi >= 0) v->himage = nib_make(nb, hi); }
    if (v->kind == K_LABEL) { const char *t = nv_string(nb, idx, "UIText"); if (t) v->text = strdup(t); int tc = nv_ref(nb, idx, "UITextColor"); if (tc >= 0) color_get(nib_make(nb, tc), v->tcol); v->align = nv_int(nb, idx, "UITextAlignment", 0); v->lines = nv_int(nb, idx, "UINumberOfLines", 1); }
    if (v->kind == K_BUTTON) {
        int sc = nv_ref(nb, idx, "UIButtonStatefulContent");
        if (sc >= 0) { NObj *o = &nb->objs[sc]; int curstate = -1;
            for (int i = 0; i < o->n; i++) { NV *e = &nb->vals[o->first + i]; if (e->type != 10) continue; int ei = e->u.ref; const char *cn = nclass(nb, ei);
                if (!strcmp(cn, "NSNumber")) curstate = nv_int(nb, ei, "NS.intval", 0);
                else if (!strcmp(cn, "UIButtonContent") && curstate >= 0) { int s = curstate == 0 ? 0 : curstate == 1 ? 1 : curstate == 2 ? 2 : 3;
                    const char *t = nv_string(nb, ei, "UITitle"); if (t) v->btitle[s] = strdup(t); int tc = nv_ref(nb, ei, "UITitleColor"); if (tc >= 0) color_get(nib_make(nb, tc), v->btc[s]);
                    int im = nv_ref(nb, ei, "UIImage"); if (im >= 0) v->bimg[s] = nib_make(nb, im); int bi = nv_ref(nb, ei, "UIBackgroundImage"); if (bi >= 0) v->bbg[s] = nib_make(nb, bi); } } }
        if (v->btc[0][3] == 0) { v->btc[0][0] = v->btc[0][1] = v->btc[0][2] = 1; v->btc[0][3] = 1; }
    }
    int sv = nv_ref(nb, idx, "UISubviews");
    if (sv >= 0) { NObj *o = &nb->objs[sv]; for (int i = 0; i < o->n; i++) { NV *e = &nb->vals[o->first + i]; if (e->type != 10) continue; gptr k = nib_make(nb, e->u.ref); View *kv = view_of(k); if (kv) view_insert(v, kv, -1); } }
}
static const char *view_family(const char *c) { static const char *f[] = {"UIView", "UIImageView", "UILabel", "UIButton", "UIRoundedRectButton", "UIScrollView", "UIActivityIndicatorView", "UITextField", "UIWindow", "UIControl", NULL}; for (int i = 0; f[i]; i++) if (!strcmp(c, f[i])) return f[i]; return NULL; }
typedef struct { gptr vc; char nib[96]; gptr view; int loaded; int loading; } VcState;
static VcState vcs[16384]; static int nvcs;   /* one per view controller ever created: the Upgrades yard makes ~31 per visit and the tech shop more, so 64 ran out after 3 flips -> blank page */
static VcState *vc_of(gptr o) { for (int i = nvcs - 1; i >= 0; i--) if (vcs[i].vc == o) return &vcs[i]; if (nvcs >= 16384) return NULL; vcs[nvcs].vc = o; return &vcs[nvcs++]; }

static gptr nib_make(Nib *nb, int idx) {
    if (idx < 0 || idx >= nb->nobj) return 0; if (nb->made[idx]) return nb->made[idx];
    const char *cn = nclass(nb, idx); gptr r = 0;
    if (!strcmp(cn, "NSString") || !strcmp(cn, "NSMutableString")) { NV *b = nv(nb, idx, "NS.bytes"); char tmp[1024]; int n = b && b->type == 8 ? (b->u.d.n < 1023 ? b->u.d.n : 1023) : 0; if (n) memcpy(tmp, b->u.d.p, n); tmp[n] = 0; r = new_string(tmp); }
    else if (!strcmp(cn, "NSNumber")) { NV *v = nv(nb, idx, "NS.intval"); if (!v) v = nv(nb, idx, "NS.dblval"); r = new_number_i(v ? (v->type == 7 ? (int)v->u.f : (int)v->u.i) : 0); }
    else if (!strcmp(cn, "NSArray") || !strcmp(cn, "NSMutableArray") || !strcmp(cn, "NSSet") || !strcmp(cn, "NSMutableSet")) { r = new_array(); nb->made[idx] = r; { extern void gm_gc_pin(gptr); gm_gc_pin(r); } NObj *o = &nb->objs[idx]; for (int i = 0; i < o->n; i++) { NV *e = &nb->vals[o->first + i]; if (e->type == 10) array_add(r, nib_make(nb, e->u.ref)); } }
    else if (!strcmp(cn, "NSDictionary") || !strcmp(cn, "NSMutableDictionary")) { r = new_dict(); nb->made[idx] = r; { extern void gm_gc_pin(gptr); gm_gc_pin(r); } NObj *o = &nb->objs[idx]; gptr k = 0; int have = 0; for (int i = 0; i < o->n; i++) { NV *e = &nb->vals[o->first + i]; if (e->type != 10) continue; gptr x = nib_make(nb, e->u.ref); if (!have) { k = x; have = 1; } else { dict_set(r, k, x); have = 0; } } }
    else if (!strcmp(cn, "UIColor")) { float rr = nv_float(nb, idx, "UIRed", -1), gg = nv_float(nb, idx, "UIGreen", 0), bb = nv_float(nb, idx, "UIBlue", 0), aa = nv_float(nb, idx, "UIAlpha", 1);
        if (rr < 0) { float w = nv_float(nb, idx, "UIWhite", 0); rr = gg = bb = w; }
        const char *sysn = nv_string(nb, idx, "UISystemColorName");      /* named system colours carry no components */
        if (sysn && strstr(sysn, "BackgroundColor")) { rr = gg = bb = 0.8f; aa = 0; }                          /* pattern system colours (groupTableView..., scrollViewTextured...): not reproduced, draw nothing */
        else if (sysn) { if (!strcmp(sysn, "clearColor")) { rr = gg = bb = 0; aa = 0; } else if (!strcmp(sysn, "whiteColor")) rr = gg = bb = 1; else if (!strcmp(sysn, "blackColor")) rr = gg = bb = 0; else if (!strcmp(sysn, "lightGrayColor")) rr = gg = bb = .667f; else if (!strcmp(sysn, "darkGrayColor")) rr = gg = bb = .333f; else if (!strcmp(sysn, "grayColor")) rr = gg = bb = .5f; else if (!strcmp(sysn, "redColor")) { rr = 1; gg = bb = 0; } else if (!strcmp(sysn, "blueColor")) { rr = gg = 0; bb = 1; } else if (!strcmp(sysn, "greenColor")) { rr = bb = 0; gg = 1; } }
        r = color_obj(rr, gg, bb, aa); }
    else if (!strcmp(cn, "UIFont")) { const char *fn = nv_string(nb, idx, "UIFontName"); r = font_obj(fn, nv_float(nb, idx, "UIFontPointSize", 17)); }
    else if (!strcmp(cn, "UIImageNibPlaceholder")) { const char *rn = nv_string(nb, idx, "UIResourceName"); r = image_named(rn); }
    else if (!strcmp(cn, "UIProxyObject")) { const char *id = nv_string(nb, idx, "UIProxiedObjectIdentifier"); r = (id && !strcmp(id, "IBFilesOwner")) ? nb->owner : 0; }
    else if (!strcmp(cn, "UIClassSwapper")) {
        const char *cls = nv_string(nb, idx, "UIClassName"), *orig = nv_string(nb, idx, "UIOriginalClassName"); char cl[96], og[96]; strncpy(cl, cls ? cls : "NSObject", 95); strncpy(og, orig ? orig : "NSObject", 95);
        GCls *g = find_gcls_name(cl); gptr o;
        if (g) { o = gm_malloc(g->instsize + 16); wr32(o, g->addr); { extern void objc_construct(gptr, gptr); objc_construct(g->addr, o); } } else { o = new_obj(objc_class_object(cl)); }
        nb->made[idx] = o; r = o;
        if (view_family(og)) { gptr real = o; View *v0 = view_of(o); if (v0) { const char *vf = view_family(og); if (!strcmp(vf, "UIImageView")) v0->kind = K_IMAGEVIEW; else if (!strcmp(vf, "UILabel")) v0->kind = K_LABEL; else if (strstr(vf, "Button")) v0->kind = K_BUTTON; else if (!strcmp(vf, "UIScrollView")) v0->kind = K_SCROLL; else if (!strcmp(vf, "UIWindow")) v0->kind = K_WINDOW; }
            if (!strcmp(cl, "EAGLView")) { v0->is_gl = 1; }
            if (g && guest_responds(o, "initWithCoder:")) { gptr coder = new_obj(cls_NSCoder); wr32(coder + 4, nib_id(nb)); wr32(coder + 8, idx); u32 a[1] = { coder }; gptr rr = send_guest(o, "initWithCoder:", 1, a); if (rr && rr != o) { r = rr; nb->made[idx] = rr; } }
            else nib_apply_view(nb, idx, real); }
        else if (!strcmp(og, "UIViewController") || strstr(og, "ViewController")) {
            const char *nn = nv_string(nb, idx, "UINibName"); VcState *vs = vc_of(o); if (vs && nn) strncpy(vs->nib, nn, 95);
            if (g && guest_responds(o, "initWithNibName:bundle:")) { gptr ns = nn ? new_string(nn) : 0; u32 a[2] = { ns, 0 }; send_guest(o, "initWithNibName:bundle:", 2, a); } else if (g && guest_responds(o, "init")) send_guest(o, "init", 0, NULL); }
        else if (g && guest_responds(o, "init")) send_guest(o, "init", 0, NULL);
    }
    else if (view_family(cn)) { gptr o = new_obj(objc_class_object(cn)); nb->made[idx] = o; View *v = view_of(o); (void)v; nib_apply_view(nb, idx, o); r = o; }
    else if (!strcmp(cn, "NSObject")) r = 0;       /* root */
    else { hle_log("[nib] unknown class %s\n", cn); r = new_obj(objc_class_object(cn)); }
    nb->made[idx] = r; { extern void gm_gc_pin(gptr); gm_gc_pin(r); } return r;
}
/* KVC-ish outlet assignment: setter, else ivar (name or _name) */
static const char *ivar_find(gptr cls, const char *name, u32 *off) {
    for (int guard = 0; cls && guard < 16; guard++) { GCls *g = find_gcls(cls); if (!g) return NULL; gptr ro = rd32(cls + 16) & ~3u; gptr il = rd32(ro + 28);
        if (il) { u32 n = rd32(il + 4), es = rd32(il) & ~3u; for (u32 i = 0; i < n; i++) { gptr iv = il + 8 + i * es; const char *nm = gstr(rd32(iv + 4)); if (!strcmp(nm, name)) { *off = rd32(rd32(iv)); return nm; } } } cls = g->super; }
    return NULL;
}
static void kvc_set(gptr obj, const char *key, gptr val) {
    if (!obj || obj >= ARENA_SIZE - 16) return; char setter[160]; snprintf(setter, sizeof setter, "set%c%s:", toupper(key[0]), key + 1);
    if (guest_responds(obj, setter)) { u32 a[1] = { val }; send_guest(obj, setter, 1, a); return; }
    u32 off; char alt[160]; snprintf(alt, sizeof alt, "_%s", key);
    if (ivar_find(rd32(obj), key, &off) || ivar_find(rd32(obj), alt, &off)) { wr32(obj + off, val); return; }
    { u32 a[1] = { val }; send_any(obj, setter, 1, a); }          /* host-implemented property of a fake base class (e.g. UIViewController.view) */
}
static gptr s_app_obj_for_nib;
gptr nib_load(const char *name, gptr owner, int *out_isapp) {
    static const char *sfx[] = {"~ipad.nib", ".nib", NULL}; char path[1024]; Nib *nb = NULL;
    for (int i = 0; sfx[i] && !nb; i++) { snprintf(path, sizeof path, "%s/%s%s", g_bundle_dir, name, sfx[i]); nb = nib_parse(path); }
    if (!nb) { hle_log("[nib] %s not found\n", name); return 0; } nb->owner = owner; (void)out_isapp;
    hle_log("[nib] loading %s (%d objects) owner=%s\n", path, nb->nobj, owner ? v_clsname(owner) : "nil");
    if (strstr(path, "SpiceShopViewController")) g_spice_view_loaded = 1;
    int top = nv_ref(nb, 0, "UINibTopLevelObjectsKey"); gptr tops = new_array(); int ntop = 0, *topidx = NULL;
    if (top >= 0) { NObj *o = &nb->objs[top]; topidx = malloc(sizeof(int) * (o->n + 1)); for (int i = 0; i < o->n; i++) { NV *e = &nb->vals[o->first + i]; if (e->type == 10) { topidx[ntop++] = e->u.ref; gptr x = nib_make(nb, e->u.ref); if (x && strcmp(nclass(nb, e->u.ref), "UIProxyObject")) array_add(tops, x); }   /* proxies (File's Owner, First Responder) are not returned */ } }
    /* also instantiate everything reachable via UINibObjectsKey (covers non-top-level views referenced only by connections) */
    int conn = nv_ref(nb, 0, "UINibConnectionsKey");
    if (conn >= 0) { NObj *o = &nb->objs[conn]; for (int i = 0; i < o->n; i++) { NV *e = &nb->vals[o->first + i]; if (e->type != 10) continue; int c = e->u.ref; const char *cc = nclass(nb, c);
        const char *lab = nv_string(nb, c, "UILabel"); int s = nv_ref(nb, c, "UISource"), d = nv_ref(nb, c, "UIDestination"); gptr so = nib_make(nb, s), de = nib_make(nb, d);
        if (!lab) continue;
        if (!strcmp(cc, "UIRuntimeOutletConnection")) {
            if (so && so == owner && !strcmp(v_clsname(so), "UIApplication")) { if (!strcmp(lab, "delegate")) g_delegate = de; continue; }
            if (!so) continue; if (so == s_app_obj_for_nib && !strcmp(lab, "delegate")) { g_delegate = de; continue; } kvc_set(so, lab, de);
            if (de && (rd32(de) == 0 ? 0 : 1) && so && !strcmp(v_clsname(so), "UIApplication") && !strcmp(lab, "delegate")) g_delegate = de;
        } else if (!strcmp(cc, "UIRuntimeEventConnection")) { View *sv = view_of(so); if (sv && sv->nact < 8) { Action *a = &sv->act[sv->nact++]; a->target = de; strncpy(a->sel, lab, 63); a->ev = nv_int(nb, c, "UIEventMask", 64); } } } }
    for (int i = 0; i < nb->nobj; i++) if (nb->made[i] && nb->made[i] < ARENA_SIZE - 16 && !strcmp(nclass(nb, i), "UIClassSwapper") && guest_responds(nb->made[i], "awakeFromNib")) send_guest(nb->made[i], "awakeFromNib", 0, NULL);
    free(topidx); return tops;
}

/* origin of a view's coordinate space in window coordinates (ignores transforms) */
static void view_window_origin(View *v, float *ox, float *oy) { float x = 0, y = 0; for (View *p = v; p && p != g_window; p = p->parent) { x += p->cx - p->bw / 2 - p->bx; y += p->cy - p->bh / 2 - p->by; if (p->parent && p->parent->kind == K_SCROLL) { x -= p->parent->sx; y -= p->parent->sy; } } *ox = x; *oy = y; }
/* ------------------------------------------------------------------ input ------------------------------------------------------------------ */
static View *hit_test(View *v, float px, float py, Mat inv_parent_unused) {
    (void)inv_parent_unused; if (v->hidden || v->alpha < 0.01f || !v->interact) return NULL;
    /* px,py are in v's parent coordinate space */
    float x = px - v->cx, y = py - v->cy; if (v->ta != 1 || v->td != 1 || v->tb != 0 || v->tc_ != 0) { float det = v->ta * v->td - v->tb * v->tc_; if (fabsf(det) > 1e-6f) { float ix = (v->td * x - v->tc_ * y) / det, iy = (-v->tb * x + v->ta * y) / det; x = ix; y = iy; } }
    x += v->bw / 2; y += v->bh / 2;
    if (!(x >= 0 && y >= 0 && x < v->bw && y < v->bh) && v->kind != K_WINDOW) return NULL;
    float cx = x + (v->kind == K_SCROLL ? v->sx : 0), cy = y + (v->kind == K_SCROLL ? v->sy : 0);
    for (int i = v->nk - 1; i >= 0; i--) { View *h = hit_test(v->kids[i], cx - v->bw / 2 + v->bw / 2 - 0, cy, (Mat){0}); if (h) return h; }
    /* a clear plain container (no background, not a control) does not swallow touches meant for the view behind it (the GL game view) */
    if (v->kind == 0 && !(v->hasbg && v->bg[3] > 0.01f) && !v->is_gl && v->obj && !strcmp(v_clsname(v->obj), "UIView") && v->parent && v->parent != g_window) return NULL;
    /* a plain UILabel / UIImageView is not user-interaction-enabled by default: it never swallows a tap meant for the button under it (spice shop 'Get it!') */
    if ((v->kind == K_LABEL || v->kind == K_IMAGEVIEW) && v->obj && (!strcmp(v_clsname(v->obj), "UILabel") || !strcmp(v_clsname(v->obj), "UIImageView"))) return NULL;
    return v;
}
extern int g_touch_phase; extern gptr g_engine_input;
static View *g_touch_view; static int g_touch_inside; static gptr g_touch_obj_set;
static void fire_actions(View *v, int ev) {
    for (int i = 0; i < v->nact; i++) if (v->act[i].ev & ev) { hle_log("[ui] action %s -> %s\n", v->act[i].sel, objc_describe(v->act[i].target)); u32 a[1] = { v->obj }; Target t = lookup(v->act[i].target, v->act[i].sel, 0, 0); if (t.imp) send_guest(v->act[i].target, v->act[i].sel, 1, a); else hle_log("[ui] action %s has no guest implementation\n", v->act[i].sel); }
}
static View *button_ancestor(View *v) { for (; v; v = v->parent) if (v->kind == K_BUTTON) return v; return NULL; }
void uikit_touch(int phase, int x, int y) {          /* phase 0 down, 1 move, 2 up; x,y in screen points */
    if (!g_window) return;
    if (phase == 0) { View *h = NULL; for (int i = g_window->nk - 1; i >= 0 && !h; i--) h = hit_test(g_window->kids[i], (float)x, (float)y, (Mat){0}); g_touch_view = h;
        hle_log("[ui] touch down at (%d,%d) -> %s%s tag=%d\n", x, y, h ? v_clsname(h->obj) : "none", h && h->is_gl ? " [GL]" : "", h ? h->tag : 0);
        View *b = button_ancestor(h); if (b && !b->disabled) { b->highlighted = 1; g_touch_view = b; } }
    View *tv = g_touch_view; if (!tv) return;
    if (tv->kind == K_BUTTON) { if (phase == 2) { tv->highlighted = 0; float bx, by; (void)bx; (void)by; fire_actions(tv, 64); } return; }
    /* find the nearest ancestor (or self) that wants touches: GL view or a guest class implementing touchesBegan */
    for (View *v = tv; v; v = v->parent) {
        const char *sel = phase == 0 ? "touchesBegan:withEvent:" : phase == 1 ? "touchesMoved:withEvent:" : "touchesEnded:withEvent:";
        if (guest_responds(v->obj, sel)) { gptr touch, set, event; static gptr tch, st, ev; if (!tch) { tch = new_obj(cls_Touch); st = new_obj(cls_NSSet); wr32(st + 8, gm_malloc(16)); wr32(st + 12, 4); ev = new_obj(cls_Event); } touch = tch; set = st; event = ev;
            g_touch_phase = phase == 0 ? 0 : phase == 1 ? 1 : 3; g_touch_x = x; g_touch_y = y; wr32(set + 4, 1); wr32(rd32(set + 8), touch); g_cur_touchset = set; u32 a[2] = { set, event }; g_touch_target = g_touch_view ? g_touch_view->obj : v->obj; hle_log("[ui] deliver %s to %s at (%d,%d)\n", sel, v_clsname(v->obj), x, y); send_guest(v->obj, sel, 2, a);
            if (phase == 2 && g_engine_input && guest_responds(v->obj, "firstTouch") && !strcmp(v_clsname(v->obj), "EAGLView")) { u32 ea[1] = { g_engine_input }; call_guest(0xf86dd, 1, ea); hle_log("[ui] engine touch-up sent\n"); }   /* Tower: the ObjC release path does not reach the engine's release function */
            break; }
        if (phase == 2 && v->ngr) { for (int i = 0; i < v->ngr; i++) { u32 a[1] = { v->obj }; send_guest(v->gr_target[i], v->gr_sel[i], 1, a); } break; }
    }
    if (phase == 2) g_touch_view = NULL;
}
