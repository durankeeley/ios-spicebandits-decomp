/* foundation.inc - host-side Foundation: collections, data, strings, numbers, dates, URLs, files, errors, locale. Included into objc.c. */
#include <dirent.h>
static gptr cls_NSDate, cls_NSError, cls_NSURLc;
#define FS(s) (!strcmp(sel, s))
static double dbl_of64(u64 b) { double d; memcpy(&d, &b, 8); return d; }
static void ret_double(double d) { u64 b; memcpy(&b, &d, 8); ret64(b); }
static double num_double(gptr n) { if (!n || n >= ARENA_SIZE - 16 || rd32(n) != cls_NSNumber) return 0; u32 k = rd32(n + 4); if (k == 2) { double d; memcpy(&d, H(n + 8), 8); return d; } if (k == 1) return (double)*(u64 *)H(n + 8); return (double)(s32)rd32(n + 8); }
static gptr new_number_d(double d) { gptr o = new_obj(cls_NSNumber); wr32(o + 4, 2); memcpy(H(o + 8), &d, 8); return o; }
static int is_number(gptr o) { return o && o < ARENA_SIZE - 16 && rd32(o) == cls_NSNumber; }
static void arr_insert(gptr a, u32 at, gptr x) { array_add(a, 0); u32 n = rd32(a + 4); if (at > n - 1) at = n - 1; gptr items = rd32(a + 8); memmove(H(items + 4 * (at + 1)), H(items + 4 * at), 4 * (n - 1 - at)); wr32(items + 4 * at, x); }
static void arr_remove(gptr a, u32 at) { u32 n = rd32(a + 4); if (at >= n) return; gptr items = rd32(a + 8); memmove(H(items + 4 * at), H(items + 4 * (at + 1)), 4 * (n - at - 1)); wr32(a + 4, n - 1); }
static int objs_equal(gptr a, gptr b) { if (a == b) return 1; if (is_string(a) && is_string(b)) return !strcmp(scstr(a), scstr(b)); if (is_number(a) && is_number(b)) return num_double(a) == num_double(b); return 0; }
static gptr arr_copy(gptr src, gptr cls) { gptr a = new_obj(cls); wr32(a + 4, 0); wr32(a + 8, gm_malloc(256)); wr32(a + 12, 64); for (u32 i = 0; i < rd32(src + 4); i++) array_add(a, array_get(src, i)); return a; }
static void dict_put(gptr d, gptr k, gptr v) { for (u32 i = 0; i < rd32(d + 4); i++) { gptr kk = rd32(rd32(d + 8) + 4 * i); if (objs_equal(kk, k)) { wr32(rd32(d + 12) + 4 * i, v); return; } }
    u32 n = rd32(d + 4); u32 cap = 32; (void)cap; wr32(d + 8, gm_realloc(rd32(d + 8), 4 * (n + 1))); wr32(d + 12, gm_realloc(rd32(d + 12), 4 * (n + 1))); wr32(rd32(d + 8) + 4 * n, k); wr32(rd32(d + 12) + 4 * n, v); wr32(d + 4, n + 1); }
static gptr dict_lookup(gptr d, gptr k) { for (u32 i = 0; i < rd32(d + 4); i++) if (objs_equal(rd32(rd32(d + 8) + 4 * i), k)) return rd32(rd32(d + 12) + 4 * i); return 0; }
static void dict_del(gptr d, gptr k) { u32 n = rd32(d + 4); for (u32 i = 0; i < n; i++) if (objs_equal(rd32(rd32(d + 8) + 4 * i), k)) { gptr ks = rd32(d + 8), vs = rd32(d + 12); memmove(H(ks + 4 * i), H(ks + 4 * (i + 1)), 4 * (n - i - 1)); memmove(H(vs + 4 * i), H(vs + 4 * (i + 1)), 4 * (n - i - 1)); wr32(d + 4, n - 1); return; } }
static gptr new_data(const void *p, u32 n) { gptr o = new_obj(objc_class_object("NSMutableData")); gptr b = gm_malloc(n + 1); if (p) memcpy(H(b), p, n); wr32(o + 4, b); wr32(o + 8, n); wr32(o + 12, n + 1); return o; }
static void data_append(gptr d, const void *p, u32 n) { u32 len = rd32(d + 8), cap = rd32(d + 12); if (len + n + 1 > cap) { u32 nc = (len + n + 1) * 2; wr32(d + 4, gm_realloc(rd32(d + 4), nc)); wr32(d + 12, nc); } memcpy(H(rd32(d + 4) + len), p, n); wr32(d + 8, len + n); }
static gptr data_from_file(const char *path) { if (getenv("RA_IMGLOG")) fprintf(stderr, "[file] %s\n", path); FILE *f = fopen(host_path(path), "rb"); if (!f) return 0; fseek(f, 0, SEEK_END); long n = ftell(f); fseek(f, 0, SEEK_SET); gptr o = new_data(NULL, (u32)n); if (n && fread(H(rd32(o + 4)), 1, n, f) != (size_t)n) { fclose(f); return 0; } fclose(f); return o; }
static const char *url_path(gptr u) { return u && u < ARENA_SIZE - 16 ? gstr(rd32(u + 8)) : ""; }
static gptr new_url(const char *s, int isfile) { gptr o = new_obj(cls_NSURLc ? cls_NSURLc : (cls_NSURLc = objc_class_object("NSURL"))); wr32(o + 8, gm_strdup(s)); wr32(o + 12, isfile); return o; }
static gptr new_error(const char *domain, int code) { gptr o = new_obj(cls_NSError ? cls_NSError : (cls_NSError = objc_class_object("NSError"))); wr32(o + 4, new_string(domain)); wr32(o + 8, code); return o; }
static gptr new_date(double t) { gptr o = new_obj(cls_NSDate ? cls_NSDate : (cls_NSDate = objc_class_object("NSDate"))); memcpy(H(o + 8), &t, 8); return o; }
static double date_t(gptr o) { double d = 0; if (o && o < ARENA_SIZE - 16) memcpy(&d, H(o + 8), 8); return d; }
static double unix_now(void) { struct timespec ts; clock_gettime(CLOCK_REALTIME, &ts); return ts.tv_sec + ts.tv_nsec * 1e-9; }
static void ret_range(u32 loc, u32 len) { gptr p = arg(0); wr32(p, loc); wr32(p + 4, len); }
static int cmp_sel(gptr a, gptr b, const char *sel) { u32 x[1] = { b }; return (s32)send_any(a, sel, 1, x); }
static void sort_array(gptr a, const char *sel) { u32 n = rd32(a + 4); for (u32 i = 1; i < n; i++) { gptr x = array_get(a, i); u32 j = i; while (j > 0 && cmp_sel(array_get(a, j - 1), x, sel) > 0) { wr32(rd32(a + 8) + 4 * j, array_get(a, j - 1)); j--; } wr32(rd32(a + 8) + 4 * j, x); } }
static int is_cls_named(const char *cls, const char *a) { return strstr(cls, a) != NULL; }


