/* cxxabi.c - the subset of libstdc++ (gcc 4.x, 32-bit COW std::string, red-black tree helpers, stringstream) that Tower imports.
 * The guest inlines parts of these (size(), c_str(), empty()) and reads the string rep header at p-12 directly, so layouts must be exact:
 *   std::string = { char *_M_p }  ->  chars;  rep header {u32 length, u32 capacity, s32 refcount-1} lives at _M_p - 12. */
#include "ra.h"
#include <time.h>
gptr resolve_symbol(const char *name);
static gptr s_empty;
static gptr empty_rep(void) { if (!s_empty) s_empty = resolve_symbol("__ZNSs4_Rep20_S_empty_rep_storageE") + 12; return s_empty; }   /* chars of the shared empty rep */
static u32 slen(gptr p) { return rd32(p - 12); }
static gptr rep_new(u32 len, u32 cap) { if (cap < len) cap = len; gptr r = gm_malloc(12 + cap + 1); wr32(r, len); wr32(r + 4, cap); wr32(r + 8, 0); *(char *)H(r + 12 + len) = 0; return r + 12; }
static void rep_release(gptr p) { if (p == empty_rep()) return; s32 rc = (s32)rd32(p - 4); if (rc <= 0) gm_free(p - 12); else wr32(p - 4, rc - 1); }
static gptr rep_from(const char *s, u32 n) { if (!n) return empty_rep(); gptr p = rep_new(n, n); memcpy(H(p), s, n); return p; }
static void set_str(gptr obj, gptr p) { wr32(obj, p); }
/* make *obj uniquely owned with capacity >= want, keeping content */
static gptr unshare(gptr obj, u32 want) {
    gptr p = rd32(obj); u32 len = slen(p), cap = rd32(p - 8); s32 rc = (s32)rd32(p - 4);
    if (p != empty_rep() && rc <= 0 && cap >= want) return p;
    gptr q = rep_new(len, want > len ? want : len); memcpy(H(q), H(p), len); rep_release(p); wr32(obj, q); return q;
}

/* ---- operator new/delete ---- */
static void h_new(void) { gptr p = gm_malloc(arg(0)); if (!p) { fprintf(stderr, "[guest] operator new failed (%u)\n", arg(0)); abort(); } ret(p); }
static void h_delete(void) { gm_free(arg(0)); }
static void h_throw_bad_alloc(void) { fprintf(stderr, "[guest] std::bad_alloc thrown\n"); abort(); }
static void h_throw_len(void) { fprintf(stderr, "[guest] std::length_error: %s\n", gstr(arg(0))); abort(); }
static void h_terminate(void) { fprintf(stderr, "[guest] std::terminate\n"); abort(); }

/* ---- std::string ---- */
static void h_str_ctor_cstr(void) { const char *s = gstr(arg(1)); set_str(arg(0), rep_from(s, strlen(s))); }
static void h_str_ctor_copy(void) { gptr src = rd32(arg(1)); if (src != empty_rep()) { s32 rc = (s32)rd32(src - 4); if (rc < 0) { set_str(arg(0), rep_from((char *)H(src), slen(src))); return; } wr32(src - 4, rc + 1); } set_str(arg(0), src); }
static void h_str_dtor(void) { rep_release(rd32(arg(0))); }
static void assign_bytes(gptr obj, const char *s, u32 n) { gptr p = rd32(obj); if (!n) { rep_release(p); set_str(obj, empty_rep()); return; }
    char *tmp = malloc(n); memcpy(tmp, s, n); rep_release(p); set_str(obj, rep_from(tmp, n)); free(tmp); }
static void h_str_assign_cn(void) { assign_bytes(arg(0), (const char *)H(arg(1)), arg(2)); ret(arg(0)); }
static void h_str_assign_str(void) { gptr d = arg(0), src = rd32(arg(1)); if (rd32(d) == src) { ret(d); return; } rep_release(rd32(d));
    if (src != empty_rep()) { s32 rc = (s32)rd32(src - 4); if (rc < 0) { set_str(d, rep_from((char *)H(src), slen(src))); ret(d); return; } wr32(src - 4, rc + 1); } set_str(d, src); ret(d); }
