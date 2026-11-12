/* objc.c - minimal Objective-C runtime + fake Foundation/UIKit for the iFuse shell.
 *  Guest-defined classes (iFuseAppDelegate, EAGLView, ...) are parsed from __objc_classlist and dispatched in the emulator.
 *  Everything else is a "fake class" implemented on the host (see host_msg()).
 *  Object layout used for all host objects (CFString-literal compatible):  { isa, w1, w2, w3 }
 */
#include "ra.h"
#include <SDL2/SDL.h>
#include <stdarg.h>
#include <math.h>
#include <time.h>
#include <sys/statvfs.h>
#include <sys/stat.h>

#define FAKE_CLASS_MAGIC 0xFA4EC1A5u
typedef struct GCls { gptr addr, meta, super; const char *name; u32 instsize; gptr methods, cmethods; } GCls;
static GCls gcls[4096]; static int ngcls;
typedef struct FCls { const char *name; gptr addr; const char *parent; } FCls;
static FCls fcls[4096]; static int nfcls;

/* ---------- classes ---------- */
static const char *fake_parent(const char *n) {
    static const struct { const char *c, *p; } t[] = {
        {"NSMutableString", "NSString"}, {"__NSCFConstantString", "NSString"}, {"NSMutableArray", "NSArray"}, {"NSMutableDictionary", "NSDictionary"},
        {"NSMutableData", "NSData"}, {"UIView", "UIResponder"}, {"UIWindow", "UIView"}, {"UIImageView", "UIView"}, {"UILabel", "UIView"}, {"UIButton", "UIControl"},
        {"UIWebView", "UIView"}, {"UIControl", "UIView"}, {"UIScrollView", "UIView"}, {"UITableView", "UIScrollView"}, {"UITextView", "UIScrollView"}, {"UITextField", "UIControl"}, {"UIRoundedRectButton", "UIButton"}, {"UISlider", "UIControl"}, {"UISwitch", "UIControl"}, {"UIPageControl", "UIControl"}, {"UIProgressView", "UIView"}, {"UISegmentedControl", "UIControl"}, {"UITableViewCell", "UIView"}, {"UIToolbar", "UIView"}, {"UINavigationBar", "UIView"}, {"UIPickerView", "UIView"}, {"UIActivityIndicatorView", "UIView"}, {"UIViewController", "UIResponder"}, {"UIResponder", "NSObject"}, {"NSMutableSet", "NSSet"}, {"NSSet", "NSObject"}, {NULL, NULL}};
    for (int i = 0; t[i].c; i++) if (!strcmp(t[i].c, n)) return t[i].p;
    return strcmp(n, "NSObject") ? "NSObject" : NULL;
}
gptr objc_class_object(const char *name) {
    for (int i = 0; i < nfcls; i++) if (!strcmp(fcls[i].name, name)) return fcls[i].addr;
    if (nfcls >= 4096) { fprintf(stderr, "too many fake classes\n"); exit(1); }
    gptr a = alloc_fake(32);
    wr32(a, a); wr32(a + 4, 0); wr32(a + 8, gm_strdup(name)); wr32(a + 12, FAKE_CLASS_MAGIC);
    fcls[nfcls].name = strdup(name); fcls[nfcls].addr = a; fcls[nfcls].parent = fake_parent(name); nfcls++;
    return a;
}
static int is_fake_class(gptr a) { return a >= FAKE_BASE && a < FAKE_END && rd32(a + 12) == FAKE_CLASS_MAGIC; }
static const char *fake_name(gptr a) { return gstr(rd32(a + 8)); }
typedef struct { gptr cls, imeths, cmeths; const char *name; } Cat;
static Cat cats[2048]; static int ncats;
static GCls *find_gcls(gptr a) { for (int i = 0; i < ngcls; i++) if (gcls[i].addr == a) return &gcls[i]; return NULL; }
static GCls *find_gcls_name(const char *n) { for (int i = 0; i < ngcls; i++) if (!strcmp(gcls[i].name, n)) return &gcls[i]; return NULL; }

static void load_guest_classes(void) {
    Section *cs = find_section("__objc_catlist", 0);
    if (cs) for (u32 o = 0; o < cs->size && ncats < 2048; o += 4) { gptr c = rd32(cs->addr + o); cats[ncats].name = gstr(rd32(c)); cats[ncats].cls = rd32(c + 4); cats[ncats].imeths = rd32(c + 8); cats[ncats].cmeths = rd32(c + 12); ncats++; }
    hle_log("[objc] %d categories\n", ncats);
    Section *s = find_section("__objc_classlist", 0); if (!s) return;
    for (u32 o = 0; o < s->size; o += 4) {
        gptr c = rd32(s->addr + o); gptr data = rd32(c + 16) & ~3u, meta = rd32(c); gptr mdata = rd32(meta + 16) & ~3u;
        if (ngcls >= 4096) { fprintf(stderr, "too many guest classes\n"); exit(1); }
        GCls *g = &gcls[ngcls++]; g->addr = c; g->meta = meta; g->super = rd32(c + 4); g->name = gstr(rd32(data + 16)); g->instsize = rd32(data + 8);
        g->methods = rd32(data + 20); g->cmethods = rd32(mdata + 20);
        hle_log("[objc] class %s @%08x super=%08x size=%u\n", g->name, c, g->super, g->instsize);
    }
}
static gptr find_in_methods(gptr list, const char *sel) {
    if (!list) return 0;
    u32 n = rd32(list + 4);
    for (u32 i = 0; i < n; i++) { gptr m = list + 8 + i * 12; if (!strcmp(gstr(rd32(m)), sel)) return rd32(m + 8); }
    return 0;
}

