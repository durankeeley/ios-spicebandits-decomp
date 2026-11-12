extern unsigned sdl_fbo_tex(void);
/* gles.c - OpenGL ES 1.1 (fixed-point) -> desktop OpenGL compatibility profile */
#include "ra.h"
#include <SDL2/SDL.h>
#define GL_GLEXT_PROTOTYPES 1
#include <GL/gl.h>
#include <GL/glext.h>

#define FX(v) ((float)(s32)(v) / 65536.0f)
extern void sdl_win_size(int *w, int *h);
static int log_gl;

/* client array state (GL_FIXED needs conversion) */
typedef struct { int enabled, size, type, stride; gptr ptr; } CArr;
static CArr va[4]; /* 0 vertex 1 color 2 normal 3 texcoord (unit0 only) */
static int client_tex_unit;
static int max_tex_units = 2;

static const char *arrname[4] = {"vertex", "color", "normal", "texcoord"};
static int type_size(int t) { switch (t) { case 0x1400: case 0x1401: return 1; case 0x1402: case 0x1403: return 2; case 0x140C: case 0x1406: return 4; default: return 4; } }

/* ---- simple state fns ---- */
#define V0(name, call) static void h_##name(void) { call; }
V0(glActiveTexture, glActiveTexture(arg(0)))
V0(glBindTexture, glBindTexture(arg(0), arg(1)))
V0(glBlendFunc, glBlendFunc(arg(0), arg(1)))
V0(glClear, glClear(arg(0)))
V0(glClearColorx, glClearColor(FX(arg(0)), FX(arg(1)), FX(arg(2)), FX(arg(3))))
V0(glColor4x, glColor4f(FX(arg(0)), FX(arg(1)), FX(arg(2)), FX(arg(3))))
V0(glCullFace, glCullFace(arg(0)))
V0(glDepthFunc, glDepthFunc(arg(0)))
V0(glDepthMask, glDepthMask(arg(0)))
V0(glDisable, glDisable(arg(0)))
V0(glEnable, glEnable(arg(0)))
V0(glFrontFace, glFrontFace(arg(0)))
V0(glHint, glHint(arg(0), arg(1)))
V0(glLoadIdentity, glLoadIdentity())
V0(glMatrixMode, glMatrixMode(arg(0)))
V0(glPopMatrix, glPopMatrix())
V0(glPushMatrix, glPushMatrix())
V0(glShadeModel, glShadeModel(arg(0)))
V0(glPixelStorei, glPixelStorei(arg(0), arg(1)))
V0(glRotatex, glRotatef(FX(arg(0)), FX(arg(1)), FX(arg(2)), FX(arg(3))))
V0(glScalex, glScalef(FX(arg(0)), FX(arg(1)), FX(arg(2))))
V0(glTranslatex, glTranslatef(FX(arg(0)), FX(arg(1)), FX(arg(2))))
V0(glAlphaFuncx, glAlphaFunc(arg(0), FX(arg(1))))
/* Texture names live in one GL namespace shared with the host's own UI textures. Track what the guest generated and only honour a delete for a name it still owns:
 * the game can delete a name twice (level teardown + Texture2D dealloc); by then the host may have reused the id for a UI image, which then turned white. */
static unsigned char guest_tex[1 << 16];
static void h_glDeleteTextures(void) { int n = arg(0); const GLuint *ids = (const GLuint *)H(arg(1)); for (int i = 0; i < n; i++) { if (ids[i] == sdl_fbo_tex()) { fprintf(stderr, "[gl] guest tried to delete the host render-target texture %u (ignored)\n", ids[i]); continue; }
    if (ids[i] >= (1u << 16) || !guest_tex[ids[i]]) continue; guest_tex[ids[i]] = 0; glDeleteTextures(1, &ids[i]); } }
