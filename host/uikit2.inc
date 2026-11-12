/* uikit2.inc - Objective-C message handlers for the UIKit layer (included into objc.c after host_msg's helpers). */
#define UA(n) arg((stret ? 1 : 0) + (n))
#define UIS(s) (!strcmp(sel, s))
static void ret_double(double d);
static float uf(int n, int stret) { return f_of(arg((stret ? 1 : 0) + n)); }
static void ret_point(float x, float y) { gptr p = arg(0); wr32(p, w_of(x)); wr32(p + 4, w_of(y)); }
static void ret_size(float w, float h) { gptr p = arg(0); wr32(p, w_of(w)); wr32(p + 4, w_of(h)); }
static gptr s_layer_for(gptr view) { View *v = view_of(view); if (!v) return 0; if (!v->layer) { v->layer = new_obj(objc_class_object("CAEAGLLayer")); wr32(v->layer + 4, view); } return v->layer; }
static View *find_tag(View *v, int tag) { for (int i = 0; i < v->nk; i++) { if (v->kids[i]->tag == tag) return v->kids[i]; View *r = find_tag(v->kids[i], tag); if (r) return r; } return NULL; }
static gptr ui_view_array(View *v) { gptr a = new_array(); for (int i = 0; i < v->nk; i++) array_add(a, v->kids[i]->obj); return a; }
static void run_block(gptr blk, int nargs, u32 a1) { if (!blk || blk >= ARENA_SIZE - 32) return; u32 a[2] = { blk, a1 }; call_guest(rd32(blk + 12), 1 + nargs, a); }
/* ---- UIView animations: scope snapshot -> diff -> time-interpolated View fields ---- */
#define NPROP 11
typedef struct { View *v; float from[NPROP], to[NPROP], last[NPROP]; unsigned char det[NPROP]; double t0, dur, delay; float rep; int autorev, curve, live; gptr comp, dlg, ctx, name; char stop[64]; } Anim;
static Anim anims[2048]; static int nanims;
static struct { int open; double dur, delay; float rep; int autorev, curve; gptr dlg, ctx, name, comp; char stop[64]; float (*snap)[NPROP]; int n; } sc;
static unsigned long g_anim_frames;
static double g_movie_start;
static double anim_now(void) { if (getenv("RA_NOVSYNC")) return g_anim_frames / 60.0; struct timespec ts; clock_gettime(CLOCK_MONOTONIC, &ts); return ts.tv_sec + ts.tv_nsec * 1e-9; }
static void get_props(View *v, float *o) { o[0] = v->cx; o[1] = v->cy; o[2] = v->bw; o[3] = v->bh; o[4] = v->alpha; o[5] = v->ta; o[6] = v->tb; o[7] = v->tc_; o[8] = v->td; o[9] = v->ttx; o[10] = v->tty; }
static void put_props(View *v, const float *o) { v->cx = o[0]; v->cy = o[1]; v->bw = o[2]; v->bh = o[3]; v->alpha = o[4]; v->ta = o[5]; v->tb = o[6]; v->tc_ = o[7]; v->td = o[8]; v->ttx = o[9]; v->tty = o[10]; }
static float curve_f(int c, float t) { if (t <= 0) return 0; if (t >= 1) return 1; switch (c) { case 1: return t * t; case 2: return t * (2 - t); case 3: return t; default: return t * t * (3 - 2 * t); } }
/* completion blocks outlive the caller's stack frame: copy them to the heap (Block_copy semantics, incl. the descriptor's copy helper) */
static gptr block_copy_ui(gptr b) {
    if (!b || b >= ARENA_SIZE - 32) return 0; u32 flags = rd32(b + 8 - 4 + 0);   /* isa@0 flags@4 reserved@8 invoke@12 desc@16 */
    flags = rd32(b + 4); if (flags & (1u << 28)) return b;                     /* global block: immortal */
    gptr desc = rd32(b + 16); u32 size = desc ? rd32(desc + 4) : 20; if (size < 20 || size > 4096) size = 20;
    gptr n = gm_malloc(size); memcpy(H(n), H(b), size); wr32(n + 4, (flags | (1u << 24)) & ~(1u << 29));
    if ((flags & (1u << 25)) && desc) { u32 a[2] = { n, b }; call_guest(rd32(desc + 8), 2, a); }
    return n;
}
static void anim_complete(Anim *a, int finished) {
    gptr comp = a->comp, dlg = a->dlg, ctx = a->ctx, nm = a->name; char stop[64]; strcpy(stop, a->stop);
    hle_log("[anim] complete comp=%08x dlg=%08x stop=%s\n", comp, dlg, stop); if (comp) run_block(comp, 1, finished);
    if (dlg && stop[0]) { u32 args[3] = { nm, new_number_i(finished), ctx }; send_any(dlg, stop, 3, args); }
}
static void anim_remove(int i) { anims[i] = anims[--nanims]; }
static void scope_begin(double dur, double delay, float rep, int autorev, int curve, gptr comp) {
    sc.open = 1; sc.dur = dur; sc.delay = delay; sc.rep = rep; sc.autorev = autorev; sc.curve = curve; sc.comp = comp;
    free(sc.snap); sc.n = nvt; sc.snap = malloc(sizeof(float[NPROP]) * (nvt + 1)); for (int i = 0; i < nvt; i++) get_props(vtab[i], sc.snap[i]);
}
static void scope_end(void) {
    if (!sc.open) return; sc.open = 0; int made = 0; double now = anim_now();
    for (int i = 0; i < sc.n && i < nvt; i++) { float cur[NPROP]; View *v = vtab[i]; get_props(v, cur); if (!memcmp(cur, sc.snap[i], sizeof cur)) continue;
        if (sc.dur <= 0.001 && sc.delay <= 0.001) continue;
        for (int k = 0; k < nanims; k++) if (anims[k].v == v) { gptr c = anims[k].comp; anims[k].comp = 0; anims[k].dlg = 0; (void)c; anim_remove(k); break; }
        if (nanims >= 2048) break; Anim *a = &anims[nanims++]; memset(a, 0, sizeof *a); a->v = v; memcpy(a->from, sc.snap[i], sizeof cur); memcpy(a->to, cur, sizeof cur); memcpy(a->last, a->from, sizeof cur);
        a->t0 = now; a->dur = sc.dur > 0.001 ? sc.dur : 0.001; a->delay = sc.delay; a->rep = sc.rep; a->autorev = sc.autorev; a->curve = sc.curve; a->live = 1; put_props(v, a->from); made++; }
    /* completion / delegate fire once, from a timer-only record (or immediately when nothing animates) */
    if (sc.comp || (sc.dlg && sc.stop[0])) {
        if (sc.dur <= 0.001 && sc.delay <= 0.001) { Anim t; memset(&t, 0, sizeof t); t.comp = sc.comp; t.dlg = sc.dlg; t.ctx = sc.ctx; t.name = sc.name; strcpy(t.stop, sc.stop); anim_complete(&t, 1); }
        else if (nanims < 2048) { Anim *a = &anims[nanims++]; memset(a, 0, sizeof *a); a->t0 = now; a->dur = sc.dur > 0.001 ? sc.dur : 0.001; a->delay = sc.delay; a->rep = sc.rep; a->autorev = sc.autorev; a->comp = sc.comp; a->dlg = sc.dlg; a->ctx = sc.ctx; a->name = sc.name; strcpy(a->stop, sc.stop); a->live = 1; }
    }
    sc.comp = 0; sc.dlg = 0; sc.stop[0] = 0; (void)made;
}
void uikit_anim_tick(void) {
    g_anim_frames++; double now = anim_now();
    for (int i = 0; i < nanims; ) { Anim *a = &anims[i]; double p = (now - a->t0 - a->delay) / a->dur; int done = 0; float cur[NPROP];
        float rep = a->rep > 0 ? a->rep : 1; if (a->v) { get_props(a->v, cur); if (p > 0) for (int j = 0; j < NPROP; j++) if (cur[j] != a->last[j]) a->det[j] = 1; }   /* a property the app wrote itself stops animating */
        if (p >= rep) done = 1;
        if (a->v && p > 0) { float o[NPROP]; float f = 1; if (!done) { int k = (int)p; float fr = (float)(p - k); if (a->autorev && (k & 1)) fr = 1 - fr; f = curve_f(a->curve, fr); }
            for (int j = 0; j < NPROP; j++) o[j] = a->det[j] ? cur[j] : (done ? a->to[j] : a->from[j] + (a->to[j] - a->from[j]) * f);
            put_props(a->v, o); memcpy(a->last, o, sizeof o); }
        if (done || (!a->v && !a->comp && !a->dlg)) { Anim t = *a; anim_remove(i); if (done && (t.comp || t.dlg)) anim_complete(&t, 1); i = 0; if (nanims == 0) break; continue; }
        i++; }
}
static int anim_block(gptr blk, double dur, double delay, int opts, gptr comp) {
    if (getenv("RA_NOANIM")) { run_block(blk, 0, 0); if (comp) run_block(comp, 1, 1); return 1; }
    int curve = (opts >> 16) & 3; float rep = (opts & 8) ? 1e9f : 1; int ar = (opts & 16) != 0;
    int nested = sc.open; if (nested) return 0;
    scope_begin(dur, delay, rep, ar, curve, 0); sc.comp = comp ? block_copy_ui(comp) : 0; run_block(blk, 0, 0); scope_end(); return 1;
}
/* ---- NSNotificationCenter ---- */
typedef struct { gptr obs; char sel[64]; char name[96]; gptr object; } NObs; static NObs nobs[512]; static int nnobs;
static gptr cls_Notification;
static gptr new_notification(const char *name, gptr object, gptr userinfo) { gptr n = new_obj(cls_Notification); wr32(n + 4, new_string(name)); wr32(n + 8, object); wr32(n + 12, userinfo); return n; }
/* The offline server stand-in answers a join with room data that has no "players" list, but the engine looks its own player up there by slot
 * (-[TDPlayer playerWithSlot:]); the real server always sends it.  Add the local player (slot 1) built from the profile. */