/* ---------- host objects ---------- */
static gptr cls_NSString, cls_NSMutableString, cls_NSNumber, cls_NSArray, cls_NSMutableArray, cls_NSDictionary, cls_NSData, cls_NSSet, cls_URL, cls_cfstr, cls_Touch, cls_Event, cls_Generic;
static gptr new_obj(gptr cls) { gptr o = gm_malloc(16); wr32(o, cls); return o; }
static gptr new_string(const char *s) { gptr o = new_obj(cls_NSString); wr32(o + 4, 0x7C8); wr32(o + 8, gm_strdup(s)); wr32(o + 12, strlen(s)); return o; }
gptr objc_make_nsstring_global(const char *s) { if (!cls_NSString) cls_NSString = objc_class_object("NSString"); return new_string(s); }
static int is_string(gptr o) { if (!o || o >= ARENA_SIZE - 16) return 0; gptr c = rd32(o); return c == cls_NSString || c == cls_NSMutableString || c == cls_cfstr; }
static const char *scstr(gptr o) { return is_string(o) ? gstr(rd32(o + 8)) : ""; }
static void set_string(gptr o, const char *s) { wr32(o + 8, gm_strdup(s)); wr32(o + 12, strlen(s)); }
static gptr new_number_i(s32 v) { gptr o = new_obj(cls_NSNumber); wr32(o + 4, 0); wr32(o + 8, v); return o; }
static gptr new_number_u64(u64 v) { gptr o = new_obj(cls_NSNumber); wr32(o + 4, 1); *(u64 *)H(o + 8) = v; return o; }
static gptr new_array(void) { gptr o = new_obj(cls_NSMutableArray); wr32(o + 4, 0); wr32(o + 8, gm_malloc(256)); wr32(o + 12, 64); return o; }
static void array_add(gptr a, gptr x) { u32 n = rd32(a + 4), cap = rd32(a + 12); if (n >= cap) { wr32(a + 8, gm_realloc(rd32(a + 8), cap * 8)); wr32(a + 12, cap * 2); } wr32(rd32(a + 8) + 4 * n, x); wr32(a + 4, n + 1); }
static gptr array_get(gptr a, u32 i) { return i < rd32(a + 4) ? rd32(rd32(a + 8) + 4 * i) : 0; }
static gptr new_dict(void) { gptr o = new_obj(cls_NSDictionary); wr32(o + 4, 0); wr32(o + 8, gm_malloc(128)); wr32(o + 12, gm_malloc(128)); return o; }
static void dict_set(gptr d, gptr k, gptr v) { u32 n = rd32(d + 4); wr32(rd32(d + 8) + 4 * n, k); wr32(rd32(d + 12) + 4 * n, v); wr32(d + 4, n + 1); }
static gptr dict_get(gptr d, gptr k) { for (u32 i = 0; i < rd32(d + 4); i++) { gptr kk = rd32(rd32(d + 8) + 4 * i); if (kk == k || (is_string(kk) && is_string(k) && !strcmp(scstr(kk), scstr(k)))) return rd32(rd32(d + 12) + 4 * i); } return 0; }

const char *objc_describe(gptr o) {
    static char buf[8][256]; static int bi; char *b = buf[bi++ & 7];
    if (!o) return "(null)";
    if (o < 0x1000 || o >= ARENA_SIZE - 16) { snprintf(b, 256, "<bad %%@ arg 0x%x>", o); return b; }
    if (is_string(o)) return scstr(o);
    gptr c = rd32(o); GCls *g = find_gcls(c);
    snprintf(b, 256, "<%s: 0x%x>", g ? g->name : is_fake_class(c) ? fake_name(c) : "?", o); return b;
}

/* class of receiver; returns nonzero if `recv` is a class object */
static int recv_is_class(gptr r) { return is_fake_class(r) || find_gcls(r); }

/* ---------- dispatch ---------- */
typedef struct { int host; gptr imp; const char *fake; } Target;
/* Third-party ad / analytics / social SDKs are disabled: every message to their classes is a silent no-op returning nil. */
static int sdk_stubbed(const char *n) {
    static const char *pre[] = {"Tapjoy", "TJC", "Flurry", "Burstly", "GAD", "MM", "IM", "Greystripe", "OAI", "FB", "CFB", "Facebook", "MGTwitter", "Twitter", "FullScreenAdViewController", "AdServe", "AdManager", "CC", "Crystal", NULL};
    for (int i = 0; pre[i]; i++) if (!strncmp(n, pre[i], strlen(pre[i]))) return 1; return 0;
}
static Target lookup(gptr recv, const char *sel, int use_super_of, gptr start_cls) {
    Target t = {0, 0, "NSObject"}; const char *leaf = NULL;
    gptr cls; int cmeth;
    if (start_cls) { cls = start_cls; cmeth = recv_is_class(recv); if (use_super_of) { GCls *g = find_gcls(cls); cls = g ? g->super : 0; } }
    else if (recv_is_class(recv)) { cls = recv; cmeth = 1; }
    else { cls = rd32(recv); cmeth = 0; }
    for (int guard = 0; cls && guard < 32; guard++) {
        for (int ci = 0; ci < ncats; ci++) if (cats[ci].cls == cls) { gptr imp = find_in_methods(cmeth ? cats[ci].cmeths : cats[ci].imeths, sel); if (imp) { t.imp = imp; return t; } }
        GCls *g = find_gcls(cls);
        if (g) { if (sdk_stubbed(g->name) && strcmp(sel, "dealloc")) { t.host = 1; t.fake = "__stub__"; return t; } gptr imp = find_in_methods(cmeth ? g->cmethods : g->methods, sel); if (imp) { t.imp = imp; return t; } cls = g->super; continue; }
        if (is_fake_class(cls)) {                                   /* walk fake superclasses so categories on NSString/NSObject/UIView apply */
            if (!leaf) leaf = fake_name(cls);
            const char *par = NULL; for (int fi = 0; fi < nfcls; fi++) if (fcls[fi].addr == cls) par = fcls[fi].parent;
            if (par) { cls = objc_class_object(par); continue; }
            t.host = 1; t.fake = leaf; return t; }
        break;
    }
    t.host = 1; return t;
}

static int host_msg(const char *cls, gptr recv, const char *sel, int stret, int *handled);
/* objc_constructInstance: run each class's C++ ivar constructor (.cxx_construct), superclass first. EAGLView's builds the engine,
 * including the modal message-box manager the pause menu needs (the host used to skip it, so the "abandon game?" box was never drawn). */
void objc_construct(gptr cls, gptr o) {
    gptr chain[16]; int nc = 0;
    for (gptr k = cls; k && nc < 16; ) { GCls *gk = find_gcls(k); if (!gk) break; chain[nc++] = k; k = gk->super; }
    for (int ci = nc - 1; ci >= 0; ci--) { GCls *gk = find_gcls(chain[ci]); gptr imp = find_in_methods(gk->methods, ".cxx_construct"); if (imp) { u32 ca[2] = { o, 0 }; hle_log("[objc] .cxx_construct %s\n", gk->name); call_guest(imp, 2, ca); } }
}
static int g_handled_ok;

