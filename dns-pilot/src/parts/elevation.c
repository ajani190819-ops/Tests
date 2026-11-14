/* ---- elevation ---- */
static int IsElevated(void) {
    HANDLE tok = NULL;
    TOKEN_ELEVATION te;
    DWORD sz = 0;
    int elev = 0;
    if (OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &tok)) {
        if (GetTokenInformation(tok, TokenElevation, &te, sizeof(te), &sz))
            elev = te.TokenIsElevated ? 1 : 0;
        CloseHandle(tok);
    }
    return elev;
}

static void RelaunchElevated(const wchar_t *args) {
    wchar_t path[MAX_PATH];
    GetModuleFileNameW(NULL, path, MAX_PATH);
    ShellExecuteW(NULL, L"runas", path, args, NULL, SW_SHOWNORMAL);
}

/* Background suite apps start with Windows - quietly in the tray, like
   Bluetooth Guard and Keyboard Lighting. Re-written every launch so an
   updated install path stays correct. */
/* Autostart at logon through a highest-privilege scheduled task so the
   pilot comes up in the tray already elevated - a Run-key entry would trip
   a UAC prompt at every boot now that the exe requires administrator. */
static void EnsureAutostart(void) {
    wchar_t exe[MAX_PATH];
    if (!GetModuleFileNameW(NULL, exe, MAX_PATH)) return;
    HKEY k = NULL;
    if (RegCreateKeyExW(HKEY_CURRENT_USER,
                        L"Software\\Microsoft\\Windows\\CurrentVersion\\Run",
                        0, NULL, 0, KEY_SET_VALUE, NULL, &k, NULL) == ERROR_SUCCESS) {
        RegDeleteValueW(k, L"TestsSuite-DnsPilot");   /* migrate away from the old Run entry */
        RegCloseKey(k);
    }
    wchar_t args[MAX_PATH * 2];
    _snwprintf(args, MAX_PATH * 2 - 1,
               L"/Create /F /SC ONLOGON /TN \"TestsSuite-DnsPilot\" "
               L"/TR \"\\\"%s\\\" -Silent\" /RL HIGHEST", exe);
    args[MAX_PATH * 2 - 1] = L'\0';
    ShellExecuteW(NULL, L"open", L"schtasks.exe", args, NULL, SW_HIDE);
}

