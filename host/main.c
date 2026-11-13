/* main.c - ra_host: run the iOS Spice Bandits armv7 binary natively (Linux/Windows) via Unicorn + HLE */
#include "ra.h"
#include <SDL2/SDL.h>
#include <sys/stat.h>
#include <unistd.h>
void cpu_init(void); void extra_init(void); void cxxabi_init(void); void zlib_init(void);
extern int g_quit_request;

int main(int argc, char **argv) {
    const char *app = NULL; int w = 1024, h = 768;
    g_trace = getenv("RA_TRACE") ? atoi(getenv("RA_TRACE")) : 0;
    for (int i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "--app") && i + 1 < argc) app = argv[++i];
        else if (!strcmp(argv[i], "--size") && i + 1 < argc) sscanf(argv[++i], "%dx%d", &w, &h);
        else if (!strcmp(argv[i], "--trace")) g_trace = 1;
        else if (!strcmp(argv[i], "--trace2")) g_trace = 2;
        else if (!strcmp(argv[i], "--docs") && i + 1 < argc) g_docs_dir = argv[++i];
    }
    if (!app) { fprintf(stderr, "usage: ra_host --app <path to Tower.app> [--size WxH] [--trace]\n"); return 2; }
    static char docs[4096], appabs[4096];
    if (!realpath(app, appabs)) { perror(app); return 1; } app = appabs;
    if (!strcmp(g_docs_dir, "./save")) { const char *x = getenv("XDG_DATA_HOME"), *hm = getenv("HOME");
        if (x) snprintf(docs, sizeof docs, "%s/spicebandits-ios", x); else snprintf(docs, sizeof docs, "%s/.local/share/spicebandits-ios", hm ? hm : "."); g_docs_dir = docs; }
    { char tmp[4096]; snprintf(tmp, sizeof tmp, "mkdir -p '%s'", g_docs_dir); if (system(tmp)) {} }
    g_bundle_dir = app;
    /* offline: the game normally downloads level packs into <docs>/downloads/maps. Link the bundle's maps and the packs
       fetched by tools/fetch_levels.sh (<repo>/levels/maps, or $RA_LEVELS/maps) in file by file (cp -rs), so a pack
       can add files to a bundle directory (urban/700/...). Older runs symlinked whole directories: drop those first. */
    static char lvbuf[4096]; { const char *lv = getenv("RA_LEVELS"); char t[4096]; snprintf(t, sizeof t, "%s/../../../../levels", app); if (!realpath(lv && *lv ? lv : t, lvbuf)) lvbuf[0] = 0; g_levels_dir = lvbuf; }
    { char cmd[12000]; const char *lv = getenv("RA_LEVELS");
      snprintf(cmd, sizeof cmd, "D='%s/downloads/maps'; mkdir -p \"$D\"; find \"$D\" -maxdepth 1 -type l -xtype d -delete; find \"$D\" -xtype l -delete; "
          "cp -rsn '%s'/maps/. \"$D\"/ 2>/dev/null; L=\"%s\"; [ -n \"$L\" ] || L='%s'/../../../../levels; [ -d \"$L/maps\" ] && cp -rsn \"$(realpath \"$L\")\"/maps/. \"$D\"/ 2>/dev/null; true",
          g_docs_dir, app, lv ? lv : "", app);
      if (system(cmd)) {} }
    if (chdir(app)) perror("chdir bundle");   /* iOS runs apps with cwd = bundle */
    char bin[4096]; snprintf(bin, sizeof bin, "%s/Tower", app);
    cpu_init();
    libc_init(); extra_init(); cxxabi_init(); zlib_init(); objc_init(); gles_init(); audio_init();
    sdl_init(w, h);
    g_screen_w = w; g_screen_h = h;
    if (macho_load(bin)) return 1;
    fprintf(stderr, "[host] loaded %s, entry %08x\n", bin, g_entry);
    if (getenv("RA_DEBUG_PEEK")) { u32 a = strtoul(getenv("RA_DEBUG_PEEK"), NULL, 16); fprintf(stderr, "[peek] %08x: %08x %08x %08x %08x\n", a, rd32(a), rd32(a+4), rd32(a+8), rd32(a+12)); }
    { extern void cpu_capture_r0(gptr, gptr *); extern gptr g_engine_input; cpu_capture_r0(0xf8568, &g_engine_input); }   /* Tower's engine input object (see docs/PORT_STATUS.md) */
    objc_init();     /* guest classes are parsed after the image is mapped */
    /* dyld runs the module initialisers (C++ static constructors, +load-time setup) before main */
    { Section *mi = find_section("__mod_init_func", 0); if (mi) { u32 n = mi->size / 4; fprintf(stderr, "[host] running %u module initialisers\n", n);
        setreg(13, STACK_TOP - 4096);
        for (u32 i = 0; i < n; i++) { u32 fn = rd32(mi->addr + 4 * i); call_guest(fn, 0, NULL); } } }
    /* iOS process start: sp -> argc, argv[], NULL, envp NULL, apple NULL */
    u32 sp = STACK_TOP - 256; gptr arg0 = gm_strdup(bin);
    wr32(sp, 1); wr32(sp + 4, arg0); wr32(sp + 8, 0); wr32(sp + 12, 0); wr32(sp + 16, 0);
    setreg(13, sp); setreg(0, 0x1000);
    call_guest(g_entry, 0, NULL);
    if (!g_quit_request) { fprintf(stderr, "[host] guest returned without UIApplicationMain\n"); return 1; }
    return objc_run_app();
}