static void do_send(gptr recv, const char *sel, int stret, gptr start_cls, int use_super) {
    if (!recv) { ret(0); return; }
    if (recv < 0x1000 || recv >= ARENA_SIZE - 16 || (rd32(recv) >= ARENA_SIZE && !recv_is_class(recv))) {   /* garbage receiver: behave like nil */
        static int warned; if (warned++ < 5) fprintf(stderr, "[objc] message %s to invalid receiver %08x (lr=%08x) -> nil\n", sel, recv, reg(14)); ret(0); return; }
    Target t = lookup(recv, sel, use_super, start_cls);
    if (g_trace > 1) hle_log("[msg] %s %s%s\n", objc_describe(recv), sel, t.host ? "  (host)" : "  (guest)");
    if (t.imp) {
        static int no_tut = -1; if (no_tut < 0) no_tut = getenv("RA_NO_TUTORIAL") != NULL;   /* test aid: skip the first-run tutorial overlays */
        if (no_tut && !strncmp(sel, "showTutorial:", 13)) { ret(0); return; }
        if (!strcmp(sel, "updateUserInfo:")) { extern void offline_levelup_check(void); offline_levelup_check(); }
        /* always start on the main menu: dissolveSplash jumps straight to the map while the welcome tutorial is pending */
        if (!strcmp(sel, "canShowTutorial:") && arg(2) == 2 && reg(14) >= 0x1d430 && reg(14) < 0x1d760) { ret(0); return; }
        /* no publisher / studio splash: go straight to the game */
        if (!strcmp(sel, "startIntroSequence")) { GCls *g = find_gcls(rd32(recv)); if (g && !strcmp(g->name, "IntroSequenceView")) { Target t2 = lookup(recv, "finishedIntroSequence", 0, 0); if (t2.imp) { set_next_pc(t2.imp); return; } } }
        set_next_pc(t.imp); return; }
    if (!strcmp(t.fake, "__stub__")) { ret(0); return; }
    int handled = 0;
    host_msg(t.fake, recv, sel, stret, &handled);
    if (!handled) {
        static char seen[256][96]; static int ns; int dup = 0;
        for (int i = 0; i < ns; i++) if (!strcmp(seen[i], sel)) dup = 1;
        if (!dup && ns < 256) { strncpy(seen[ns++], sel, 95); fprintf(stderr, "[objc] unhandled -[%s %s] (recv %08x) -> nil\n", t.fake, sel, recv); }
        ret(0);
    }
}
static void h_msgSend(void) { do_send(arg(0), gstr(arg(1)), 0, 0, 0); }
static void h_msgSend_stret(void) { do_send(arg(1), gstr(arg(2)), 1, 0, 0); }
static void h_msgSendSuper2(void) {
    gptr sp = arg(0); gptr recv = rd32(sp), cur = rd32(sp + 4);
    setreg(0, recv);
    do_send(recv, gstr(arg(1)), 0, cur, 1);
}
static void h_msgSendSuper2_stret(void) {
    gptr sp = arg(1); gptr recv = rd32(sp), cur = rd32(sp + 4);
    setreg(1, recv);
    do_send(recv, gstr(arg(2)), 1, cur, 1);
}

/* call a selector on a guest object from the host (top level only) */
u32 send_guest(gptr recv, const char *sel, int nargs, const u32 *args) {
    Target t = lookup(recv, sel, 0, 0);
    if (!t.imp) return 0;
    u32 a[8] = { recv, 0, 0, 0, 0, 0, 0, 0 };
    /* selector pointer must be a guest string */
    static struct { char *s; gptr g; } cache[256]; static int nc; gptr sg = 0;
    for (int i = 0; i < nc; i++) if (!strcmp(cache[i].s, sel)) sg = cache[i].g;
    if (!sg) { sg = gm_strdup(sel); cache[nc].s = strdup(sel); cache[nc++].g = sg; }
    a[1] = sg; for (int i = 0; i < nargs; i++) a[2 + i] = args[i];
    return call_guest(t.imp, 2 + nargs, a);
}
int guest_responds(gptr recv, const char *sel) { return lookup(recv, sel, 0, 0).imp != 0; }

/* ---------- timers ---------- */
typedef struct { gptr obj, target; char sel[96]; double interval, next; int repeats, valid; } Timer;
static Timer timers[128];
static double now_s(void) { struct timespec ts; clock_gettime(CLOCK_MONOTONIC, &ts); return ts.tv_sec + ts.tv_nsec * 1e-9; }
static int timer_add(gptr target, const char *sel, double interval, int repeats, gptr obj) {
    for (int i = 0; i < 128; i++) if (!timers[i].valid) { timers[i].valid = 1; timers[i].target = target; strncpy(timers[i].sel, sel, 95); timers[i].interval = interval; timers[i].next = now_s() + interval; timers[i].repeats = repeats; timers[i].obj = obj; return i; } return -1; }

/* ---------- CG helpers ---------- */
static float f_of(u32 w) { float f; memcpy(&f, &w, 4); return f; }
static u32 w_of(float f) { u32 w; memcpy(&w, &f, 4); return w; }
static void ret_rect(int stret, float x, float y, float w, float h) {
    gptr p = arg(0); (void)stret; wr32(p, w_of(x)); wr32(p + 4, w_of(y)); wr32(p + 8, w_of(w)); wr32(p + 12, w_of(h));
}

/* touch state shared with sdl layer */
int g_touch_x, g_touch_y;
static gptr singleton(gptr *slot, gptr cls) { if (!*slot) *slot = new_obj(cls); return *slot; }
static gptr s_bundle, s_fm, s_defaults, s_app, s_screen, s_device, s_center, s_runloop, s_context, s_layer, s_mainwin;
gptr g_delegate, g_glview; gptr g_touch_target; int g_touch_phase; gptr g_engine_input;
int g_running = 1;
static gptr g_cur_touchset;

static u32 send_any(gptr recv, const char *sel, int nargs, const u32 *args);
static gptr new_error(const char *domain, int code);
#include "uikit.inc"

/* send a message to anything: guest IMP via the emulator, else the host handler (registers are scratch-saved) */
static int host_msg(const char *cls, gptr recv, const char *sel, int stret, int *handled);
static u32 send_any(gptr recv, const char *sel, int nargs, const u32 *args) {
    Target t = lookup(recv, sel, 0, 0);
    if (t.imp) return send_guest(recv, sel, nargs, args);
    u32 sv[4] = { reg(0), reg(1), reg(2), reg(3) };
    static struct { char *s; gptr g; } sc[256]; static int nsc; gptr sg = 0; for (int i = 0; i < nsc; i++) if (!strcmp(sc[i].s, sel)) sg = sc[i].g;
    if (!sg) { sg = gm_strdup(sel); sc[nsc].s = strdup(sel); sc[nsc++].g = sg; }
    setreg(0, recv); setreg(1, sg); for (int i = 0; i < nargs && i < 2; i++) setreg(2 + i, args[i]);
    int handled = 0; host_msg(t.fake, recv, sel, 0, &handled); u32 r = handled ? reg(0) : 0;
    for (int i = 0; i < 4; i++) setreg(i, sv[i]); return r;
}
#include "uikit2.inc"
#include "uikit3.inc"
#include "foundation.inc"

