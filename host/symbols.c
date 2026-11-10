/* symbols.c - decide how a dyld-bound symbol is satisfied: function trampoline vs fake data object */
#include "ra.h"
gptr objc_class_object(const char *classname);       /* objc.c */
gptr objc_make_nsstring_global(const char *s);       /* objc.c */
gptr runelocale_addr(void);                          /* libc.c */
gptr stdio_slot(int which);                          /* libc.c */

static struct { const char *name; gptr addr; } cache[512]; static int ncache;

static int is_data_name(const char *n) {
    static const char *pre[] = {"_OBJC_CLASS_$_", "_OBJC_METACLASS_$_", "_OBJC_EHTYPE_$_", "__ZTV", "__ZTI", "___CFConstantStringClassReference",
        "__objc_empty", "__DefaultRuneLocale", "___stdoutp", "___stderrp", "___stdinp", "_kEAGL", "_NSLocalizedDescriptionKey", "_NSUnderlyingErrorKey",
        "_kCFRunLoop", "_MPMusicPlayerController", "_NSFile", "_UI", "_kCF", "_MP", "_NSUser", "_GK", "_SK", "_AV", "__NSConcrete", "_NSConcrete", "_environ", "___progname", "_NSDefaultRunLoopMode", NULL};
    for (int i = 0; pre[i]; i++) if (!strncmp(n, pre[i], strlen(pre[i]))) {
        /* some UI/NS-prefixed names are functions, filter them out */
        if (!strncmp(n, "_UIApplicationMain", 18) || !strncmp(n, "_UIGraphics", 11) || !strncmp(n, "_UIImage", 8) || !strncmp(n, "_UIRectFill", 11) || !strncmp(n, "_UIEdgeInsets", 13) || !strncmp(n, "_UIRectFrame", 12) || !strncmp(n, "_NSFileSystemFreeSize", 21)) return 0;
        return 1;
    }
    return 0;
}

gptr resolve_symbol(const char *name) {
    for (int i = 0; i < ncache; i++) if (!strcmp(cache[i].name, name)) return cache[i].addr;
    gptr a;
    if (!strncmp(name, "_OBJC_CLASS_$_", 14)) a = objc_class_object(name + 14);
    else if (!strncmp(name, "_OBJC_METACLASS_$_", 18)) a = objc_class_object(name + 18);   /* treated as same fake class */
    else if (!strcmp(name, "___CFConstantStringClassReference")) a = objc_class_object("__NSCFConstantString");
    else if (!strcmp(name, "__DefaultRuneLocale")) a = runelocale_addr();
    else if (!strcmp(name, "___stdoutp")) a = stdio_slot(1);
    else if (!strcmp(name, "___stderrp")) a = stdio_slot(2);
    else if (!strcmp(name, "___stdinp")) a = stdio_slot(0);
    else if (!strncmp(name, "_kEAGL", 6) || !strcmp(name, "_NSLocalizedDescriptionKey") || !strcmp(name, "_NSUnderlyingErrorKey") ||
             !strncmp(name, "_kCFRunLoop", 11) || !strncmp(name, "_MP", 3) || !strncmp(name, "_NSDefaultRunLoopMode", 21)) {
        a = alloc_fake(16); wr32(a, objc_make_nsstring_global(name + 1));      /* variable holding an NSString* */
    }
    else if (strstr(name, "_S_empty_rep_storage") || !strncmp(name, "__ZTV", 5) || !strncmp(name, "__ZTI", 5) || !strncmp(name, "__ZTS", 5)) a = alloc_fake(64);   /* zeroed rep = empty std::string; vtables/typeinfo blobs */
    else if (is_data_name(name)) a = alloc_fake(256);                           /* vtables, typeinfo, misc: zeroed blob */
    else a = hle_stub_for(name + (name[0] == '_' ? 1 : 0));                     /* function: strip leading underscore */
    cache[ncache].name = strdup(name); cache[ncache].addr = a; ncache++;
    return a;
}
