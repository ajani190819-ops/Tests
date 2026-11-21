/* Executes production Hub catalogue, grouped geometry and Orca routes with
   deterministic Win32 filesystem/profile/UI mocks. Not native Windows proof. */
#include <stdint.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>
#include <wctype.h>

typedef int BOOL;
typedef unsigned int UINT;
typedef unsigned long DWORD;
typedef unsigned char BYTE;
typedef void *HWND;
typedef void *HANDLE;
typedef void *HICON;
typedef struct { int left, top, right, bottom; } RECT;
typedef struct { int x, y; } POINT;
typedef struct { DWORD dwFileAttributes; wchar_t cFileName[260]; } WIN32_FIND_DATAW;
#define TRUE 1
#define FALSE 0
#define MAX_PATH 260
#define CP_UTF8 65001
#define FILE_ATTRIBUTE_DIRECTORY 0x10
#define FILE_ATTRIBUTE_NORMAL 0x80
#define GENERIC_WRITE 1
#define CREATE_NEW 1
#define ERROR_FILE_EXISTS 80
#define ERROR_ALREADY_EXISTS 183
#define SW_SHOWNORMAL 1
#define WM_HUB_REFRESH 42
#define MIRROR_GENERIC 5
#define INVALID_HANDLE_VALUE ((HANDLE)(intptr_t)-1)
#define ZeroMemory(p,n) memset(p,0,n)
#define _snprintf snprintf
#include "hub_groups_types.h"

static const char *scenario;
#define CHECK(c) do { if (!(c)) { fprintf(stderr, "%s C line %d: %s\n", scenario, __LINE__, #c); exit(1); } } while (0)
static ProjectCard g_cards[MAX_APPS];
static HICON g_cardIcons[MAX_APPS];
static char loadedIcons[MAX_APPS][128];
static int failIconLoad;
static int g_cardCount, g_scrollY, g_maxScrollY;
static RECT g_rcEmblem;
static ActionBtn g_topBtns[5];
static HWND g_hwnd = (HWND)(uintptr_t)1;
static wchar_t g_suiteDir[MAX_PATH] = L"C:\\Suite", g_exeDir[MAX_PATH] = L"C:\\Downloads";
static wchar_t g_roamApp[MAX_PATH] = L"C:\\Roam", g_hubCfgDir[MAX_PATH] = L"C:\\Cfg";
static wchar_t g_orcaBatDir[MAX_PATH] = L"C:\\Suite\\OrcaPlugins";
static int dpi = 96, width = 1000, height = 640, enqueued, refreshes, iconLoads;
static wchar_t lastFolder[MAX_PATH], footer[512], failCopy[MAX_PATH], failDelete[MAX_PATH];
static int profiles, findAt, downloads, lastError, stateCreates, failedDownload;
static const wchar_t *profileNames[] = { L"OrcaSlicer-beta", L"OrcaSlicer", L"OrcaSlicer-nightly" };
typedef struct { wchar_t path[MAX_PATH]; char content[2048]; int live, writes; } FakeFile;
static FakeFile files[128];
typedef struct { wchar_t key[64], value[16]; } Cfg;
static Cfg config[8];

