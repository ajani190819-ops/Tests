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
    DWORD subKeys = 0;
    LONG rc = RegQueryInfoKeyW(hKey, NULL, NULL, NULL, &subKeys, NULL, NULL,
                               NULL, NULL, NULL, NULL, NULL);
    RegCloseKey(hKey);
    if (rc != ERROR_SUCCESS) return FALSE;
    /* profile subkeys exist = this machine actually connected before */
    return subKeys > 0;
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

