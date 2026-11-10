/* uikit3.inc - software CoreGraphics subset: bitmap contexts, CTM, DrawImage/FillRect/ShowText, UIGraphics image contexts, geometry helpers. */
typedef struct { float a, b, c, d, tx, ty; } Ctm;
typedef struct { gptr data; int own; int w, h, rb, alphainfo, bpp; Ctm ctm; Ctm stack[16]; int sp; float fill[4], alpha; char font[64]; float fsize; int live; } CGCtx;
#define CG_BASE 0xC6C00000u
static CGCtx cgctx[64]; static gptr s_cur_ui_ctx;
static CGCtx *cg_of(gptr h) { u32 i = h - CG_BASE; return (h >= CG_BASE && i < 64 && cgctx[i].live) ? &cgctx[i] : NULL; }
static Ctm ctm_mul(Ctm m, Ctm n) { /* result = n then m?  CGContextConcatCTM: ctm = t * ctm  (t applied first) */ Ctm r; r.a = m.a * n.a + m.c * n.b; r.b = m.b * n.a + m.d * n.b; r.c = m.a * n.c + m.c * n.d; r.d = m.b * n.c + m.d * n.d; r.tx = m.a * n.tx + m.c * n.ty + m.tx; r.ty = m.b * n.tx + m.d * n.ty + m.ty; return r; }
static Ctm ctm_concat_t_first(Ctm ctm, Ctm t) { return ctm_mul(ctm, t); }   /* new = ctm * t : points go through t first */
static void ctm_apply(Ctm m, float x, float y, float *ox, float *oy) { *ox = m.a * x + m.c * y + m.tx; *oy = m.b * x + m.d * y + m.ty; }
static int ctm_invert(Ctm m, Ctm *o) { float det = m.a * m.d - m.b * m.c; if (fabsf(det) < 1e-9f) return 0; o->a = m.d / det; o->b = -m.b / det; o->c = -m.c / det; o->d = m.a / det; o->tx = (m.c * m.ty - m.d * m.tx) / det; o->ty = (m.b * m.tx - m.a * m.ty) / det; return 1; }