static void h_glGenTextures(void) { glGenTextures(arg(0), (GLuint *)H(arg(1))); for (u32 i = 0; i < arg(0); i++) { GLuint t = ((GLuint *)H(arg(1)))[i]; if (t < (1u << 16)) guest_tex[t] = 1; } static int n; if (n++ < 6) fprintf(stderr, "[gl] guest genTextures -> %u (host fbo tex %u)\n", ((GLuint *)H(arg(1)))[0], sdl_fbo_tex()); }
V0(glTexParameterx, glTexParameteri(arg(0), arg(1), arg(2)))
static void h_glViewport(void) { glViewport(arg(0), arg(1), arg(2), arg(3)); }
static void h_glScissor(void) { glScissor(arg(0), arg(1), arg(2), arg(3)); }
static void h_glFrustumx(void) { glFrustum(FX(arg(0)), FX(arg(1)), FX(arg(2)), FX(arg(3)), FX(arg(4)), FX(arg(5))); }
static void mat16(gptr p, float *m) { for (int i = 0; i < 16; i++) m[i] = FX(rd32(p + 4 * i)); }
static void h_glLoadMatrixx(void) { float m[16]; mat16(arg(0), m); glLoadMatrixf(m); }
static void h_glMultMatrixx(void) { float m[16]; mat16(arg(0), m); glMultMatrixf(m); }
static void h_glTexEnvxv(void) {
    u32 target = arg(0), pname = arg(1); gptr p = arg(2);
    if (pname == 0x2201) { float c[4]; for (int i = 0; i < 4; i++) c[i] = FX(rd32(p + 4 * i)); glTexEnvfv(target, pname, c); }
    else glTexEnvi(target, pname, rd32(p));
}
static void h_glMaterialx(void) { glMaterialf(arg(0), arg(1), FX(arg(2))); }
static void h_glMaterialxv(void) { float c[4]; for (int i = 0; i < 4; i++) c[i] = FX(rd32(arg(2) + 4 * i)); glMaterialfv(arg(0), arg(1), c); }
#define FL(n) f_of32(arg(n))
static float f_of32(u32 w) { float f; memcpy(&f, &w, 4); return f; }
static void h_glClearColor(void) { glClearColor(FL(0), FL(1), FL(2), FL(3)); }
static void h_glColor4f(void) { glColor4f(FL(0), FL(1), FL(2), FL(3)); }
static void h_glOrthof(void) { glOrtho(FL(0), FL(1), FL(2), FL(3), FL(4), FL(5)); }
static void h_glFrustumf(void) { glFrustum(FL(0), FL(1), FL(2), FL(3), FL(4), FL(5)); }
static void h_glTranslatef(void) { glTranslatef(FL(0), FL(1), FL(2)); }
static void h_glScalef(void) { glScalef(FL(0), FL(1), FL(2)); }
static void h_glRotatef(void) { glRotatef(FL(0), FL(1), FL(2), FL(3)); }
static void h_glLoadMatrixf(void) { glLoadMatrixf((const GLfloat *)H(arg(0))); }
static void h_glMultMatrixf(void) { glMultMatrixf((const GLfloat *)H(arg(0))); }
static void h_glTexParameterf(void) { glTexParameterf(arg(0), arg(1), FL(2)); }
static void h_glTexParameteri(void) { glTexParameteri(arg(0), arg(1), arg(2)); }
static void h_glFlush(void) { }
static void h_glColorMask(void) { glColorMask(arg(0) & 1, arg(1) & 1, arg(2) & 1, arg(3) & 1); }
static void h_glClearStencil(void) { glClearStencil(arg(0)); }
static void h_glStencilFunc(void) { glStencilFunc(arg(0), arg(1), arg(2)); }
static void h_glStencilOp(void) { glStencilOp(arg(0), arg(1), arg(2)); }
static void h_glBlendEquationOES(void) { glBlendEquation(arg(0)); }
static void h_glIsTexture(void) { ret(glIsTexture(arg(0))); }
static void h_glGetFloatv(void) { GLfloat v[16] = {0}; glGetFloatv(arg(0), v); for (int i = 0; i < 16; i++) wr32(arg(1) + 4 * i, *(u32 *)&v[i]); }
static void h_glGetError(void) { ret(0); }
static void h_glGetString(void) { static gptr s[8]; u32 n = arg(0); const char *r = (const char *)glGetString(n);
    if (n == 0x1F03) r = "GL_OES_framebuffer_object GL_IMG_texture_compression_pvrtc GL_OES_compressed_paletted_texture";   /* extensions */
    int k = n & 7; if (!s[k]) s[k] = gm_strdup(r ? r : ""); ret(s[k]); }