gptr offline_room_players(gptr room); gptr offline_room_field(gptr room, gptr key);
static int is_number(gptr o); static void dict_put(gptr d, gptr k, gptr v); static gptr dict_lookup(gptr d, gptr k);
gptr offline_room_players(gptr room) {
    GCls *gc = find_gcls_name("TDSession"); gptr cls = gc ? gc->addr : 0; gptr sess = send_any(cls, "session", 0, NULL); gptr pl = send_any(cls, "player", 0, NULL); if (!pl && sess) pl = send_any(sess, "currentPlayer", 0, NULL); hle_log("[room] cls=%x\n", cls);
    gptr ud = pl ? send_any(pl, "userData", 0, NULL) : 0; hle_log("[room] sess=%x pl=%x ud=%x room=%x\n", sess, pl, ud, room); if (!room || !ud) return 0;
    gptr pk = new_string("players"); if (dict_lookup(room, pk)) return dict_lookup(room, pk);
    gptr me = new_dict(); wr32(me + 4, 0);
    for (u32 i = 0; i < rd32(ud + 4); i++) { gptr k = rd32(rd32(ud + 8) + 4 * i), v = rd32(rd32(ud + 12) + 4 * i); if (is_string(v) || is_number(v)) dict_put(me, k, v); }
    dict_put(me, new_string("slot"), new_number_i(1)); dict_put(me, new_string("userid"), new_string("offline")); dict_put(me, new_string("color"), new_number_i(1));
    gptr arr = new_array(); array_add(arr, me); dict_put(room, pk, arr); hle_log("[room] added local player to roomData\n"); return arr;
}
/* settings / grantsunlock / msgwin / msglost live in the offline config but the offline join drops them; serve them from offlineConfigs on demand. */
gptr offline_room_field(gptr room, gptr key) {
    GCls *gc = find_gcls_name("TDSession"); if (!gc) return 0;
    gptr pl = send_any(gc->addr, "player", 0, NULL); gptr ud = pl ? send_any(pl, "userData", 0, NULL) : 0; if (!ud) return 0;
    gptr cid = dict_lookup(room, new_string("configid")); gptr cfgs = dict_lookup(ud, new_string("offlineConfigs")); if (!cid || !cfgs) return 0;
    for (u32 i = 0; i < rd32(cfgs + 4); i++) { gptr c = array_get(cfgs, i); gptr id = c ? dict_lookup(c, new_string("configid")) : 0;
        if (id && !strcmp(objc_describe(id), objc_describe(cid))) { gptr v = dict_lookup(c, key); if (v) { dict_put(room, key, v); hle_log("[room] %s from offline config %s\n", scstr(key), objc_describe(cid)); } return v; } }
    return 0;
}
static void post_notification(const char *name, gptr object, gptr userinfo);
static double num_double(gptr o); static gptr new_number_d(double d);
/* Free spice shop: +[InAppItemsHandler buySpice:] gets the product id; grant that pack's spice (and unlock) from userData.tower_inapp and refresh the bars. */
void offline_buy_spice(gptr pid) {
    GCls *gc = find_gcls_name("TDSession"); hle_log("[shop] buySpice: %s\n", pid ? (is_string(pid) ? scstr(pid) : objc_describe(pid)) : "(nil)"); if (!gc || !pid) return;
    gptr pl = send_any(gc->addr, "player", 0, NULL); gptr ud = pl ? send_any(pl, "userData", 0, NULL) : 0; gptr list = ud ? dict_lookup(ud, new_string("tower_inapp")) : 0; if (!list) return;
    for (u32 i = 0; i < rd32(list + 4); i++) { gptr e = array_get(list, i); gptr id = e ? dict_lookup(e, new_string("inappid")) : 0; if (!id || strcmp(scstr(id), scstr(pid))) continue;
        gptr sv = dict_lookup(e, new_string("spice")); int add = sv ? (int)(is_string(sv) ? atof(scstr(sv)) : num_double(sv)) : 0; gptr cur = dict_lookup(ud, new_string("spice")); int have = cur ? (int)(is_string(cur) ? atof(scstr(cur)) : num_double(cur)) : 0;
        char b[16]; snprintf(b, sizeof b, "%d", have + add); dict_put(ud, new_string("spice"), new_string(b));
        gptr gu = dict_lookup(e, new_string("grantsunlock")); gptr ul = dict_lookup(ud, new_string("unlocks")); if (gu && ul && rd32(ul) && rd32(ul + 4) < 4096) array_add(ul, gu);
        hle_log("[shop] free spice pack %s: +%d (now %d)\n", scstr(pid), add, have + add);
        send_guest(pl, "saveUserData", 0, NULL);
        { gptr nc = send_any(objc_class_object("NSNotificationCenter"), "defaultCenter", 0, NULL); u32 a[2] = { new_string("O5PlayerReceivedUserData"), 0 }; if (nc) send_any(nc, "postNotificationName:object:", 2, a); }
        return; }
}
void offline_post_configs(void);
/* The server levels the player up ("You will level up when you get online!" otherwise): apply it to the offline profile - level+1, one
 * upgrade point, next threshold 10 xp higher per level (level 1 needs 10) - before the map's player bar is refreshed. */