static void append_bytes(gptr obj, const char *s, u32 n) { if (!n) return; gptr p = rd32(obj); u32 len = slen(p); char *tmp = malloc(n); memcpy(tmp, s, n); p = unshare(obj, len + n + (len + n) / 2 + 8); memcpy(H(p + len), tmp, n); wr32(p - 12, len + n); *(char *)H(p + len + n) = 0; free(tmp); }
static void h_str_append_cn(void) { append_bytes(arg(0), (const char *)H(arg(1)), arg(2)); ret(arg(0)); }
static void h_str_append_str(void) { gptr s = rd32(arg(1)); append_bytes(arg(0), (const char *)H(s), slen(s)); ret(arg(0)); }
static void h_str_reserve(void) { unshare(arg(0), arg(1)); }
static void h_str_erase(void) { gptr obj = arg(0); u32 pos = arg(1), n = arg(2); gptr p = rd32(obj); u32 len = slen(p); if (pos > len) { h_throw_len(); } if (n > len - pos) n = len - pos; if (!n) { ret(obj); return; }
    p = unshare(obj, len); memmove(H(p + pos), H(p + pos + n), len - pos - n); wr32(p - 12, len - n); *(char *)H(p + len - n) = 0; ret(obj); }
static void h_str_mutate(void) { gptr obj = arg(0); u32 pos = arg(1), l1 = arg(2), l2 = arg(3); gptr p = rd32(obj); u32 len = slen(p), nl = len - l1 + l2; gptr q = rep_new(nl, nl);
    memcpy(H(q), H(p), pos); memcpy(H(q + pos + l2), H(p + pos + l1), len - pos - l1); rep_release(p); wr32(obj, q); }
static void h_str_leak_hard(void) { gptr obj = arg(0); gptr p = rd32(obj); if (p == empty_rep()) return; if ((s32)rd32(p - 4) > 0) unshare(obj, rd32(p - 8)); wr32(rd32(obj) - 4, (u32)-1); }
static void h_str_find(void) { gptr p = rd32(arg(0)); const char *s = (char *)H(arg(1)); u32 pos = arg(2), n = arg(3), len = slen(p); if (n == 0) { ret(pos <= len ? pos : (u32)-1); return; }
    for (u32 i = pos; i + n <= len; i++) if (!memcmp(H(p + i), s, n)) { ret(i); return; } ret((u32)-1); }
static void h_str_rfind(void) { gptr p = rd32(arg(0)); const char *s = (char *)H(arg(1)); u32 pos = arg(2), n = arg(3), len = slen(p); if (n > len) { ret((u32)-1); return; } u32 i = len - n < pos ? len - n : pos;
    for (;; i--) { if (!memcmp(H(p + i), s, n)) { ret(i); return; } if (!i) break; } ret((u32)-1); }
static void h_str_substr(void) { gptr ret_slot = arg(0), p = rd32(arg(1)); u32 pos = arg(2), n = arg(3), len = slen(p); if (pos > len) h_throw_len(); if (n > len - pos) n = len - pos; set_str(ret_slot, rep_from((char *)H(p + pos), n)); }
static void h_str_compare_c(void) { gptr p = rd32(arg(0)); ret((u32)strcmp((char *)H(p), gstr(arg(1)))); }
static void h_str_compare_s(void) { gptr a = rd32(arg(0)), b = rd32(arg(1)); u32 la = slen(a), lb = slen(b), m = la < lb ? la : lb; int r = memcmp(H(a), H(b), m); ret(r ? (u32)r : (u32)((s32)la - (s32)lb)); }

/* ---- std::list node hook/unhook ---- */
static void h_list_hook(void) { gptr x = arg(0), pos = arg(1); wr32(x, pos); wr32(x + 4, rd32(pos + 4)); wr32(rd32(pos + 4), x); wr32(pos + 4, x); }   /* node {next, prev} */
static void h_list_unhook(void) { gptr x = arg(0), nx = rd32(x), pv = rd32(x + 4); wr32(pv, nx); wr32(nx + 4, pv); }

