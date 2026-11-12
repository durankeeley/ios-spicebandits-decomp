/* sdl.c - window, GL context, input (mouse -> touch) */
#include "ra.h"
#include <SDL2/SDL.h>
#include <SDL2/SDL_image.h>
#include <GL/gl.h>

#define GL_GLEXT_PROTOTYPES 1
#include <GL/glext.h>
int g_screen_w = 1024, g_screen_h = 768;      /* landscape window size (what the player sees) */
int g_buf_w = 1024, g_buf_h = 768;            /* native iPad portrait backing store the game renders into */
int g_rot = -1;                               /* +1: buffer shown rotated clockwise, -1: counter-clockwise (LandscapeRight) */
static GLuint fbo, fbo_tex, fbo_depth;
static PFNGLGENFRAMEBUFFERSPROC p_genfb; static PFNGLBINDFRAMEBUFFERPROC p_bindfb; static PFNGLFRAMEBUFFERTEXTURE2DPROC p_fbtex;
static PFNGLGENRENDERBUFFERSPROC p_genrb; static PFNGLBINDRENDERBUFFERPROC p_bindrb; static PFNGLRENDERBUFFERSTORAGEPROC p_rbstore; static PFNGLFRAMEBUFFERRENDERBUFFERPROC p_fbrb;
static int frame_no, shot_at = -1; static const char *shot_path;
static SDL_Window *win; static SDL_GLContext gl; static int win_w, win_h, mouse_down;
/* a click must stay "down" for a few frames: the engine polls the touch state once per frame, so a press+release inside one frame is lost */
static int up_pending, up_frame, up_x, up_y, down_frame;
void objc_post_touch(int phase, int x, int y);