static gptr cg_new(gptr data, int w, int h, int rb, int alphainfo, int bpc) {
    for (int i = 0; i < 64; i++) if (!cgctx[i].live) { CGCtx *c = &cgctx[i]; memset(c, 0, sizeof *c); c->live = 1; c->w = w; c->h = h; c->alphainfo = alphainfo & 0x1f; c->bpp = (bpc == 8 && (alphainfo & 0x1f) == 7) ? 1 : (bpc == 8 && !(alphainfo & 0x1f) ? 1 : 4);
        if (bpc == 8 && c->bpp == 4 && rb && rb < w * 4) c->bpp = 1;
        c->rb = rb ? rb : w * c->bpp; c->own = !data; c->data = data ? data : gm_malloc((size_t)c->rb * h + 16); if (!data) memset(H(c->data), 0, (size_t)c->rb * h);
        c->ctm = (Ctm){1, 0, 0, 1, 0, 0}; c->alpha = 1; c->fill[3] = 1; strcpy(c->font, "Helvetica"); c->fsize = 12; return CG_BASE + i; }
    return 0;
}
/* blend one source pixel (straight RGBA 0..255 + extra alpha) into the destination bitmap */
static void cg_blend(CGCtx *c, int x, int y, float r, float g, float b, float a) {
    if (x < 0 || y < 0 || x >= c->w || y >= c->h || a <= 0) return; uint8_t *d = (uint8_t *)H(c->data) + (size_t)y * c->rb + (size_t)x * c->bpp;
    if (c->bpp == 1) { if (c->alphainfo == 7) { d[0] = (uint8_t)(a * 255 + d[0] * (1 - a)); } else { float lum = 0.299f * r + 0.587f * g + 0.114f * b; d[0] = (uint8_t)(lum * 255 * a + d[0] * (1 - a)); } return; }
    int premult = (c->alphainfo == 1 || c->alphainfo == 2); int first = (c->alphainfo == 2 || c->alphainfo == 4 || c->alphainfo == 6); uint8_t *dr = d + (first ? 1 : 0), *da = first ? d : d + 3;
    float sa = a, dstA = (c->alphainfo == 5 || c->alphainfo == 6) ? 1.f : da[0] / 255.f;
    if (premult) { float sr = r * sa, sg = g * sa, sb = b * sa; dr[0] = (uint8_t)fminf(255, sr * 255 + dr[0] * (1 - sa)); dr[1] = (uint8_t)fminf(255, sg * 255 + dr[1] * (1 - sa)); dr[2] = (uint8_t)fminf(255, sb * 255 + dr[2] * (1 - sa)); da[0] = (uint8_t)fminf(255, sa * 255 + da[0] * (1 - sa)); }
    else { float oa = sa + dstA * (1 - sa); if (oa > 0) { dr[0] = (uint8_t)((r * 255 * sa + dr[0] * dstA * (1 - sa)) / oa); dr[1] = (uint8_t)((g * 255 * sa + dr[1] * dstA * (1 - sa)) / oa); dr[2] = (uint8_t)((b * 255 * sa + dr[2] * dstA * (1 - sa)) / oa); } if (!(c->alphainfo == 5 || c->alphainfo == 6)) da[0] = (uint8_t)(oa * 255); else da[0] = 255; }
}
static void cg_draw_image(CGCtx *c, float rx, float ry, float rw, float rh, ImageRec *im, int crop) {
    (void)crop; if (!im || !im->rgba || rw == 0 || rh == 0) return; Ctm inv; if (!ctm_invert(c->ctm, &inv)) return;
    float cx[4], cy[4]; float xs[4] = {rx, rx + rw, rx + rw, rx}, ys[4] = {ry, ry, ry + rh, ry + rh};
    for (int i = 0; i < 4; i++) { float ux, uy; ctm_apply(c->ctm, xs[i], ys[i], &ux, &uy); cx[i] = ux; cy[i] = c->h - uy; }
    int x0 = (int)floorf(fminf(fminf(cx[0], cx[1]), fminf(cx[2], cx[3]))), x1 = (int)ceilf(fmaxf(fmaxf(cx[0], cx[1]), fmaxf(cx[2], cx[3]))), y0 = (int)floorf(fminf(fminf(cy[0], cy[1]), fminf(cy[2], cy[3]))), y1 = (int)ceilf(fmaxf(fmaxf(cy[0], cy[1]), fmaxf(cy[2], cy[3])));
    if (x0 < 0) x0 = 0; if (y0 < 0) y0 = 0; if (x1 > c->w) x1 = c->w; if (y1 > c->h) y1 = c->h;
    for (int y = y0; y < y1; y++) for (int x = x0; x < x1; x++) {
        float ux, uy; ctm_apply(inv, x + 0.5f, c->h - (y + 0.5f), &ux, &uy); float u = (ux - rx) / rw, v = 1.f - (uy - ry) / rh; if (u < 0 || u >= 1 || v < 0 || v >= 1) continue;
        float fx = u * im->w - 0.5f, fy = v * im->h - 0.5f; int ix = (int)floorf(fx), iy = (int)floorf(fy); float tx = fx - ix, ty = fy - iy; float acc[4] = {0, 0, 0, 0};
        for (int j = 0; j < 2; j++) for (int i = 0; i < 2; i++) { int sx = ix + i < 0 ? 0 : (ix + i >= im->w ? im->w - 1 : ix + i), sy = iy + j < 0 ? 0 : (iy + j >= im->h ? im->h - 1 : iy + j); float wgt = (i ? tx : 1 - tx) * (j ? ty : 1 - ty); const uint8_t *p = im->rgba + ((size_t)sy * im->w + sx) * 4; float pa = p[3] / 255.f;
            acc[0] += p[0] * pa * wgt; acc[1] += p[1] * pa * wgt; acc[2] += p[2] * pa * wgt; acc[3] += pa * wgt; }
        if (acc[3] > 0.0001f) cg_blend(c, x, y, acc[0] / acc[3] / 255.f, acc[1] / acc[3] / 255.f, acc[2] / acc[3] / 255.f, acc[3] * c->alpha);
    }
}
static void cg_fill_rect(CGCtx *c, float rx, float ry, float rw, float rh) {
    float xs[4] = {rx, rx + rw, rx + rw, rx}, ys[4] = {ry, ry, ry + rh, ry + rh}, cx[4], cy[4]; for (int i = 0; i < 4; i++) { float ux, uy; ctm_apply(c->ctm, xs[i], ys[i], &ux, &uy); cx[i] = ux; cy[i] = c->h - uy; }
    int x0 = (int)floorf(fminf(fminf(cx[0], cx[1]), fminf(cx[2], cx[3]))), x1 = (int)ceilf(fmaxf(fmaxf(cx[0], cx[1]), fmaxf(cx[2], cx[3]))), y0 = (int)floorf(fminf(fminf(cy[0], cy[1]), fminf(cy[2], cy[3]))), y1 = (int)ceilf(fmaxf(fmaxf(cy[0], cy[1]), fmaxf(cy[2], cy[3])));
    for (int y = y0 < 0 ? 0 : y0; y < y1 && y < c->h; y++) for (int x = x0 < 0 ? 0 : x0; x < x1 && x < c->w; x++) cg_blend(c, x, y, c->fill[0], c->fill[1], c->fill[2], c->fill[3] * c->alpha);
}
static void cg_clear_rect(CGCtx *c, float rx, float ry, float rw, float rh) {
    float ux, uy; ctm_apply(c->ctm, rx, ry, &ux, &uy); float ux2, uy2; ctm_apply(c->ctm, rx + rw, ry + rh, &ux2, &uy2); int x0 = (int)floorf(fminf(ux, ux2)), x1 = (int)ceilf(fmaxf(ux, ux2)), y0 = (int)floorf(c->h - fmaxf(uy, uy2)), y1 = (int)ceilf(c->h - fminf(uy, uy2));
    for (int y = y0 < 0 ? 0 : y0; y < y1 && y < c->h; y++) { int xs = x0 < 0 ? 0 : x0, xe = x1 > c->w ? c->w : x1; if (xe > xs) memset((uint8_t *)H(c->data) + (size_t)y * c->rb + (size_t)xs * c->bpp, 0, (size_t)(xe - xs) * c->bpp); }
}
static void cg_text(CGCtx *c, float x, float y, const char *text, int len) {
    char buf[512]; if (len > 511) len = 511; memcpy(buf, text, len); buf[len] = 0; TTF_Font *f = font_for(c->font, c->fsize); if (!f) return; SDL_Color col = {255, 255, 255, 255};
    SDL_Surface *s = TTF_RenderUTF8_Blended(f, buf, col); if (!s) return; SDL_Surface *r = SDL_ConvertSurfaceFormat(s, SDL_PIXELFORMAT_ABGR8888, 0); SDL_FreeSurface(s); if (!r) return;
    float ux, uy; ctm_apply(c->ctm, x, y, &ux, &uy); int dx = (int)ux, dy = (int)(c->h - uy) - TTF_FontAscent(f);
    for (int j = 0; j < r->h; j++) for (int i = 0; i < r->w; i++) { uint8_t *p = (uint8_t *)r->pixels + (size_t)j * r->pitch + i * 4; if (p[3]) cg_blend(c, dx + i, dy + j, c->fill[0], c->fill[1], c->fill[2], p[3] / 255.f * c->fill[3] * c->alpha); }
    SDL_FreeSurface(r);
}
/* ARGB helpers for arguments: CGFloat is float on armv7 (softfp: bits in core registers; 64-bit ints unaligned) */
#define AF(n) f_of(arg(n))
static void ret_ctm(Ctm m) { gptr p = arg(0); float v[6] = {m.a, m.b, m.c, m.d, m.tx, m.ty}; for (int i = 0; i < 6; i++) wr32(p + 4 * i, w_of(v[i])); }
static Ctm arg_ctm(int first) { Ctm m; float *f = &m.a; for (int i = 0; i < 6; i++) f[i] = f_of(arg(first + i)); return m; }

