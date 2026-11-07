// phyprobe.c: dump the APPLE80211_IOC_PHY_MODE(0x0E) reply (PHYMODE_ACTIVE/SUPPORTED)
// that feeds System Profiler's "PHY Mode". Read-only, user-level, dlopen private IO80211.
#include <CoreFoundation/CoreFoundation.h>
#include <dlfcn.h>
#include <stdio.h>
typedef struct __Apple80211 *Apple80211Ref;
typedef int (*open_t)(Apple80211Ref*);
typedef int (*bind_t)(Apple80211Ref, CFStringRef);
typedef int (*iflist_t)(Apple80211Ref, CFArrayRef*);
typedef int (*copyv_t)(Apple80211Ref, int, int, void*, int);
int main(void) {
    void *h = dlopen("/System/Library/PrivateFrameworks/IO80211.framework/Versions/A/IO80211", RTLD_NOW);
    if (!h) { printf("dlopen fail: %s\n", dlerror()); return 1; }
    open_t Open=(open_t)dlsym(h,"Apple80211Open"); bind_t Bind=(bind_t)dlsym(h,"Apple80211BindToInterface");
    iflist_t Lst=(iflist_t)dlsym(h,"Apple80211GetIfListCopy"); copyv_t Copy=(copyv_t)dlsym(h,"Apple80211CopyValue");
    Apple80211Ref ref=NULL;
    if (Open(&ref)) { printf("open failed\n"); return 1; }
    CFArrayRef ifs=NULL; int r=Lst(ref,&ifs);
    CFStringRef target=NULL;
    if (r==0 && ifs && CFArrayGetCount(ifs)>0) target=CFArrayGetValueAtIndex(ifs,0);
    if (!target) { target=CFStringCreateWithCString(NULL,"en1",kCFStringEncodingUTF8); }
    char buf[64]={0}; CFStringGetCString(target,buf,sizeof(buf),kCFStringEncodingUTF8);
    Bind(ref,target);
    CFDictionaryRef d=NULL;
    int rc=Copy(ref,0x0e,0,&d,8);
    printf("if=%s key0x0E rc=%d\n", buf, rc);
    if (d) {
        CFNumberRef act=CFDictionaryGetValue(d,CFSTR("PHYMODE_ACTIVE"));
        CFNumberRef sup=CFDictionaryGetValue(d,CFSTR("PHYMODE_SUPPORTED"));
        int av=0,sv=0;
        if (act) CFNumberGetValue(act,kCFNumberIntType,&av);
        if (sup) CFNumberGetValue(sup,kCFNumberIntType,&sv);
        printf("PHYMODE_ACTIVE = %d (0x%x)\nPHYMODE_SUPPORTED = %d (0x%x)\n", av,av,sv,sv);
        printf("ax active: %s  | ax supported: %s\n",
               (av&0x100)?"YES":"NO", (sv&0x100)?"YES":"NO");
        CFRelease(d);
    } else printf("no dict (rc=%d)\n", rc);
    return 0;
}