/* ---------- host-implemented classes ---------- */
static int host_msg(const char *cls, gptr recv, const char *sel, int stret, int *handled) {
    int is_cls = recv_is_class(recv);
    gptr self = recv; *handled = 1;
    if (uikit_msg(cls, recv, sel, stret, is_cls)) return 0;
    if (found_msg(cls, recv, sel, stret, is_cls)) return 0;
    #define IS(s) (!strcmp(sel, s))
    #define A(n) arg((stret ? 1 : 0) + (n))   /* n counts from receiver=0, sel=1 */
    /* ---- NSObject protocol ---- */
    if (IS("alloc") || IS("allocWithZone:")) { const char *cn = is_cls ? (is_fake_class(recv) ? fake_name(recv) : cls) : cls; gptr c = objc_class_object(cn);
        GCls *g = find_gcls(recv); if (g) { gptr o = gm_malloc(g->instsize + 16); wr32(o, recv); if (!strcmp(g->name, "EAGLView")) g_glview = o;
            objc_construct(recv, o);
            ret(o); return 0; }
        gptr o = new_obj(c);
        if (strstr(cn, "Array") || strstr(cn, "Set")) { wr32(o + 8, gm_malloc(256)); wr32(o + 12, 64); }
        if (strstr(cn, "Dictionary")) { wr32(o + 8, gm_malloc(128)); wr32(o + 12, gm_malloc(128)); } if (!strncmp(cn, "NS", 2) && strstr(cn, "String")) { wr32(o + 8, gm_strdup("")); wr32(o + 4, 0x7C8); } ret(o); return 0; }
    if (IS("init") || IS("retain") || IS("autorelease") || IS("self") || IS("copy") || IS("mutableCopy")) { ret(self); return 0; }
    if (IS("release") || IS("dealloc") || IS("removeObserver:")) { ret(0); return 0; }
    if (IS("retainCount")) { ret(1); return 0; }
    if (IS("class")) { ret(is_cls ? recv : rd32(recv)); return 0; }
    if (IS("isKindOfClass:") || IS("isMemberOfClass:")) {
        gptr target = A(2), c = recv_is_class(recv) ? rd32(recv) == recv ? recv : recv : rd32(recv); int ok = 0, member = IS("isMemberOfClass:");
        if (!target) { ret(0); return 0; }
        for (int guard = 0; c && guard < 32; guard++) {
            if (c == target) { ok = 1; break; } if (member) break;
            GCls *g = find_gcls(c);
            if (g) { c = g->super; continue; }
            if (is_fake_class(c)) { const char *pn = fake_parent(fake_name(c)); c = pn ? objc_class_object(pn) : 0; continue; }
            break; }
        ret(ok); return 0; }
    if (IS("respondsToSelector:")) { if (!strcmp(cls, "UIDevice") && !strcmp(gstr(A(2)), "userInterfaceIdiom")) { ret(1); return 0; } ret(guest_responds(recv, gstr(A(2))) ? 1 : 0); return 0; }
    if (IS("conformsToProtocol:")) { ret(1); return 0; }
    if (IS("hash")) { ret(recv); return 0; }
    if (IS("isEqual:")) { ret(recv == A(2)); return 0; }
    if (IS("description")) { ret(new_string(objc_describe(recv))); return 0; }
    if (IS("performSelector:withObject:afterDelay:")) {
        for (int i = 0; i < 128; i++) if (!timers[i].valid) { timers[i].valid = 1; timers[i].target = recv; strncpy(timers[i].sel, gstr(A(2)), 95); u64 d = arg64(4); double iv; memcpy(&iv, &d, 8); timers[i].interval = iv; timers[i].next = now_s() + iv; timers[i].repeats = 0; timers[i].obj = A(3); hle_log("[perform] %s %s afterDelay %.2f\n", objc_describe(recv), timers[i].sel, iv); break; }
        ret(0); return 0; }
    if (IS("performSelector:") || IS("performSelector:withObject:") || IS("performSelector:withObject:withObject:")) { gptr imp = 0; Target t = lookup(recv, gstr(A(2)), 0, 0); imp = t.imp; if (imp) { setreg(1, A(2)); if (IS("performSelector:withObject:")) setreg(2, A(3)); set_next_pc(imp); return 0; } ret(0); return 0; }
    /* ---- NSString ---- */
    if (strstr(cls, "String")) {
        if (IS("stringWithUTF8String:") || IS("stringWithCString:") || IS("stringWithCString:encoding:")) { if (getenv("RA_STRLOG")) hle_log("[cstr] %.80s\n", gstr(A(2))); ret(new_string(gstr(A(2)))); return 0; }
        if (IS("stringWithFormat:") || IS("stringWithCapacity:")) {
            if (IS("stringWithCapacity:")) { ret(new_string("")); return 0; }
            char b[8192]; extern int gformat_regs(char *, size_t, gptr, int); const char *f = scstr(A(2)); gptr fg = gm_strdup(f);
            gformat_regs(b, sizeof b, fg, stret ? 4 : 3); gm_free(fg); ret(new_string(b)); return 0; }
        if (IS("appendFormat:")) { char b[8192]; extern int gformat_regs(char *, size_t, gptr, int); gptr fg = gm_strdup(scstr(A(2))); gformat_regs(b, sizeof b, fg, 3); gm_free(fg);
            char *nb = malloc(strlen(scstr(recv)) + strlen(b) + 1); strcpy(nb, scstr(recv)); strcat(nb, b); set_string(recv, nb); free(nb); ret(0); return 0; }
        if (IS("UTF8String") || IS("cString") || IS("cStringUsingEncoding:")) { ret(rd32(recv + 8)); return 0; }
        if (IS("length")) { ret(strlen(scstr(recv))); return 0; }
        if (IS("isEqualToString:")) { ret(!strcmp(scstr(recv), scstr(A(2)))); return 0; }
        if (IS("compare:")) { int r = strcmp(scstr(recv), scstr(A(2))); ret(r < 0 ? -1 : r > 0 ? 1 : 0); return 0; }
        if (IS("intValue")) { ret(atoi(scstr(recv))); return 0; }
        if (IS("description")) { ret(recv); return 0; }
    }
    if (!strcmp(cls, "NSString") || !strcmp(cls, "NSMutableString") || !strcmp(cls, "__NSCFConstantString")) {
        if (IS("stringByDeletingLastPathComponent")) { char b[1024]; strncpy(b, scstr(recv), 1023); b[1023] = 0; char *q = strrchr(b, '/'); if (q) *q = 0; ret(new_string(b)); return 0; }
        if (IS("getCString:")) { strcpy((char *)H(A(2)), scstr(recv)); ret(1); return 0; }
        if (IS("stringByAppendingFormat:") || IS("stringByAppendingString:")) {
            char b[8192]; extern int gformat_regs(char *, size_t, gptr, int);
            if (IS("stringByAppendingString:")) snprintf(b, sizeof b, "%s%s", scstr(recv), scstr(A(2)));
            else { char t[4096]; gptr fg = gm_strdup(scstr(A(2))); gformat_regs(t, sizeof t, fg, 3); gm_free(fg); snprintf(b, sizeof b, "%s%s", scstr(recv), t); }
            ret(new_string(b)); return 0; }
        if (IS("floatValue") || IS("doubleValue")) { double d = atof(scstr(recv)); if (IS("floatValue")) { float f = d; u32 w; memcpy(&w, &f, 4); ret(w); } else { u64 w; memcpy(&w, &d, 8); ret64(w); } return 0; }
        if (IS("hasPrefix:")) { const char *p = scstr(A(2)); ret(!strncmp(scstr(recv), p, strlen(p))); return 0; }
        if (IS("hasSuffix:")) { const char *a = scstr(recv), *b = scstr(A(2)); size_t la = strlen(a), lb = strlen(b); ret(la >= lb && !strcmp(a + la - lb, b)); return 0; }
        if (IS("stringByAppendingPathComponent:")) { char b[2048]; snprintf(b, sizeof b, "%s/%s", scstr(recv), scstr(A(2))); ret(new_string(b)); return 0; }
        if (IS("dataUsingEncoding:")) { gptr d = new_obj(cls_NSData); wr32(d + 4, rd32(recv + 8)); wr32(d + 8, strlen(scstr(recv))); ret(d); return 0; }
        if (IS("copyWithZone:") || IS("mutableCopyWithZone:")) { ret(recv); return 0; }
    }
    if (!strcmp(cls, "UIAccelerometer")) { if (IS("sharedAccelerometer")) { ret(new_obj(objc_class_object("UIAccelerometer"))); return 0; } ret(0); return 0; }
    if (!strcmp(cls, "UIDevice")) {
        if (IS("name") || IS("localizedModel") || IS("model")) { ret(new_string("iPad")); return 0; }
        if (IS("systemName")) { ret(new_string("iPhone OS")); return 0; }
        if (IS("systemVersion")) { ret(new_string("4.3.5")); return 0; }
        if (IS("uniqueIdentifier")) { ret(new_string("0000000000000000000000000000000000000001")); return 0; }
    }
    if (!strcmp(cls, "NSDictionary") && IS("dictionaryWithObjectsAndKeys:")) {
        gptr d = new_dict(); for (int i = 2; i < 20; i += 2) { gptr v = arg(i), k = arg(i + 1); if (!v) break; dict_set(d, k, v); } ret(d); return 0; }
    if (!strcmp(cls, "NSBundle") && IS("infoDictionary")) { ret(new_dict()); return 0; }
    if (!strcmp(cls, "NSFileManager") && IS("contentsAtPath:")) { ret(0); return 0; }
    if (!strcmp(cls, "NSValue") && IS("valueWithPointer:")) { gptr o = new_obj(objc_class_object("NSValue")); wr32(o + 8, A(2)); ret(o); return 0; }
    if (!strcmp(cls, "NSValue") && IS("pointerValue")) { ret(rd32(recv + 8)); return 0; }
    if (!strcmp(cls, "NSKeyedUnarchiver")) { ret(0); return 0; }
    if (!strcmp(cls, "NSMutableArray") && IS("containsObject:")) { for (u32 i = 0; i < rd32(recv + 4); i++) if (array_get(recv, i) == A(2) || (is_string(array_get(recv, i)) && !strcmp(scstr(array_get(recv, i)), scstr(A(2))))) { ret(1); return 0; } ret(0); return 0; }
    if (!strcmp(cls, "NSMutableArray") && IS("indexOfObject:")) { for (u32 i = 0; i < rd32(recv + 4); i++) if (array_get(recv, i) == A(2)) { ret(i); return 0; } ret(0x7fffffff); return 0; }
    if (!strcmp(cls, "CAEAGLLayer") || !strcmp(cls, "CALayer")) { ret(0); return 0; }
    if (!strcmp(cls, "UIViewController")) { if (IS("view")) { ret(singleton(&s_mainwin, objc_class_object("UIView"))); return 0; } ret(0); return 0; }
    /* ---- NSNumber ---- */
    if (!strcmp(cls, "NSNumber") || !strcmp(cls, "NSDecimalNumber")) {
        if (IS("unsignedLongLongValue") || IS("longLongValue")) { ret64(rd32(recv + 4) ? *(u64 *)H(recv + 8) : rd32(recv + 8)); return 0; }
        if (IS("intValue") || IS("integerValue") || IS("unsignedIntValue") || IS("boolValue")) { ret(rd32(recv + 8)); return 0; }
        if (IS("null") && !strcmp(cls, "NSNull")) { static gptr nn; if (!nn) nn = new_obj(objc_class_object("NSNull")); ret(nn); return 0; }
        if (IS("numberWithInt:") || IS("numberWithInteger:") || IS("numberWithBool:")) { ret(new_number_i(A(2))); return 0; }
    }
    /* ---- NSArray / NSDictionary / NSSet ---- */
    if (!strcmp(cls, "NSArray") || !strcmp(cls, "NSMutableArray") || !strcmp(cls, "NSSet") || !strcmp(cls, "NSMutableSet")) {
        if (IS("count")) { ret(rd32(recv + 4)); return 0; }
        if (IS("objectAtIndex:")) { ret(array_get(recv, A(2))); return 0; }
        if (IS("anyObject")) { ret(array_get(recv, 0)); return 0; }
        if (IS("lastObject")) { u32 n = rd32(recv + 4); ret(n ? array_get(recv, n - 1) : 0); return 0; }
        if (IS("allObjects")) { ret(recv); return 0; }
        if (IS("addObject:")) { array_add(recv, A(2)); ret(0); return 0; }
        if (IS("array") || IS("arrayWithCapacity:")) { ret(new_array()); return 0; }
        if (IS("countByEnumeratingWithState:objects:count:")) { gptr st = A(2); if (rd32(st)) { ret(0); return 0; } wr32(st, 1); wr32(st + 4, rd32(recv + 8)); wr32(st + 8, st + 16); ret(rd32(recv + 4)); return 0; }
    }
    if (!strcmp(cls, "NSDictionary") || !strcmp(cls, "NSMutableDictionary")) {
        if (IS("objectForKey:") || IS("valueForKey:")) { ret(dict_get(recv, A(2))); return 0; }
        if (IS("count")) { ret(rd32(recv + 4)); return 0; }
        if (IS("allKeys")) { gptr a = new_array(); for (u32 i = 0; i < rd32(recv + 4); i++) array_add(a, rd32(rd32(recv + 8) + 4 * i)); ret(a); return 0; }
        if (IS("setObject:forKey:")) { dict_set(recv, A(2), A(3)); ret(0); return 0; }
        if (IS("dictionary")) { ret(new_dict()); return 0; }
    }
    /* ---- Foundation singletons ---- */
    if (!strcmp(cls, "NSBundle")) {
        if (IS("mainBundle")) { ret(singleton(&s_bundle, objc_class_object("NSBundle"))); return 0; }
        if (IS("bundlePath") || IS("resourcePath")) { ret(new_string(g_bundle_dir)); return 0; }
        if (IS("localizedStringForKey:value:table:")) { gptr v = A(3); ret(v && *scstr(v) ? v : A(2)); return 0; }
        if (IS("pathForResource:ofType:")) { char b[1024]; snprintf(b, sizeof b, "%s/%s%s%s", g_bundle_dir, scstr(A(2)), A(3) ? "." : "", scstr(A(3))); ret(new_string(b)); return 0; }
    }
    if (!strcmp(cls, "NSFileManager")) {
        if (IS("defaultManager")) { ret(singleton(&s_fm, objc_class_object("NSFileManager"))); return 0; }
        if (IS("attributesOfFileSystemForPath:error:")) { struct statvfs sv; unsigned long long fr = 4ull << 30; if (!statvfs(g_docs_dir, &sv)) fr = (unsigned long long)sv.f_bavail * sv.f_frsize;
            gptr d = new_dict(); dict_set(d, objc_make_nsstring_global("NSFileSystemFreeSize"), new_number_u64(fr)); ret(d); return 0; }
    }
    if (!strcmp(cls, "NSUserDefaults")) {
        if (IS("standardUserDefaults")) { ret(singleton(&s_defaults, objc_class_object("NSUserDefaults"))); return 0; }
        if (IS("objectForKey:") || IS("arrayForKey:") || IS("valueForKey:")) {
            const char *k = scstr(A(2));
            if (!strcmp(k, "AppleLanguages")) { gptr a = new_array(); const char *l = getenv("RA_LANG"); array_add(a, new_string(l ? l : "en")); ret(a); return 0; }
            if (!strcmp(k, "AppleLocale")) { ret(new_string("en_US")); return 0; }
            ret(0); return 0; }
        if (IS("setObject:forKey:") || IS("setBool:forKey:") || IS("setInteger:forKey:")) hle_log("[defaults] %s key=%s val=%s\n", sel, scstr(A(3)), IS("setObject:forKey:") ? objc_describe(A(2)) : "-");
        ret(0); return 0;
    }
    if (!strcmp(cls, "NSDate")) { if (IS("date")) { ret(new_obj(objc_class_object("NSDate"))); return 0; } }
    if (!strcmp(cls, "NSNotificationCenter")) { if (IS("defaultCenter")) { ret(singleton(&s_center, objc_class_object("NSNotificationCenter"))); return 0; } ret(0); return 0; }
    if (!strcmp(cls, "NSTimer")) {
        if (IS("scheduledTimerWithTimeInterval:target:selector:userInfo:repeats:")) {
            /* args: self,sel, double interval (r2:r3), target, selector, userInfo, repeats on stack */
            u64 d = arg64(2); double iv; memcpy(&iv, &d, 8);
            gptr target = arg(4), selp = arg(5); u32 reps = arg(7);
            { GCls *tg = find_gcls(rd32(target)); if (tg && (!strncmp(tg->name, "EAMTX", 5) || !strncmp(tg->name, "DMG", 3))) { ret(new_obj(objc_class_object("NSTimer"))); return 0; } }   /* dead store/ad services */
            for (int i = 0; i < 128; i++) if (!timers[i].valid) { timers[i].valid = 1; timers[i].target = target; strncpy(timers[i].sel, gstr(selp), 95); timers[i].interval = iv; timers[i].next = now_s() + iv; timers[i].repeats = reps; timers[i].obj = new_obj(objc_class_object("NSTimer")); hle_log("[timer] #%d every %.4fs -> %s %s (repeats=%u)\n", i, iv, objc_describe(target), gstr(selp), reps); wr32(timers[i].obj + 4, i + 1); ret(timers[i].obj); return 0; }
            ret(0); return 0; }
        if (IS("invalidate")) { u32 i = rd32(recv + 4); if (i) timers[i - 1].valid = 0; ret(0); return 0; }
    }
    /* ---- UIKit shell ---- */
    if (!strcmp(cls, "UIApplication")) {
        if (IS("sharedApplication")) { ret(singleton(&s_app, objc_class_object("UIApplication"))); return 0; }
        if (IS("delegate")) { ret(g_delegate); return 0; }
        ret(0); return 0;
    }
    if (!strcmp(cls, "UIScreen")) {
        if (IS("mainScreen")) { ret(singleton(&s_screen, objc_class_object("UIScreen"))); return 0; }
        if (IS("bounds") || IS("applicationFrame")) { ret_rect(stret, 0, 0, g_scr_w, g_scr_h); return 0; }
        if (IS("scale")) { ret(w_of(1.0f)); return 0; }
    }
    if (!strcmp(cls, "UIDevice")) {
        if (IS("currentDevice")) { ret(singleton(&s_device, objc_class_object("UIDevice"))); return 0; }
        if (IS("userInterfaceIdiom")) { ret(g_screen_w >= 768 ? 1 : 0); return 0; }
    }
    if (!strcmp(cls, "UIView") || !strcmp(cls, "UIWindow") || !strcmp(cls, "UIResponder")) {
        if (IS("initWithFrame:")) { ret(self); return 0; }
        if (IS("layer")) { ret(singleton(&s_layer, objc_class_object("CAEAGLLayer"))); return 0; }
        if (IS("bounds") || IS("frame")) { ret_rect(stret, 0, 0, g_screen_w, g_screen_h); return 0; }
        if (IS("center")) { gptr p = arg(0); wr32(p, w_of(g_screen_w / 2.f)); wr32(p + 4, w_of(g_screen_h / 2.f)); return 0; }
        if (IS("subviews")) { ret(new_array()); return 0; }
        if (IS("layerClass")) { ret(objc_class_object("CAEAGLLayer")); return 0; }
        if (IS("makeKeyAndVisible") || IS("addSubview:") || IS("setFrame:") || IS("setTransform:") || IS("removeFromSuperview")) { ret(0); return 0; }
    }
    if (!strcmp(cls, "EAGLContext")) {
        if (IS("initWithAPI:") || IS("initWithAPI:sharegroup:")) { if (A(2) == 2) { ret(0); return 0; } ret(singleton(&s_context, objc_class_object("EAGLContext"))); return 0; }   /* ES2 unsupported -> game falls back to ES1 */
        if (IS("setCurrentContext:")) { ret(1); return 0; }
        if (IS("renderbufferStorage:fromDrawable:")) { gptr layer = A(3); View *lv = layer ? view_of(rd32(layer + 4)) : NULL; extern void sdl_fbo_resize(int, int); if (lv) { lv->is_gl = 1; sdl_fbo_resize((int)lv->bw, (int)lv->bh); } ret(1); return 0; }
        if (IS("presentRenderbuffer:")) { ret(1); return 0; }   /* UI compositor swaps once per frame */
    }
    if (!strcmp(cls, "UITouch")) {
        if (IS("locationInView:") || IS("previousLocationInView:")) { gptr p = arg(0); float ox = 0, oy = 0; View *tv = A(2) ? view_of(A(2)) : NULL; if (tv) view_window_origin(tv, &ox, &oy); wr32(p, w_of((float)g_touch_x - ox)); wr32(p + 4, w_of((float)g_touch_y - oy)); return 0; }
        if (IS("view") || IS("window")) { extern gptr g_touch_target; ret(IS("view") ? g_touch_target : (g_window ? g_window->obj : 0)); return 0; }
        if (IS("tapCount")) { ret(1); return 0; }
        if (IS("timestamp")) { u64 b; double d = now_s(); memcpy(&b, &d, 8); ret64(b); return 0; }
        if (IS("phase")) { extern int g_touch_phase; ret(g_touch_phase); return 0; }
    }
    if (!strcmp(cls, "UIEvent")) {
        if (IS("allTouches")) { ret(g_cur_touchset); return 0; }
        if (IS("touchesForView:")) { ret(g_cur_touchset); return 0; }
    }
    if (!strcmp(cls, "NSObject") || is_cls) {
        if (IS("new")) { ret(new_obj(recv)); return 0; }
    }
    *handled = 0; return 0;
}