static void cg_ColorSpace(void) { ret(0xC5000001u); } static void cg_noop(void) { } static void cg_zero(void) { ret(0); } static void cg_one(void) { ret(1); }
static void cg_BitmapContextCreate(void) { ret(cg_new(arg(0), arg(1), arg(2), arg(4), arg(6), arg(3))); }
static void cg_ContextRelease(void) { CGCtx *c = cg_of(arg(0));
    if (c && getenv("RA_CGDUMP")) { static int n; char fn[256]; snprintf(fn, sizeof fn, "%s/cg_%03d_%dx%d_bpp%d_a%d.pgm", getenv("RA_CGDUMP"), n++, c->w, c->h, c->bpp, c->alphainfo); FILE *f = fopen(fn, "wb"); if (f) { fprintf(f, "P5 %d %d 255\n", c->w, c->h); for (int y = 0; y < c->h; y++) for (int x = 0; x < c->w; x++) fputc(((uint8_t *)H(c->data))[(size_t)y * c->rb + (size_t)x * c->bpp + (c->bpp == 4 ? 3 : 0)], f); fclose(f); } }
    if (c) { c->live = 0; } }
static void cg_ClearRect(void) { CGCtx *c = cg_of(arg(0)); if (c) cg_clear_rect(c, AF(1), AF(2), AF(3), f_of(rd32(reg(13)))); }
static void cg_DrawImage(void) { CGCtx *c = cg_of(arg(0)); gptr img = rd32(reg(13) + 4); if (c) cg_draw_image(c, AF(1), AF(2), AF(3), f_of(rd32(reg(13))), img_of(img), 0); }
static void cg_FillRect(void) { CGCtx *c = cg_of(arg(0)); if (c) cg_fill_rect(c, AF(1), AF(2), AF(3), f_of(rd32(reg(13)))); }
static void cg_Save(void) { CGCtx *c = cg_of(arg(0)); if (c && c->sp < 16) c->stack[c->sp++] = c->ctm; }
static void cg_Restore(void) { CGCtx *c = cg_of(arg(0)); if (c && c->sp > 0) c->ctm = c->stack[--c->sp]; }
static void cg_Translate(void) { CGCtx *c = cg_of(arg(0)); if (c) c->ctm = ctm_concat_t_first(c->ctm, (Ctm){1, 0, 0, 1, AF(1), AF(2)}); }
static void cg_Scale(void) { CGCtx *c = cg_of(arg(0)); if (c) c->ctm = ctm_concat_t_first(c->ctm, (Ctm){AF(1), 0, 0, AF(2), 0, 0}); }
static void cg_Concat(void) { CGCtx *c = cg_of(arg(0)); if (c) c->ctm = ctm_concat_t_first(c->ctm, arg_ctm(1)); }
static void cg_SetAlpha(void) { CGCtx *c = cg_of(arg(0)); if (c) c->alpha = AF(1); }
static void cg_SetRGBFill(void) { CGCtx *c = cg_of(arg(0)); if (c) { c->fill[0] = AF(1); c->fill[1] = AF(2); c->fill[2] = AF(3); c->fill[3] = f_of(rd32(reg(13))); } }
static void cg_SetGrayFill(void) { CGCtx *c = cg_of(arg(0)); if (c) { c->fill[0] = c->fill[1] = c->fill[2] = AF(1); c->fill[3] = AF(2); } }
static void cg_SetFillColor(void) { CGCtx *c = cg_of(arg(0)); if (c) for (int i = 0; i < 4; i++) c->fill[i] = f_of(rd32(arg(1) + 4 * i)); }
static void cg_SetFillColorWithColor(void) { CGCtx *c = cg_of(arg(0)); if (c) color_get(arg(1), c->fill); }
static void cg_SelectFont(void) { CGCtx *c = cg_of(arg(0)); if (c) { strncpy(c->font, gstr(arg(1)), 63); c->fsize = AF(2); } }
static void cg_ShowTextAtPoint(void) { CGCtx *c = cg_of(arg(0)); if (c) cg_text(c, AF(1), AF(2), gstr(arg(3)), arg(4)); }
static void cg_ColorCreate(void) { gptr o = gm_malloc(32); wr32(o, cls_UIColor); for (int i = 0; i < 4; i++) wr32(o + 4 + 4 * i, rd32(arg(1) + 4 * i)); ret(o); }
static void cg_ColorGetComponents(void) { ret(arg(0) + 4); }
static void cg_ColorGetNum(void) { ret(4); }
/* CGImage == our UIImage object */
static void cgi_Width(void) { ImageRec *i = img_of(arg(0)); ret(i ? i->w : 0); }
static void cgi_Height(void) { ImageRec *i = img_of(arg(0)); ret(i ? i->h : 0); }
static void cgi_Bpc(void) { ret(8); } static void cgi_Bpp(void) { ret(32); } static void cgi_BitmapInfo(void) { ret(1); }
static void cgi_InRect(void) { ImageRec *im = img_of(arg(0)); if (!im || nimg >= 4096) { ret(0); return; } int x = (int)AF(1), y = (int)AF(2), w = (int)AF(3), h = (int)f_of(rd32(reg(13)));
    if (x < 0) x = 0; if (y < 0) y = 0; if (x + w > im->w) w = im->w - x; if (y + h > im->h) h = im->h - y; if (w <= 0 || h <= 0) { ret(0); return; }
    uint8_t *px = malloc((size_t)w * h * 4); for (int j = 0; j < h; j++) memcpy(px + (size_t)j * w * 4, im->rgba + ((size_t)(y + j) * im->w + x) * 4, (size_t)w * 4);
    snprintf(images[nimg].name, 199, "#crop%d", nimg); images[nimg].rgba = px; images[nimg].w = w; images[nimg].h = h; ret(new_image(nimg++)); }
