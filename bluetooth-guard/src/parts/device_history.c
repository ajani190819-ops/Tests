/* TRUE when a registry subkey name is a Bluetooth service class GUID:
   {8-4-4-4-12} hex (38 chars, braces, hyphens at 9/14/19/24). */
static BOOL IsServiceGuidSubkey(const wchar_t *n) {
    if (!n || wcslen(n) != 38) return FALSE;
    if (n[0] != L'{' || n[37] != L'}') return FALSE;
    if (n[9] != L'-' || n[14] != L'-' || n[19] != L'-' || n[24] != L'-') return FALSE;
    for (int i = 1; i < 37; i++) {
        if (i == 9 || i == 14 || i == 19 || i == 24) continue;
        wchar_t c = n[i];
        if (!((c >= L'0' && c <= L'9') || (c >= L'a' && c <= L'f') || (c >= L'A' && c <= L'F')))
            return FALSE;
    }
    return TRUE;
}

/* Has this device ever carried a profile connection on this machine?
   BTHPORT keeps one subkey per service under the device key, and those
   appear the first time a profile actually connects. A paired-but-never-
   used entry (Swift Pair bait, stale cache rows) has the key but no
   services - exactly the devices that must never be listed here. When the
   registry cannot answer, err on showing the device. */
static BOOL HasConnectedBefore(ULONGLONG addr) {
    wchar_t folder[16], path[160];
    for (int i = 0; i < 6; i++) {
        unsigned v = (unsigned)((addr >> (8 * i)) & 0xFF);
        folder[i * 2]     = (wchar_t)(L"0123456789ABCDEF"[(v >> 4) & 15]);
        folder[i * 2 + 1] = (wchar_t)(L"0123456789ABCDEF"[v & 15]);
    }
    folder[12] = L'\0';
    _snwprintf(path, 160, L"SYSTEM\\CurrentControlSet\\Services\\BTHPORT\\Parameters\\Devices\\%s", folder);
    path[159] = L'\0';
    HKEY hKey = NULL;
    /* Key missing = never connected = STRANGER, never show it */
    if (RegOpenKeyExW(HKEY_LOCAL_MACHINE, path, 0, KEY_READ, &hKey) != ERROR_SUCCESS)
        return FALSE;
    /* A real profile connection leaves a subkey named after the service class
       GUID. A paired-but-never-used row can carry the device key with only a
       non-service subkey, so count GUID-named subkeys - not just any subkey -
       or Swift Pair bait and stale cache rows leak into the list. */
    BOOL sawService = FALSE;
    for (DWORD i = 0; ; i++) {
        wchar_t sub[64];
        DWORD cch = 64;
        if (RegEnumKeyExW(hKey, i, sub, &cch, NULL, NULL, NULL, NULL) != ERROR_SUCCESS)
            break;
        if (IsServiceGuidSubkey(sub)) { sawService = TRUE; break; }
    }
    RegCloseKey(hKey);
    return sawService;
}

static void FormatMac(ULONGLONG addr, wchar_t *out, size_t cap) {
    _snwprintf(out, cap, L"%02X:%02X:%02X:%02X:%02X:%02X",
        (unsigned)((addr >> 40) & 0xFF), (unsigned)((addr >> 32) & 0xFF),
        (unsigned)((addr >> 24) & 0xFF), (unsigned)((addr >> 16) & 0xFF),
        (unsigned)((addr >> 8)  & 0xFF), (unsigned)(addr & 0xFF));
    out[cap - 1] = L'\0';
}

static void FormatCompactHex(ULONGLONG addr, wchar_t *out, size_t cap) {
    _snwprintf(out, cap, L"%012llX", addr);
    out[cap - 1] = L'\0';
}

static BOOL ContainsCI(const wchar_t *hay, const wchar_t *needle) {
    size_t nh = wcslen(hay), nn = wcslen(needle);
    if (nn == 0 || nn > nh) return FALSE;
    for (size_t i = 0; i + nn <= nh; i++) {
        if (_wcsnicmp(hay + i, needle, nn) == 0) return TRUE;
    }
    return FALSE;
}