/* ---- red-black tree (libstdc++ tree.cc) : node {u32 color, parent, left, right} ---- */
#define COL(x) rd32(x)
#define PAR(x) rd32((x) + 4)
#define LEFT(x) rd32((x) + 8)
#define RIGHT(x) rd32((x) + 12)
#define SCOL(x, v) wr32((x), (v))
#define SPAR(x, v) wr32((x) + 4, (v))
#define SLEFT(x, v) wr32((x) + 8, (v))
#define SRIGHT(x, v) wr32((x) + 12, (v))
static void rotl(gptr x, gptr hdr) { gptr y = RIGHT(x); SRIGHT(x, LEFT(y)); if (LEFT(y)) SPAR(LEFT(y), x); SPAR(y, PAR(x)); if (x == PAR(hdr)) SPAR(hdr, y); else if (x == LEFT(PAR(x))) SLEFT(PAR(x), y); else SRIGHT(PAR(x), y); SLEFT(y, x); SPAR(x, y); }
static void rotr(gptr x, gptr hdr) { gptr y = LEFT(x); SLEFT(x, RIGHT(y)); if (RIGHT(y)) SPAR(RIGHT(y), x); SPAR(y, PAR(x)); if (x == PAR(hdr)) SPAR(hdr, y); else if (x == RIGHT(PAR(x))) SRIGHT(PAR(x), y); else SLEFT(PAR(x), y); SRIGHT(y, x); SPAR(x, y); }
static gptr rb_inc(gptr x) { if (RIGHT(x)) { x = RIGHT(x); while (LEFT(x)) x = LEFT(x); } else { gptr y = PAR(x); while (x == RIGHT(y)) { x = y; y = PAR(y); } if (RIGHT(x) != y) x = y; } return x; }
static gptr rb_dec(gptr x) { if (COL(x) == 0 && PAR(PAR(x)) == x) x = RIGHT(x); else if (LEFT(x)) { gptr y = LEFT(x); while (RIGHT(y)) y = RIGHT(y); x = y; } else { gptr y = PAR(x); while (x == LEFT(y)) { x = y; y = PAR(y); } x = y; } return x; }
static void h_rb_inc(void) { ret(rb_inc(arg(0))); }
static void h_rb_dec(void) { ret(rb_dec(arg(0))); }
static void h_rb_insert(void) {
    int left = arg(0) & 0xff; gptr x = arg(1), p = arg(2), hdr = arg(3);
    SPAR(x, p); SLEFT(x, 0); SRIGHT(x, 0); SCOL(x, 0);
    if (left) { SLEFT(p, x); if (p == hdr) { SPAR(hdr, x); SRIGHT(hdr, x); } else if (p == LEFT(hdr)) SLEFT(hdr, x); } else { SRIGHT(p, x); if (p == RIGHT(hdr)) SRIGHT(hdr, x); }
    while (x != PAR(hdr) && COL(PAR(x)) == 0) {
        gptr xpp = PAR(PAR(x));
        if (PAR(x) == LEFT(xpp)) { gptr y = RIGHT(xpp); if (y && COL(y) == 0) { SCOL(PAR(x), 1); SCOL(y, 1); SCOL(xpp, 0); x = xpp; } else { if (x == RIGHT(PAR(x))) { x = PAR(x); rotl(x, hdr); } SCOL(PAR(x), 1); SCOL(PAR(PAR(x)), 0); rotr(PAR(PAR(x)), hdr); } }
        else { gptr y = LEFT(xpp); if (y && COL(y) == 0) { SCOL(PAR(x), 1); SCOL(y, 1); SCOL(xpp, 0); x = xpp; } else { if (x == LEFT(PAR(x))) { x = PAR(x); rotr(x, hdr); } SCOL(PAR(x), 1); SCOL(PAR(PAR(x)), 0); rotl(PAR(PAR(x)), hdr); } }
    }
    SCOL(PAR(hdr), 1);
}
static gptr rb_min(gptr x) { while (LEFT(x)) x = LEFT(x); return x; }
static gptr rb_max(gptr x) { while (RIGHT(x)) x = RIGHT(x); return x; }
static void h_rb_erase(void) {
    gptr z = arg(0), hdr = arg(1), y = z, x = 0, xp = 0;
    if (!LEFT(y)) x = RIGHT(y); else if (!RIGHT(y)) x = LEFT(y); else { y = RIGHT(y); while (LEFT(y)) y = LEFT(y); x = RIGHT(y); }
    if (y != z) {
        SPAR(LEFT(z), y); SLEFT(y, LEFT(z));
        if (y != RIGHT(z)) { xp = PAR(y); if (x) SPAR(x, PAR(y)); SLEFT(PAR(y), x); SRIGHT(y, RIGHT(z)); SPAR(RIGHT(z), y); } else xp = y;
        if (PAR(hdr) == z) SPAR(hdr, y); else if (LEFT(PAR(z)) == z) SLEFT(PAR(z), y); else SRIGHT(PAR(z), y);
        SPAR(y, PAR(z)); u32 t = COL(y); SCOL(y, COL(z)); SCOL(z, t); y = z;
    } else {
        xp = PAR(y); if (x) SPAR(x, PAR(y));
        if (PAR(hdr) == z) SPAR(hdr, x); else if (LEFT(PAR(z)) == z) SLEFT(PAR(z), x); else SRIGHT(PAR(z), x);
        if (LEFT(hdr) == z) { if (!RIGHT(z)) SLEFT(hdr, PAR(z)); else SLEFT(hdr, rb_min(x)); }
        if (RIGHT(hdr) == z) { if (!LEFT(z)) SRIGHT(hdr, PAR(z)); else SRIGHT(hdr, rb_max(x)); }
    }
    if (COL(y) != 0) {
        while (x != PAR(hdr) && (x == 0 || COL(x) == 1)) {
            if (x == LEFT(xp)) { gptr w = RIGHT(xp);
                if (COL(w) == 0) { SCOL(w, 1); SCOL(xp, 0); rotl(xp, hdr); w = RIGHT(xp); }
                if ((LEFT(w) == 0 || COL(LEFT(w)) == 1) && (RIGHT(w) == 0 || COL(RIGHT(w)) == 1)) { SCOL(w, 0); x = xp; xp = PAR(xp); }
                else { if (RIGHT(w) == 0 || COL(RIGHT(w)) == 1) { SCOL(LEFT(w), 1); SCOL(w, 0); rotr(w, hdr); w = RIGHT(xp); } SCOL(w, COL(xp)); SCOL(xp, 1); if (RIGHT(w)) SCOL(RIGHT(w), 1); rotl(xp, hdr); break; } }
            else { gptr w = LEFT(xp);
                if (COL(w) == 0) { SCOL(w, 1); SCOL(xp, 0); rotr(xp, hdr); w = LEFT(xp); }
                if ((RIGHT(w) == 0 || COL(RIGHT(w)) == 1) && (LEFT(w) == 0 || COL(LEFT(w)) == 1)) { SCOL(w, 0); x = xp; xp = PAR(xp); }
                else { if (LEFT(w) == 0 || COL(LEFT(w)) == 1) { SCOL(RIGHT(w), 1); SCOL(w, 0); rotl(w, hdr); w = LEFT(xp); } SCOL(w, COL(xp)); SCOL(xp, 1); if (LEFT(w)) SCOL(LEFT(w), 1); rotr(xp, hdr); break; } }
        }
        if (x) SCOL(x, 1);
    }
    ret(y);
}