/* ================= NSUserDefaults: real, persistent (<docs>/defaults.bin) ================= */
typedef struct { char key[128]; gptr val; } DefEnt;
static DefEnt g_defs[512]; static int g_ndefs, g_defs_loaded;
typedef struct { char *p; size_t n, cap; } SBuf;
static void sb_put(SBuf *b, const void *d, size_t n) { if (b->n + n + 1 > b->cap) { b->cap = (b->n + n + 1) * 2; b->p = realloc(b->p, b->cap); } memcpy(b->p + b->n, d, n); b->n += n; }
static void sb_fmt(SBuf *b, const char *f, ...) { char t[96]; va_list ap; va_start(ap, f); int n = vsnprintf(t, sizeof t, f, ap); va_end(ap); sb_put(b, t, n); }
static int defs_ser(SBuf *b, gptr o, int depth) {                /* tagged, length-prefixed text; returns 0 if the object can't be stored */
    if (!o || o >= ARENA_SIZE - 16 || depth > 12) { sb_put(b, "n;", 2); return 0; }
    if (is_string(o)) { const char *t = scstr(o); sb_fmt(b, "s%zu:", strlen(t)); sb_put(b, t, strlen(t)); return 1; }
    if (is_number(o)) { u32 k = rd32(o + 4); if (k == 0) sb_fmt(b, "i%d;", (s32)rd32(o + 8)); else if (k == 1) sb_fmt(b, "u%llu;", (unsigned long long)*(u64 *)H(o + 8)); else sb_fmt(b, "f%.17g;", num_double(o)); return 1; }
    gptr c = rd32(o); if (c == (cls_NSDate ? cls_NSDate : (cls_NSDate = objc_class_object("NSDate")))) { sb_fmt(b, "d%.17g;", date_t(o)); return 1; }
    if (c == cls_NSArray || c == cls_NSMutableArray) { u32 n = rd32(o + 4); sb_fmt(b, "a%u[", n); for (u32 i = 0; i < n; i++) defs_ser(b, array_get(o, i), depth + 1); sb_put(b, "]", 1); return 1; }
    if (c == cls_NSDictionary || c == objc_class_object("NSMutableDictionary")) { u32 n = rd32(o + 4); sb_fmt(b, "m%u{", n); for (u32 i = 0; i < n; i++) { defs_ser(b, rd32(rd32(o + 8) + 4 * i), depth + 1); defs_ser(b, rd32(rd32(o + 12) + 4 * i), depth + 1); } sb_put(b, "}", 1); return 1; }
    if (c == cls_NSData || c == objc_class_object("NSMutableData")) { u32 n = rd32(o + 8); sb_fmt(b, "x%u:", n); sb_put(b, H(rd32(o + 4)), n); return 1; }
    sb_put(b, "n;", 2); return 0;
}
static gptr defs_parse(const char **pp) {
    const char *p = *pp; gptr r = 0; char *e;
    switch (*p++) {
    case 's': { size_t n = strtoul(p, &e, 10); p = e + 1; char *t = strndup(p, n); r = new_string(t); free(t); p += n; break; }
    case 'i': r = new_number_i((s32)strtol(p, &e, 10)); p = e + 1; break;
    case 'u': r = new_number_u64(strtoull(p, &e, 10)); p = e + 1; break;
    case 'f': r = new_number_d(strtod(p, &e)); p = e + 1; break;
    case 'd': r = new_date(strtod(p, &e)); p = e + 1; break;
    case 'a': { u32 n = strtoul(p, &e, 10); p = e + 1; r = new_array(); for (u32 i = 0; i < n; i++) array_add(r, defs_parse(&p)); p++; break; }
    case 'm': { u32 n = strtoul(p, &e, 10); p = e + 1; r = new_dict(); wr32(r + 4, 0); for (u32 i = 0; i < n; i++) { gptr k = defs_parse(&p), v = defs_parse(&p); if (k && v) dict_put(r, k, v); } p++; break; }
    case 'x': { u32 n = strtoul(p, &e, 10); p = e + 1; r = new_data(p, n); p += n; break; }
    case 'n': p++; break;
    default: break; }
    *pp = p; return r;
}
static void defs_path(char *out, size_t n) { snprintf(out, n, "%s/defaults.bin", g_docs_dir); }
static void defs_save(void) {
    SBuf b = {0}; for (int i = 0; i < g_ndefs; i++) { if (!g_defs[i].val) continue; sb_fmt(&b, "s%zu:", strlen(g_defs[i].key)); sb_put(&b, g_defs[i].key, strlen(g_defs[i].key)); defs_ser(&b, g_defs[i].val, 0); }
    char path[4200], tmp[4210]; defs_path(path, sizeof path); snprintf(tmp, sizeof tmp, "%s.tmp", path); mkdir(g_docs_dir, 0755);
    FILE *f = fopen(tmp, "wb"); if (f) { fwrite(b.p ? b.p : "", 1, b.n, f); fclose(f); rename(tmp, path); } free(b.p);
}
static void defs_load(void) {
    if (g_defs_loaded) return; g_defs_loaded = 1; char path[4200]; defs_path(path, sizeof path); FILE *f = fopen(path, "rb"); if (!f) return;
    fseek(f, 0, SEEK_END); long n = ftell(f); fseek(f, 0, SEEK_SET); char *d = malloc(n + 1); if (fread(d, 1, n, f) != (size_t)n) n = 0; d[n] = 0; fclose(f);
    const char *p = d; while (p < d + n && g_ndefs < 512) { gptr k = defs_parse(&p); gptr v = defs_parse(&p); if (!is_string(k)) break; snprintf(g_defs[g_ndefs].key, sizeof g_defs[0].key, "%s", scstr(k)); g_defs[g_ndefs++].val = v; }
    free(d); hle_log("[defaults] loaded %d keys from %s\n", g_ndefs, path);
}
static DefEnt *defs_find(const char *k);
gptr defs_get_obj(const char *k) { DefEnt *e = defs_find(k); return e ? e->val : 0; }
static DefEnt *defs_find(const char *k) { defs_load(); for (int i = 0; i < g_ndefs; i++) if (!strcmp(g_defs[i].key, k)) return &g_defs[i]; return NULL; }
static void defs_set(const char *k, gptr v) {
    defs_load(); DefEnt *e = defs_find(k); if (!v) { if (e) { e->val = 0; defs_save(); } return; }
    SBuf b = {0}; defs_ser(&b, v, 0); sb_put(&b, "", 1); const char *pp = b.p; gptr copy = defs_parse(&pp); free(b.p);   /* store an independent snapshot, as UserDefaults does */
    if (!e) { if (g_ndefs >= 512) return; e = &g_defs[g_ndefs++]; snprintf(e->key, sizeof e->key, "%s", k); } e->val = copy; defs_save();
}
static int defaults_msg(const char *sel, gptr recv, int is_cls, int stret) {
    (void)recv; if (is_cls) return 0;
    if (strstr(sel, "ForKey:") && !strncmp(sel, "set", 3) == 0) hle_log("[defaults] get %s %s\n", sel, scstr(UA(2)));
    if (FS("objectForKey:") || FS("valueForKey:") || FS("stringForKey:") || FS("arrayForKey:") || FS("dictionaryForKey:") || FS("dataForKey:") || FS("stringArrayForKey:")) { const char *k = scstr(UA(2)); DefEnt *e = defs_find(k); if (e && e->val) { ret(e->val); return 1; } return 0; }
    if (FS("integerForKey:") || FS("boolForKey:")) { DefEnt *e = defs_find(scstr(UA(2))); ret(e && e->val ? (u32)(FS("boolForKey:") ? num_double(e->val) != 0 : (s32)num_double(e->val)) : 0); return 1; }
    if (FS("floatForKey:")) { DefEnt *e = defs_find(scstr(UA(2))); float f = e && e->val ? (float)num_double(e->val) : 0; ret(w_of(f)); return 1; }
    if (FS("doubleForKey:")) { DefEnt *e = defs_find(scstr(UA(2))); ret_double(e && e->val ? num_double(e->val) : 0); return 1; }
    if (FS("setObject:forKey:")) { defs_set(scstr(UA(3)), UA(2)); ret(0); return 1; }
    if (FS("setInteger:forKey:") || FS("setBool:forKey:")) { defs_set(scstr(UA(3)), new_number_i((s32)UA(2))); ret(0); return 1; }
    if (FS("setFloat:forKey:")) { defs_set(scstr(UA(3)), new_number_d(f_of(UA(2)))); ret(0); return 1; }
    if (FS("setDouble:forKey:")) { defs_set(scstr(arg(4)), new_number_d(dbl_of64(arg64(2)))); ret(0); return 1; }
    if (FS("removeObjectForKey:")) { defs_set(scstr(UA(2)), 0); ret(0); return 1; }
    if (FS("synchronize")) { defs_save(); ret(1); return 1; }
    if (FS("registerDefaults:")) { gptr d = UA(2); defs_load(); for (u32 i = 0; d && i < rd32(d + 4); i++) { const char *k = scstr(rd32(rd32(d + 8) + 4 * i)); if (!defs_find(k)) defs_set(k, rd32(rd32(d + 12) + 4 * i)); } ret(0); return 1; }
    return 0;
}

