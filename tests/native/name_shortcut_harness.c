/* Production shortcut-alias replay with deterministic shell/file I/O.
   This is not native ShellLink or Explorer acceptance. */
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <wchar.h>
#include <wctype.h>

typedef int BOOL;
#define TRUE 1
#define FALSE 0
#define MAX_PATH 260
#define INVALID_FILE_ATTRIBUTES ((unsigned long)-1)
static wchar_t present[4][MAX_PATH], saved[4][MAX_PATH];
static int existing, saves, failSave;
static int _wcsicmp(const wchar_t *a, const wchar_t *b) {
    while (*a && *b && towlower(*a) == towlower(*b)) { a++; b++; }
    return (int)towlower(*a) - (int)towlower(*b);
}
static unsigned long GetFileAttributesW(const wchar_t *path) {
    for (int i = 0; i < existing; i++)
        if (!_wcsicmp(path, present[i])) return 0;
    return INVALID_FILE_ATTRIBUTES;
}
static BOOL CreateShortcutLink(const wchar_t *path, const wchar_t *target,
                                const wchar_t *dir, const wchar_t *desc) {
    /* No update may invent a new link or redirect the app's compatibility path. */
    assert(GetFileAttributesW(path) != INVALID_FILE_ATTRIBUTES);
    assert(wcscmp(target, L"C:\\Suite\\WeatherRadar\\WeatherRadar.exe") == 0);
    assert(wcscmp(dir, L"C:\\Suite\\WeatherRadar") == 0);
    assert(wcscmp(desc, L"Weather Report") == 0);
    assert(saves < 4);
    wcscpy(saved[saves++], path);
    return !failSave;
}
#include "name_shortcut_production.h"

static BOOL repoint(const wchar_t *name, const wchar_t *legacy) {
    return RepointAppDesktopShortcutsIfPresent(L"C:\\Desktop", name, legacy,
        L"C:\\Suite\\WeatherRadar\\WeatherRadar.exe", L"C:\\Suite\\WeatherRadar",
        L"Weather Report");
}
static void reset(void) {
    memset(present, 0, sizeof(present));
    memset(saved, 0, sizeof(saved));
    existing = saves = failSave = 0;
}
static void add(const wchar_t *path) { assert(existing < 4); wcscpy(present[existing++], path); }
int main(void) {
    const wchar_t *current = L"C:\\Desktop\\Weather Report.lnk";
    const wchar_t *legacy = L"C:\\Desktop\\Weather Radar.lnk";
    reset();
    assert(!repoint(L"Weather Report", L"Weather Radar") && saves == 0);
    add(legacy);
    assert(repoint(L"Weather Report", L"Weather Radar") && saves == 1);
    assert(wcscmp(saved[0], legacy) == 0 && existing == 1);
    reset(); add(current);
    assert(repoint(L"Weather Report", L"Weather Radar") && saves == 1);
    assert(wcscmp(saved[0], current) == 0 && existing == 1);
    reset(); add(current); add(legacy);
    assert(repoint(L"Weather Report", L"Weather Radar") && saves == 2 && existing == 2);
    assert(wcscmp(saved[0], current) == 0 && wcscmp(saved[1], legacy) == 0);
    saves = 0;
    assert(repoint(L"Weather Report", L"weather report") && saves == 1);
    saves = 0;
    assert(repoint(L"Weather Report", NULL) && saves == 1);
    saves = 0;
    assert(repoint(NULL, L"Weather Radar") && saves == 1);
    saves = 0; failSave = 1;
    assert(!repoint(L"Weather Report", L"Weather Radar") && saves == 2);
    reset(); add(L"C:\\Desktop\\Prism Lighting.lnk"); add(L"C:\\Desktop\\Keyboard Lighting.lnk");
    assert(repoint(L"Prism Lighting", L"Keyboard Lighting") && saves == 2 && existing == 2);
    wchar_t huge[MAX_PATH + 1];
    wmemset(huge, L'x', MAX_PATH); huge[MAX_PATH] = 0;
    saves = 0;
    assert(!repoint(huge, NULL) && saves == 0);
    assert(!repoint(L"", NULL) && saves == 0);
    puts("Public and legacy shortcut aliases: existing-only, deduplicated, bounded, failure-honest");
    return 0;
}