static gptr image_from_ctx(CGCtx *c) {
    if (!c || nimg >= 4096) return 0; uint8_t *px = malloc((size_t)c->w * c->h * 4);
    for (int y = 0; y < c->h; y++) for (int x = 0; x < c->w; x++) { const uint8_t *s = (uint8_t *)H(c->data) + (size_t)y * c->rb + (size_t)x * c->bpp; uint8_t *d = px + ((size_t)y * c->w + x) * 4;
        if (c->bpp == 1) { d[0] = d[1] = d[2] = c->alphainfo == 7 ? 255 : s[0]; d[3] = c->alphainfo == 7 ? s[0] : 255; }
        else { int first = (c->alphainfo == 2 || c->alphainfo == 4 || c->alphainfo == 6); const uint8_t *sr = s + (first ? 1 : 0); uint8_t a = (c->alphainfo == 5 || c->alphainfo == 6) ? 255 : (first ? s[0] : s[3]);
            if ((c->alphainfo == 1 || c->alphainfo == 2) && a && a < 255) { d[0] = sr[0] * 255 / a > 255 ? 255 : sr[0] * 255 / a; d[1] = sr[1] * 255 / a > 255 ? 255 : sr[1] * 255 / a; d[2] = sr[2] * 255 / a > 255 ? 255 : sr[2] * 255 / a; } else { d[0] = sr[0]; d[1] = sr[1]; d[2] = sr[2]; } d[3] = a; } }
    snprintf(images[nimg].name, 199, "#ctx%d", nimg); images[nimg].rgba = px; images[nimg].w = c->w; images[nimg].h = c->h; return new_image(nimg++);
}
static void cg_BitmapContextCreateImage(void) { ret(image_from_ctx(cg_of(arg(0)))); }
static void ug_Begin(void) { int w = (int)AF(0), h = (int)AF(1); gptr c = cg_new(0, w < 1 ? 1 : w, h < 1 ? 1 : h, 0, 1, 8); CGCtx *cc = cg_of(c); if (cc) { cc->ctm = (Ctm){1, 0, 0, -1, 0, (float)cc->h}; } s_cur_ui_ctx = c; }   /* UIKit contexts are y-down */
static gptr ui_ctx_stack[16]; static int ui_ctx_n;
static void ug_Push(void) { if (ui_ctx_n < 16) ui_ctx_stack[ui_ctx_n++] = s_cur_ui_ctx; s_cur_ui_ctx = arg(0); ret(0); }
static void ug_Pop(void) { if (ui_ctx_n > 0) s_cur_ui_ctx = ui_ctx_stack[--ui_ctx_n]; ret(0); }
static void ug_GetCurrent(void) { ret(s_cur_ui_ctx); }
static void ug_GetImage(void) { ret(image_from_ctx(cg_of(s_cur_ui_ctx))); }
static void ug_End(void) { CGCtx *c = cg_of(s_cur_ui_ctx); if (c) c->live = 0; s_cur_ui_ctx = 0; }
/* geometry helpers: CGRect by value = 4 floats in r0..r3 */
static void ret_f(float f) { ret(w_of(f)); }
static void r_GetHeight(void) { ret_f(AF(3)); } static void r_GetWidth(void) { ret_f(AF(2)); } static void r_GetMinX(void) { ret_f(AF(0)); } static void r_GetMinY(void) { ret_f(AF(1)); }
static void r_GetMaxX(void) { ret_f(AF(0) + AF(2)); } static void r_GetMidX(void) { ret_f(AF(0) + AF(2) / 2); } static void r_GetMidY(void) { ret_f(AF(1) + AF(3) / 2); }
static void r_Inset(void) { gptr p = arg(0); float x = AF(1), y = AF(2), w = AF(3), h = f_of(rd32(reg(13))), dx = f_of(rd32(reg(13) + 4)), dy = f_of(rd32(reg(13) + 8)); wr32(p, w_of(x + dx)); wr32(p + 4, w_of(y + dy)); wr32(p + 8, w_of(w - 2 * dx)); wr32(p + 12, w_of(h - 2 * dy)); }
static void r_Offset(void) { gptr p = arg(0); float x = AF(1), y = AF(2), w = AF(3), h = f_of(rd32(reg(13))), dx = f_of(rd32(reg(13) + 4)), dy = f_of(rd32(reg(13) + 8)); wr32(p, w_of(x + dx)); wr32(p + 4, w_of(y + dy)); wr32(p + 8, w_of(w)); wr32(p + 12, w_of(h)); }
static void r_Integral(void) { gptr p = arg(0); float x = AF(1), y = AF(2), w = AF(3), h = f_of(rd32(reg(13))); float x0 = floorf(x), y0 = floorf(y), x1 = ceilf(x + w), y1 = ceilf(y + h); wr32(p, w_of(x0)); wr32(p + 4, w_of(y0)); wr32(p + 8, w_of(x1 - x0)); wr32(p + 12, w_of(y1 - y0)); }
static void r_IsNull(void) { ret(isinf(AF(0)) || isinf(AF(1))); }
static void r_Equal(void) { ret(AF(0) == f_of(rd32(reg(13))) && AF(1) == f_of(rd32(reg(13) + 4)) && AF(2) == f_of(rd32(reg(13) + 8)) && AF(3) == f_of(rd32(reg(13) + 12))); }
static void r_ContainsPoint(void) { float x = AF(0), y = AF(1), w = AF(2), h = AF(3), px = f_of(rd32(reg(13))), py = f_of(rd32(reg(13) + 4)); ret(px >= x && px < x + w && py >= y && py < y + h); }
static void r_Intersection(void) { gptr p = arg(0); float x = AF(1), y = AF(2), w = AF(3), h = f_of(rd32(reg(13))), X = f_of(rd32(reg(13) + 4)), Y = f_of(rd32(reg(13) + 8)), W = f_of(rd32(reg(13) + 12)), Hh = f_of(rd32(reg(13) + 16));
    float x0 = fmaxf(x, X), y0 = fmaxf(y, Y), x1 = fminf(x + w, X + W), y1 = fminf(y + h, Y + Hh); if (x1 < x0 || y1 < y0) { x0 = y0 = INFINITY; x1 = y1 = 0; } wr32(p, w_of(x0)); wr32(p + 4, w_of(y0)); wr32(p + 8, w_of(x1 - x0)); wr32(p + 12, w_of(y1 - y0)); }