static int found_msg(const char *cls, gptr recv, const char *sel, int stret, int is_cls) {
    if (!strcmp(cls, "NSUserDefaults") && defaults_msg(sel, recv, is_cls, stret)) return 1;
    /* ================= NSArray / NSMutableArray / NSSet ================= */
    if (is_cls_named(cls, "Array") || !strcmp(cls, "NSSet") || !strcmp(cls, "NSMutableSet")) {
        gptr ac = objc_class_object(strstr(cls, "Mutable") ? "NSMutableArray" : "NSArray");
        if (is_cls) {
            if (FS("set")) { ret(new_array()); return 1; } if (FS("setWithArray:")) { ret(arr_copy(UA(2), objc_class_object("NSMutableArray"))); return 1; }
            if (FS("array")) { (void)ac; ret(new_array()); return 1; }
            if (FS("arrayWithObject:")) { gptr a = new_array(); array_add(a, UA(2)); ret(a); return 1; }
            if (FS("arrayWithObjects:")) { gptr a = new_array(); for (int i = 2; i < 40; i++) { gptr x = arg(i); if (!x) break; array_add(a, x); } ret(a); return 1; }
            if (FS("arrayWithArray:")) { ret(arr_copy(UA(2), objc_class_object("NSMutableArray"))); return 1; }
            if (FS("arrayWithCapacity:")) { ret(new_array()); return 1; }
            return 0;
        }
        if (FS("initWithObjects:")) { for (int i = 2; i < 40; i++) { gptr x = arg(i); if (!x) break; array_add(recv, x); } ret(recv); return 1; }
        if (FS("initWithArray:")) { gptr src = UA(2); for (u32 i = 0; src && i < rd32(src + 4); i++) array_add(recv, array_get(src, i)); ret(recv); return 1; }
        if (FS("initWithCapacity:")) { ret(recv); return 1; }
        if (FS("firstObject")) { ret(array_get(recv, 0)); return 1; }
        if (FS("insertObject:atIndex:")) { arr_insert(recv, UA(3), UA(2)); ret(0); return 1; }
        if (FS("removeObjectAtIndex:")) { arr_remove(recv, UA(2)); ret(0); return 1; }
        if (FS("removeLastObject")) { arr_remove(recv, rd32(recv + 4) - 1); ret(0); return 1; }
        if (FS("removeAllObjects")) { wr32(recv + 4, 0); ret(0); return 1; }
        if (FS("removeObject:")) { for (int i = (int)rd32(recv + 4) - 1; i >= 0; i--) if (objs_equal(array_get(recv, i), UA(2))) arr_remove(recv, i); ret(0); return 1; }
        if (FS("addObjectsFromArray:")) { gptr src = UA(2); for (u32 i = 0; src && i < rd32(src + 4); i++) array_add(recv, array_get(src, i)); ret(0); return 1; }
        if (FS("replaceObjectAtIndex:withObject:")) { if (UA(2) < rd32(recv + 4)) wr32(rd32(recv + 8) + 4 * UA(2), UA(3)); ret(0); return 1; }
        if (FS("indexOfObject:")) { for (u32 i = 0; i < rd32(recv + 4); i++) if (objs_equal(array_get(recv, i), UA(2))) { ret(i); return 1; } ret(0x7fffffff); return 1; }
        if (FS("containsObject:")) { for (u32 i = 0; i < rd32(recv + 4); i++) if (objs_equal(array_get(recv, i), UA(2))) { ret(1); return 1; } ret(0); return 1; }
        if (FS("objectEnumerator") || FS("reverseObjectEnumerator")) { gptr e = arr_copy(recv, objc_class_object("NSMutableArray")); wr32(e + 12 + 0, rd32(e + 12)); gptr en = new_obj(objc_class_object("__NSEnum")); wr32(en + 4, e); wr32(en + 8, 0); ret(en); return 1; }
        if (FS("componentsJoinedByString:")) { const char *sep = scstr(UA(2)); size_t cap = 256; char *b = malloc(cap); b[0] = 0; for (u32 i = 0; i < rd32(recv + 4); i++) { const char *p = objc_describe(array_get(recv, i)); size_t need = strlen(b) + strlen(p) + strlen(sep) + 2; if (need > cap) { cap = need * 2; b = realloc(b, cap); } if (i) strcat(b, sep); strcat(b, p); } gptr r = new_string(b); free(b); ret(r); return 1; }
        if (FS("sortUsingSelector:")) { sort_array(recv, gstr(UA(2))); ret(0); return 1; }
        if (FS("sortedArrayUsingSelector:")) { gptr c = arr_copy(recv, objc_class_object("NSMutableArray")); sort_array(c, gstr(UA(2))); ret(c); return 1; }
        if (FS("subarrayWithRange:")) { gptr a = new_array(); u32 loc = UA(2), len = UA(3); for (u32 i = loc; i < loc + len && i < rd32(recv + 4); i++) array_add(a, array_get(recv, i)); ret(a); return 1; }
        if (FS("copy") || FS("mutableCopy") || FS("copyWithZone:") || FS("mutableCopyWithZone:")) { ret(arr_copy(recv, objc_class_object(FS("mutableCopy") || FS("mutableCopyWithZone:") ? "NSMutableArray" : "NSArray"))); return 1; }
        if (FS("isEqualToArray:")) { gptr o = UA(2); if (!o || rd32(o + 4) != rd32(recv + 4)) { ret(0); return 1; } for (u32 i = 0; i < rd32(recv + 4); i++) if (!objs_equal(array_get(recv, i), array_get(o, i))) { ret(0); return 1; } ret(1); return 1; }
        if (FS("addObject:")) { array_add(recv, UA(2)); ret(0); return 1; }
        if (FS("anyObject")) { ret(array_get(recv, 0)); return 1; }
    }
    if (!strcmp(cls, "__NSEnum")) { if (FS("nextObject")) { gptr a = rd32(recv + 4); u32 i = rd32(recv + 8); wr32(recv + 8, i + 1); ret(array_get(a, i)); return 1; } if (FS("allObjects")) { ret(rd32(recv + 4)); return 1; } return 0; }
    /* ================= NSDictionary / NSMutableDictionary ================= */
    if (is_cls_named(cls, "Dictionary")) {
        if (is_cls) {
            if (FS("dictionary") || FS("dictionaryWithCapacity:")) { ret(new_dict()); return 1; }
            if (FS("dictionaryWithObject:forKey:")) { gptr d = new_dict(); if (UA(2) && UA(3)) dict_put(d, UA(3), UA(2)); ret(d); return 1; }   /* ShopItemView -> ShopViewController selection travels in this userInfo */
            if (FS("dictionaryWithObjectsAndKeys:")) { gptr d = new_dict(); for (int i = 2; i < 60; i += 2) { gptr v = arg(i), k = arg(i + 1); if (!v || !k) break; dict_put(d, k, v); } ret(d); return 1; }
            if (FS("dictionaryWithObjects:forKeys:")) { gptr d = new_dict(), vs = UA(2), ks = UA(3); for (u32 i = 0; vs && ks && i < rd32(ks + 4); i++) dict_put(d, array_get(ks, i), array_get(vs, i)); ret(d); return 1; }
            if (FS("dictionaryWithDictionary:")) { gptr d = new_dict(), s = UA(2); for (u32 i = 0; s && i < rd32(s + 4); i++) dict_put(d, rd32(rd32(s + 8) + 4 * i), rd32(rd32(s + 12) + 4 * i)); ret(d); return 1; }
            return 0;
        }
        if (FS("initWithCapacity:")) { ret(recv); return 1; }
        if (FS("initWithObjectsAndKeys:")) { for (int i = 2; i < 60; i += 2) { gptr v = arg(i), k = arg(i + 1); if (!v || !k) break; dict_put(recv, k, v); } ret(recv); return 1; }
        if (FS("initWithDictionary:")) { gptr s = UA(2); for (u32 i = 0; s && i < rd32(s + 4); i++) dict_put(recv, rd32(rd32(s + 8) + 4 * i), rd32(rd32(s + 12) + 4 * i)); ret(recv); return 1; }
        if (FS("setObject:forKey:") || FS("setValue:forKey:")) { if (UA(3)) { if (UA(2)) dict_put(recv, UA(3), UA(2)); else dict_del(recv, UA(3)); } ret(0); return 1; }
        if (FS("objectForKey:") || FS("valueForKey:")) { gptr r = dict_lookup(recv, UA(2)); if (!r && !strcmp(scstr(UA(2)), "players") && dict_lookup(recv, new_string("roomid"))) { extern gptr offline_room_players(gptr); r = offline_room_players(recv); }
            if (!r) { const char *kk = scstr(UA(2)); if ((!strcmp(kk, "settings") || !strcmp(kk, "grantsunlock") || !strcmp(kk, "msgwin") || !strcmp(kk, "msglost") || !strcmp(kk, "needsunlock")) && dict_lookup(recv, new_string("roomid"))) { extern gptr offline_room_field(gptr, gptr); r = offline_room_field(recv, UA(2)); } } if (!r && getenv("RA_DICTKEYS") && !strcmp(scstr(UA(2)), getenv("RA_DICTKEYS"))) { static int once; if (once++ < 2) { hle_log("[dict-keys] miss %s; keys:", scstr(UA(2))); for (u32 i = 0; i < rd32(recv + 4); i++) hle_log(" %s", objc_describe(rd32(rd32(recv + 8) + 4 * i))); hle_log("\n"); } } if (g_trace > 1) hle_log("[dict] %s -> %s (count %u)\n", objc_describe(UA(2)), r ? "found" : "NIL", rd32(recv + 4)); ret(r); return 1; }
        if (FS("removeObjectForKey:")) { dict_del(recv, UA(2)); ret(0); return 1; }
        if (FS("removeAllObjects")) { wr32(recv + 4, 0); ret(0); return 1; }
        if (FS("allKeys") || FS("allValues")) { gptr a = new_array(); gptr src = rd32(recv + (FS("allKeys") ? 8 : 12)); for (u32 i = 0; i < rd32(recv + 4); i++) array_add(a, rd32(src + 4 * i)); ret(a); return 1; }
        if (FS("keyEnumerator") || FS("objectEnumerator")) { gptr a = new_array(); gptr src = rd32(recv + (FS("keyEnumerator") ? 8 : 12)); for (u32 i = 0; i < rd32(recv + 4); i++) array_add(a, rd32(src + 4 * i)); gptr en = new_obj(objc_class_object("__NSEnum")); wr32(en + 4, a); ret(en); return 1; }
        if (FS("addEntriesFromDictionary:")) { gptr s = UA(2); for (u32 i = 0; s && i < rd32(s + 4); i++) dict_put(recv, rd32(rd32(s + 8) + 4 * i), rd32(rd32(s + 12) + 4 * i)); ret(0); return 1; }
        if (FS("count")) { ret(rd32(recv + 4)); return 1; }
        if (FS("copy") || FS("mutableCopy") || FS("copyWithZone:") || FS("mutableCopyWithZone:")) { gptr d = new_dict(); for (u32 i = 0; i < rd32(recv + 4); i++) dict_put(d, rd32(rd32(recv + 8) + 4 * i), rd32(rd32(recv + 12) + 4 * i)); ret(d); return 1; }
    }
    /* ================= NSData / NSMutableData ================= */
    if (is_cls_named(cls, "Data")) {
        if (is_cls) {
            if (FS("dataWithContentsOfFile:")) { ret(data_from_file(scstr(UA(2)))); return 1; } if (FS("dataWithContentsOfURL:")) { ret(data_from_file(url_path(UA(2)))); return 1; }
            if (FS("dataWithBytes:length:")) { ret(new_data(H(UA(2)), UA(3))); return 1; } if (FS("data") || FS("dataWithCapacity:") || FS("dataWithLength:")) { gptr d = new_data(NULL, FS("dataWithLength:") ? UA(2) : 0); if (FS("dataWithLength:")) memset(H(rd32(d + 4)), 0, UA(2)); ret(d); return 1; }
            return 0;
        }
        if (FS("initWithBytes:length:")) { gptr b = gm_malloc(UA(3) + 1); memcpy(H(b), H(UA(2)), UA(3)); wr32(recv + 4, b); wr32(recv + 8, UA(3)); wr32(recv + 12, UA(3) + 1); ret(recv); return 1; }
        if (FS("initWithContentsOfFile:") || FS("initWithContentsOfURL:")) { gptr d = data_from_file(FS("initWithContentsOfFile:") ? scstr(UA(2)) : url_path(UA(2))); ret(d); return 1; }
        if (FS("initWithCapacity:") || FS("initWithLength:")) { wr32(recv + 4, gm_malloc(256)); wr32(recv + 8, FS("initWithLength:") ? UA(2) : 0); wr32(recv + 12, 256); ret(recv); return 1; }
        if (FS("length")) { ret(rd32(recv + 8)); return 1; } if (FS("bytes") || FS("mutableBytes")) { ret(rd32(recv + 4)); return 1; }
        if (FS("appendData:")) { gptr o = UA(2); if (o) data_append(recv, H(rd32(o + 4)), rd32(o + 8)); ret(0); return 1; }
        if (FS("appendBytes:length:")) { data_append(recv, H(UA(2)), UA(3)); ret(0); return 1; }
        if (FS("setLength:")) { u32 n = UA(2); if (n + 1 > rd32(recv + 12)) { wr32(recv + 4, gm_realloc(rd32(recv + 4), n + 1)); wr32(recv + 12, n + 1); } if (n > rd32(recv + 8)) memset(H(rd32(recv + 4) + rd32(recv + 8)), 0, n - rd32(recv + 8)); wr32(recv + 8, n); ret(0); return 1; }
        if (FS("getBytes:length:")) { memcpy(H(UA(2)), H(rd32(recv + 4)), UA(3)); ret(0); return 1; }
        if (FS("writeToFile:atomically:")) { FILE *f = fopen(host_path(scstr(UA(2))), "wb"); if (f) { fwrite(H(rd32(recv + 4)), 1, rd32(recv + 8), f); fclose(f); } ret(f != NULL); return 1; }
    }
    /* ================= NSNumber ================= */
    if (!strcmp(cls, "NSNumber")) {
        if (is_cls) { if (FS("numberWithFloat:")) { ret(new_number_d(f_of(arg(2)))); return 1; } if (FS("numberWithDouble:")) { ret(new_number_d(dbl_of64(arg64(2)))); return 1; }
            if (FS("numberWithInt:") || FS("numberWithInteger:") || FS("numberWithUnsignedInt:") || FS("numberWithBool:") || FS("numberWithLong:") || FS("numberWithUnsignedInteger:") || FS("numberWithChar:") || FS("numberWithShort:")) { ret(new_number_i((s32)UA(2))); return 1; }
            if (FS("numberWithLongLong:") || FS("numberWithUnsignedLongLong:")) { ret(new_number_u64(arg64(2))); return 1; } return 0; }
        if (FS("floatValue")) { ret(w_of((float)num_double(recv))); return 1; } if (FS("doubleValue")) { ret_double(num_double(recv)); return 1; }
        if (FS("intValue") || FS("integerValue") || FS("unsignedIntValue") || FS("boolValue") || FS("longValue") || FS("charValue") || FS("shortValue")) { ret((u32)(s32)num_double(recv)); return 1; }
        if (FS("longLongValue") || FS("unsignedLongLongValue")) { ret64((u64)(s64)num_double(recv)); return 1; }
        if (FS("isEqualToNumber:")) { ret(num_double(recv) == num_double(UA(2))); return 1; } if (FS("compare:")) { double a = num_double(recv), b = num_double(UA(2)); ret(a < b ? (u32)-1 : a > b ? 1 : 0); return 1; }
        if (FS("stringValue")) { char b[64]; snprintf(b, sizeof b, "%g", num_double(recv)); ret(new_string(b)); return 1; }
    }
    /* ================= NSString extras ================= */
    if (is_cls_named(cls, "String")) {
        if (is_cls) { if (FS("string")) { ret(new_string("")); return 1; } if (FS("stringWithString:")) { ret(new_string(scstr(UA(2)))); return 1; }
            if (FS("stringWithContentsOfFile:encoding:error:") || FS("stringWithContentsOfFile:")) { gptr d = data_from_file(scstr(UA(2))); if (!d) { ret(0); return 1; } char *t = malloc(rd32(d + 8) + 1); memcpy(t, H(rd32(d + 4)), rd32(d + 8)); t[rd32(d + 8)] = 0; gptr r = new_string(t); free(t); ret(r); return 1; }
            return 0; }
        const char *me = scstr(recv);
        if (FS("initWithFormat:")) { extern int gformat_regs(char *, size_t, gptr, int); char b[8192]; gptr fg = gm_strdup(scstr(UA(2))); gformat_regs(b, sizeof b, fg, 3); gm_free(fg); wr32(recv, strstr(cls, "Mutable") ? cls_NSMutableString : cls_NSString); wr32(recv + 4, 0x7C8); set_string(recv, b); ret(recv); return 1; }
        if (FS("initWithString:") || FS("initWithUTF8String:") || FS("initWithCString:encoding:")) { wr32(recv, strstr(cls, "Mutable") ? cls_NSMutableString : cls_NSString); wr32(recv + 4, 0x7C8); set_string(recv, FS("initWithString:") ? scstr(UA(2)) : gstr(UA(2))); ret(recv); return 1; }
        if (FS("initWithData:encoding:")) { gptr d = UA(2); char *t = malloc(rd32(d + 8) + 1); memcpy(t, H(rd32(d + 4)), rd32(d + 8)); t[rd32(d + 8)] = 0; wr32(recv, cls_NSString); wr32(recv + 4, 0x7C8); set_string(recv, t); free(t); ret(recv); return 1; }
        if (FS("rangeOfString:") || FS("rangeOfString:options:")) { const char *n = scstr(UA(2)); if (g_trace) hle_log("[str] \"%s\" rangeOf \"%s\"\n", me, n); const char *p = strstr(me, n); if (p) ret_range((u32)(p - me), (u32)strlen(n)); else ret_range(0x7fffffff, 0); return 1; }
        if (FS("containsString:")) { ret(strstr(me, scstr(UA(2))) != NULL); return 1; }
        if (FS("capitalizedString")) { char *t = strdup(me); int up = 1; for (char *q = t; *q; q++) { *q = up ? toupper(*q) : tolower(*q); up = !isalnum((unsigned char)*q); } gptr r = new_string(t); free(t); ret(r); return 1; }
        if (FS("stringByReplacingCharactersInRange:withString:")) { u32 loc = arg(2), len = arg(3); const char *rep = scstr(UA(4)); size_t n = strlen(me); if (loc > n) loc = n; if (loc + len > n) len = n - loc; char *t = malloc(n + strlen(rep) + 1); memcpy(t, me, loc); strcpy(t + loc, rep); strcat(t, me + loc + len); gptr r = new_string(t); free(t); ret(r); return 1; }
        if (FS("lowercaseString") || FS("uppercaseString")) { char *t = strdup(me); for (char *q = t; *q; q++) *q = FS("lowercaseString") ? tolower(*q) : toupper(*q); gptr r = new_string(t); free(t); ret(r); return 1; }
        if (FS("stringByReplacingOccurrencesOfString:withString:")) { const char *a = scstr(UA(2)), *b = scstr(UA(3)); size_t la = strlen(a); if (!la) { ret(recv); return 1; } char *o = malloc(strlen(me) * (strlen(b) + 1) + 1), *w = o; const char *r = me; for (const char *q; (q = strstr(r, a));) { memcpy(w, r, q - r); w += q - r; strcpy(w, b); w += strlen(b); r = q + la; } strcpy(w, r); gptr res = new_string(o); free(o); ret(res); return 1; }
        if (FS("componentsSeparatedByString:")) { const char *sep = scstr(UA(2)); gptr a = new_array(); size_t ls = strlen(sep); const char *r = me; if (!ls) { array_add(a, recv); ret(a); return 1; } for (const char *q; (q = strstr(r, sep));) { char *t = strndup(r, q - r); array_add(a, new_string(t)); free(t); r = q + ls; } array_add(a, new_string(r)); ret(a); return 1; }
        if (FS("substringToIndex:")) { char *t = strndup(me, UA(2) < strlen(me) ? UA(2) : strlen(me)); gptr r = new_string(t); free(t); ret(r); return 1; }
        if (FS("substringFromIndex:")) { ret(new_string(UA(2) <= strlen(me) ? me + UA(2) : "")); return 1; }
        if (FS("substringWithRange:")) { u32 loc = UA(2), len = UA(3), n = strlen(me); if (loc > n) loc = n; if (loc + len > n) len = n - loc; char *t = strndup(me + loc, len); gptr r = new_string(t); free(t); ret(r); return 1; }
        if (FS("characterAtIndex:")) { ret(UA(2) < strlen(me) ? (unsigned char)me[UA(2)] : 0); return 1; }
        if (FS("boolValue")) { ret(me[0] == 'Y' || me[0] == 'y' || me[0] == 'T' || me[0] == 't' || atoi(me) != 0); return 1; }
        if (FS("integerValue")) { ret(atoi(me)); return 1; } if (FS("longLongValue")) { ret64((u64)atoll(me)); return 1; }
        if (FS("appendString:")) { char *t = malloc(strlen(me) + strlen(scstr(UA(2))) + 1); strcpy(t, me); strcat(t, scstr(UA(2))); set_string(recv, t); free(t); ret(0); return 1; }
        if (FS("setString:")) { set_string(recv, scstr(UA(2))); ret(0); return 1; }
        if (FS("lastPathComponent") && !getenv("RA_NOLPC")) { const char *q = strrchr(me, '/'); ret(new_string(q ? q + 1 : me)); return 1; }
        if (FS("pathExtension")) { const char *q = strrchr(me, '.'); ret(new_string(q ? q + 1 : "")); return 1; }
        if (FS("stringByDeletingPathExtension")) { char *t = strdup(me); char *q = strrchr(t, '.'); if (q && !strchr(q, '/')) *q = 0; gptr r = new_string(t); free(t); ret(r); return 1; }
        if (FS("stringByAppendingPathExtension:")) { char *t = malloc(strlen(me) + strlen(scstr(UA(2))) + 2); sprintf(t, "%s.%s", me, scstr(UA(2))); gptr r = new_string(t); free(t); ret(r); return 1; }
        if (FS("caseInsensitiveCompare:")) { int r = strcasecmp(me, scstr(UA(2))); ret(r < 0 ? (u32)-1 : r > 0 ? 1 : 0); return 1; }
        if (FS("isEqual:")) { ret(is_string(UA(2)) && !strcmp(me, scstr(UA(2)))); return 1; } if (FS("hash")) { u32 h = 5381; for (const char *q = me; *q; q++) h = h * 33 + *q; ret(h); return 1; }
        if (FS("stringByTrimmingCharactersInSet:")) { gptr cs = UA(2); const uint8_t *m = cs && rd32(cs + 4) ? (const uint8_t *)H(rd32(cs + 4)) : NULL; const char *st = me; while (*st && (m ? m[(uint8_t)*st] : isspace((unsigned char)*st))) st++; char *t = strdup(st); size_t n = strlen(t); while (n && (m ? m[(uint8_t)t[n - 1]] : isspace((unsigned char)t[n - 1]))) t[--n] = 0; gptr r = new_string(t); free(t); ret(r); return 1; }
        if (FS("sizeWithFont:") || FS("sizeWithFont:constrainedToSize:") || FS("sizeWithFont:constrainedToSize:lineBreakMode:") || FS("sizeWithFont:forWidth:lineBreakMode:")) {
            gptr f = UA(2); TTF_Font *tf = font_for(f ? gstr(rd32(f + 4)) : "Helvetica", f ? f_of(rd32(f + 8)) : 17); int w = 0, h = 0; if (tf) { if (FS("sizeWithFont:")) TTF_SizeUTF8(tf, me, &w, &h); else { float maxw = f_of(arg(4)); SDL_Color c = {255, 255, 255, 255}; SDL_Surface *sf = TTF_RenderUTF8_Blended_Wrapped(tf, me, c, maxw > 1 ? (int)maxw : 1); if (sf) { w = sf->w; h = sf->h; SDL_FreeSurface(sf); } } }
            ret_size((float)w, (float)h); return 1; }
        if (FS("drawInRect:withFont:") || FS("drawInRect:withFont:lineBreakMode:") || FS("drawInRect:withFont:lineBreakMode:alignment:") || FS("drawAtPoint:withFont:")) {
            CGCtx *c = cg_of(s_cur_ui_ctx); if (getenv("RA_TXTDBG")) hle_log("[draw] '%s' ctx=%08x live=%d %dx%d bpp=%d fill=%.1f,%.1f,%.1f,%.1f alpha=%.1f ctm=%.1f,%.1f,%.1f,%.1f,%.1f,%.1f\n", me, s_cur_ui_ctx, c ? c->live : -1, c ? c->w : 0, c ? c->h : 0, c ? c->bpp : 0, c ? c->fill[0] : 0, c ? c->fill[1] : 0, c ? c->fill[2] : 0, c ? c->fill[3] : 0, c ? c->alpha : 0, c ? c->ctm.a : 0, c ? c->ctm.b : 0, c ? c->ctm.c : 0, c ? c->ctm.d : 0, c ? c->ctm.tx : 0, c ? c->ctm.ty : 0); if (!c) { ret(0); return 1; }
            float x, y, w = 0; gptr f; int align = 0;
            if (FS("drawAtPoint:withFont:")) { x = f_of(UA(2)); y = f_of(UA(3)); f = UA(4); } else { x = f_of(UA(2)); y = f_of(UA(3)); w = f_of(UA(4)); f = UA(6); align = FS("drawInRect:withFont:lineBreakMode:alignment:") ? (int)UA(8) : 0; }
            TTF_Font *tf = font_for(f ? gstr(rd32(f + 4)) : "Helvetica", f ? f_of(rd32(f + 8)) : 17); if (!tf) { ret(0); return 1; } SDL_Color col = {255, 255, 255, 255};
            SDL_Surface *sf = w > 1 ? TTF_RenderUTF8_Blended_Wrapped(tf, me, col, (int)w) : TTF_RenderUTF8_Blended(tf, me, col); if (!sf) { ret(0); return 1; } SDL_Surface *r = SDL_ConvertSurfaceFormat(sf, SDL_PIXELFORMAT_ABGR8888, 0); SDL_FreeSurface(sf); if (!r) { ret(0); return 1; }
            float ux, uy; ctm_apply(c->ctm, x, y, &ux, &uy); int dx = (int)ux, dy = (int)(c->h - uy); if (align == 1 && w > 0) dx += (int)(w - r->w) / 2; else if (align == 2 && w > 0) dx += (int)(w - r->w);
            for (int j = 0; j < r->h; j++) for (int i = 0; i < r->w; i++) { uint8_t *p = (uint8_t *)r->pixels + (size_t)j * r->pitch + i * 4; if (p[3]) cg_blend(c, dx + i, dy + j, c->fill[0], c->fill[1], c->fill[2], p[3] / 255.f * c->fill[3] * c->alpha); }
            if (getenv("RA_TXTDBG")) hle_log("[draw2] x=%.1f y=%.1f w=%.1f -> surf %dx%d dx=%d dy=%d font=%p\n", x, y, w, r->w, r->h, dx, dy, (void *)tf);
            if (stret) { wr32(arg(0), w_of((float)r->w)); wr32(arg(0) + 4, w_of((float)r->h)); } SDL_FreeSurface(r); ret(0); return 1; }
    }
    /* ================= NSDate / NSDateFormatter / NSTimeZone / NSLocale ================= */
    if (!strcmp(cls, "NSDate")) {
        if (is_cls) { if (FS("date")) { ret(new_date(unix_now())); return 1; } if (FS("dateWithTimeIntervalSince1970:")) { ret(new_date(dbl_of64(arg64(2)))); return 1; } if (FS("dateWithTimeIntervalSinceNow:")) { ret(new_date(unix_now() + dbl_of64(arg64(2)))); return 1; } if (FS("distantFuture")) { ret(new_date(4e9)); return 1; } if (FS("distantPast")) { ret(new_date(0)); return 1; } return 0; }
        if (FS("init")) { double t = unix_now(); memcpy(H(recv + 8), &t, 8); ret(recv); return 1; } if (FS("initWithTimeIntervalSince1970:")) { u64 b = arg64(2); memcpy(H(recv + 8), &b, 8); ret(recv); return 1; }
        if (FS("timeIntervalSince1970") || FS("timeIntervalSinceReferenceDate")) { ret_double(date_t(recv) - (FS("timeIntervalSinceReferenceDate") ? 978307200.0 : 0)); return 1; }
        if (FS("timeIntervalSinceNow")) { ret_double(date_t(recv) - unix_now()); return 1; } if (FS("timeIntervalSinceDate:")) { ret_double(date_t(recv) - date_t(UA(2))); return 1; }
        if (FS("addTimeInterval:") || FS("dateByAddingTimeInterval:")) { ret(new_date(date_t(recv) + dbl_of64(arg64(2)))); return 1; }
        if (FS("compare:")) { double a = date_t(recv), b = date_t(UA(2)); ret(a < b ? (u32)-1 : a > b ? 1 : 0); return 1; } if (FS("earlierDate:")) { ret(date_t(recv) <= date_t(UA(2)) ? recv : UA(2)); return 1; } if (FS("laterDate:")) { ret(date_t(recv) >= date_t(UA(2)) ? recv : UA(2)); return 1; }
    }
    if (!strcmp(cls, "NSDateFormatter")) {
        if (FS("setDateFormat:")) { wr32(recv + 4, UA(2)); ret(0); return 1; }
        if (FS("stringFromDate:")) { const char *f = rd32(recv + 4) ? scstr(rd32(recv + 4)) : "yyyy-MM-dd HH:mm:ss"; time_t t = (time_t)date_t(UA(2)); struct tm tm; localtime_r(&t, &tm); char fmt[128] = "", out[256]; size_t n = 0;
            for (const char *q = f; *q && n < 120;) { if (!strncmp(q, "yyyy", 4)) { strcpy(fmt + n, "%Y"); n += 2; q += 4; } else if (!strncmp(q, "MM", 2)) { strcpy(fmt + n, "%m"); n += 2; q += 2; } else if (!strncmp(q, "dd", 2)) { strcpy(fmt + n, "%d"); n += 2; q += 2; } else if (!strncmp(q, "HH", 2)) { strcpy(fmt + n, "%H"); n += 2; q += 2; } else if (!strncmp(q, "mm", 2)) { strcpy(fmt + n, "%M"); n += 2; q += 2; } else if (!strncmp(q, "ss", 2)) { strcpy(fmt + n, "%S"); n += 2; q += 2; } else fmt[n++] = *q++; fmt[n] = 0; }
            strftime(out, sizeof out, fmt, &tm); ret(new_string(out)); return 1; }
        if (FS("dateFromString:")) { ret(0); return 1; } if (FS("setTimeZone:") || FS("setLocale:") || FS("setDateStyle:") || FS("setTimeStyle:")) { ret(0); return 1; }
    }
    if (!strcmp(cls, "NSTimeZone")) { if (is_cls) { ret(new_obj(objc_class_object("NSTimeZone"))); return 1; } if (FS("secondsFromGMT") || FS("secondsFromGMTForDate:")) { ret(0); return 1; } if (FS("name") || FS("abbreviation")) { ret(new_string("GMT")); return 1; } return 0; }
    if (!strcmp(cls, "NSLocale")) { if (is_cls) { if (FS("currentLocale") || FS("systemLocale")) { ret(new_obj(objc_class_object("NSLocale"))); return 1; } if (FS("preferredLanguages")) { gptr a = new_array(); array_add(a, new_string("en")); ret(a); return 1; } return 0; }
        if (FS("objectForKey:")) { const char *k = scstr(UA(2)); ret(new_string(strstr(k, "ountry") ? "US" : strstr(k, "anguage") ? "en" : "en_US")); return 1; } if (FS("localeIdentifier")) { ret(new_string("en_US")); return 1; } return 0; }
    /* ================= StoreKit (offline): a fixed catalogue answers SKProductsRequest ================= */
    if (!strcmp(cls, "SKProductsRequest")) {
        if (FS("initWithProductIdentifiers:")) { wr32(recv + 4, UA(2)); ret(recv); return 1; }
        if (FS("setDelegate:")) { wr32(recv + 8, UA(2)); ret(0); return 1; } if (FS("delegate")) { ret(rd32(recv + 8)); return 1; }
        if (FS("start")) { host_defer(4, recv, 0, 0.3); ret(0); return 1; } if (FS("cancel")) { ret(0); return 1; } return 0;
    }
    if (!strcmp(cls, "SKProduct")) {
        if (FS("productIdentifier")) { ret(rd32(recv + 4)); return 1; } if (FS("localizedTitle")) { ret(rd32(recv + 8)); return 1; } if (FS("localizedDescription")) { ret(rd32(recv + 8)); return 1; }
        if (FS("price")) { ret(rd32(recv + 12)); return 1; } if (FS("priceLocale")) { ret(new_obj(objc_class_object("NSLocale"))); return 1; } return 0;
    }
    if (!strcmp(cls, "SKProductsResponse")) { if (FS("products")) { ret(rd32(recv + 4)); return 1; } if (FS("invalidProductIdentifiers")) { ret(new_array()); return 1; } return 0; }
    if (!strcmp(cls, "NSNumberFormatter")) {
        if (FS("setNumberStyle:")) { wr32(recv + 4, UA(2)); ret(0); return 1; }
        if (FS("stringFromNumber:")) { char b[64]; double v = num_double(UA(2)); if (rd32(recv + 4) == 2) snprintf(b, sizeof b, "$%.2f", v); else snprintf(b, sizeof b, "%g", v); ret(new_string(b)); return 1; }
        if (FS("setLocale:") || FS("setFormatterBehavior:") || FS("setMaximumFractionDigits:") || FS("setMinimumFractionDigits:")) { ret(0); return 1; } return 0;
    }
    /* ================= NSBundle / NSFileManager ================= */
    if (!strcmp(cls, "NSBundle")) {
        if (FS("objectForInfoDictionaryKey:")) { const char *k = scstr(UA(2)); const char *v = !strcmp(k, "CFBundleVersion") || !strcmp(k, "CFBundleShortVersionString") ? "1.14" : !strcmp(k, "CFBundleIdentifier") ? "com.chillingo.spicepirates" : !strcmp(k, "CFBundleDisplayName") || !strcmp(k, "CFBundleName") ? "Spice Bandits" : NULL; ret(v ? new_string(v) : 0); return 1; }
        if (FS("pathForResource:ofType:inDirectory:")) { char b[1024]; snprintf(b, sizeof b, "%s/%s/%s%s%s", g_bundle_dir, scstr(UA(4)), scstr(UA(2)), UA(3) ? "." : "", scstr(UA(3))); struct stat st; ret(stat(b, &st) ? 0 : new_string(b)); return 1; }
        if (FS("pathForResource:ofType:")) { char b[1024]; snprintf(b, sizeof b, "%s/%s%s%s", g_bundle_dir, scstr(UA(2)), UA(3) ? "." : "", scstr(UA(3))); struct stat st; ret(stat(b, &st) ? 0 : new_string(b)); return 1; }
        if (FS("URLForResource:withExtension:")) { char b[1024]; snprintf(b, sizeof b, "%s/%s%s%s", g_bundle_dir, scstr(UA(2)), UA(3) ? "." : "", scstr(UA(3))); struct stat st; ret(stat(b, &st) ? 0 : new_url(b, 1)); return 1; }
        if (FS("resourceURL") || FS("bundleURL")) { ret(new_url(g_bundle_dir, 1)); return 1; }
    }
    if (!strcmp(cls, "NSFileManager")) {
        if (FS("fileExistsAtPath:") || FS("fileExistsAtPath:isDirectory:")) { struct stat st; int ok = !stat(host_path(scstr(UA(2))), &st); if (ok && FS("fileExistsAtPath:isDirectory:") && UA(3)) *(uint8_t *)H(UA(3)) = S_ISDIR(st.st_mode); ret(ok); return 1; }
        if (FS("createDirectoryAtPath:withIntermediateDirectories:attributes:error:")) { char cmd[2200]; snprintf(cmd, sizeof cmd, "mkdir -p '%s'", host_path(scstr(UA(2)))); ret(system(cmd) == 0); return 1; }
        if (FS("createDirectoryAtPath:attributes:")) { ret(mkdir(host_path(scstr(UA(2))), 0755) == 0); return 1; }
        if (FS("removeItemAtPath:error:")) { ret(remove(host_path(scstr(UA(2)))) == 0); return 1; }
        if (FS("copyItemAtPath:toPath:error:")) { gptr d = data_from_file(scstr(UA(2))); if (!d) { ret(0); return 1; } FILE *f = fopen(host_path(scstr(UA(3))), "wb"); if (f) { fwrite(H(rd32(d + 4)), 1, rd32(d + 8), f); fclose(f); } ret(f != NULL); return 1; }
        if (FS("moveItemAtPath:toPath:error:")) { char a[2200]; snprintf(a, sizeof a, "%s", host_path(scstr(UA(2)))); ret(rename(a, host_path(scstr(UA(3)))) == 0); return 1; }
        if (FS("contentsOfDirectoryAtPath:error:")) { DIR *d = opendir(host_path(scstr(UA(2)))); if (!d) { ret(0); return 1; } gptr a = new_array(); struct dirent *e; while ((e = readdir(d))) if (strcmp(e->d_name, ".") && strcmp(e->d_name, "..")) array_add(a, new_string(e->d_name)); closedir(d); ret(a); return 1; }
        if (FS("contentsAtPath:")) { ret(data_from_file(scstr(UA(2)))); return 1; }
        if (FS("attributesOfItemAtPath:error:")) { struct stat st; if (stat(host_path(scstr(UA(2))), &st)) { ret(0); return 1; } gptr d = new_dict(); dict_put(d, new_string("NSFileSize"), new_number_u64(st.st_size)); ret(d); return 1; }
        if (FS("createFileAtPath:contents:attributes:")) { FILE *f = fopen(host_path(scstr(UA(2))), "wb"); if (f) { if (UA(3)) fwrite(H(rd32(UA(3) + 4)), 1, rd32(UA(3) + 8), f); fclose(f); } ret(f != NULL); return 1; }
    }
    /* ================= NSURL / NSURLRequest / NSURLConnection / NSError ================= */
    if (!strcmp(cls, "NSURL")) {
        if (is_cls) { if (FS("URLWithString:")) { if (g_trace) hle_log("[url] %s\n", scstr(UA(2))); ret(new_url(scstr(UA(2)), 0)); return 1; } if (FS("fileURLWithPath:") || FS("fileURLWithPath:isDirectory:")) { ret(new_url(scstr(UA(2)), 1)); return 1; } return 0; }
        if (FS("initWithString:")) { wr32(recv + 8, gm_strdup(scstr(UA(2)))); wr32(recv + 12, 0); ret(recv); return 1; } if (FS("initFileURLWithPath:")) { wr32(recv + 8, gm_strdup(scstr(UA(2)))); wr32(recv + 12, 1); ret(recv); return 1; }
        if (FS("path") || FS("relativePath")) { const char *p = url_path(recv); const char *q = strstr(p, "://"); if (q) { q = strchr(q + 3, '/'); p = q ? q : ""; } ret(new_string(p)); return 1; }
        if (FS("lastPathComponent") && !getenv("RA_NOLPC")) { const char *u = url_path(recv); const char *b = strrchr(u, '/'); b = b ? b + 1 : u; char t[512]; snprintf(t, sizeof t, "%s", b); char *q = strchr(t, '?'); if (q) *q = 0; ret(new_string(t)); return 1; }
        if (FS("pathExtension")) { const char *u = url_path(recv); const char *d = strrchr(u, '.'); ret(new_string(d && !strchr(d, '/') ? d + 1 : "")); return 1; }
        if (FS("absoluteString") || FS("relativeString") || FS("description")) { ret(new_string(url_path(recv))); return 1; }
        if (FS("isFileURL")) { ret(rd32(recv + 12) & 1); return 1; } if (FS("scheme")) { const char *p = url_path(recv); const char *q = strstr(p, "://"); char *t = q ? strndup(p, q - p) : strdup("file"); gptr r = new_string(t); free(t); ret(r); return 1; }
        if (FS("host")) { const char *p = url_path(recv); const char *q = strstr(p, "://"); if (!q) { ret(0); return 1; } q += 3; const char *e = strpbrk(q, "/:"); char *t = e ? strndup(q, e - q) : strdup(q); gptr r = new_string(t); free(t); ret(r); return 1; }
        if (FS("copy") || FS("copyWithZone:")) { ret(recv); return 1; }
    }
    if (strstr(cls, "URLRequest")) {
        if (is_cls || FS("initWithURL:") || FS("initWithURL:cachePolicy:timeoutInterval:")) { if (is_cls) { gptr o = new_obj(objc_class_object("NSMutableURLRequest")); wr32(o + 4, UA(2)); ret(o); return 1; } wr32(recv + 4, UA(2)); ret(recv); return 1; }
        if (FS("URL")) { ret(rd32(recv + 4)); return 1; } if (FS("setHTTPMethod:") || FS("setHTTPBody:") || FS("setValue:forHTTPHeaderField:") || FS("setTimeoutInterval:") || FS("setCachePolicy:") || FS("addValue:forHTTPHeaderField:") || FS("setHTTPShouldHandleCookies:")) { ret(0); return 1; }
        if (FS("HTTPMethod")) { ret(new_string("GET")); return 1; } return 0;
    }
    if (!strcmp(cls, "NSURLConnection")) {
        if (FS("initWithRequest:delegate:") || FS("initWithRequest:delegate:startImmediately:")) { wr32(recv + 4, UA(3)); wr32(recv + 8, UA(2)); host_defer(3, recv, 0, 0.2); ret(recv); return 1; }
        if (FS("connectionWithRequest:delegate:")) { gptr o = new_obj(objc_class_object("NSURLConnection")); wr32(o + 4, UA(3)); wr32(o + 8, UA(2)); host_defer(3, o, 0, 0.2); ret(o); return 1; }
        if (FS("sendSynchronousRequest:returningResponse:error:")) { if (UA(4)) wr32(UA(4), new_error("NSURLErrorDomain", -1009)); ret(0); return 1; }
        if (FS("start") || FS("cancel") || FS("scheduleInRunLoop:forMode:") || FS("unscheduleFromRunLoop:forMode:")) { ret(0); return 1; } return 0;
    }
    if (!strcmp(cls, "NSError")) {
        if (is_cls) { if (FS("errorWithDomain:code:userInfo:")) { gptr e = new_error(scstr(UA(2)), UA(3)); wr32(e + 12, UA(4)); ret(e); return 1; } return 0; }
        if (FS("initWithDomain:code:userInfo:")) { wr32(recv + 4, UA(2)); wr32(recv + 8, UA(3)); wr32(recv + 12, UA(4)); ret(recv); return 1; }
        if (FS("code")) { ret(rd32(recv + 8)); return 1; } if (FS("domain")) { ret(rd32(recv + 4)); return 1; } if (FS("userInfo")) { ret(rd32(recv + 12)); return 1; }
        if (FS("localizedDescription")) { ret(new_string("The Internet connection appears to be offline.")); return 1; } return 0;
    }
    /* ---- NSObject conveniences ---- */
    {
        if (FS("performSelectorOnMainThread:withObject:waitUntilDone:")) { Target t = lookup(recv, gstr(UA(2)), 0, 0); if (t.imp) { u32 a[1] = { UA(3) }; send_guest(recv, gstr(UA(2)), 1, a); } ret(0); return 1; }
        if (FS("performSelectorInBackground:withObject:")) { Target t = lookup(recv, gstr(UA(2)), 0, 0); if (t.imp) { u32 a[1] = { UA(3) }; send_guest(recv, gstr(UA(2)), 1, a); } ret(0); return 1; }
        if (FS("performSelector:withObject:afterDelay:inModes:")) { u64 d = arg64(4); double iv = dbl_of64(d); timer_add(recv, gstr(UA(2)), iv, 0, UA(3)); ret(0); return 1; }
        if (FS("cancelPreviousPerformRequestsWithTarget:") || FS("cancelPreviousPerformRequestsWithTarget:selector:object:")) { for (int i = 0; i < 128; i++) if (timers[i].valid && !timers[i].repeats && timers[i].target == UA(2) && (FS("cancelPreviousPerformRequestsWithTarget:") || !strcmp(timers[i].sel, gstr(UA(3))))) timers[i].valid = 0; ret(0); return 1; }
        if (FS("willChangeValueForKey:") || FS("didChangeValueForKey:") || FS("addObserver:forKeyPath:options:context:") || FS("removeObserver:forKeyPath:")) { ret(0); return 1; }
    }
    if (is_cls_named(cls, "String") && !is_cls) {
        const char *me = scstr(recv);
        if (FS("compare:options:")) { int r = (UA(3) & 1) ? strcasecmp(me, scstr(UA(2))) : strcmp(me, scstr(UA(2))); ret(r < 0 ? (u32)-1 : r > 0 ? 1 : 0); return 1; }
        if (FS("fileSystemRepresentation")) { ret(rd32(recv + 8)); return 1; }
        if (FS("initWithBytes:length:encoding:") || FS("initWithBytesNoCopy:length:encoding:freeWhenDone:")) { char *t = malloc(UA(3) + 1); memcpy(t, H(UA(2)), UA(3)); t[UA(3)] = 0; wr32(recv, cls_NSString); wr32(recv + 4, 0x7C8); set_string(recv, t); free(t); ret(recv); return 1; }
        if (FS("rangeOfCharacterFromSet:") || FS("rangeOfCharacterFromSet:options:")) { gptr cs = UA(2); const uint8_t *m = cs && rd32(cs + 4) ? (const uint8_t *)H(rd32(cs + 4)) : NULL; for (u32 i = 0; m && me[i]; i++) if (m[(uint8_t)me[i]]) { ret_range(i, 1); return 1; } ret_range(0x7fffffff, 0); return 1; }
    }
    if (is_cls_named(cls, "Dictionary") && FS("countByEnumeratingWithState:objects:count:")) { gptr st = UA(2); if (rd32(st)) { ret(0); return 1; } wr32(st, 1); wr32(st + 4, rd32(recv + 8)); wr32(st + 8, st + 16); ret(rd32(recv + 4)); return 1; }
    if (!strcmp(cls, "NSMutableArray") && FS("removeObjectsAtIndexes:")) { ret(0); return 1; }
    if (is_cls_named(cls, "Data") && FS("replaceBytesInRange:withBytes:length:")) { u32 loc = UA(2), len = UA(3), n = UA(5); gptr b = rd32(recv + 4); u32 total = rd32(recv + 8); u32 nl = total - len + n; if (nl + 1 > rd32(recv + 12)) { wr32(recv + 4, gm_realloc(b, nl + 1)); wr32(recv + 12, nl + 1); b = rd32(recv + 4); } memmove(H(b + loc + n), H(b + loc + len), total - loc - len); memcpy(H(b + loc), H(UA(4)), n); wr32(recv + 8, nl); ret(0); return 1; }
    if (!strcmp(cls, "NSCharacterSet") || !strcmp(cls, "NSMutableCharacterSet")) {          /* object: {isa, +4 -> 256-byte membership bitmap} */
        if (is_cls) {
            gptr o = new_obj(objc_class_object("NSCharacterSet")); gptr bm = gm_malloc(256); wr32(o + 4, bm); uint8_t *m = (uint8_t *)H(bm);
            if (FS("characterSetWithCharactersInString:")) { for (const char *q = scstr(UA(2)); *q; q++) m[(uint8_t)*q] = 1; }
            else if (FS("decimalDigitCharacterSet")) { for (int c = '0'; c <= '9'; c++) m[c] = 1; }
            else if (FS("whitespaceCharacterSet")) { m[' '] = m['\t'] = 1; }
            else if (FS("whitespaceAndNewlineCharacterSet")) { m[' '] = m['\t'] = m['\n'] = m['\r'] = 1; }
            else if (FS("newlineCharacterSet")) { m['\n'] = m['\r'] = 1; }
            else if (FS("alphanumericCharacterSet")) { for (int c = 0; c < 128; c++) if (isalnum(c)) m[c] = 1; }
            else if (FS("letterCharacterSet")) { for (int c = 0; c < 128; c++) if (isalpha(c)) m[c] = 1; }
            else if (FS("lowercaseLetterCharacterSet")) { for (int c = 'a'; c <= 'z'; c++) m[c] = 1; }
            else if (FS("uppercaseLetterCharacterSet")) { for (int c = 'A'; c <= 'Z'; c++) m[c] = 1; }
            else if (FS("punctuationCharacterSet") || FS("symbolCharacterSet")) { for (int c = 0; c < 128; c++) if (ispunct(c)) m[c] = 1; }
            else if (FS("illegalCharacterSet") || FS("controlCharacterSet")) { for (int c = 0; c < 32; c++) m[c] = 1; }
            else { memset(m, 1, 256); }
            ret(o); return 1; }
        gptr bm = rd32(recv + 4); const uint8_t *m = bm ? (const uint8_t *)H(bm) : NULL;
        if (FS("characterIsMember:")) { u32 c = UA(2) & 0xffff; ret(m && c < 256 && m[c]); return 1; }
        if (FS("invertedSet")) { gptr o = new_obj(objc_class_object("NSCharacterSet")); gptr nb = gm_malloc(256); wr32(o + 4, nb); for (int i = 0; i < 256; i++) *(uint8_t *)H(nb + i) = m ? !m[i] : 1; ret(o); return 1; }
        if (FS("addCharactersInString:")) { if (m) for (const char *q = scstr(UA(2)); *q; q++) ((uint8_t *)H(bm))[(uint8_t)*q] = 1; ret(0); return 1; }
        if (FS("removeCharactersInString:")) { if (m) for (const char *q = scstr(UA(2)); *q; q++) ((uint8_t *)H(bm))[(uint8_t)*q] = 0; ret(0); return 1; }
        ret(0); return 1;
    }
    if (!strcmp(cls, "NSMutableIndexSet") || !strcmp(cls, "NSIndexSet")) { ret(is_cls ? new_obj(objc_class_object(cls)) : 0); return 1; }
    if (!strcmp(cls, "UIScreen") && FS("instancesRespondToSelector:")) { ret(0); return 1; }
    if (!strcmp(cls, "AVAudioPlayer")) { ret(is_cls ? 0 : 0); return 1; }
    if (!strcmp(cls, "NSRunLoop")) { if (is_cls) { ret(new_obj(objc_class_object("NSRunLoop"))); return 1; } ret(0); return 1; }
    /* -[NSDictionary keysSortedByValueUsingComparator:] (UpgradesViewController builds its list from it) and -[NSArray makeObjectsPerformSelector:] */
    if (is_cls_named(cls, "Dictionary") && FS("keysSortedByValueUsingComparator:")) {
        gptr blk = UA(2), a = new_array(); u32 n = rd32(recv + 4);
        for (u32 i = 0; i < n; i++) { gptr k = rd32(rd32(recv + 8) + 4 * i), v = rd32(rd32(recv + 12) + 4 * i); array_add(a, k); u32 j = i;
            while (j > 0) { gptr pk = array_get(a, j - 1), pv = dict_lookup(recv, pk); u32 ca[3] = { blk, pv, v }; if ((s32)call_guest(rd32(blk + 12), 3, ca) <= 0) break; wr32(rd32(a + 8) + 4 * j, pk); j--; }
            wr32(rd32(a + 8) + 4 * j, k); }
        ret(a); return 1; }
    if (is_cls_named(cls, "Array") && (FS("makeObjectsPerformSelector:") || FS("makeObjectsPerformSelector:withObject:"))) {
        for (u32 i = 0; i < rd32(recv + 4); i++) { u32 ca[1] = { UA(3) }; send_guest(array_get(recv, i), gstr(UA(2)), FS("makeObjectsPerformSelector:") ? 0 : 1, ca); }
        ret(0); return 1; }
    if (is_cls_named(cls, "Array") && (FS("sortedArrayUsingComparator:") || FS("sortUsingComparator:"))) {
        gptr blk = UA(2); gptr arr = FS("sortUsingComparator:") ? recv : arr_copy(recv, objc_class_object("NSMutableArray")); u32 n = rd32(arr + 4);
        for (u32 i = 1; i < n; i++) { gptr x = array_get(arr, i); u32 j = i; while (j > 0) { u32 a[3] = { blk, array_get(arr, j - 1), x }; if ((s32)call_guest(rd32(blk + 12), 3, a) <= 0) break; wr32(rd32(arr + 8) + 4 * j, array_get(arr, j - 1)); j--; } wr32(rd32(arr + 8) + 4 * j, x); }
        ret(FS("sortUsingComparator:") ? 0 : arr); return 1; }
    if (!strcmp(cls, "UIImage") && is_cls && FS("imageWithData:")) { gptr d = UA(2); if (!d || nimg >= 4096) { ret(0); return 1; } int w, h; uint8_t *px = NULL;
        { FILE *f = tmpfile(); if (f) { fwrite(H(rd32(d + 4)), 1, rd32(d + 8), f); fflush(f); char pp[64]; snprintf(pp, sizeof pp, "/proc/self/fd/%d", fileno(f)); px = load_rgba(pp, &w, &h); fclose(f); } }
        if (!px) { ret(0); return 1; } snprintf(images[nimg].name, 199, "#data%d", nimg); images[nimg].rgba = px; images[nimg].w = w; images[nimg].h = h; ret(new_image(nimg++)); return 1; }
    if (!strcmp(cls, "NSAutoreleasePool") || !strcmp(cls, "NSAssertionHandler")) { ret(!strcmp(cls, "NSAutoreleasePool") && (FS("init") || FS("alloc")) ? recv : 0); return 1; }
    if (!strcmp(cls, "NSInvocation") || !strcmp(cls, "NSMethodSignature")) { ret(0); return 1; }
    return 0;
}