/* ---- std::stringstream / ostream (insertion only) ---- */
typedef struct { gptr base; char *buf; size_t len, cap; } SS; static SS sstab[256]; static int nss;
static SS *ss_find(gptr p) { for (int i = 0; i < nss; i++) if (sstab[i].base && p >= sstab[i].base && p < sstab[i].base + 0x100) return &sstab[i]; return NULL; }
static void ss_add(SS *s, const char *t, size_t n) { if (s->len + n + 1 > s->cap) { s->cap = (s->len + n + 1) * 2; s->buf = realloc(s->buf, s->cap); } memcpy(s->buf + s->len, t, n); s->len += n; s->buf[s->len] = 0; }
static void h_ss_ctor(void) { gptr b = arg(0); SS *s = NULL; for (int i = 0; i < nss; i++) if (!sstab[i].base) s = &sstab[i]; if (!s && nss < 256) s = &sstab[nss++]; if (!s) return; s->base = b; free(s->buf); s->buf = NULL; s->len = s->cap = 0; }
static void h_ss_dtor(void) { SS *s = ss_find(arg(0)); if (s) { free(s->buf); s->buf = NULL; s->base = 0; } }
static void h_ostream_insert(void) { SS *s = ss_find(arg(0)); if (s) ss_add(s, gstr(arg(1)), arg(2)); ret(arg(0)); }
static void h_ostream_int(void) { SS *s = ss_find(arg(0)); char b[32]; int n = snprintf(b, sizeof b, "%d", (int)arg(1)); if (s) ss_add(s, b, n); ret(arg(0)); }
static void h_stringbuf_str(void) { SS *s = ss_find(arg(1)); gptr slot = arg(0); set_str(slot, s ? rep_from(s->buf ? s->buf : "", (u32)s->len) : empty_rep()); }
static void h_noop(void) { }
static void h_atexit(void) { ret(0); }
static void h_rand(void) { ret((u32)rand() & 0x7fffffff); }
static void h_mediatime(void) { struct timespec ts; clock_gettime(CLOCK_MONOTONIC, &ts); double d = ts.tv_sec + ts.tv_nsec * 1e-9; u64 b; memcpy(&b, &d, 8); ret64(b); }