static int _snwprintf(wchar_t *out, size_t cap, const wchar_t *format, ...) {
    /* MSVCRT wide printf uses %s=wide, %S=narrow; libc uses %ls/%s. */
    wchar_t translated[1024];
    int n = 0;
    for (int i = 0; format[i]; i++) {
        if (format[i] == L'%' && format[i+1] == L's') {
            translated[n++] = L'%'; translated[n++] = L'l'; translated[n++] = L's'; i++;
        } else if (format[i] == L'%' && format[i+1] == L'S') {
            translated[n++] = L'%'; translated[n++] = L's'; i++;
        } else translated[n++] = format[i];
    }
    translated[n] = 0;
    va_list args; va_start(args, format);
    int result = vswprintf(out, cap, translated, args);
    va_end(args); return result;
}
static int _wcsicmp(const wchar_t *a, const wchar_t *b) {
    while (*a && towlower(*a) == towlower(*b)) { a++; b++; }
    return (int)towlower(*a) - (int)towlower(*b);
}
static int MultiByteToWideChar(int cp, int flags, const char *in, int count, wchar_t *out, int cap) {
    CHECK(cp == CP_UTF8 && !flags && count == -1);
    size_t n = mbstowcs(out, in, (size_t)cap);
    return n == (size_t)-1 || n >= (size_t)cap ? 0 : (int)n + 1;
}
static void SetRect(RECT *r, int l, int t, int right, int b) { *r = (RECT){l,t,right,b}; }
static BOOL PtInRect(const RECT *r, POINT p) { return p.x >= r->left && p.x < r->right && p.y >= r->top && p.y < r->bottom; }
static int GetWinDpi(HWND hwnd) { CHECK(hwnd == g_hwnd); return dpi; }
static int S(int x, int d) { return (x*d + 48)/96; }
static void GetClientRect(HWND hwnd, RECT *r) { CHECK(hwnd == g_hwnd); SetRect(r,0,0,width,height); }
static void BranchCfgPath(wchar_t *out, size_t cap) { CHECK(cap == MAX_PATH); wcscpy(out,L"hub.ini"); }
static UINT GetPrivateProfileStringW(const wchar_t *section, const wchar_t *key, const wchar_t *fallback,
                                    wchar_t *out, int cap, const wchar_t *ini) {
    CHECK(wcscmp(section,L"Apps")==0 && wcscmp(ini,L"hub.ini")==0);
    const wchar_t *v = fallback;
    for (int i=0;i<8;i++) if (wcscmp(config[i].key,key)==0) v=config[i].value;
    wcsncpy(out,v,(size_t)cap-1); out[cap-1]=0; return (UINT)wcslen(out);
}
static BOOL WritePrivateProfileStringW(const wchar_t *section, const wchar_t *key,
                                      const wchar_t *value, const wchar_t *ini) {
    CHECK(wcscmp(section,L"Apps")==0 && wcscmp(ini,L"hub.ini")==0);
    for (int i=0;i<8;i++) if (!config[i].key[0] || wcscmp(config[i].key,key)==0) {
        wcscpy(config[i].key,key); wcscpy(config[i].value,value); return TRUE;
    }
    CHECK(FALSE); return FALSE;
}
static FakeFile *lookup(const wchar_t *path) {
    for (int i=0;i<128;i++) if (files[i].live && wcscmp(files[i].path,path)==0) return &files[i];
    return NULL;
}
static FakeFile *put(const wchar_t *path, const char *content) {
    FakeFile *file=lookup(path);
    if (!file) for (int i=0;i<128;i++) if (!files[i].live) { file=&files[i]; break; }
    CHECK(file); file->live=TRUE; wcscpy(file->path,path); strcpy(file->content,content); file->writes++; return file;
}
static BOOL FileExistsW(const wchar_t *path) { return lookup(path)!=NULL; }
static void EnsureDirTree(const wchar_t *path) { CHECK(path && path[0]); }
static void SuiteAppPath(wchar_t *out, size_t cap, const wchar_t *sub) { _snwprintf(out,cap,L"%s\\%s",g_suiteDir,sub); }
static void AddMirrorDir(int slot, const wchar_t *path) { CHECK(slot==MIRROR_GENERIC && path[0]); }
static void ReplaceCardIcon(ProjectCard *c, HICON icon) { c->hIcon=icon; }
static void LoadCardIcon(ProjectCard *c, const char *icon) {
    CHECK(icon[0]); iconLoads++;
    snprintf(loadedIcons[c-g_cards],sizeof(loadedIcons[0]),"%s",icon);
    if (!failIconLoad) c->hIcon=(HICON)(uintptr_t)(strstr(icon,"undahang-wave-v1.ico") ? 201:202);
}
static void SetFooterStatus(const wchar_t *text, int progress) { CHECK(progress>=0); wcscpy(footer,text); }
static void EnsureProbeRef(wchar_t *out, size_t cap) { CHECK(cap==64); wcscpy(out,L"released-ref"); }
static BOOL DownloadRawRepoFile(const wchar_t *ref, const wchar_t *repo, const wchar_t *target) {
    CHECK(wcscmp(ref,L"released-ref")==0); downloads++;
    if (failedDownload) return FALSE;
    put(target,wcsstr(repo,L".py") ? "PLUGIN_VERSION = \"2.0.0\"\n" : "batch menu"); return TRUE;
}
static BOOL CopyFileW(const wchar_t *src, const wchar_t *dest, BOOL exclusive) {
    CHECK(!exclusive); FakeFile *file=lookup(src); CHECK(file);
    if (failCopy[0] && wcsstr(dest,failCopy)) return FALSE;
    put(dest,file->content); return TRUE;
}
static BOOL DeleteFileW(const wchar_t *path) {
    FakeFile *file=lookup(path);
    if (!file || (failDelete[0] && wcsstr(path,failDelete))) return FALSE;
    file->live=FALSE; return TRUE;
}
static BOOL RemoveDirectoryW(const wchar_t *path) { CHECK(path[0]); return TRUE; }
static char *ReadTextFileUtf8(const wchar_t *path, DWORD *length) {
    FakeFile *f=lookup(path); if (!f) return NULL;
    size_t n=strlen(f->content); char *out=malloc(n+1); CHECK(out); memcpy(out,f->content,n+1);
    if (length) *length=(DWORD)n;
    return out;
}
static void fillProfile(WIN32_FIND_DATAW *data) { data->dwFileAttributes=FILE_ATTRIBUTE_DIRECTORY; wcscpy(data->cFileName,profileNames[findAt]); }
static HANDLE FindFirstFileW(const wchar_t *pattern, WIN32_FIND_DATAW *data) {
    CHECK(wcscmp(pattern,L"C:\\Roam\\OrcaSlicer*")==0); findAt=0;
    if (!profiles) return INVALID_HANDLE_VALUE;
    fillProfile(data); return (HANDLE)(uintptr_t)2;
}
static BOOL FindNextFileW(HANDLE h, WIN32_FIND_DATAW *data) {
    CHECK(h==(HANDLE)(uintptr_t)2); if (++findAt>=profiles) return FALSE; fillProfile(data); return TRUE;
}
static BOOL FindClose(HANDLE h) { CHECK(h==(HANDLE)(uintptr_t)2); return TRUE; }
static HANDLE CreateFileW(const wchar_t *path, DWORD access, DWORD share, void *security,
                          DWORD mode, DWORD attrs, HANDLE template) {
    CHECK(access==GENERIC_WRITE && !share && !security && mode==CREATE_NEW && attrs==FILE_ATTRIBUTE_NORMAL && !template);
    if (lookup(path)) { lastError=ERROR_FILE_EXISTS; return INVALID_HANDLE_VALUE; }
    stateCreates++; return put(path,"");
}
static DWORD GetLastError(void) { return (DWORD)lastError; }
static BOOL WriteFile(HANDLE h, const void *bytes, DWORD len, DWORD *written, void *overlapped) {
    FakeFile *file=h; CHECK(file->live && !overlapped && len<sizeof(file->content));
    memcpy(file->content,bytes,len); file->content[len]=0; *written=len; return TRUE;
}
static BOOL FlushFileBuffers(HANDLE h) { CHECK(h); return TRUE; }
static BOOL CloseHandle(HANDLE h) { CHECK(h); return TRUE; }
static void PostMessageW(HWND hwnd, int message, int wp, int lp) { CHECK(hwnd==g_hwnd && message==WM_HUB_REFRESH && !wp && !lp); }
static void ShellExecuteW(HWND hwnd, const wchar_t *verb, const wchar_t *path,
                          const wchar_t *args, const wchar_t *dir, int show) {
    CHECK(hwnd==g_hwnd && wcscmp(verb,L"open")==0 && !args && !dir && show==SW_SHOWNORMAL); wcscpy(lastFolder,path);
}
static void EnqueueTask(int task) { enqueued=task; }
static void RefreshProjectStatuses(void) { refreshes++; }
#include "hub_groups_production.h"