/* ---------- misc C-level ObjC / Foundation imports ---------- */
static void h_NSLog(void) { char b[8192]; extern int gformat_regs(char *, size_t, gptr, int); gptr fg = gm_strdup(scstr(arg(0))); gformat_regs(b, sizeof b, fg, 1); gm_free(fg); fprintf(stderr, "[NSLog] %s\n", b); }
static void h_getProperty(void) { ret(rd32(arg(0) + arg(2))); }
static void h_setProperty(void) { wr32(arg(0) + arg(2), arg(3)); }
static void h_copyStruct(void) { memcpy(H(arg(0)), H(arg(1)), arg(2)); }
static void h_noop(void) { }
static void h_zero(void) { ret(0); }
static void h_NSClassFromString(void) { const char *n = scstr(arg(0)); GCls *g = find_gcls_name(n); ret(g ? g->addr : objc_class_object(n)); }
static void h_NSSearchPath(void) { gptr a = new_array(); mkdir(g_docs_dir, 0755); array_add(a, new_string(g_docs_dir)); ret(a); }
static void h_NSTempDir(void) { ret(new_string("/tmp/")); }
static void h_CFBundleGetMainBundle(void) { ret(singleton(&s_bundle, objc_class_object("NSBundle"))); }
static void h_CFStringAppendCharacters(void) { const char *cur = scstr(arg(0)); u32 n = arg(2), p = arg(1); char *t = malloc(strlen(cur) + n * 3 + 1); strcpy(t, cur); char *o = t + strlen(t);
    for (u32 i = 0; i < n; i++) { unsigned c = rd32(p + i * 2) & 0xffff; if (c < 0x80) *o++ = c; else if (c < 0x800) { *o++ = 0xC0 | (c >> 6); *o++ = 0x80 | (c & 63); } else { *o++ = 0xE0 | (c >> 12); *o++ = 0x80 | ((c >> 6) & 63); *o++ = 0x80 | (c & 63); } } *o = 0; set_string(arg(0), t); free(t); ret(0); }