static double num_double(gptr o); static gptr new_number_d(double d);
void offline_levelup_check(void) {
    GCls *gc = find_gcls_name("TDSession"); if (!gc) return;
    gptr pl = send_any(gc->addr, "player", 0, NULL); gptr ud = pl ? send_any(pl, "userData", 0, NULL) : 0; if (!ud) return;
    #define UDF(k) ({ gptr _v = dict_lookup(ud, new_string(k)); _v ? (is_string(_v) ? atof(scstr(_v)) : num_double(_v)) : 0.0; })
    /* tonextlevel = xp still missing for the next level (negative once the threshold is passed); levelpoints = xp at the start of this level */
    double xp = UDF("xppoints"), lp = UDF("levelpoints"), tn = UDF("tonextlevel"); int lv = (int)UDF("level"), sp = (int)UDF("specpoints"), n = 0;
    /* Spice is the reward currency offline (the server paid it online): 1 spice per 5 xp earned plus 10 for each level gained, so tech can be bought. */
    { static double last_xp = -1, carry; double sp_now = UDF("spice");
      if (last_xp >= 0 && xp > last_xp) { carry += (xp - last_xp) / 5.0; int add = (int)carry; if (add > 0) { carry -= add; char b[16]; snprintf(b, sizeof b, "%d", (int)sp_now + add); dict_put(ud, new_string("spice"), new_string(b)); hle_log("[spice] +%d for xp\n", add); } }
      last_xp = xp; }
    if (tn > 0) return;
    while (tn <= 0 && n < 50) { lp = xp + tn; lv++; sp++; tn += 10 * lv; n++; }   /* level L needs 10*L xp */
    if (!n) return;
    dict_put(ud, new_string("levelpoints"), new_number_d(lp)); { char b[16]; snprintf(b, sizeof b, "%d", lv); dict_put(ud, new_string("level"), new_string(b)); snprintf(b, sizeof b, "%d", sp); dict_put(ud, new_string("specpoints"), new_string(b)); }
    dict_put(ud, new_string("tonextlevel"), new_number_d(tn));
    { char b[16]; snprintf(b, sizeof b, "%d", (int)UDF("spice") + 10 * n); dict_put(ud, new_string("spice"), new_string(b)); }
    hle_log("[levelup] now level %d (xp %.1f, %.1f to go)\n", lv, xp, tn);
}
/* The online server delivers the per-level configs (settings, grantsunlock, msgwin, ...) in the login response and -[TDPlayer processResponse:] posts
 * TDPlayerReceivedConfigs with them; the offline profile keeps them in userData.configs but nothing posts them, so the engine never learns the level
 * settings (tutorial prompts, build-around-base, creep hp ...).  Post them once, before the first room data (the join). */
/* Offline the game rebuilds the player from the bundled starter profile on every launch and only merges "unlocks" back from the saved
 * AnonymousUserData; restore the earned progress (xp, level, spice, upgrade points and purchased upgrades) from that saved copy. */
gptr defs_get_obj(const char *k);
static void offline_restore_progress(gptr ud) {
    gptr saved = defs_get_obj("AnonymousUserData"); if (!saved || rd32(saved + 4) == 0) return; int n = 0;
    for (u32 i = 0; i < rd32(saved + 4); i++) { gptr k = rd32(rd32(saved + 8) + 4 * i), v = rd32(rd32(saved + 12) + 4 * i); if (!k || !v || !is_string(k)) continue; const char *ks = scstr(k);
        if (!strcmp(ks, "xppoints") || !strcmp(ks, "levelpoints") || !strcmp(ks, "tonextlevel") || !strcmp(ks, "level") || !strcmp(ks, "specpoints") || !strcmp(ks, "spice") ||
            !strcmp(ks, "rank") || !strcmp(ks, "rank_short") || !strcmp(ks, "unlocks") || !strncmp(ks, "up_", 3)) { dict_put(ud, k, v); n++; } }
    hle_log("[profile] restored %d saved progress fields\n", n);
}
void offline_post_configs(void) {
    static int done; if (done) return;
    GCls *gc = find_gcls_name("TDSession"); if (!gc) return;
    gptr pl = send_any(gc->addr, "player", 0, NULL); gptr ud = pl ? send_any(pl, "userData", 0, NULL) : 0; if (!ud) return;
    gptr cfgs = dict_lookup(ud, new_string("configs")); if (!cfgs || rd32(cfgs + 4) == 0) return;
    done = 1; offline_restore_progress(ud); post_notification("TDPlayerReceivedConfigs", 0, cfgs);
}
static void post_notification(const char *name, gptr object, gptr userinfo) {
    gptr n = 0; hle_log("[notif] post %s\n", name);
    for (int i = 0; i < nnobs; i++) if (nobs[i].obs && (!nobs[i].name[0] || !strcmp(nobs[i].name, name)) && (!nobs[i].object || nobs[i].object == object)) {
        if (!n) n = new_notification(name, object, userinfo); u32 a[1] = { n };
        if (guest_responds(nobs[i].obs, nobs[i].sel)) send_guest(nobs[i].obs, nobs[i].sel, 1, a);
    }
}
/* ---- deferred host actions (run from the main loop) ---- */
typedef struct { double due; int kind; gptr a, b; } Deferred; static Deferred defers[64]; static int ndefer;
void sk_deliver(gptr req);
static gptr new_data(const void *p, u32 n); static const char *url_path(gptr u);
static void host_defer(int kind, gptr a, gptr b, double delay) { int fr = -1; for (int i = 0; i < ndefer; i++) if (!defers[i].kind) { fr = i; break; } if (fr >= 0) { defers[fr].due = now_s() + delay; defers[fr].kind = kind; defers[fr].a = a; defers[fr].b = b; return; } if (ndefer < 64) { defers[ndefer].due = now_s() + delay; defers[ndefer].kind = kind; defers[ndefer].a = a; defers[ndefer].b = b; ndefer++; } }
void host_run_deferred(void) {
    double t = now_s();
    for (int i = 0; i < ndefer; i++) if (defers[i].kind && defers[i].due <= t) { Deferred d = defers[i]; defers[i].kind = 0;
        if (d.kind == 1) post_notification("MPMoviePlayerPlaybackDidFinishNotification", d.a, 0);
        else if (d.kind == 3) { gptr del = rd32(d.a + 4); gptr req = rd32(d.a + 8), url = req ? rd32(req + 4) : 0; const char *us = url ? url_path(url) : "";
            /* offline: serve server-hosted assets (avatar parts, banners) from the app bundle by file name */
            const char *base = strrchr(us, '/'); base = base ? base + 1 : us; char q[256]; snprintf(q, sizeof q, "%s", base); char *qm = strchr(q, '?'); if (qm) *qm = 0;
            char path[1100] = ""; static const char *dirs[] = { "", "avatars/", "dress/", "images/", "maps/", NULL };
            for (int k = 0; dirs[k] && q[0]; k++) { snprintf(path, sizeof path, "%s/%s%s", g_bundle_dir, dirs[k], q); if (access(path, R_OK) == 0) break; path[0] = 0; }
            hle_log("[net] %s -> %s\n", us, path[0] ? path : "(no local file)");
            if (path[0] && del) { FILE *f = fopen(path, "rb"); if (f) { fseek(f, 0, SEEK_END); long n = ftell(f); fseek(f, 0, SEEK_SET); unsigned char *buf = malloc(n + 1); size_t rn = fread(buf, 1, n, f); fclose(f); gptr dat = new_data(buf, (u32)rn); free(buf);
                if (guest_responds(del, "connection:didReceiveData:")) { u32 a[2] = { d.a, dat }; send_guest(del, "connection:didReceiveData:", 2, a); }
                if (guest_responds(del, "connectionDidFinishLoading:")) { u32 a[1] = { d.a }; send_guest(del, "connectionDidFinishLoading:", 1, a); } continue; } }
            if (del && guest_responds(del, "connection:didFailWithError:")) { u32 a[2] = { d.a, new_error("NSURLErrorDomain", -1009) }; send_guest(del, "connection:didFailWithError:", 2, a); } }
        else if (d.kind == 4) { if (g_spice_view_loaded) sk_deliver(d.a); else host_defer(4, d.a, 0, 0.3); }
        else if (d.kind == 2) { post_notification("MPMoviePlayerLoadStateDidChangeNotification", d.a, 0); post_notification("MPMediaPlaybackIsPreparedToPlayDidChangeNotification", d.a, 0); } }
    int w = 0; for (int i = 0; i < ndefer; i++) if (defers[i].kind) defers[w++] = defers[i]; ndefer = w;
}
static gptr modal_stack[16], modal_parent[16]; static int nmodal;
static int g_orient = 4;     /* UIInterfaceOrientationLandscapeRight */