static void init(void) {
    int widths[]={92,112,108,96,84};
    for (int i=0;i<5;i++) { g_topBtns[i].id=1001+i; g_topBtns[i].widthPx=widths[i]; }
    InitCardDefinitions(); RestoreBuiltInCardText();
    CHECK(g_cardCount==9 && BUILTIN_CARD_COUNT==8);
    CHECK(g_cards[3].category==HUB_GROUP_ORCA && g_cards[7].category==HUB_GROUP_ORCA);
    CHECK(g_cards[2].category==HUB_GROUP_MINECRAFT);
    CHECK(wcscmp(g_cards[3].title,L"Undahang")==0 && wcscmp(g_cards[7].title,L"Unlayered Infill")==0);
    CHECK(!g_cards[3].exeRepo && !g_cards[7].exeRepo);
    CHECK(wcscmp(g_groupTitles[0],L"General apps")==0 && wcscmp(g_groupTitles[1],L"Orca plugins")==0 && wcscmp(g_groupTitles[2],L"Minecraft mods")==0);
}
static void iconsScenario(void) {
    for (size_t i=0;i<sizeof(testCardIconIds)/sizeof(testCardIconIds[0]);i++)
        g_cardIcons[i]=(HICON)(uintptr_t)(1000+testCardIconIds[i]);
    CHECK(g_cards[3].resIconId==RES_ICON_UNDAHANG && g_cards[7].resIconId==RES_ICON_ORCA);
    CHECK(g_cards[3].resIconId!=g_cards[7].resIconId);
    CHECK(HubCardIcon(&g_cards[3],3)==(HICON)(uintptr_t)(1000+RES_ICON_UNDAHANG));
    CHECK(HubCardIcon(&g_cards[7],7)==(HICON)(uintptr_t)(1000+RES_ICON_ORCA));
    CHECK(HubCardIcon(&g_cards[8],8)==(HICON)(uintptr_t)(1000+RES_ICON_WEATHER));
    ProjectCard extra={0}; CHECK(!HubCardIcon(&extra,9));
    const char *old="{\"apps\":[{\"id\":\"orca\",\"icon\":\"docs/brand/orca-plugins.ico\"},"
        "{\"id\":\"unlayered-infill\",\"icon\":\"docs/brand/undahang-wave-v1.ico\"}]}";
    RebuildExtraCards(old);
    CHECK(strcmp(loadedIcons[3],"docs/brand/undahang-wave-v1.ico")==0);
    CHECK(strcmp(loadedIcons[7],"docs/brand/orca-plugins.ico")==0);
    CHECK(HubCardIcon(&g_cards[3],3)==(HICON)(uintptr_t)201);
    CHECK(HubCardIcon(&g_cards[7],7)==(HICON)(uintptr_t)202);
    failIconLoad=1; RebuildExtraCards(old);
    CHECK(HubCardIcon(&g_cards[3],3)==(HICON)(uintptr_t)201);
    g_cards[3].hIcon=NULL; g_cards[7].hIcon=NULL;
    RebuildExtraCards(old);
    CHECK(HubCardIcon(&g_cards[3],3)==(HICON)(uintptr_t)(1000+RES_ICON_UNDAHANG));
    CHECK(HubCardIcon(&g_cards[7],7)==(HICON)(uintptr_t)(1000+RES_ICON_ORCA));
    RebuildExtraCards(NULL);
    CHECK(HubCardIcon(&g_cards[3],3)!=HubCardIcon(&g_cards[7],7));
    g_cardIcons[8]=NULL; CHECK(!HubCardIcon(&g_cards[3],3));
    CHECK(HubCardIcon(&g_cards[7],7)==(HICON)(uintptr_t)(1000+RES_ICON_ORCA));
    CHECK(HubCardIcon(&g_cards[0],0)==(HICON)(uintptr_t)(1000+RES_ICON_BTG));
    puts("Hub independent plugin icon and stale catalogue scenarios passed");
}
static void configScenario(void) {
    WritePrivateProfileStringW(L"Apps",L"OrcaDisabled",L"1",L"hub.ini"); LoadAppsCfg();
    CHECK(g_orcaDisabled && g_infillDisabled);
    CHECK(HandleOrcaPluginCommand(g_hwnd,ID_ORCA_DISABLE));
    CHECK(!g_orcaDisabled && g_infillDisabled); LoadAppsCfg(); CHECK(!g_orcaDisabled && g_infillDisabled);
    CHECK(HandleOrcaPluginCommand(g_hwnd,ID_INFILL_DISABLE)); LoadAppsCfg(); CHECK(!g_orcaDisabled && !g_infillDisabled);
    CHECK(HandleOrcaPluginCommand(g_hwnd,ID_ORCA_UPDATE) && enqueued==TASK_INSTALL_ORCA_WAVE);
    CHECK(HandleOrcaPluginCommand(g_hwnd,ID_INFILL_UPDATE) && enqueued==TASK_INSTALL_ORCA_INFILL);
    CHECK(!HandleOrcaPluginCommand(g_hwnd,ID_BTG_OPEN));
    char buf[32]; CHECK(JsonGet("{\"id\":\"x\"}","id",buf,sizeof(buf)) && strcmp(buf,"x")==0);
    CHECK(BuiltInSlot("orca")==3 && BuiltInSlot("unlayered-infill")==7);
    CHECK(BuiltInBtnId(3,"remove")==ID_ORCA_REMOVE && BuiltInBtnId(7,"remove")==ID_INFILL_REMOVE);
    CHECK(BuiltInBtnId(7,"disable")==ID_INFILL_DISABLE);
    puts("Hub independent configuration and routing passed");
}
static void filesScenario(void) {
    profiles=2;
    const wchar_t *wave=L"C:\\Roam\\OrcaSlicer-beta\\orca_plugins\\wave-overhangs\\wave_overhangs_orca.py";
    const wchar_t *infill=L"C:\\Roam\\OrcaSlicer-beta\\orca_plugins\\unlayered-infill\\unlayered_infill_orca.py";
    const wchar_t *state=L"C:\\Roam\\OrcaSlicer-beta\\orca_plugins\\wave-overhangs\\.install_state.json";
    const wchar_t *settings=L"C:\\Roam\\OrcaSlicer-beta\\orca_plugins\\wave-overhangs\\preferences.json";
    put(infill,"PLUGIN_VERSION = \"7.0.0\"\n"); put(state,"{\"enabled\": false, \"saved\": 1}"); put(settings,"user settings");
    WorkerInstallOrcaPlugin(0);
    CHECK(wcsstr(footer,L"updated"));
    CHECK(strstr(lookup(wave)->content,"2.0.0")); CHECK(strstr(lookup(infill)->content,"7.0.0")); CHECK(lookup(infill)->writes==1);
    CHECK(strcmp(lookup(state)->content,"{\"enabled\": false, \"saved\": 1}")==0 && lookup(state)->writes==1);
    CHECK(stateCreates==1); CHECK(strstr(lookup(L"C:\\Roam\\OrcaSlicer\\plugins\\wave_overhangs_orca.py")->content,"2.0.0"));
    int before=downloads; g_infillDisabled=1; WorkerInstallOrcaPlugin(1); CHECK(downloads==before && lookup(infill)->writes==1);
    g_infillDisabled=0; wcscpy(failCopy,L"OrcaSlicer\\plugins"); WorkerInstallOrcaPlugin(1);
    CHECK(wcsstr(footer,L"not fully written")); CHECK(strstr(lookup(wave)->content,"2.0.0"));
    failCopy[0]=0; WorkerInstallOrcaPlugin(1); CHECK(wcsstr(footer,L"updated"));
    wcscpy(failDelete,L"wave_overhangs_orca.py"); RemoveOrcaPlugin(0); CHECK(wcsstr(footer,L"not be fully removed") && lookup(wave));
    CHECK(lookup(state) && strstr(lookup(state)->content,"false"));
    failDelete[0]=0; CHECK(HandleOrcaPluginCommand(g_hwnd,ID_ORCA_REMOVE));
    CHECK(!lookup(wave) && lookup(infill) && lookup(settings));
    CHECK(!lookup(L"C:\\Roam\\OrcaSlicer\\plugins\\wave_overhangs_orca.py"));
    RemoveOrcaPlugin(0); CHECK(wcsstr(footer,L"nothing to remove"));
    CHECK(HandleOrcaPluginCommand(g_hwnd,ID_INFILL_FOLDER)); CHECK(wcsstr(lastFolder,L"unlayered-infill"));
    memset(files,0,sizeof(files)); profiles=0; WorkerInstallOrcaPlugin(1);
    CHECK(lookup(L"C:\\Roam\\OrcaSlicer\\orca_plugins\\unlayered-infill\\unlayered_infill_orca.py"));
    CHECK(!lookup(L"C:\\Roam\\OrcaSlicer\\orca_plugins\\wave-overhangs\\wave_overhangs_orca.py"));
    failedDownload=1; before=downloads; WorkerInstallOrcaPlugin(0); CHECK(downloads==before+1 && wcsstr(footer,L"no installed plugin"));
    puts("Hub independent plugin filesystem scenarios passed");
}
static void statusScenario(void) {
    profiles=1;
    const wchar_t *wave=L"C:\\Roam\\OrcaSlicer-beta\\orca_plugins\\wave-overhangs\\wave_overhangs_orca.py";
    const wchar_t *legacy=L"C:\\Roam\\OrcaSlicer-beta\\plugins\\wave_overhangs_orca.py";
    const wchar_t *infill=L"C:\\Roam\\OrcaSlicer-beta\\plugins\\unlayered_infill_orca.py";
    put(wave,"PLUGIN_VERSION = \"1.0.0\"\n");
    RefreshOrcaCard(0,"2.0.0"); RefreshOrcaCard(1,"2.0.0");
    CHECK(!g_cards[3].btns[0].hidden && wcscmp(g_cards[3].btns[0].label,L"Update")==0);
    CHECK(!g_cards[7].btns[0].hidden && wcscmp(g_cards[7].btns[0].label,L"Install")==0);
    put(wave,"PLUGIN_VERSION = \"2.0.0\"\n"); put(infill,"PLUGIN_VERSION = \"2.0.0\"\n");
    RefreshOrcaCard(0,"2.0.0"); RefreshOrcaCard(1,"2.0.0"); CHECK(g_cards[3].btns[0].hidden && g_cards[7].btns[0].hidden);
    CHECK(wcscmp(g_cards[7].badge,L"Up to Date")==0);
    profiles=2;
    const wchar_t *other=L"C:\\Roam\\OrcaSlicer\\plugins\\wave_overhangs_orca.py";
    put(other,"PLUGIN_VERSION = \"1.0.0\"\n"); RefreshOrcaCard(0,"2.0.0");
    CHECK(!g_cards[3].btns[0].hidden);
    DeleteFileW(other); profiles=1;
    RefreshOrcaCard(0,""); CHECK(g_cards[3].btns[0].hidden && wcscmp(g_cards[3].badge,L"Installed")==0);
    put(legacy,"PLUGIN_VERSION = \"1.0.0\"\n"); RefreshOrcaCard(0,"2.0.0");
    CHECK(!g_cards[3].btns[0].hidden && wcsstr(g_cards[3].detail,L"Mixed"));
    g_orcaDisabled=1; RefreshOrcaCard(0,"2.0.0"); CHECK(g_cards[3].btns[0].hidden && wcscmp(g_cards[3].btns[3].label,L"Enable")==0);
    CHECK(wcscmp(g_cards[7].btns[3].label,L"Disable")==0); g_orcaDisabled=0;
    put(wave,"unreadable version"); DeleteFileW(legacy); RefreshOrcaCard(0,"2.0.0");
    CHECK(g_cards[3].btns[0].hidden && wcscmp(g_cards[3].badge,L"Version unknown")==0);
    put(wave,"PLUGIN_VERSION = \"3.0.0\"\n"); RefreshOrcaCard(0,"2.0.0");
    CHECK(g_cards[3].btns[0].hidden && wcscmp(g_cards[3].badge,L"Newer installed")==0);
    puts("Hub independent plugin status scenarios passed");
}
static void catalogScenario(void) {
    g_orcaDisabled=1; RefreshOrcaCard(0,"");
    const char *json="{\"apps\":[{\"id\":\"orca\",\"title\":\"OrcaSlicer Plugins\",\"buttons\":\"branch:Both:74\"},"
        "{\"id\":\"addon\",\"exe\":\"x/Addon.exe\",\"title\":\"A } quoted \\\" label\",\"blurb\":\"owned descriptor\"},"
        "{\"id\":\"addon\",\"exe\":\"x/Other.exe\"},"
        "{\"id\":\"unlayered-infill\",\"title\":\"Combined?\"}]}";
    for (int i=0;i<100;i++) {
        RebuildExtraCards(i%2 ? "{\"apps\":[{\"id\":\"incomplete" : json);
        CHECK(g_cardCount==10 && HasWeatherRadarCatalogCard());
        CHECK(wcscmp(g_cards[3].title,L"Undahang")==0 && wcscmp(g_cards[7].title,L"Unlayered Infill")==0);
        CHECK(g_cards[3].btns[0].id==ID_ORCA_UPDATE && g_cards[7].btns[0].id==ID_INFILL_UPDATE);
        CHECK(g_cards[3].btns[0].hidden && wcscmp(g_cards[3].btns[3].label,L"Enable")==0);
        int addon=0;
        for (int c=BUILTIN_CARD_COUNT;c<g_cardCount;c++) if (wcscmp(g_cards[c].id,L"addon")==0) {
            addon++; CHECK(wcscmp(g_cards[c].blurb,L"owned descriptor")==0); CHECK(wcscmp(g_cards[c].exeName,L"Addon.exe")==0);
        }
        CHECK(addon==1 && g_strPoolUsed<12288);
    }
    RebuildExtraCards(NULL); CHECK(g_cardCount==10); CHECK(iconLoads>0);
    AddCatalogCard("{\"id\":\"addon\",\"exe\":\"x/Duplicate.exe\"}",FALSE); CHECK(g_cardCount==10);
    RebuildExtraCards("{\"apps\":[{\"id\":\"new1\",\"exe\":\"x/1.exe\"},"
        "{\"id\":\"new2\",\"exe\":\"x/2.exe\"},{\"id\":\"new3\",\"exe\":\"x/3.exe\"}]}");
    CHECK(g_cardCount==MAX_APPS && wcscmp(g_cards[8].id,L"weatherradar")==0);
    CHECK(wcscmp(g_cards[9].id,L"addon")==0 && wcscmp(g_cards[9].exeName,L"Addon.exe")==0);
    RebuildExtraCards("{\"apps\":[]}"); CHECK(g_cardCount==MAX_APPS && wcscmp(g_cards[9].id,L"addon")==0);
    puts("Hub catalogue fallback and duplicate scenarios passed");
}
static void layoutScenario(void) {
    const int dpis[]={96,120,144,192};
    const int sizes[][2]={{700,420},{920,640},{1280,1250}};
    for (int d=0;d<4;d++) for (int sz=0;sz<3;sz++) {
        dpi=dpis[d]; width=S(sizes[sz][0],dpi); height=S(sizes[sz][1],dpi);
        for (int hidden=0;hidden<2;hidden++) {
            g_topBtns[1].hidden=hidden; g_cards[3].btns[0].hidden=hidden;
            g_scrollY=0; ComputeLayout(g_hwnd);
            int header=HubHeaderHeight(width,dpi), foot=height-S(42,dpi);
            CHECK(header==S(sizes[sz][0]<920 ? 124:76,dpi));
            CHECK(g_groupRects[0].top==header+S(12,dpi) || !g_maxScrollY);
            CHECK(g_cards[8].rcCard.bottom<g_groupRects[1].top);
            CHECK(g_cards[3].rcCard.bottom<g_cards[7].rcCard.top && g_cards[7].rcCard.bottom<g_groupRects[2].top);
            CHECK(g_cards[2].rcCard.top>g_groupRects[2].bottom);
            for (int t=0;t<5;t++) if (!g_topBtns[t].hidden) {
                RECT r=g_topBtns[t].rc;
                CHECK(r.left>=S(28,dpi) && r.right<=width-S(28,dpi) && r.bottom<header);
                CHECK(HitTestButtons((POINT){r.left+1,r.top+1},header,foot)==g_topBtns[t].id);
            }
            if (header>S(76,dpi)) CHECK(g_topBtns[0].rc.top>g_rcEmblem.bottom);
            int seen[2]={0};
            for (int offset=0;offset<=g_maxScrollY+S(36,dpi);offset+=S(36,dpi)) {
                g_scrollY=offset; ComputeLayout(g_hwnd);
                for (int plugin=0;plugin<2;plugin++) {
                    ProjectCard *card=&g_cards[plugin ? 7:3];
                    RECT r=card->btns[1].rc;
                    POINT point={r.left+1,r.top+1};
                    int hit=HitTestButtons(point,header,foot);
                    if (point.y>header && point.y<foot-1) { CHECK(hit==card->btns[1].id); seen[plugin]=1; }
                    else if (point.y>header) CHECK(hit!=card->btns[1].id);
                    CHECK(card->btns[3].rc.right<=card->rcCard.right-S(16,dpi));
                }
            }
            CHECK(seen[0] && seen[1]);
            CHECK(g_cards[2].rcCard.bottom<=foot-S(12,dpi));
            CHECK(HitTestButtons((POINT){S(400,dpi),foot},header,foot)==0);
            if (hidden) CHECK(g_cards[3].btns[0].rc.right==0);
            g_scrollY=-100; ComputeLayout(g_hwnd); CHECK(g_scrollY==0);
        }
    }
    puts("Hub grouped DPI geometry and clipped routing passed");
}
int main(int argc, char **argv) {
    CHECK(argc==2); scenario=argv[1]; init();
    if (strcmp(scenario,"icons")==0) iconsScenario();
    else if (strcmp(scenario,"config")==0) configScenario();
    else if (strcmp(scenario,"files")==0) filesScenario();
    else if (strcmp(scenario,"status")==0) statusScenario();
    else if (strcmp(scenario,"catalog")==0) catalogScenario();
    else if (strcmp(scenario,"layout")==0) layoutScenario();
    else CHECK(FALSE);
    return 0;
}