static void h_CFStringCreateWithCString(void) { ret(new_string(gstr(arg(1)))); }
static void h_CFBundleCopyResourceURL(void) {
    char b[1024]; const char *sub = arg(3) ? scstr(arg(3)) : "";
    snprintf(b, sizeof b, "%s/%s%s%s%s%s", g_bundle_dir, *sub ? sub : "", *sub ? "/" : "", scstr(arg(1)), arg(2) ? "." : "", scstr(arg(2)));
    gptr o = new_obj(cls_URL); wr32(o + 8, gm_strdup(b)); ret(o);
}
static void h_CGAffineTransformMakeRotation(void) { float a = f_of(arg(1)); gptr p = arg(0); wr32(p, w_of(cosf(a))); wr32(p + 4, w_of(sinf(a))); wr32(p + 8, w_of(-sinf(a))); wr32(p + 12, w_of(cosf(a))); wr32(p + 16, 0); wr32(p + 20, 0); }
static void h_NSFileSystemFreeSize_dummy(void) { ret(0); }
extern int g_quit_request;
static void h_UIApplicationMain(void) {
    gptr dname = arg(3); const char *dn = dname ? scstr(dname) : NULL;
    if (!dn || !*dn) { for (int i = 0; i < ngcls; i++) if (strstr(gcls[i].name, "AppDelegate")) { dn = gcls[i].name; break; } if (!dn) dn = "AppDelegate"; }   /* real iOS: delegate comes from MainWindow.nib */
    hle_log("[objc] UIApplicationMain delegate=%s\n", dn);
    { gptr appo = singleton(&s_app, objc_class_object("UIApplication")); s_app_obj_for_nib = appo; nib_load("MainWindow", appo, NULL); }
    if (!g_delegate) { GCls *g = find_gcls_name(dn); if (!g) { fprintf(stderr, "delegate class %s not found\n", dn); exit(1); } g_delegate = gm_malloc(g->instsize + 16); wr32(g_delegate, g->addr); }
    g_quit_request = 1; uc_emu_stop(uc);
}
int g_quit_request;