static void h_glGetIntegerv(void) {
    u32 pn = arg(0); gptr o = arg(1); GLint v = 0;
    if (pn == 0x84E2 /*MAX_TEXTURE_UNITS*/) { v = 2; } else if (pn == 0x0D33 /*MAX_TEXTURE_SIZE*/) v = 2048;
    else if (pn == 0x86A2 /*NUM_COMPRESSED_TEXTURE_FORMATS*/) v = 4;
    else if (pn == 0x86A3 /*COMPRESSED_TEXTURE_FORMATS*/) { u32 f[4] = {0x8C00, 0x8C01, 0x8C02, 0x8C03}; memcpy(H(o), f, 16); return; }
    else glGetIntegerv(pn, &v);
    wr32(o, v);
}

/* ---- OES framebuffer: render to the window's default framebuffer ---- */
static void h_fb_noop(void) { }
extern void sdl_bind_backbuffer(void);
static void h_bindfb(void) { sdl_bind_backbuffer(); }
static void h_glGenFramebuffersOES(void) { for (u32 i = 0; i < arg(0); i++) wr32(arg(1) + 4 * i, 1 + i); }
static void h_glGenRenderbuffersOES(void) { for (u32 i = 0; i < arg(0); i++) wr32(arg(1) + 4 * i, 1 + i); }
static void h_glCheckFramebufferStatusOES(void) { ret(0x8CD5); }
static void h_glGetRenderbufferParameterivOES(void) { int w, h; sdl_win_size(&w, &h); hle_log("[gl] renderbuffer size %dx%d\n", w, h); u32 pn = arg(1); wr32(arg(2), pn == 0x8D42 ? w : pn == 0x8D43 ? h : 0); }
extern int g_screen_w, g_screen_h;

/* ---- client arrays ---- */
static void h_glEnableClientState(void) { u32 a = arg(0); if (a == 0x8074) va[0].enabled = 1; else if (a == 0x8076) va[1].enabled = 1; else if (a == 0x8075) va[2].enabled = 1; else if (a == 0x8078) { if (!client_tex_unit) va[3].enabled = 1; } glEnableClientState(a); }
static void h_glDisableClientState(void) { u32 a = arg(0); if (a == 0x8074) va[0].enabled = 0; else if (a == 0x8076) va[1].enabled = 0; else if (a == 0x8075) va[2].enabled = 0; else if (a == 0x8078) { if (!client_tex_unit) va[3].enabled = 0; } glDisableClientState(a); }
static void h_glClientActiveTexture(void) { client_tex_unit = arg(0) - 0x84C0; glClientActiveTexture(arg(0)); }
static void h_glVertexPointer(void) { va[0] = (CArr){va[0].enabled, arg(0), arg(1), arg(2), arg(3)}; }
static void h_glColorPointer(void) { va[1] = (CArr){va[1].enabled, arg(0), arg(1), arg(2), arg(3)}; }
static void h_glNormalPointer(void) { va[2] = (CArr){va[2].enabled, 3, arg(0), arg(1), arg(2)}; }
static void h_glTexCoordPointer(void) { if (client_tex_unit == 0) va[3] = (CArr){va[3].enabled, arg(0), arg(1), arg(2), arg(3)}; else { /* second unit: apply directly if not FIXED */ } }

