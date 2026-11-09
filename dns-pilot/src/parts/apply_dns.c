/* ---- applying DNS (netsh with numeric ifIndex; needs elevation) ---- */
static int RunElevatedCmd(const wchar_t *cmd) {
    STARTUPINFOW si;
    PROCESS_INFORMATION pi;
    wchar_t buf[512];
    wcsncpy(buf, cmd, 511);
    buf[511] = L'\0';
    ZeroMemory(&si, sizeof(si));
    si.cb = sizeof(si);
    ZeroMemory(&pi, sizeof(pi));
    if (!CreateProcessW(NULL, buf, NULL, NULL, FALSE, CREATE_NO_WINDOW, NULL, NULL, &si, &pi))
        return 0;
    WaitForSingleObject(pi.hProcess, 10000);
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);
    return 1;
}

static void ApplyDns(int ifIndex, const char *v4a, const char *v4b, const char *v6a, const char *v6b) {
    wchar_t cmd[512], w1[128], w2[128];
    if (ifIndex <= 0) return;
    if (v4a && v4a[0]) {
        Widen(v4a, w1, 128);
        _snwprintf(cmd, 511, L"netsh interface ipv4 set dnsservers %d static %s primary", ifIndex, w1);
        RunElevatedCmd(cmd);
        if (v4b && v4b[0]) {
            Widen(v4b, w2, 128);
            _snwprintf(cmd, 511, L"netsh interface ipv4 add dnsservers %d %s index=2", ifIndex, w2);
            RunElevatedCmd(cmd);
        }
    }
    if (v6a && v6a[0]) {
        Widen(v6a, w1, 128);
        _snwprintf(cmd, 511, L"netsh interface ipv6 set dnsservers %d static %s primary", ifIndex, w1);
        RunElevatedCmd(cmd);
        if (v6b && v6b[0]) {
            Widen(v6b, w2, 128);
            _snwprintf(cmd, 511, L"netsh interface ipv6 add dnsservers %d %s index=2", ifIndex, w2);
            RunElevatedCmd(cmd);
        }
    }
    RunElevatedCmd(L"ipconfig /flushdns");
    if (v4a && v4a[0]) { strncpy(g_curV4, v4a, 63); g_curV4[63] = '\0'; }
    if (v6a && v6a[0]) { strncpy(g_curV6, v6a, 63); g_curV6[63] = '\0'; }
    strncpy(g_lastV4, g_curV4, 63);
    strncpy(g_lastV6, g_curV6, 63);
}