static void r_Union(void) { gptr p = arg(0); float x = AF(1), y = AF(2), w = AF(3), h = f_of(rd32(reg(13))), X = f_of(rd32(reg(13) + 4)), Y = f_of(rd32(reg(13) + 8)), W = f_of(rd32(reg(13) + 12)), Hh = f_of(rd32(reg(13) + 16));
    float x0 = fminf(x, X), y0 = fminf(y, Y), x1 = fmaxf(x + w, X + W), y1 = fmaxf(y + h, Y + Hh); wr32(p, w_of(x0)); wr32(p + 4, w_of(y0)); wr32(p + 8, w_of(x1 - x0)); wr32(p + 12, w_of(y1 - y0)); }
static void r_FromString(void) { gptr p = arg(0); float v[4] = {0}; const char *s = scstr(arg(1)); int k = 0; for (const char *q = s; *q && k < 4;) { if ((*q >= '0' && *q <= '9') || *q == '-' || *q == '.') { v[k++] = strtof(q, (char **)&q); } else q++; } for (int i = 0; i < 4; i++) wr32(p + 4 * i, w_of(v[i])); }
static void r_ApplyAffine(void) { gptr p = arg(0); float x = AF(1), y = AF(2), w = AF(3), h = f_of(rd32(reg(13))); Ctm m; float *f = &m.a; for (int i = 0; i < 6; i++) f[i] = f_of(rd32(reg(13) + 4 + 4 * i));
    float xs[4] = {x, x + w, x + w, x}, ys[4] = {y, y, y + h, y + h}, ox[4], oy[4]; for (int i = 0; i < 4; i++) ctm_apply(m, xs[i], ys[i], &ox[i], &oy[i]); float x0 = fminf(fminf(ox[0], ox[1]), fminf(ox[2], ox[3])), x1 = fmaxf(fmaxf(ox[0], ox[1]), fmaxf(ox[2], ox[3])), y0 = fminf(fminf(oy[0], oy[1]), fminf(oy[2], oy[3])), y1 = fmaxf(fmaxf(oy[0], oy[1]), fmaxf(oy[2], oy[3]));
    wr32(p, w_of(x0)); wr32(p + 4, w_of(y0)); wr32(p + 8, w_of(x1 - x0)); wr32(p + 12, w_of(y1 - y0)); }