/* deliver a canned product list to an SKProductsRequest's delegate (called from host_run_deferred) */
void sk_deliver(gptr req) {
    gptr del = rd32(req + 8), ids = rd32(req + 4); if (!del) return;
    static const struct { const char *tag; const char *name; double price; } cat[] = { {".t1.", "Spice can", 0.99}, {".t3.", "Spice tin", 2.99}, {".t5.", "Spice box", 4.99}, {".t10.", "Spice canister", 9.99} };
    gptr products = new_array();
    for (u32 i = 0; ids && i < rd32(ids + 4); i++) { gptr id = array_get(ids, i); const char *ids_s = scstr(id); gptr p = new_obj(objc_class_object("SKProduct")); const char *nm = "Spice"; double pr = 0.99; (void)pr;
        for (int k = 0; k < 4; k++) if (strstr(ids_s, cat[k].tag)) { nm = cat[k].name; pr = cat[k].price; }
        wr32(p + 4, id); wr32(p + 8, new_string(nm)); wr32(p + 12, new_number_d(0.0 /* every pack is free offline */)); array_add(products, p); }
    gptr resp = new_obj(objc_class_object("SKProductsResponse")); wr32(resp + 4, products);
    hle_log("[sk] delivering %u products\n", rd32(products + 4));
    if (guest_responds(del, "productsRequest:didReceiveResponse:")) { u32 a[2] = { req, resp }; send_guest(del, "productsRequest:didReceiveResponse:", 2, a); }
    if (guest_responds(del, "requestDidFinish:")) { u32 a[1] = { req }; send_guest(del, "requestDidFinish:", 1, a); }
}