/* ---------- app run loop ---------- */
void objc_post_touch(int phase, int x, int y) { uikit_touch(phase, x, y); }
void objc_post_touch_old(int phase, int x, int y) {
    if (!g_glview) return;
    static gptr touch, set, event;
    if (!touch) { touch = new_obj(cls_Touch); set = new_obj(cls_NSSet); wr32(set + 8, gm_malloc(16)); wr32(set + 12, 4); event = new_obj(cls_Event); }
    g_touch_x = x; g_touch_y = y;
    wr32(set + 4, 1); wr32(rd32(set + 8), touch); g_cur_touchset = set;
    const char *sel = phase == 0 ? "touchesBegan:withEvent:" : phase == 1 ? "touchesMoved:withEvent:" : "touchesEnded:withEvent:";
    u32 a[2] = { set, event };
    send_guest(g_glview, sel, 2, a);
}

int objc_run_app(void) {
    gptr app = singleton(&s_app, objc_class_object("UIApplication"));
    u32 a[2] = { app, 0 };
    if (guest_responds(g_delegate, "application:didFinishLaunchingWithOptions:")) send_guest(g_delegate, "application:didFinishLaunchingWithOptions:", 2, a);
    else send_guest(g_delegate, "applicationDidFinishLaunching:", 1, a);
    if (guest_responds(g_delegate, "applicationDidBecomeActive:")) { u32 b[1] = { app }; send_guest(g_delegate, "applicationDidBecomeActive:", 1, b); }
    { gptr v = send_guest(g_delegate, "glView", 0, NULL); if (v) g_glview = v; }
    fprintf(stderr, "[host] launched; delegate=%08x glView=%08x\n", g_delegate, g_glview);
    extern int sdl_pump(void);
    double last = now_s(); int frames = 0; double t0 = last;
    while (g_running) {
        if (!sdl_pump()) break;
        double t = now_s(); int fired = 0;
        for (int i = 0; i < 128; i++) if (timers[i].valid && t >= timers[i].next) {
            timers[i].next = t + timers[i].interval; fired = 1;
            if (!timers[i].repeats) timers[i].valid = 0;
            u32 args[1] = { timers[i].obj }; if (g_trace && !timers[i].repeats) hle_log("[timer-fire] %s %s\n", objc_describe(timers[i].target), timers[i].sel);
            { static int dv; static double dt0; if (!strcmp(timers[i].sel, "drawView:")) { dv++; double tn = now_s(); if (!dt0) dt0 = tn; if (tn - dt0 >= 2.0 && getenv("RA_FPS")) { fprintf(stderr, "[fps] game drawView %.1f/s\n", dv / (tn - dt0)); dv = 0; dt0 = tn; } } }
            send_guest(timers[i].target, timers[i].sel, 1, args);
            frames++;
        }
        if (now_s() - t0 > 2.0) { if (g_trace) fprintf(stderr, "[host] %.1f ticks/s\n", frames / (now_s() - t0)); t0 = now_s(); frames = 0; }
        { extern void host_run_deferred(void); host_run_deferred(); }
        { extern int extra_drain_dispatch(void); if (extra_drain_dispatch()) fired = 1; }
        { static double lastui; double tn = now_s(); if (tn - lastui >= 1.0 / 60) { lastui = tn; extern void sdl_present(void); sdl_present(); fired = 1; } }
        if (!fired) SDL_Delay(1);
        (void)last;
    }
    return 0;
}