static const HleEntry cxx_tab[] = {
    {"_Znwm", h_new}, {"_Znam", h_new}, {"_ZnwmRKSt9nothrow_t", h_new}, {"_ZdlPv", h_delete}, {"_ZdaPv", h_delete}, {"_ZdlPvRKSt9nothrow_t", h_delete},
    {"_ZSt17__throw_bad_allocv", h_throw_bad_alloc}, {"_ZSt20__throw_length_errorPKc", h_throw_len}, {"_ZSt9terminatev", h_terminate},
    {"_ZNSsC1EPKcRKSaIcE", h_str_ctor_cstr}, {"_ZNSsC1ERKSs", h_str_ctor_copy}, {"_ZNSsD2Ev", h_str_dtor}, {"_ZNSs6assignEPKcm", h_str_assign_cn}, {"_ZNSs6assignERKSs", h_str_assign_str},
    {"_ZNSs6appendEPKcm", h_str_append_cn}, {"_ZNSs6appendERKSs", h_str_append_str}, {"_ZNSs7reserveEm", h_str_reserve}, {"_ZNSs5eraseEmm", h_str_erase}, {"_ZNSs9_M_mutateEmmm", h_str_mutate},
    {"_ZNSs12_M_leak_hardEv", h_str_leak_hard}, {"_ZNKSs4findEPKcmm", h_str_find}, {"_ZNKSs5rfindEPKcmm", h_str_rfind}, {"_ZNKSs6substrEmm", h_str_substr},
    {"_ZNKSs7compareEPKc", h_str_compare_c}, {"_ZNKSs7compareERKSs", h_str_compare_s},
    {"_ZNSt15_List_node_base4hookEPS_", h_list_hook}, {"_ZNSt15_List_node_base6unhookEv", h_list_unhook},
    {"_ZSt18_Rb_tree_incrementPSt18_Rb_tree_node_base", h_rb_inc}, {"_ZSt18_Rb_tree_incrementPKSt18_Rb_tree_node_base", h_rb_inc}, {"_ZSt18_Rb_tree_decrementPSt18_Rb_tree_node_base", h_rb_dec},
    {"_ZSt29_Rb_tree_insert_and_rebalancebPSt18_Rb_tree_node_baseS0_RS_", h_rb_insert}, {"_ZSt28_Rb_tree_rebalance_for_erasePSt18_Rb_tree_node_baseRS_", h_rb_erase},
    {"_ZNSt18basic_stringstreamIcSt11char_traitsIcESaIcEEC1ESt13_Ios_Openmode", h_ss_ctor}, {"_ZNSt18basic_stringstreamIcSt11char_traitsIcESaIcEED1Ev", h_ss_dtor},
    {"_ZSt16__ostream_insertIcSt11char_traitsIcEERSt13basic_ostreamIT_T0_ES6_PKS3_i", h_ostream_insert}, {"_ZNSolsEi", h_ostream_int},
    {"_ZNKSt15basic_stringbufIcSt11char_traitsIcESaIcEE3strEv", h_stringbuf_str}, {"_ZNSt15basic_stringbufIcSt11char_traitsIcESaIcEE17_M_stringbuf_initESt13_Ios_Openmode", h_noop},
    {"_ZNSt9basic_iosIcSt11char_traitsIcEE5clearESt12_Ios_Iostate", h_noop}, {"_ZNSt8ios_base4InitC1Ev", h_noop}, {"__cxa_atexit", h_atexit}, {"rand", h_rand}, {"CACurrentMediaTime", h_mediatime},
    {"CGPathCreateMutable", h_noop}, {"CGPathMoveToPoint", h_noop}, {"CGPathAddLineToPoint", h_noop}, {"CGPathAddCurveToPoint", h_noop}, {"CGPathRelease", h_noop},
    {NULL, NULL}
};
void cxxabi_init(void) { hle_register_table(cxx_tab); }