static void t_MakeScale(void) { ret_ctm((Ctm){AF(1), 0, 0, AF(2), 0, 0}); } static void t_MakeTranslation(void) { ret_ctm((Ctm){1, 0, 0, 1, AF(1), AF(2)}); }
static void t_Rotate(void) { Ctm t = arg_ctm(1); float a = f_of(rd32(reg(13) + 12)); Ctm r = {cosf(a), sinf(a), -sinf(a), cosf(a), 0, 0}; ret_ctm(ctm_mul(t, r)); }   /* rotate first, then t */
static void t_Scale(void) { Ctm t = arg_ctm(1); float sx = f_of(rd32(reg(13) + 12)), sy = f_of(rd32(reg(13) + 16)); ret_ctm(ctm_mul(t, (Ctm){sx, 0, 0, sy, 0, 0})); }
static void t_Translate(void) { Ctm t = arg_ctm(1); float tx = f_of(rd32(reg(13) + 12)), ty = f_of(rd32(reg(13) + 16)); ret_ctm(ctm_mul(t, (Ctm){1, 0, 0, 1, tx, ty})); }
static void t_Concat(void) { Ctm a = arg_ctm(1); Ctm b; float *f = &b.a; for (int i = 0; i < 6; i++) f[i] = f_of(rd32(reg(13) + 12 + 4 * i)); ret_ctm(ctm_mul(b, a)); }

