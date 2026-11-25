/* Production panel persistence/master adapter, with an in-memory filesystem.
   No host profiles, owner data, Win32 input or lighting hardware are accessed. */
#include <stdint.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <wchar.h>
#include <math.h>
#include <errno.h>
#include <limits.h>
typedef int BOOL;
typedef uint32_t DWORD, COLORREF;
typedef unsigned UINT;
typedef uint64_t ULONGLONG;
typedef void *HWND;
typedef struct { int left,top,right,bottom; } RECT;
enum { HIT_SLIDER_MASTER=4,HIT_WIDTH_0=20,HIT_SLIDER_SPEED=32,HIT_SLIDER_BRIGHT=33 };
static int g_tab;
#define TRUE 1
#define FALSE 0
#define MAX_PATH 260
#define MOD_CONTROL 2
#define MOD_ALT 1
#define RGB(r,g,b) ((COLORREF)((r) | ((g)<<8) | ((b)<<16)))
#define GetRValue(c) ((unsigned)((c)&255))
#define GetGValue(c) ((unsigned)(((c)>>8)&255))
#define GetBValue(c) ((unsigned)(((c)>>16)&255))
#define ZeroMemory(p,n) memset(p,0,n)
#define _stricmp strcasecmp
#define _snwprintf swprintf
#define _snprintf snprintf
static const char *scenario;
#define CHECK(c) do { if (!(c)) { fprintf(stderr,"%s C line %d: %s\n",scenario,__LINE__,#c); exit(1); } } while (0)
typedef struct {
    BOOL on; int effectIdx, speed, brightness; BOOL mirror, reverse, equalise, loop;
    int swatchCount; COLORREF swatches[8]; double widths[8];
} GroupCfg;
typedef struct {
    int masterBrightness; BOOL overlay, link, smooth; char onExit[16];
    COLORREF customColors[16]; GroupCfg kbd, bar;
} PanelCfg;
static const struct { const char *label, *token; } kEffects[] = {
    {"Scrolling gradient", "gradient"}, {"Rainbow", "rainbow"}, {"Solid", "static"}
};
#define EFFECT_COUNT 3
static PanelCfg g_cfg;
static HWND g_hwnd;
static wchar_t g_toastText[96], g_statusText[256];
static ULONGLONG tick=1000, g_toastAt, g_lastFnPoll, g_applyAtTick;
static BOOL g_applyPending, g_wantOff;
static const wchar_t *g_panelPath=L"panel", *g_livePath=L"live", *g_themePath=L"theme";
static char panel[32768], live[64], theme[8192];
static BOOL panelExists, liveExists, writeOk=TRUE;
static int panelWrites, liveWrites, themeWrites, osdShows;
static char *ReadFileUtf8(const wchar_t *p,DWORD *len) {
    const char *s=p==g_panelPath?panel:p==g_livePath?live:theme;
    if ((p==g_panelPath&&!panelExists)||(p==g_livePath&&!liveExists)) return NULL;
    if(len) *len=(DWORD)strlen(s);
    char *r=malloc(strlen(s)+1); CHECK(r); strcpy(r,s); return r;
}
static BOOL WriteAtomicUtf8(const wchar_t *p,const char *s) {
    if(p==g_panelPath) panelWrites++;
    else if(p==g_livePath) liveWrites++;
    else if(p==g_themePath) themeWrites++;
    else CHECK(FALSE);
    if(!writeOk) return FALSE;
    if(p==g_panelPath) { CHECK(strlen(s)<sizeof(panel)); strcpy(panel,s); panelExists=TRUE; }
    if(p==g_livePath) { CHECK(strlen(s)<sizeof(live)); strcpy(live,s); liveExists=TRUE; }
    if(p==g_themePath) { CHECK(strlen(s)<sizeof(theme)); strcpy(theme,s); }
    return TRUE;
}
static BOOL FileExistsW(const wchar_t *p) { return p==g_panelPath?panelExists:liveExists; }
static ULONGLONG GetTickCount64(void) { return tick; }
static void PresetPath(wchar_t *p) { p[0]=0; }
static UINT GetPrivateProfileIntW(const wchar_t *s,const wchar_t *k,UINT d,const wchar_t *p) {
    (void)s;(void)k;(void)p;return d;
}
static BOOL RegisterHotKey(HWND h,int id,UINT mod,UINT key) { (void)h;(void)id;(void)mod;(void)key;return TRUE; }
static void InvalidateRect(HWND h,const void *r,BOOL e) { (void)h;(void)r;(void)e; }
static void OsdShow(int pct) { CHECK(pct>=0&&pct<=100); osdShows++; }
static BOOL IsEngineAlive(void) { return TRUE; }
static void StartEngine(void) { CHECK(FALSE); }
static GroupCfg *CurGroup(void) { return g_tab?&g_cfg.bar:&g_cfg.kbd; }
static void SyncLinkedZone(void);
static void SavePanelCfg(void);
static void ScheduleLiveApply(void);
static void WriteThemeJson(void);
#include "lighting_state_subject.h"
static void Fixture(int smooth,int kbd,int bar,int reverseOrder) {
    snprintf(panel,sizeof(panel),
        "{\"metadata\":{\"Smooth\":true,\"Equalise\":true,\"text\":\"} Kbd \\\" Bar\"},"
        "%s\"Bar\":{\"On\":false,\"Equalise\":%s,\"Brightness\":23,\"keepBar\":7},"
        "\"Kbd\":{\"On\":true,\"Equalise\":%s,\"Brightness\":77,\"Speed\":13,\"keepKbd\":[1,2]},"
        "\"Brightness\":50,\"Overlay\":false,\"Link\":false,\"OnExit\":\"white\","
        "\"Custom\":[16777215,123,16777215,456],%s\"keepRoot\":{\"a\":[false,1.5,null]}}",
        reverseOrder?"":"\"Smooth\":true,",bar?"true":"false",kbd?"true":"false",
        reverseOrder?(smooth?"\"Smooth\":true,":"\"Smooth\":false,"):"\"Smooth\":false,");
    panelExists=TRUE; liveExists=TRUE; strcpy(live,"500\r\n");
    panelWrites=liveWrites=themeWrites=osdShows=0; writeOk=TRUE;
    g_lastFnPoll=0;g_applyPending=FALSE;g_wantOff=FALSE;tick+=1000;
    LoadPanelCfg();
    g_brightnessChannelReady=TRUE; /* fixture represents an already-running engine */
}
static void TestRestoration(void) {
    for(int sm=0;sm<2;sm++) for(int k=0;k<2;k++) for(int b=0;b<2;b++) {
        Fixture(sm,k,b,TRUE);
        CHECK(g_cfg.smooth==sm && g_cfg.kbd.equalise==k && g_cfg.bar.equalise==b);
        CHECK(g_cfg.masterBrightness==50 && g_cfg.kbd.brightness==77 && g_cfg.bar.brightness==23);
        CHECK(!g_cfg.overlay && !g_cfg.link && !strcmp(g_cfg.onExit,"white"));
        g_cfg.kbd.speed=19; SavePanelCfg();
        CHECK(panelWrites==1);
        LoadPanelCfg(); WriteThemeJson();
        CHECK(g_cfg.smooth==sm && g_cfg.kbd.equalise==k && g_cfg.bar.equalise==b);
        CHECK(g_cfg.kbd.speed==19 && g_cfg.kbd.on && !g_cfg.bar.on);
        char want[32]; snprintf(want,sizeof(want),"\"Smooth\": %s",sm?"true":"false");
        CHECK(strstr(theme,want));
    }
}
static void TestScopeAndLegacy(void) {
    strcpy(panel,"{\"Kbd\":{\"metadata\":{\"Equalise\":true},\"Equalise\":false},\"Bar\":{\"Equalise\":true},\"Smooth\":false}");
    panelExists=TRUE;LoadPanelCfg();
    CHECK(!g_cfg.kbd.equalise && g_cfg.bar.equalise && !g_cfg.smooth);
    strcpy(panel,"{\"Smooth\":false,\"Equalise\":false,\"Mirror\":true,\"Effect\":\"Rainbow\",\"Brightness\":42}");
    LoadPanelCfg();CHECK(!g_cfg.smooth && !g_cfg.kbd.equalise && g_cfg.kbd.mirror);
    CHECK(g_cfg.bar.equalise && g_cfg.masterBrightness==42 && g_cfg.kbd.brightness==100);
    SavePanelCfg();LoadPanelCfg();CHECK(!g_cfg.kbd.equalise && g_cfg.bar.equalise);
}
static void TestExternalMerge(void) {
    Fixture(TRUE,TRUE,FALSE,TRUE);
    /* Hub changes both preferences after the native panel loaded them. */
    strcpy(panel,"{\"Smooth\":false,\"Brightness\":50,\"Kbd\":{\"Equalise\":false,\"Speed\":13},\"Bar\":{\"Equalise\":true,\"Brightness\":23},\"external\":99}");
    g_cfg.kbd.speed=24;SavePanelCfg();LoadPanelCfg();
    CHECK(!g_cfg.smooth && !g_cfg.kbd.equalise && g_cfg.bar.equalise);
    CHECK(g_cfg.kbd.speed==24 && g_cfg.bar.brightness==23);
    CHECK(strstr(panel,"\"external\":99"));
}
static void TestUnknownAndCustom(void) {
    Fixture(FALSE,FALSE,TRUE,TRUE);
    CHECK(g_cfg.customColors[0]==16777215 && g_cfg.customColors[1]==123 &&
          g_cfg.customColors[2]==16777215 && g_cfg.customColors[3]==456);
    g_cfg.kbd.speed++;SavePanelCfg();
    CHECK(strstr(panel,"\"keepRoot\":{\"a\":[false,1.5,null]}"));
    CHECK(strstr(panel,"\"keepKbd\":[1,2]") && strstr(panel,"\"keepBar\":7"));
    LoadPanelCfg();CHECK(g_cfg.customColors[1]==123 && g_cfg.customColors[3]==456);
}
static void TestHotkeyRoundTrip(void) {
    for(int dir=-1;dir<=1;dir+=2) {
        Fixture(TRUE,TRUE,FALSE,TRUE);
        StepMasterBrightness(dir);
        tick+=45;PollFnBrightness();FlushLiveApply();
        CHECK(atoi(live)==500+dir*100);
        CHECK(g_cfg.masterBrightness==50+dir*10);
        FlushPendingBrightnessCfg();LoadPanelCfg();
        CHECK(g_cfg.masterBrightness==50+dir*10);
    }
}
static void TestMalformedRefusal(void) {
    Fixture(FALSE,FALSE,TRUE,TRUE);
    strcpy(panel,"{\"Smooth\":false,\"Kbd\":");
    char before[128];strcpy(before,panel);g_cfg.kbd.speed=29;SavePanelCfg();
    CHECK(!strcmp(panel,before));
    CHECK(!g_cfg.smooth && !g_cfg.kbd.equalise && g_cfg.kbd.speed==29);
}
static void TestExactAndSlider(void) {
    Fixture(FALSE,FALSE,TRUE,TRUE);
    SetMasterBrightnessLevel(565);SavePanelCfg();LoadPanelCfg();
    CHECK(g_brightness.level==565&&g_cfg.masterBrightness==57);
    WriteLiveBrightness();CHECK(atoi(live)==565);
    RECT track={0,0,100,10};
    BrightnessPress(&g_brightness,1,1,tick);
    ApplySliderDrag(HIT_SLIDER_MASTER,0,track,25);
    CHECK(g_brightness.level==250&&atoi(live)==250&&!g_brightness.active);
    ApplySliderDrag(HIT_SLIDER_BRIGHT,0,track,63);FlushLiveApply();
    CHECK(g_cfg.kbd.brightness==63&&g_cfg.bar.brightness==23);
    CHECK(g_brightness.level==250&&atoi(live)==250&&!g_cfg.kbd.equalise&&g_cfg.bar.equalise&&!g_cfg.smooth);
    strcpy(live,"565");tick+=45;PollFnBrightness();
    int writes=liveWrites;g_cfg.kbd.speed++;FlushLiveApply();
    CHECK(atoi(live)==565&&g_brightness.level==565&&liveWrites==writes);
    SavePanelCfg();LoadPanelCfg();CHECK(g_brightness.level==565);
    /* A legacy percent-only write invalidates an obsolete exact projection. */
    strcpy(panel,"{\"Brightness\":42,\"MasterLevel\":565}");LoadPanelCfg();
    CHECK(g_brightness.level==420);
}
static void TestStaleStartup(void) {
    Fixture(FALSE,FALSE,TRUE,TRUE);
    strcpy(panel,"{\"Brightness\":42,\"Smooth\":false,\"Kbd\":{\"Equalise\":false}}");
    strcpy(live,"990");LoadPanelCfg();SyncMasterBrightness();
    CHECK(g_brightness.level==420&&!g_cfg.smooth&&!g_cfg.kbd.equalise);
    WriteLiveBrightness();CHECK(atoi(live)==420&&g_brightnessChannelReady);
    strcpy(live,"565");tick+=35;PollFnBrightness();CHECK(g_brightness.level==565);
    FlushPendingBrightnessCfg(); /* normal restart/quit flush */
    SavePanelCfg();LoadPanelCfg();WriteLiveBrightness();
    CHECK(g_brightness.level==565&&atoi(live)==565);
}
static void TestFailedSaveRetry(void) {
    Fixture(FALSE,FALSE,TRUE,TRUE);
    SetMasterBrightnessLevel(565);g_cfg.kbd.speed=24;
    char before[32768];strcpy(before,panel);
    writeOk=FALSE;SavePanelCfg();CHECK(g_panelSaveFailed&&!strcmp(before,panel));
    writeOk=TRUE;tick+=400;FlushPendingBrightnessCfg();
    CHECK(!g_panelSaveFailed);LoadPanelCfg();
    CHECK(g_brightness.level==565&&g_cfg.kbd.speed==24&&!g_cfg.smooth&&!g_cfg.kbd.equalise&&g_cfg.bar.equalise);
}
int main(int argc,char **argv) {
    CHECK(argc==2);scenario=argv[1];
    if(!strcmp(scenario,"restore")) TestRestoration();
    else if(!strcmp(scenario,"scope-legacy")) TestScopeAndLegacy();
    else if(!strcmp(scenario,"external")) TestExternalMerge();
    else if(!strcmp(scenario,"unknown-custom")) TestUnknownAndCustom();
    else if(!strcmp(scenario,"hotkey")) TestHotkeyRoundTrip();
    else if(!strcmp(scenario,"malformed")) TestMalformedRefusal();
    else if(!strcmp(scenario,"exact-slider")) TestExactAndSlider();
    else if(!strcmp(scenario,"stale-startup")) TestStaleStartup();
    else if(!strcmp(scenario,"failed-save")) TestFailedSaveRetry();
    else CHECK(FALSE);
    printf("Lighting state %s passed\n",scenario);return 0;
}