void sdl_init(int w, int h) {
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_EVENTS)) { fprintf(stderr, "SDL_Init: %s\n", SDL_GetError()); exit(1); }
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_COMPATIBILITY);
    SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24); SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
    win = SDL_CreateWindow("Spice Bandits (iOS port)", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, w, h, SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE | SDL_WINDOW_SHOWN);
    if (!win) { fprintf(stderr, "window: %s\n", SDL_GetError()); exit(1); }
    { extern const char *g_bundle_dir; char ip[4096]; snprintf(ip, sizeof ip, "%s/../../iTunesArtwork", g_bundle_dir);   /* window icon = the IPA's iTunesArtwork (next to Payload/) */
      SDL_Surface *ic = IMG_Load(ip); if (!ic) { snprintf(ip, sizeof ip, "%s/iTunesArtwork", g_bundle_dir); ic = IMG_Load(ip); } if (ic) { SDL_SetWindowIcon(win, ic); SDL_FreeSurface(ic); } }
    gl = SDL_GL_CreateContext(win); if (!gl) { fprintf(stderr, "GL ctx: %s\n", SDL_GetError()); exit(1); }
    SDL_GL_SetSwapInterval(getenv("RA_NOVSYNC") ? 0 : 1);
    win_w = w; win_h = h; g_screen_w = w; g_screen_h = h;
    fprintf(stderr, "[sdl] GL: %s | %s\n", glGetString(GL_RENDERER), glGetString(GL_VERSION));
    p_genfb = (PFNGLGENFRAMEBUFFERSPROC)SDL_GL_GetProcAddress("glGenFramebuffers"); p_bindfb = (PFNGLBINDFRAMEBUFFERPROC)SDL_GL_GetProcAddress("glBindFramebuffer");
    p_fbtex = (PFNGLFRAMEBUFFERTEXTURE2DPROC)SDL_GL_GetProcAddress("glFramebufferTexture2D"); p_genrb = (PFNGLGENRENDERBUFFERSPROC)SDL_GL_GetProcAddress("glGenRenderbuffers");
    p_bindrb = (PFNGLBINDRENDERBUFFERPROC)SDL_GL_GetProcAddress("glBindRenderbuffer"); p_rbstore = (PFNGLRENDERBUFFERSTORAGEPROC)SDL_GL_GetProcAddress("glRenderbufferStorage");
    p_fbrb = (PFNGLFRAMEBUFFERRENDERBUFFERPROC)SDL_GL_GetProcAddress("glFramebufferRenderbuffer");
    if (getenv("RA_ROT")) g_rot = atoi(getenv("RA_ROT"));
    if (getenv("RA_SHOT")) { shot_path = getenv("RA_SHOT"); shot_at = getenv("RA_SHOT_FRAME") ? atoi(getenv("RA_SHOT_FRAME")) : 120; }
    glGenTextures(1, &fbo_tex); glBindTexture(GL_TEXTURE_2D, fbo_tex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, g_buf_w, g_buf_h, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR); glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    p_genrb(1, &fbo_depth); p_bindrb(GL_RENDERBUFFER, fbo_depth); p_rbstore(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, g_buf_w, g_buf_h);
    p_genfb(1, &fbo); p_bindfb(GL_FRAMEBUFFER, fbo);
    p_fbtex(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, fbo_tex, 0); p_fbrb(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, fbo_depth);
    glBindTexture(GL_TEXTURE_2D, 0);
}
void sdl_bind_backbuffer(void) { p_bindfb(GL_FRAMEBUFFER, fbo); }
static void write_ppm(const char *path, int w, int h) {
    uint8_t *px = malloc((size_t)w * h * 3); glReadBuffer(GL_BACK); glPixelStorei(GL_PACK_ALIGNMENT, 1); glReadPixels(0, 0, w, h, GL_RGB, GL_UNSIGNED_BYTE, px);
    FILE *f = fopen(path, "wb"); fprintf(f, "P6\n%d %d\n255\n", w, h);
    for (int y = h - 1; y >= 0; y--) fwrite(px + (size_t)y * w * 3, 1, w * 3, f); fclose(f); free(px);
}
GLuint sdl_fbo_tex(void) { return fbo_tex; }
void sdl_fbo_resize(int w, int h) {
    if (w <= 0 || h <= 0 || (w == g_buf_w && h == g_buf_h && fbo)) return; g_buf_w = w; g_buf_h = h;
    glBindTexture(GL_TEXTURE_2D, fbo_tex); glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL); glBindTexture(GL_TEXTURE_2D, 0);
    p_bindrb(GL_RENDERBUFFER, fbo_depth); p_rbstore(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, w, h);
}
void sdl_present(void) {
    if (getenv("RA_GLDUMP")) fprintf(stderr, "[gldump] ==== present frame %d\n", frame_no);
    if (frame_no == shot_at && shot_path && getenv("RA_FBOSHOT")) {   /* dump the guest's own render target */
        uint8_t *px = malloc((size_t)g_buf_w * g_buf_h * 4); p_bindfb(GL_FRAMEBUFFER, fbo); glReadPixels(0, 0, g_buf_w, g_buf_h, GL_RGBA, GL_UNSIGNED_BYTE, px); p_bindfb(GL_FRAMEBUFFER, 0);
        char fp[4200]; snprintf(fp, sizeof fp, "%s.fbo.ppm", shot_path); FILE *f = fopen(fp, "wb"); fprintf(f, "P6\n%d %d\n255\n", g_buf_w, g_buf_h);
        for (int y = g_buf_h - 1; y >= 0; y--) for (int x = 0; x < g_buf_w; x++) fwrite(px + ((size_t)y * g_buf_w + x) * 4, 1, 3, f); fclose(f); free(px); fprintf(stderr, "[sdl] fbo dump -> %s\n", fp); }
    int ww, wh; SDL_GL_GetDrawableSize(win, &ww, &wh); p_bindfb(GL_FRAMEBUFFER, 0); glViewport(0, 0, ww, wh);
    { extern void uikit_anim_tick(void); extern void uikit_layout(void); extern void uikit_composite(void); { extern void engine_poll(void); engine_poll(); } uikit_anim_tick(); uikit_layout(); uikit_composite(); }
    { extern int g_blkcnt_on; static int bs = -1; if (bs < 0) bs = getenv("RA_BLKCNT") ? atoi(getenv("RA_BLKCNT")) : 0; if (bs && frame_no >= (unsigned)bs) g_blkcnt_on = 1; }
    if (frame_no == shot_at) { extern void prof_report(void), blkcnt_dump(void); prof_report(); blkcnt_dump(); }
    if (frame_no == shot_at && shot_path) { extern void uikit_dump(void); if (getenv("RA_UIDUMP")) uikit_dump(); write_ppm(shot_path, ww, wh); { extern unsigned gm_heap_used_mb(void); fprintf(stderr, "[heap] %u MB used\n", gm_heap_used_mb()); } fprintf(stderr, "[sdl] screenshot -> %s\n", shot_path); if (getenv("RA_EXIT_AFTER_SHOT")) { fflush(stderr); exit(0); } }
    SDL_GL_SwapWindow(win); frame_no++; p_bindfb(GL_FRAMEBUFFER, fbo);
    if (getenv("RA_FPS") && frame_no % 100 == 0) { static double t0; double t = SDL_GetTicks() / 1000.0; fprintf(stderr, "[fps] frame %d  %.1f fps (last 100)\n", frame_no, t0 ? 100.0 / (t - t0) : 0.0); t0 = t; }
}
void sdl_swap(void) {
    int ww, wh; SDL_GL_GetDrawableSize(win, &ww, &wh);
    glPushAttrib(GL_ALL_ATTRIB_BITS); glPushClientAttrib(GL_CLIENT_ALL_ATTRIB_BITS);
    glMatrixMode(GL_TEXTURE); glPushMatrix(); glLoadIdentity(); glMatrixMode(GL_PROJECTION); glPushMatrix(); glLoadIdentity(); glMatrixMode(GL_MODELVIEW); glPushMatrix(); glLoadIdentity();
    p_bindfb(GL_FRAMEBUFFER, 0);
    glViewport(0, 0, ww, wh); glDisable(GL_DEPTH_TEST); glDisable(GL_BLEND); glDisable(GL_ALPHA_TEST); glDisable(GL_CULL_FACE); glDisable(GL_SCISSOR_TEST); glDisable(GL_LIGHTING);
    glDisableClientState(GL_COLOR_ARRAY); glDisableClientState(GL_NORMAL_ARRAY); glDisableClientState(GL_TEXTURE_COORD_ARRAY); glDisableClientState(GL_VERTEX_ARRAY);
    glActiveTexture(GL_TEXTURE0); glEnable(GL_TEXTURE_2D); glBindTexture(GL_TEXTURE_2D, fbo_tex); glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);
    glColor4f(1, 1, 1, 1);
    /* buffer corners (u,v in texture space, v=0 bottom) -> window corners. Buffer is portrait; window landscape. */
    /* buffer corners in order TL,BL,BR,TR (texture v=1 is the visible top) mapped to window NDC corners */
    static const float tcA[4][2] = {{0,1},{0,0},{1,0},{1,1}};
    static const float cw [4][2] = {{ 1, 1},{-1, 1},{-1,-1},{ 1,-1}};   /* rotated 90 deg clockwise */
    static const float ccw[4][2] = {{-1,-1},{ 1,-1},{ 1, 1},{-1, 1}};   /* rotated 90 deg counter-clockwise */
    const float (*pos)[2] = g_rot > 0 ? cw : ccw;
    glBegin(GL_QUADS);
    for (int i = 0; i < 4; i++) { glTexCoord2f(tcA[i][0], tcA[i][1]); glVertex2f(pos[i][0], pos[i][1]); }
    glEnd();
    if (frame_no == shot_at && shot_path) { write_ppm(shot_path, ww, wh); fprintf(stderr, "[sdl] screenshot -> %s\n", shot_path); if (getenv("RA_EXIT_AFTER_SHOT")) { fflush(stderr); exit(0); } }
    SDL_GL_SwapWindow(win); frame_no++;
    glMatrixMode(GL_MODELVIEW); glPopMatrix(); glMatrixMode(GL_PROJECTION); glPopMatrix(); glMatrixMode(GL_TEXTURE); glPopMatrix(); glMatrixMode(GL_MODELVIEW);
    glPopClientAttrib(); glPopAttrib();
    p_bindfb(GL_FRAMEBUFFER, fbo);
}
void sdl_win_size(int *w, int *h) { *w = g_buf_w; *h = g_buf_h; }
int sdl_frame(void) { return frame_no; }