static const HleEntry objc_tab[] = {
    {"objc_msgSend", h_msgSend}, {"objc_msgSend_stret", h_msgSend_stret}, {"objc_msgSendSuper2", h_msgSendSuper2}, {"objc_msgSendSuper2_stret", h_msgSendSuper2_stret},
    {"NSLog", h_NSLog}, {"objc_getProperty", h_getProperty}, {"objc_setProperty", h_setProperty}, {"objc_copyStruct", h_copyStruct},
    {"objc_sync_enter", h_zero}, {"objc_sync_exit", h_zero}, {"objc_begin_catch", h_zero}, {"objc_end_catch", h_noop}, {"objc_enumerationMutation", h_noop},
    {"NSClassFromString", h_NSClassFromString}, {"NSSearchPathForDirectoriesInDomains", h_NSSearchPath}, {"NSTemporaryDirectory", h_NSTempDir},
    {"NSSetUncaughtExceptionHandler", h_noop}, {"CFBundleGetMainBundle", h_CFBundleGetMainBundle}, {"CFRelease", h_noop}, {"CFRunLoopGetCurrent", h_zero},
    {"CFStringCreateWithCString", h_CFStringCreateWithCString}, {"CFStringAppendCharacters", h_CFStringAppendCharacters}, {"CFBundleCopyResourceURL", h_CFBundleCopyResourceURL},
    {"CGAffineTransformMakeRotation", h_CGAffineTransformMakeRotation}, {"UIApplicationMain", h_UIApplicationMain},
    {"NSFileSystemFreeSize", h_NSFileSystemFreeSize_dummy},
    {NULL, NULL}
};
void objc_init(void) {
    hle_register_table(objc_tab); hle_register_table(cg_tab);
    cls_NSString = objc_class_object("NSString"); cls_NSMutableString = objc_class_object("NSMutableString"); cls_NSNumber = objc_class_object("NSNumber");
    cls_NSArray = objc_class_object("NSArray"); cls_NSMutableArray = objc_class_object("NSMutableArray"); cls_NSDictionary = objc_class_object("NSDictionary");
    cls_NSData = objc_class_object("NSData"); cls_NSSet = objc_class_object("NSSet"); cls_URL = objc_class_object("NSURL"); cls_cfstr = objc_class_object("__NSCFConstantString");
    cls_Touch = objc_class_object("UITouch"); cls_Event = objc_class_object("UIEvent"); cls_Generic = objc_class_object("NSObject");
    cls_UIImage = objc_class_object("UIImage"); cls_UIColor = objc_class_object("UIColor"); cls_UIFont = objc_class_object("UIFont"); cls_NSCoder = objc_class_object("NSCoder"); cls_UIView = objc_class_object("UIView");
    load_guest_classes();
}