static float *fxbuf[4];
/* bind arrays for vertices [first, first+count) ; converts GL_FIXED into temp float buffers */
static void bind_arrays(int first, int count) {
    for (int k = 0; k < 4; k++) {
        CArr *a = &va[k]; if (!a->enabled || !a->ptr) continue;
        int es = type_size(a->type) * a->size; int stride = a->stride ? a->stride : es;
        const uint8_t *base = (const uint8_t *)H(a->ptr);
        GLenum type = a->type; const void *ptr = base; int st = a->stride;
        if (a->type == 0x140C) {
            int n = first + count; fxbuf[k] = realloc(fxbuf[k], sizeof(float) * a->size * n);
            for (int v = first; v < n; v++) for (int c = 0; c < a->size; c++) { s32 x; memcpy(&x, base + (size_t)v * stride + 4 * c, 4); fxbuf[k][v * a->size + c] = x / 65536.0f; }
            ptr = fxbuf[k]; type = GL_FLOAT; st = 0;
        }
        switch (k) {
        case 0: glVertexPointer(a->size, type, st, ptr); break;
        case 1: glColorPointer(a->size, type, st, ptr); break;
        case 2: glNormalPointer(type, st, ptr); break;
        case 3: glClientActiveTexture(GL_TEXTURE0); glTexCoordPointer(a->size, type, st, ptr); glClientActiveTexture(0x84C0 + client_tex_unit); break;
        }
    }
}
static int gl_err_budget = 20;
static void gl_check(const char *what) { if (!getenv("RA_GLERR") || gl_err_budget <= 0) return; GLenum e = glGetError(); if (e) { gl_err_budget--; GLint fb = 0, vp[4]; glGetIntegerv(GL_FRAMEBUFFER_BINDING, &fb); glGetIntegerv(GL_VIEWPORT, vp); fprintf(stderr, "[glerr] %s -> 0x%x (fb=%d status=0x%x viewport %d,%d %dx%d)\n", what, e, fb, (unsigned)glCheckFramebufferStatus(GL_FRAMEBUFFER), vp[0], vp[1], vp[2], vp[3]); } }
static int dump_budget = 1 << 30;
static void gl_dump(int first, int count) {
    if (!getenv("RA_GLDUMP") || dump_budget <= 0) return; dump_budget--; GLfloat mv[16], pj[16]; GLint vp[4], tex = 0; glGetFloatv(GL_MODELVIEW_MATRIX, mv); glGetFloatv(GL_PROJECTION_MATRIX, pj); glGetIntegerv(GL_VIEWPORT, vp); glGetIntegerv(GL_TEXTURE_BINDING_2D, &tex);
    fprintf(stderr, "[gldump] draw first=%d count=%d tex=%d blend=%d tex2d=%d depth=%d viewport=%d,%d,%dx%d\n", first, count, tex, glIsEnabled(GL_BLEND), glIsEnabled(GL_TEXTURE_2D), glIsEnabled(GL_DEPTH_TEST), vp[0], vp[1], vp[2], vp[3]);
    fprintf(stderr, "   proj: %g %g %g %g | %g %g %g %g | %g %g %g %g | %g %g %g %g\n", pj[0], pj[1], pj[2], pj[3], pj[4], pj[5], pj[6], pj[7], pj[8], pj[9], pj[10], pj[11], pj[12], pj[13], pj[14], pj[15]);
    fprintf(stderr, "   mv:   %g %g %g %g | %g %g %g %g | %g %g %g %g | %g %g %g %g\n", mv[0], mv[1], mv[2], mv[3], mv[4], mv[5], mv[6], mv[7], mv[8], mv[9], mv[10], mv[11], mv[12], mv[13], mv[14], mv[15]);
    for (int k = 0; k < 3 && k < count; k++) { CArr *a = &va[0]; int stride = a->stride ? a->stride : 8; const float *v = (const float *)(H(a->ptr) + (size_t)(first + k) * stride); const uint8_t *c = va[1].ptr ? H(va[1].ptr) + (size_t)(first + k) * (va[1].stride ? va[1].stride : 4) : NULL;
        fprintf(stderr, "   v%d: pos=(%g,%g) col=%s%d,%d,%d,%d\n", k, v[0], v[1], c ? "" : "(none) ", c ? c[0] : 0, c ? c[1] : 0, c ? c[2] : 0, c ? c[3] : 0); }
}
static void h_glDrawArrays(void) { int first = arg(1), count = arg(2); gl_dump(first, count); bind_arrays(first, count); glDrawArrays(arg(0), first, count); gl_check("glDrawArrays"); }
static void h_glDrawElements(void) {
    u32 mode = arg(0), count = arg(1), type = arg(2); gptr idx = arg(3);
    int maxv = 0;
    for (u32 i = 0; i < count; i++) { int v = type == 0x1403 ? ((uint16_t *)H(idx))[i] : ((uint8_t *)H(idx))[i]; if (v > maxv) maxv = v; }
    bind_arrays(0, maxv + 1); glDrawElements(mode, count, type, H(idx));
}