/* image -> NSData: the only caller is the avatar cache writing files, so empty data is enough */
static gptr new_data(const void *p, u32 n);
static void ui_ImageData(void) { ret(new_data("", 0)); }
static const HleEntry cg_tab[] = {
    {"CGColorSpaceCreateDeviceRGB", cg_ColorSpace}, {"CGColorSpaceCreateDeviceGray", cg_ColorSpace}, {"CGColorSpaceRelease", cg_noop},
    {"CGBitmapContextCreate", cg_BitmapContextCreate}, {"CGContextRelease", cg_ContextRelease}, {"CGContextClearRect", cg_ClearRect}, {"CGContextDrawImage", cg_DrawImage}, {"CGContextFillRect", cg_FillRect},
    {"CGContextSaveGState", cg_Save}, {"CGContextRestoreGState", cg_Restore}, {"CGContextTranslateCTM", cg_Translate}, {"CGContextScaleCTM", cg_Scale}, {"CGContextConcatCTM", cg_Concat}, {"CGContextSetAlpha", cg_SetAlpha},
    {"CGContextSetRGBFillColor", cg_SetRGBFill}, {"CGContextSetGrayFillColor", cg_SetGrayFill}, {"CGContextSetFillColor", cg_SetFillColor}, {"CGContextSetFillColorWithColor", cg_SetFillColorWithColor},
    {"CGContextSelectFont", cg_SelectFont}, {"CGContextShowTextAtPoint", cg_ShowTextAtPoint},
    {"CGColorCreate", cg_ColorCreate}, {"CGColorGetComponents", cg_ColorGetComponents}, {"CGColorGetNumberOfComponents", cg_ColorGetNum}, {"CGColorRelease", cg_noop},
    {"CGImageGetWidth", cgi_Width}, {"CGImageGetHeight", cgi_Height}, {"CGImageGetBitsPerComponent", cgi_Bpc}, {"CGImageGetBitsPerPixel", cgi_Bpp}, {"CGImageGetBitmapInfo", cgi_BitmapInfo}, {"CGImageGetColorSpace", cg_ColorSpace},
    {"CGImageCreateWithImageInRect", cgi_InRect}, {"UIImagePNGRepresentation", ui_ImageData}, {"UIImageJPEGRepresentation", ui_ImageData}, {"CGImageRelease", cg_noop}, {"CGBitmapContextCreateImage", cg_BitmapContextCreateImage},
    {"UIGraphicsBeginImageContext", ug_Begin}, {"UIGraphicsBeginImageContextWithOptions", ug_Begin}, {"UIGraphicsGetCurrentContext", ug_GetCurrent}, {"UIGraphicsGetImageFromCurrentImageContext", ug_GetImage}, {"UIGraphicsEndImageContext", ug_End},
    {"UIGraphicsPushContext", ug_Push}, {"UIGraphicsPopContext", ug_Pop},
    {"CGRectGetHeight", r_GetHeight}, {"CGRectGetWidth", r_GetWidth}, {"CGRectGetMinX", r_GetMinX}, {"CGRectGetMinY", r_GetMinY}, {"CGRectGetMaxX", r_GetMaxX}, {"CGRectGetMidX", r_GetMidX}, {"CGRectGetMidY", r_GetMidY},
    {"CGRectInset", r_Inset}, {"CGRectOffset", r_Offset}, {"CGRectIntegral", r_Integral}, {"CGRectIsNull", r_IsNull}, {"CGRectEqualToRect", r_Equal}, {"CGRectContainsPoint", r_ContainsPoint}, {"CGRectIntersection", r_Intersection},
    {"CGRectUnion", r_Union}, {"CGRectFromString", r_FromString}, {"CGRectApplyAffineTransform", r_ApplyAffine},
    {"CGAffineTransformMakeScale", t_MakeScale}, {"CGAffineTransformMakeTranslation", t_MakeTranslation}, {"CGAffineTransformRotate", t_Rotate}, {"CGAffineTransformScale", t_Scale}, {"CGAffineTransformTranslate", t_Translate}, {"CGAffineTransformConcat", t_Concat},
    /* vector drawing is not rasterised yet (no-ops): paths, strokes, clips, gradients, shadows, layers */
    {"CGContextSetShadow", cg_noop}, {"CGContextSetShadowWithColor", cg_noop}, {"CGContextSetInterpolationQuality", cg_noop}, {"CGContextSetLineWidth", cg_noop}, {"CGContextSetStrokeColor", cg_noop}, {"CGContextSetRGBStrokeColor", cg_noop},
    {"CGContextSetStrokeColorSpace", cg_noop}, {"CGContextSetStrokeColorWithColor", cg_noop}, {"CGContextSetTextMatrix", cg_noop}, {"CGContextSetTextDrawingMode", cg_noop}, {"CGContextAddArc", cg_noop}, {"CGContextAddArcToPoint", cg_noop},
    {"CGContextAddCurveToPoint", cg_noop}, {"CGContextAddEllipseInRect", cg_noop}, {"CGContextAddLineToPoint", cg_noop}, {"CGContextAddRect", cg_noop}, {"CGContextBeginPath", cg_noop}, {"CGContextClosePath", cg_noop}, {"CGContextClip", cg_noop},
    {"CGContextClipToMask", cg_noop}, {"CGContextClipToRect", cg_noop}, {"CGContextFillPath", cg_noop}, {"CGContextMoveToPoint", cg_noop}, {"CGContextStrokePath", cg_noop}, {"CGContextStrokeLineSegments", cg_noop}, {"CGContextFillEllipseInRect", cg_noop},
    {"CGContextDrawLinearGradient", cg_noop}, {"CGContextDrawLayerAtPoint", cg_noop}, {"CGGradientCreateWithColorComponents", cg_one}, {"CGGradientRelease", cg_noop}, {"CGLayerCreateWithContext", cg_one}, {"CGLayerGetContext", cg_one}, {"CGLayerRelease", cg_noop},
    {"UIImagePNGRepresentation", cg_zero}, {"UIImageJPEGRepresentation", cg_zero},
    {NULL, NULL}
};