static void to_guest(int mx, int my, int *gx, int *gy) {
    SDL_GetWindowSize(win, &win_w, &win_h);
    *gx = (int)((float)mx * g_screen_w / (win_w ? win_w : 1)); *gy = (int)((float)my * g_screen_h / (win_h ? win_h : 1));   /* UI is landscape: identity mapping */
}
/* RA_CLICKS="frame:x:y,frame:x:y"  (window pixel coords) - scripted taps for testing */
static void scripted_clicks(void) {
    static int parsed; static int n, fr[32], cx[32], cy[32], state[32];
    if (!parsed) { parsed = 1; const char *e = getenv("RA_CLICKS"); while (e && *e && n < 32) { int f, x, y, k; if (sscanf(e, "%d:%d:%d%n", &f, &x, &y, &k) < 3) break; fr[n] = f; cx[n] = x; cy[n] = y; n++; e += k; if (*e == ',') e++; } }
    for (int i = 0; i < n; i++) {
        int gx, gy;
        if (state[i] == 0 && frame_no >= fr[i]) { to_guest(cx[i], cy[i], &gx, &gy); fprintf(stderr, "[sdl] tap down win(%d,%d) -> view(%d,%d)\n", cx[i], cy[i], gx, gy); objc_post_touch(0, gx, gy); state[i] = 1; }
        else if (state[i] == 1 && frame_no >= fr[i] + (getenv("RA_CLICK_HOLD") ? atoi(getenv("RA_CLICK_HOLD")) : 4)) { to_guest(cx[i], cy[i], &gx, &gy); objc_post_touch(2, gx, gy); state[i] = 2; }
    }
}
/* RA_DRAG="frame:x0:y0:x1:y1" - scripted drag over 24 frames (pan test) */
static void scripted_drag(void) {   /* RA_DRAG="f:x0:y0:x1:y1[,f:...]" */
    static int parsed, n, f[8], x0[8], y0[8], x1[8], y1[8];
    if (!parsed) { parsed = 1; const char *e = getenv("RA_DRAG"); while (e && *e && n < 8) { int k; if (sscanf(e, "%d:%d:%d:%d:%d%n", &f[n], &x0[n], &y0[n], &x1[n], &y1[n], &k) < 5) break; n++; e += k; if (*e == ',') e++; } }
    int gx, gy;
    for (int d = 0; d < n; d++) {
        if (frame_no == f[d]) { to_guest(x0[d], y0[d], &gx, &gy); objc_post_touch(0, gx, gy); }
        else if (frame_no > f[d] && frame_no <= f[d] + 24) { int k = frame_no - f[d]; to_guest(x0[d] + (x1[d] - x0[d]) * k / 24, y0[d] + (y1[d] - y0[d]) * k / 24, &gx, &gy); objc_post_touch(1, gx, gy); if (k == 24) objc_post_touch(2, gx, gy); }
    }
}
int sdl_pump(void) {
    scripted_clicks(); scripted_drag();
    if (up_pending && frame_no >= up_frame) { up_pending = 0; objc_post_touch(2, up_x, up_y); }
    SDL_Event e; int gx, gy;
    while (SDL_PollEvent(&e)) {
        switch (e.type) {
        case SDL_QUIT: return 0;
        case SDL_KEYDOWN:
            if (e.key.keysym.sym == SDLK_ESCAPE && (e.key.keysym.mod & KMOD_SHIFT)) return 0;
            if (e.key.keysym.sym == SDLK_F11) SDL_SetWindowFullscreen(win, (SDL_GetWindowFlags(win) & SDL_WINDOW_FULLSCREEN_DESKTOP) ? 0 : SDL_WINDOW_FULLSCREEN_DESKTOP);
            break;
        case SDL_MOUSEBUTTONDOWN: if (e.button.button == SDL_BUTTON_LEFT) { if (up_pending) { up_pending = 0; objc_post_touch(2, up_x, up_y); } down_frame = frame_no; mouse_down = 1; to_guest(e.button.x, e.button.y, &gx, &gy); objc_post_touch(0, gx, gy); } break;
        case SDL_MOUSEMOTION: if (mouse_down && !up_pending) { to_guest(e.motion.x, e.motion.y, &gx, &gy); objc_post_touch(1, gx, gy); } break;
        case SDL_MOUSEBUTTONUP: if (e.button.button == SDL_BUTTON_LEFT && mouse_down) { mouse_down = 0; to_guest(e.button.x, e.button.y, &gx, &gy); up_pending = 1; up_x = gx; up_y = gy; up_frame = down_frame + 4; } break;
        }
    }
    return 1;
}