/* ---- textures ---- */
void pvrtc_decode(const uint8_t *src, int w, int h, int bpp2, uint8_t *dst);   /* pvrtc.c */
static void h_glTexImage2D(void) {
    u32 target = arg(0), level = arg(1), ifmt = arg(2), w = arg(3), h = arg(4), border = arg(5), fmt = arg(6), type = arg(7); gptr px = arg(8);
    if (getenv("RA_GLTEX")) { GLint b = 0; glGetIntegerv(GL_TEXTURE_BINDING_2D, &b); fprintf(stderr, "[gltex] id=%d %ux%u lvl=%u ifmt=%x fmt=%x type=%x px=%x\n", b, w, h, level, ifmt, fmt, type, px); }
    glTexImage2D(target, level, ifmt, w, h, border, fmt, type, px ? H(px) : NULL);
}
static void h_glCompressedTexImage2D(void) {
    u32 target = arg(0), level = arg(1), fmt = arg(2), w = arg(3), h = arg(4), size = arg(6); gptr px = arg(7); (void)size;
    int two = (fmt == 0x8C01 || fmt == 0x8C03);
    if (fmt >= 0x8C00 && fmt <= 0x8C03) {
        uint8_t *rgba = malloc((size_t)w * h * 4); pvrtc_decode(H(px), w, h, two, rgba);
        glTexImage2D(target, level, GL_RGBA, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, rgba); free(rgba);
    } else fprintf(stderr, "[gl] unsupported compressed fmt %x\n", fmt);
}
static void h_glTexEnvx(void) { glTexEnvi(arg(0), arg(1), arg(2)); }

static const HleEntry gl_tab[] = {
#define E(n) {#n, h_##n}
    E(glActiveTexture), E(glBindTexture), E(glBlendFunc), E(glClear), E(glClearColorx), E(glColor4x), E(glCullFace), E(glDepthFunc), E(glDepthMask), E(glDisable), E(glEnable),
    E(glFrontFace), E(glHint), E(glLoadIdentity), E(glMatrixMode), E(glPopMatrix), E(glPushMatrix), E(glShadeModel), E(glPixelStorei), E(glRotatex), E(glScalex), E(glTranslatex),
    E(glAlphaFuncx), {"glDeleteTextures", h_glDeleteTextures}, E(glGenTextures), E(glTexParameterx), E(glViewport), E(glScissor), E(glFrustumx), E(glLoadMatrixx), E(glMultMatrixx), E(glTexEnvxv), E(glTexEnvx),
    E(glMaterialx), E(glMaterialxv), E(glGetError), E(glGetString), E(glGetIntegerv), E(glGenFramebuffersOES), E(glGenRenderbuffersOES), E(glCheckFramebufferStatusOES),
    E(glGetRenderbufferParameterivOES), E(glEnableClientState), E(glDisableClientState), E(glClientActiveTexture), E(glVertexPointer), E(glColorPointer), E(glNormalPointer),
    E(glTexCoordPointer), E(glDrawArrays), E(glDrawElements), E(glTexImage2D), E(glCompressedTexImage2D),
    E(glClearColor), E(glColor4f), E(glOrthof), E(glFrustumf), E(glTranslatef), E(glScalef), E(glRotatef), E(glLoadMatrixf), E(glMultMatrixf), E(glTexParameterf), E(glTexParameteri), E(glFlush), E(glColorMask), E(glClearStencil), E(glStencilFunc), E(glStencilOp), E(glBlendEquationOES), E(glIsTexture), E(glGetFloatv),
    {"glBindFramebufferOES", h_bindfb}, {"glBindRenderbufferOES", h_fb_noop}, {"glBindFramebuffer", h_bindfb}, {"glBindRenderbuffer", h_fb_noop}, {"glDeleteFramebuffers", h_fb_noop}, {"glDeleteRenderbuffers", h_fb_noop}, {"glFramebufferRenderbuffer", h_fb_noop}, {"glGenFramebuffers", h_glGenFramebuffersOES}, {"glGenRenderbuffers", h_glGenRenderbuffersOES}, {"glCheckFramebufferStatus", h_glCheckFramebufferStatusOES}, {"glGetRenderbufferParameteriv", h_glGetRenderbufferParameterivOES}, {"glDeleteFramebuffersOES", h_fb_noop}, {"glDeleteRenderbuffersOES", h_fb_noop},
    {"glFramebufferRenderbufferOES", h_fb_noop}, {"glRenderbufferStorageOES", h_fb_noop},
    {NULL, NULL}
};
void gles_init(void) { hle_register_table(gl_tab); log_gl = 0; (void)arrname; (void)max_tex_units; (void)log_gl; }