static int uikit_msg(const char *cls, gptr recv, const char *sel, int stret, int is_cls) {
    gptr self = recv;
    /* ---------------- class-side helpers ---------------- */
    if (is_cls) {
        if (!strcmp(cls, "UIColor")) {
            static const struct { const char *n; float c[4]; } named[] = {{"whiteColor", {1, 1, 1, 1}}, {"blackColor", {0, 0, 0, 1}}, {"clearColor", {0, 0, 0, 0}}, {"redColor", {1, 0, 0, 1}}, {"greenColor", {0, 1, 0, 1}}, {"blueColor", {0, 0, 1, 1}},
                {"grayColor", {.5f, .5f, .5f, 1}}, {"yellowColor", {1, 1, 0, 1}}, {"orangeColor", {1, .5f, 0, 1}}, {"lightGrayColor", {.667f, .667f, .667f, 1}}, {"darkGrayColor", {.333f, .333f, .333f, 1}}, {"brownColor", {.6f, .4f, .2f, 1}}, {"purpleColor", {.5f, 0, .5f, 1}}, {"cyanColor", {0, 1, 1, 1}}, {"magentaColor", {1, 0, 1, 1}}};
            for (unsigned i = 0; i < sizeof named / sizeof *named; i++) if (UIS(named[i].n)) { ret(color_obj(named[i].c[0], named[i].c[1], named[i].c[2], named[i].c[3])); return 1; }
            if (UIS("colorWithRed:green:blue:alpha:")) { ret(color_obj(uf(2, 0), uf(3, 0), f_of(arg(4)), f_of(arg(5)))); return 1; }
            if (UIS("colorWithWhite:alpha:")) { float w = uf(2, 0); ret(color_obj(w, w, w, uf(3, 0))); return 1; }
            if (UIS("colorWithCGColor:") || UIS("colorWithPatternImage:")) { ret(UA(2) ? UA(2) : color_obj(0, 0, 0, 1)); return 1; }
        }
        if (!strcmp(cls, "UIFont")) {
            if (UIS("fontWithName:size:")) { ret(font_obj(scstr(UA(2)), uf(3, 0))); return 1; }
            if (UIS("systemFontOfSize:")) { ret(font_obj("Helvetica", uf(2, 0))); return 1; } if (UIS("boldSystemFontOfSize:")) { ret(font_obj("Helvetica-Bold", uf(2, 0))); return 1; }
            if (UIS("italicSystemFontOfSize:")) { ret(font_obj("Helvetica-Oblique", uf(2, 0))); return 1; }
        }
        if (!strcmp(cls, "UIImage")) {
            if (UIS("imageNamed:")) { ret(image_named(scstr(UA(2)))); return 1; }
            if (UIS("imageWithCGImage:") || UIS("imageWithCGImage:scale:orientation:")) { ret(arg(2)); return 1; }
            if (UIS("imageWithContentsOfFile:")) { ret(image_from_path(host_path(scstr(UA(2))))); return 1; }
        }
        if (!strcmp(cls, "UIView")) {
            if (UIS("animateWithDuration:animations:") || UIS("animateWithDuration:animations:completion:")) { u64 db = arg64(2); double d; memcpy(&d, &db, 8); anim_block(arg(4), d, 0, 0, UIS("animateWithDuration:animations:") ? 0 : arg(5)); ret(0); return 1; }
            if (UIS("animateWithDuration:delay:options:animations:completion:")) { u64 db = arg64(2), dl = arg64(4); double d, dly; memcpy(&d, &db, 8); memcpy(&dly, &dl, 8); anim_block(arg(7), d, dly, (int)arg(6), arg(8)); ret(0); return 1; }
            if (UIS("transitionWithView:duration:options:animations:completion:")) { gptr anim = arg(6), comp = arg(7); run_block(anim, 0, 0); if (comp) run_block(comp, 1, 1); ret(0); return 1; }
            if (UIS("beginAnimations:context:")) { scope_begin(0.2, 0, 1, 0, 0, 0); sc.name = arg(2); sc.ctx = arg(3); sc.dlg = 0; sc.stop[0] = 0; ret(0); return 1; }
            if (UIS("commitAnimations")) { scope_end(); ret(0); return 1; }
            if (UIS("setAnimationDuration:")) { u64 db = arg64(2); memcpy(&sc.dur, &db, 8); ret(0); return 1; }
            if (UIS("setAnimationDelay:")) { u64 db = arg64(2); memcpy(&sc.delay, &db, 8); ret(0); return 1; }
            if (UIS("setAnimationRepeatCount:")) { sc.rep = f_of(arg(2)); ret(0); return 1; }
            if (UIS("setAnimationRepeatAutoreverses:")) { sc.autorev = arg(2) & 1; ret(0); return 1; }
            if (UIS("setAnimationCurve:")) { sc.curve = (int)arg(2); ret(0); return 1; }
            if (UIS("setAnimationDelegate:")) { sc.dlg = arg(2); ret(0); return 1; }
            if (UIS("setAnimationDidStopSelector:")) { strncpy(sc.stop, gstr(arg(2)), 63); ret(0); return 1; }
            if (UIS("setAnimationBeginsFromCurrentState:") || UIS("setAnimationTransition:forView:cache:") || UIS("setAnimationWillStartSelector:") || UIS("setAnimationsEnabled:")) { ret(0); return 1; }
        }
        if (!strcmp(cls, "UIButton") && UIS("buttonWithType:")) { gptr o = new_obj(objc_class_object("UIButton")); View *v = view_of(o); v->kind = K_BUTTON; v->btc[0][0] = v->btc[0][1] = v->btc[0][2] = v->btc[0][3] = 1; v_set_frame(v, 0, 0, 100, 40); ret(o); return 1; }
        if (!strcmp(cls, "UIApplication") && UIS("sharedApplication")) return 0;
    }
    /* ---------------- UIImage ---------------- */
    if (!strcmp(cls, "UIImage")) {
        ImageRec *im = img_of(recv);
        if (UIS("size")) { ret_size(im ? im->w : 0, im ? im->h : 0); return 1; }
        if (UIS("scale")) { ret(w_of(1.f)); return 1; } if (UIS("CGImage") || UIS("stretchableImageWithLeftCapWidth:topCapHeight:") || UIS("resizableImageWithCapInsets:")) { ret(recv); return 1; }
        if (UIS("initWithContentsOfFile:") || UIS("initWithData:")) { ret(UIS("initWithData:") ? 0 : image_from_path(host_path(scstr(UA(2))))); return 1; }
        if (UIS("leftCapWidth") || UIS("topCapHeight")) { ret(0); return 1; }
        return 0;
    }
    if (!strcmp(cls, "UIColor")) { if (UIS("CGColor")) { ret(recv); return 1; } if (UIS("colorWithAlphaComponent:")) { float c[4]; color_get(recv, c); ret(color_obj(c[0], c[1], c[2], uf(2, 0))); return 1; } if (UIS("set") || UIS("setFill") || UIS("setStroke")) { ret(0); return 1; } return 0; }
    if (!strcmp(cls, "UIFont")) { if (UIS("pointSize")) { ret(rd32(recv + 8)); return 1; } if (UIS("fontName") || UIS("familyName")) { ret(new_string(gstr(rd32(recv + 4)))); return 1; } if (UIS("fontWithSize:")) { ret(font_obj(gstr(rd32(recv + 4)), uf(2, 0))); return 1; } return 0; }
    /* ---------------- NSCoder over a nib ---------------- */
    if (!strcmp(cls, "NSCoder")) {
        Nib *nb = nib_by_id(rd32(recv + 4)); int idx = rd32(recv + 8); if (!nb) return 0; const char *k = scstr(UA(2));
        if (UIS("containsValueForKey:")) { ret(nv(nb, idx, k) != NULL); return 1; }
        if (UIS("decodeObjectForKey:")) { int r = nv_ref(nb, idx, k); ret(r >= 0 ? nib_make(nb, r) : 0); return 1; }
        if (UIS("decodeIntForKey:") || UIS("decodeInt32ForKey:") || UIS("decodeBoolForKey:") || UIS("decodeIntegerForKey:")) { ret(nv_int(nb, idx, k, 0)); return 1; }
        if (UIS("decodeFloatForKey:")) { ret(w_of(nv_float(nb, idx, k, 0))); return 1; }
        return 0;
    }
    /* ---------------- gesture recognizers ---------------- */
    if (strstr(cls, "GestureRecognizer")) {
        if (UIS("initWithTarget:action:")) { wr32(recv + 4, UA(2)); wr32(recv + 8, UA(3)); ret(recv); return 1; }
        ret(0); return 1;
    }
    /* ---------------- UIViewController ---------------- */
    if (!strcmp(cls, "UIViewController") || !strcmp(cls, "UINavigationController")) {
        VcState *vs = vc_of(recv);
        if (UIS("initWithNibName:bundle:")) { if (vs && UA(2)) strncpy(vs->nib, scstr(UA(2)), 95); ret(recv); return 1; }
        if (UIS("setView:")) { if (vs) { vs->view = UA(2); vs->loaded = 1; } ret(0); return 1; }
        if (UIS("loadView")) { if (vs && !vs->loaded) { char nn[96]; if (vs->nib[0]) strcpy(nn, vs->nib); else { strcpy(nn, v_clsname(recv)); } vs->loaded = 1; nib_load(nn, recv, NULL); if (!vs->view) { gptr o = new_obj(objc_class_object("UIView")); View *v = view_of(o); v_set_frame(v, 0, 0, g_scr_w, g_scr_h); vs->view = o; } } ret(0); return 1; }
        if (UIS("view")) {
            if (vs && !vs->view && !vs->loading) {
                vs->loading = 1;
                if (guest_responds(recv, "loadView")) send_guest(recv, "loadView", 0, NULL);
                if (!vs->view && !vs->loaded) { char nn[96]; strcpy(nn, vs->nib[0] ? vs->nib : v_clsname(recv)); vs->loaded = 1; nib_load(nn, recv, NULL); }
                if (!vs->view) { gptr o = new_obj(objc_class_object("UIView")); View *v = view_of(o); v_set_frame(v, 0, 0, g_scr_w, g_scr_h); vs->view = o; }
                vs->loading = 0;
                if (guest_responds(recv, "viewDidLoad")) send_guest(recv, "viewDidLoad", 0, NULL);
                /* Career: the fitting-room Apply/Cancel buttons start hidden on device (shown by openFittingRoom:); the nib has them visible */
                if (!strcmp(v_clsname(recv), "CareerViewController")) { static const char *g[] = { "avatarApplyButton", "avatarCancelButton" }; for (int k = 0; k < 2; k++) if (guest_responds(recv, g[k])) { gptr b = send_guest(recv, g[k], 0, NULL); View *bv = b ? view_of(b) : NULL; if (bv) bv->hidden = 1; } }
                /* the interface is already landscape: tell the controller so it lays out its views accordingly (UIKit does this when the view first appears) */
                if (guest_responds(recv, "willRotateToInterfaceOrientation:duration:")) { u32 b[3] = { 4, 0, 0 }; send_guest(recv, "willRotateToInterfaceOrientation:duration:", 3, b); }
                if (guest_responds(recv, "didRotateFromInterfaceOrientation:")) { u32 b[1] = { 1 }; send_guest(recv, "didRotateFromInterfaceOrientation:", 1, b); }
            }
            ret(vs ? vs->view : 0); return 1; }
        if (UIS("isViewLoaded")) { ret(vs && vs->view); return 1; }
        if (UIS("presentModalViewController:animated:") || UIS("presentViewController:animated:completion:")) { gptr c = UA(2); VcState *cs = vc_of(c); if (cs) { gptr vw = 0; { u32 a[0]; (void)a; } Target t = lookup(c, "view", 0, 0); (void)t; vw = send_any(c, "view", 0, NULL); if (!vw) vw = cs->view; View *cv = view_of(vw), *wv = g_window; if (cv && wv) { v_set_frame(cv, 0, 0, g_scr_w, g_scr_h); view_insert(wv, cv, -1); if (nmodal < 16) { modal_stack[nmodal] = c; modal_parent[nmodal] = recv; nmodal++; } hle_log("[ui] present modal %s\n", v_clsname(c)); } if (guest_responds(c, "willRotateToInterfaceOrientation:duration:")) { u32 b[3] = { 4, 0, 0 }; send_guest(c, "willRotateToInterfaceOrientation:duration:", 3, b); } if (guest_responds(c, "didRotateFromInterfaceOrientation:")) { u32 b[1] = { 1 }; send_guest(c, "didRotateFromInterfaceOrientation:", 1, b); } if (guest_responds(c, "viewWillAppear:")) { u32 b[1] = { 0 }; send_guest(c, "viewWillAppear:", 1, b); } } ret(0); return 1; }
        if (UIS("dismissModalViewControllerAnimated:") || UIS("dismissViewControllerAnimated:completion:")) {
            int k = -1; for (int i = nmodal - 1; i >= 0; i--) if (modal_stack[i] == recv || modal_parent[i] == recv) { k = i; break; } if (k < 0 && nmodal) k = nmodal - 1;
            if (k >= 0) { gptr c = modal_stack[k]; VcState *cs = vc_of(c); if (guest_responds(c, "viewWillDisappear:")) { u32 b[1] = { 0 }; send_guest(c, "viewWillDisappear:", 1, b); } View *cv = cs ? view_of(cs->view) : NULL; if (cv) view_remove(cv); if (guest_responds(c, "viewDidDisappear:")) { u32 b[1] = { 0 }; send_guest(c, "viewDidDisappear:", 1, b); }
                gptr par = modal_parent[k]; for (int i = k; i < nmodal - 1; i++) { modal_stack[i] = modal_stack[i + 1]; modal_parent[i] = modal_parent[i + 1]; } nmodal--; if (par && guest_responds(par, "viewWillAppear:")) { u32 b[1] = { 0 }; send_guest(par, "viewWillAppear:", 1, b); } hle_log("[ui] dismissed modal %s\n", v_clsname(c)); }
            ret(0); return 1; }
        if (UIS("interfaceOrientation")) { ret(g_orient); return 1; } if (UIS("supportedInterfaceOrientations")) { ret(0x18); return 1; }
        if (UIS("title") || UIS("navigationController") || UIS("parentViewController") || UIS("modalViewController")) { ret(0); return 1; }
        if (UIS("viewWillAppear:") || UIS("viewDidAppear:") || UIS("viewWillDisappear:") || UIS("viewDidDisappear:") || UIS("viewDidLoad") || UIS("setTitle:") || UIS("didReceiveMemoryWarning")) { ret(0); return 1; }
        return 0;
    }
    /* ---------------- UIApplication / UIDevice / UIScreen extras ---------------- */
    if (!strcmp(cls, "UIApplication")) {
        if (UIS("statusBarOrientation")) { ret(g_orient); return 1; } if (UIS("keyWindow")) { ret(g_window ? g_window->obj : 0); return 1; } if (UIS("windows")) { gptr a = new_array(); if (g_window) array_add(a, g_window->obj); ret(a); return 1; }
        if (UIS("applicationState")) { ret(0); return 1; } if (UIS("beginBackgroundTaskWithExpirationHandler:")) { ret(1); return 1; } if (UIS("canOpenURL:") || UIS("openURL:")) { ret(0); return 1; }
        if (UIS("isIdleTimerDisabled") || UIS("isStatusBarHidden")) { ret(0); return 1; }
        if (UIS("setStatusBarOrientation:") || UIS("setStatusBarOrientation:animated:")) { ret(0); return 1; }
        return 0;
    }
    if (!strcmp(cls, "UIDevice")) {
        if (UIS("orientation")) { ret(g_orient); return 1; } if (UIS("userInterfaceIdiom")) { ret(1); return 1; }   /* UIUserInterfaceIdiomPad */
        if (UIS("beginGeneratingDeviceOrientationNotifications") || UIS("endGeneratingDeviceOrientationNotifications")) { ret(0); return 1; }
        if (UIS("platform")) { ret(new_string("iPad2,1")); return 1; } if (UIS("isMultitaskingSupported")) { ret(1); return 1; } if (UIS("batteryState")) { ret(0); return 1; }
        return 0;
    }
    if (!strcmp(cls, "NSNotificationCenter")) {
        if (!cls_Notification) cls_Notification = objc_class_object("NSNotification");
        if (UIS("addObserver:selector:name:object:")) { if (nnobs < 512 && UA(2)) { NObs *o = &nobs[nnobs++]; o->obs = UA(2); strncpy(o->sel, gstr(UA(3)), 63); strncpy(o->name, (UA(4) && *scstr(UA(4))) ? scstr(UA(4)) : "__unbound__", 95); o->object = UA(5); hle_log("[notif] observe %s -> %s %s\n", o->name, v_clsname(o->obs), o->sel); } ret(0); return 1; }
        if (UIS("removeObserver:") || UIS("removeObserver:name:object:")) { for (int i = 0; i < nnobs; i++) if (nobs[i].obs == UA(2)) nobs[i].obs = 0; ret(0); return 1; }
        if (UIS("postNotificationName:object:")) { post_notification(scstr(UA(2)), UA(3), 0); ret(0); return 1; }
        if (UIS("postNotificationName:object:userInfo:")) { post_notification(scstr(UA(2)), UA(3), UA(4)); ret(0); return 1; }
        if (UIS("postNotification:")) { gptr n = UA(2); if (n) post_notification(scstr(rd32(n + 4)), rd32(n + 8), rd32(n + 12)); ret(0); return 1; }
        return 0;
    }
    if (!strcmp(cls, "NSNotification")) {
        if (is_cls && UIS("notificationWithName:object:userInfo:")) { ret(new_notification(scstr(UA(2)), UA(3), UA(4))); return 1; }
        if (is_cls && UIS("notificationWithName:object:")) { ret(new_notification(scstr(UA(2)), UA(3), 0)); return 1; }
        if (UIS("name")) { ret(rd32(recv + 4)); return 1; } if (UIS("object")) { ret(rd32(recv + 8)); return 1; } if (UIS("userInfo")) { ret(rd32(recv + 12)); return 1; }
        return 0;
    }
    if (!strcmp(cls, "MPMoviePlayerController") || !strcmp(cls, "MPMoviePlayerViewController")) {
        if (UIS("initWithContentURL:")) { wr32(recv + 4, UA(2)); ret(recv); return 1; }
        if (UIS("play")) { g_movie_start = anim_now(); host_defer(1, recv, 0, 0.05); ret(0); return 1; }
        if (UIS("prepareToPlay")) { host_defer(2, recv, 0, 0.01); ret(0); return 1; }
        if (UIS("loadState")) { ret(3); return 1; } if (UIS("isPreparedToPlay")) { ret(1); return 1; } if (UIS("playbackState")) { ret(0); return 1; }
        if (UIS("view") || UIS("backgroundView")) { gptr slot = UIS("view") ? recv + 8 : recv + 12; if (!rd32(slot)) { gptr o = new_obj(objc_class_object("UIView")); View *vv = view_of(o); v_set_frame(vv, 0, 0, g_scr_w, g_scr_h); vv->hasbg = 1; vv->bg[3] = 1; wr32(slot, o); } ret(rd32(slot)); return 1; }
        if (UIS("moviePlayer")) { ret(recv); return 1; }
        if (UIS("stop") || UIS("pause") || UIS("setControlStyle:") || UIS("setScalingMode:") || UIS("setShouldAutoplay:") || UIS("setFullscreen:animated:") || UIS("setMovieSourceType:") || UIS("setUseApplicationAudioSession:") || UIS("setAllowsAirPlay:")) { ret(0); return 1; }
        if (UIS("currentPlaybackTime") || UIS("duration") || UIS("endPlaybackTime")) { double t = UIS("currentPlaybackTime") ? (g_movie_start > 0 ? anim_now() - g_movie_start : 0) : (UIS("duration") ? 5.0 : 0); if (t > 600) t = 600; u64 z; memcpy(&z, &t, 8); ret64(z); return 1; }
        return 0;
    }
    if (!strcmp(cls, "MPMusicPlayerController")) { if (is_cls && (UIS("iPodMusicPlayer") || UIS("applicationMusicPlayer"))) { ret(new_obj(objc_class_object("MPMusicPlayerController"))); return 1; } ret(0); return 1; }
    if (!strcmp(cls, "CADisplayLink")) {
        if (is_cls && UIS("displayLinkWithTarget:selector:")) { hle_log("[displaylink] create\n"); gptr o = new_obj(objc_class_object("CADisplayLink")); wr32(o + 4, UA(2)); wr32(o + 8, UA(3)); wr32(o + 12, 0); ret(o); return 1; }
        if (UIS("addToRunLoop:forMode:")) { hle_log("[displaylink] add target=%s sel=%s\n", objc_describe(rd32(recv + 4)), gstr(rd32(recv + 8))); if (!rd32(recv + 12)) { int t = timer_add(rd32(recv + 4), gstr(rd32(recv + 8)), 1.0 / 60, 1, recv); wr32(recv + 12, t + 1); } ret(0); return 1; }
        if (UIS("setFrameInterval:")) { u32 t = rd32(recv + 12); if (t && UA(2)) timers[t - 1].interval = UA(2) / 60.0; ret(0); return 1; }
        if (UIS("invalidate")) { u32 t = rd32(recv + 12); if (t) timers[t - 1].valid = 0; wr32(recv + 12, 0); ret(0); return 1; }
        if (UIS("timestamp")) { ret_double(now_s()); return 1; } if (UIS("duration")) { ret_double(1.0 / 60); return 1; }
        return 0;
    }
    if (!strcmp(cls, "NSBundle") && UIS("loadNibNamed:owner:options:")) { ret(nib_load(scstr(UA(2)), UA(3), NULL)); return 1; }
    if (!strcmp(cls, "CAEAGLLayer") || !strcmp(cls, "CALayer")) { ret(0); return 1; }
    /* ---------------- UIView family (recv is an instance) ---------------- */
    if (!(strstr(cls, "UI") == cls && (strstr(cls, "View") || strstr(cls, "Window") || strstr(cls, "Label") || strstr(cls, "Button") || strstr(cls, "Control") || strstr(cls, "Responder") || strstr(cls, "Field") || strstr(cls, "Indicator")))) return 0;
    View *v = view_of(recv); if (!v) return 0;
    if (UIS("initWithCoder:")) { Nib *nb = nib_by_id(rd32(UA(2) + 4)); if (nb && rd32(UA(2)) == cls_NSCoder) nib_apply_view(nb, rd32(UA(2) + 8), recv); ret(self); return 1; }
    if (UIS("init") || UIS("initWithActivityIndicatorStyle:")) { ret(self); return 1; }
    if (UIS("initWithFrame:")) { v_set_frame(v, uf(2, 0), uf(3, 0), f_of(arg(4)), f_of(arg(5))); ret(self); return 1; }
    if (UIS("initWithImage:")) { v->image = UA(2); float w, h; image_size(v->image, &w, &h); v_set_frame(v, 0, 0, w, h); v->kind = K_IMAGEVIEW; ret(self); return 1; }
    if (UIS("frame")) { float x, y, w, h; v_frame(v, &x, &y, &w, &h); gptr p = arg(0); wr32(p, w_of(x)); wr32(p + 4, w_of(y)); wr32(p + 8, w_of(w)); wr32(p + 12, w_of(h)); return 1; }
    if (UIS("bounds")) { gptr p = arg(0); wr32(p, w_of(v->bx)); wr32(p + 4, w_of(v->by)); wr32(p + 8, w_of(v->bw)); wr32(p + 12, w_of(v->bh)); return 1; }
    if (UIS("center")) { ret_point(v->cx, v->cy); return 1; }
    if (UIS("setFrame:")) { v_set_frame(v, uf(2, 0), uf(3, 0), f_of(arg(4)), f_of(arg(5))); ret(0); return 1; }
    if (UIS("setBounds:")) { v->bx = uf(2, 0); v->by = uf(3, 0); float cx = v->cx, cy = v->cy; v_set_frame(v, cx - f_of(arg(4)) / 2, cy - f_of(arg(5)) / 2, f_of(arg(4)), f_of(arg(5))); ret(0); return 1; }
    if (UIS("setCenter:")) { v->cx = uf(2, 0); v->cy = uf(3, 0); ret(0); return 1; }
    if (UIS("setAlpha:")) { v->alpha = uf(2, 0); ret(0); return 1; } if (UIS("alpha")) { ret(w_of(v->alpha)); return 1; }
    if (UIS("setHidden:")) { v->hidden = UA(2) & 1; if (getenv("RA_HIDELOG")) hle_log("[hidden] %s = %d (lr=%08x)\n", v_clsname(v->obj), v->hidden, reg(14)); ret(0); return 1; } if (UIS("isHidden")) { ret(v->hidden); return 1; }
    if (UIS("setTag:")) { v->tag = UA(2); ret(0); return 1; } if (UIS("tag")) { ret(v->tag); return 1; }
    if (UIS("viewWithTag:")) { View *r = find_tag(v, UA(2)); ret(r ? r->obj : 0); return 1; }
    if (UIS("addSubview:")) { View *k = view_of(UA(2)); if (k) view_insert(v, k, -1); ret(0); return 1; }
    if (UIS("insertSubview:atIndex:")) { View *k = view_of(UA(2)); if (k) view_insert(v, k, UA(3)); ret(0); return 1; }
    if (UIS("insertSubview:aboveSubview:") || UIS("insertSubview:belowSubview:")) { View *k = view_of(UA(2)), *s = view_of(UA(3)); if (k && s) { int at = 0; for (int i = 0; i < v->nk; i++) if (v->kids[i] == s) at = i; view_insert(v, k, UIS("insertSubview:aboveSubview:") ? at + 1 : at); } ret(0); return 1; }
    if (UIS("removeFromSuperview")) { view_remove(v); ret(0); return 1; }
    if (UIS("superview")) { ret(v->parent ? v->parent->obj : 0); return 1; }
    if (UIS("subviews")) { ret(ui_view_array(v)); return 1; }
    if (UIS("window")) { ret(g_window ? g_window->obj : 0); return 1; }
    if (UIS("bringSubviewToFront:")) { View *k = view_of(UA(2)); if (k && k->parent == v) view_insert(v, k, -1); ret(0); return 1; }
    if (UIS("sendSubviewToBack:")) { View *k = view_of(UA(2)); if (k && k->parent == v) view_insert(v, k, 0); ret(0); return 1; }
    if (UIS("setBackgroundColor:")) { if (UA(2)) { color_get(UA(2), v->bg); v->hasbg = 1; } else v->hasbg = 0; ret(0); return 1; }
    if (UIS("backgroundColor")) { ret(v->hasbg ? color_obj(v->bg[0], v->bg[1], v->bg[2], v->bg[3]) : 0); return 1; }
    if (UIS("setClipsToBounds:")) { v->clips = UA(2) & 1; ret(0); return 1; }
    if (UIS("setUserInteractionEnabled:")) { v->interact = UA(2) & 1; ret(0); return 1; } if (UIS("isUserInteractionEnabled")) { ret(v->interact); return 1; }
    if (UIS("setTransform:")) { v->ta = uf(2, 0); v->tb = f_of(arg(4)); v->tc_ = f_of(arg(5)); v->td = f_of(arg(6)); v->ttx = f_of(arg(7)); v->tty = f_of(arg(8)); ret(0); return 1; }
    if (UIS("transform")) { gptr p = arg(0); float t[6] = {v->ta, v->tb, v->tc_, v->td, v->ttx, v->tty}; for (int i = 0; i < 6; i++) wr32(p + 4 * i, w_of(t[i])); return 1; }
    if (UIS("layer")) { ret(v->is_gl || !strcmp(v_clsname(recv), "EAGLView") ? ({ v->is_gl = 1; s_layer_for(recv); }) : s_layer_for(recv)); return 1; }
    if (UIS("setAutoresizingMask:")) { v->mask = UA(2); ret(0); return 1; } if (UIS("autoresizingMask")) { ret(v->mask); return 1; }
    if (UIS("setNeedsLayout")) { v->dirty = 1; ret(0); return 1; }
    if (UIS("layoutIfNeeded") || UIS("layoutSubviews")) { if (v->dirty && guest_responds(recv, "layoutSubviews") && UIS("layoutIfNeeded")) { v->dirty = 0; send_guest(recv, "layoutSubviews", 0, NULL); } ret(0); return 1; }
    if (UIS("setNeedsDisplay") || UIS("setNeedsLayout") || UIS("layoutIfNeeded") || UIS("setNeedsDisplayInRect:") || UIS("sizeToFit") || UIS("setContentMode:") || UIS("setOpaque:") || UIS("setMultipleTouchEnabled:") || UIS("setExclusiveTouch:") ||
        UIS("setAutoresizesSubviews:") || UIS("setContentScaleFactor:") || UIS("setClearsContextBeforeDrawing:") || UIS("setNeedsUpdateConstraints") || UIS("setTintColor:")) { if (UIS("setContentMode:")) v->enabled_set = 0; ret(0); return 1; }
    if (UIS("addGestureRecognizer:")) { gptr gr = UA(2); if (gr && v->ngr < 4) { v->gr_target[v->ngr] = rd32(gr + 4); strncpy(v->gr_sel[v->ngr], gstr(rd32(gr + 8)), 63); v->ngr++; } ret(0); return 1; }
    if (UIS("isDescendantOfView:")) { View *t = view_of(UA(2)); for (View *p = v; p; p = p->parent) if (p == t) { ret(1); return 1; } ret(0); return 1; }
    if (UIS("convertPoint:toView:") || UIS("convertPoint:fromView:")) { ret_point(uf(2, 1), uf(3, 1)); return 1; }
    if (UIS("setDelegate:")) { v->delegate = UA(2); ret(0); return 1; } if (UIS("delegate")) { ret(v->delegate); return 1; }
    /* image view */
    if (UIS("setImage:")) { v->image = UA(2); ret(0); return 1; } if (UIS("image")) { ret(v->image); return 1; } if (UIS("setHighlightedImage:")) { v->himage = UA(2); ret(0); return 1; }
    if (UIS("startAnimating") || UIS("stopAnimating") || UIS("setAnimationImages:") || UIS("setAnimationDuration:") || UIS("setAnimationRepeatCount:") || UIS("setHidesWhenStopped:") || UIS("setActivityIndicatorViewStyle:")) { ret(0); return 1; } if (UIS("isAnimating")) { ret(0); return 1; }
    /* label */
    if (UIS("setText:")) { free(v->text); v->text = UA(2) ? strdup(scstr(UA(2))) : NULL; ret(0); return 1; } if (UIS("text")) { ret(v->text ? new_string(v->text) : 0); return 1; }
    if (UIS("setFont:")) { if (UA(2)) { strncpy(v->font, gstr(rd32(UA(2) + 4)), 63); v->fsize = f_of(rd32(UA(2) + 8)); } ret(0); return 1; } if (UIS("font")) { ret(font_obj(v->font, v->fsize)); return 1; }
    if (UIS("setTextColor:")) { color_get(UA(2), v->tcol); ret(0); return 1; } if (UIS("textColor")) { ret(color_obj(v->tcol[0], v->tcol[1], v->tcol[2], v->tcol[3])); return 1; }
    if (UIS("setTextAlignment:")) { v->align = UA(2); ret(0); return 1; } if (UIS("setNumberOfLines:")) { v->lines = UA(2); ret(0); return 1; }
    if (UIS("setShadowColor:") || UIS("setShadowOffset:") || UIS("setAdjustsFontSizeToFitWidth:") || UIS("setMinimumFontSize:") || UIS("setLineBreakMode:") || UIS("setBaselineAdjustment:") || UIS("setHighlightedTextColor:")) { ret(0); return 1; }
    /* button / control */
    if (UIS("setTitle:forState:")) { int s = UA(3) == 0 ? 0 : UA(3) == 1 ? 1 : UA(3) == 2 ? 2 : 3; free(v->btitle[s]); v->btitle[s] = UA(2) ? strdup(scstr(UA(2))) : NULL; v->kind = K_BUTTON; ret(0); return 1; }
    if (UIS("setImage:forState:")) { int s = UA(3) == 0 ? 0 : UA(3) == 1 ? 1 : UA(3) == 2 ? 2 : 3; v->bimg[s] = UA(2); ret(0); return 1; }
    if (UIS("setBackgroundImage:forState:")) { int s = UA(3) == 0 ? 0 : UA(3) == 1 ? 1 : UA(3) == 2 ? 2 : 3; v->bbg[s] = UA(2); ret(0); return 1; }
    if (UIS("setTitleColor:forState:")) { int s = UA(3) == 0 ? 0 : UA(3) == 1 ? 1 : UA(3) == 2 ? 2 : 3; color_get(UA(2), v->btc[s]); ret(0); return 1; }
    if (UIS("setTitleShadowColor:forState:") || UIS("setContentEdgeInsets:") || UIS("setTitleEdgeInsets:") || UIS("setShowsTouchWhenHighlighted:") || UIS("setAdjustsImageWhenHighlighted:") || UIS("setContentHorizontalAlignment:") || UIS("setReversesTitleShadowWhenHighlighted:")) { ret(0); return 1; }
    if (UIS("addTarget:action:forControlEvents:")) { if (v->nact < 8) { Action *a = &v->act[v->nact++]; a->target = UA(2); strncpy(a->sel, gstr(UA(3)), 63); a->ev = UA(4); } ret(0); return 1; }
    if (UIS("removeTarget:action:forControlEvents:")) { v->nact = 0; ret(0); return 1; }
    if (UIS("sendActionsForControlEvents:")) { fire_actions(v, UA(2)); ret(0); return 1; }
    if (UIS("setEnabled:")) { v->disabled = !(UA(2) & 1); ret(0); return 1; } if (UIS("isEnabled")) { ret(!v->disabled); return 1; }
    if (UIS("setSelected:")) { v->selected = UA(2) & 1; ret(0); return 1; } if (UIS("isSelected")) { ret(v->selected); return 1; }
    if (UIS("setHighlighted:")) { v->highlighted = UA(2) & 1; ret(0); return 1; } if (UIS("isHighlighted")) { ret(v->highlighted); return 1; }
    if (UIS("titleLabel") || UIS("imageView")) { ret(0); return 1; }
    /* scroll view */
    if (UIS("setContentSize:")) { v->cw = uf(2, 0); v->ch = uf(3, 0); ret(0); return 1; } if (UIS("contentSize")) { ret_size(v->cw, v->ch); return 1; }
    if (UIS("setContentOffset:") || UIS("setContentOffset:animated:")) { v->sx = uf(2, 0); v->sy = uf(3, 0); ret(0); return 1; } if (UIS("contentOffset")) { ret_point(v->sx, v->sy); return 1; }
    if (UIS("setScrollEnabled:") || UIS("setShowsHorizontalScrollIndicator:") || UIS("setShowsVerticalScrollIndicator:") || UIS("setBounces:") || UIS("setPagingEnabled:") || UIS("setDelaysContentTouches:") || UIS("setCanCancelContentTouches:")) { ret(0); return 1; }
    /* window */
    if (UIS("makeKeyAndVisible") || UIS("makeKeyWindow")) { g_window = v; v->kind = K_WINDOW; v_set_frame(v, 0, 0, g_scr_w, g_scr_h); ret(0); return 1; }
    if (UIS("setRootViewController:")) { gptr vc = UA(2); gptr vw = vc ? send_any(vc, "view", 0, NULL) : 0; View *rv = view_of(vw); if (rv) { v_set_frame(rv, 0, 0, g_scr_w, g_scr_h); view_insert(v, rv, -1); } if (vc && guest_responds(vc, "willRotateToInterfaceOrientation:duration:")) { u32 b[3] = { 4, 0, 0 }; send_guest(vc, "willRotateToInterfaceOrientation:duration:", 3, b); } if (vc && guest_responds(vc, "didRotateFromInterfaceOrientation:")) { u32 b[1] = { 1 }; send_guest(vc, "didRotateFromInterfaceOrientation:", 1, b); } if (vc && guest_responds(vc, "viewWillAppear:")) { u32 b[1] = { 0 }; send_guest(vc, "viewWillAppear:", 1, b); } if (vc && guest_responds(vc, "viewDidAppear:")) { u32 b[1] = { 0 }; send_guest(vc, "viewDidAppear:", 1, b); } ret(0); return 1; }
    if (UIS("setWindowLevel:") || UIS("setScreen:") || UIS("resignKeyWindow")) { ret(0); return 1; }
    return 0;
}
