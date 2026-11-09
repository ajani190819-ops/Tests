/* Maximize CPU scheduling priority & disable Windows 11 EcoQoS throttling so boot/wake reaction is instantaneous */
static void BoostProcessAndShutdownPriority(void) {
    if (!SetPriorityClass(GetCurrentProcess(), HIGH_PRIORITY_CLASS)) {
        SetPriorityClass(GetCurrentProcess(), ABOVE_NORMAL_PRIORITY_CLASS);
    }
    SetProcessShutdownParameters(0x3FF, 0);

    HMODULE k32 = GetModuleHandleW(L"kernel32.dll");
    if (k32) {
        PFN_SetProcessInformation pSetProcInfo =
            (PFN_SetProcessInformation)(void *)GetProcAddress(k32, "SetProcessInformation");
        if (pSetProcInfo) {
            BtPowerThrottlingState pts;
            pts.Version     = 1;
            pts.ControlMask = 0x7; /* EXECUTION_SPEED | IGNORE_TIMER_RESOLUTION | MIN_RESOLUTION */
            pts.StateMask   = 0;   /* Disable all background throttling even on battery */
            pSetProcInfo(GetCurrentProcess(), 4 /* ProcessPowerThrottling */, &pts, sizeof(pts));
        }
    }
}

/* Subscribe to live kernel Bluetooth HCI / radio interface notifications for <1ms connect detection */
static void RegisterBluetoothNotifications(HWND hwnd) {
    if (!hwnd) return;
    static const GUID kGuidBthPortIface =
        {0x0850302A, 0xB344, 0x4FDA, {0x9B,0xE9,0x90,0x57,0x6B,0x8D,0x46,0xF0}};

    if (!g_devNotifyIface) {
        DEV_BROADCAST_DEVICEINTERFACE_W dbi;
        ZeroMemory(&dbi, sizeof(dbi));
        dbi.dbcc_size       = sizeof(dbi);
        dbi.dbcc_devicetype = DBT_DEVTYP_DEVICEINTERFACE;
        dbi.dbcc_classguid  = kGuidBthPortIface;
        g_devNotifyIface = RegisterDeviceNotificationW(hwnd, &dbi, DEVICE_NOTIFY_WINDOW_HANDLE);
    }

    if (!g_devNotifyRadio && pBtFindFirstRadio && pBtFindRadioClose) {
        BLUETOOTH_FIND_RADIO_PARAMS rp;
        rp.dwSize = sizeof(rp);
        HANDLE hRadio = NULL;
        HBLUETOOTH_RADIO_FIND hRF = pBtFindFirstRadio(&rp, &hRadio);
        if (hRF) {
            pBtFindRadioClose(hRF);
            if (hRadio) {
                DEV_BROADCAST_HANDLE dbh;
                ZeroMemory(&dbh, sizeof(dbh));
                dbh.dbch_size       = sizeof(dbh);
                dbh.dbch_devicetype = DBT_DEVTYP_HANDLE;
                dbh.dbch_handle     = hRadio;
                g_devNotifyRadio = RegisterDeviceNotificationW(hwnd, &dbh, DEVICE_NOTIFY_WINDOW_HANDLE);
                if (g_devNotifyRadio) g_radioNotifyHandle = hRadio;
                else CloseHandle(hRadio);
            }
        }
    }
}

static void UnregisterBluetoothNotifications(void) {
    if (g_devNotifyRadio) { UnregisterDeviceNotification(g_devNotifyRadio); g_devNotifyRadio = NULL; }
    if (g_radioNotifyHandle) { CloseHandle(g_radioNotifyHandle); g_radioNotifyHandle = NULL; }
    if (g_devNotifyIface) { UnregisterDeviceNotification(g_devNotifyIface); g_devNotifyIface = NULL; }
}

static WorkItem         g_queue[64];
static int              g_qHead      = 0;
static int              g_qTail      = 0;
static HANDLE           g_qSem       = NULL;
static volatile LONG    g_workerRun  = 1;
static wchar_t          g_cfgDir[MAX_PATH];
static wchar_t          g_rulesPath[MAX_PATH];
static wchar_t          g_lockPath[MAX_PATH];

static int GetWinDpi(HWND hwnd) {
    HMODULE u32 = GetModuleHandleW(L"user32.dll");
    if (u32) {
        PFN_GetDpiForWindow fn = (PFN_GetDpiForWindow)(void *)GetProcAddress(u32, "GetDpiForWindow");
        if (fn && hwnd) {
            UINT d = fn(hwnd);
            if (d >= 96) return (int)d;
        }
    }
    HDC hdc = GetDC(hwnd);
    int dpi = hdc ? GetDeviceCaps(hdc, LOGPIXELSX) : 96;
    if (hdc) ReleaseDC(hwnd, hdc);
    return dpi >= 96 ? dpi : 96;
}

static int S(int px, int dpi) { return MulDiv(px, dpi, 96); }

static void ApplyModernWindowFrame(HWND hwnd) {
    HMODULE dwm = LoadLibraryW(L"dwmapi.dll");
    if (!dwm) return;
    PFN_DwmSetWindowAttribute setAttr =
        (PFN_DwmSetWindowAttribute)(void *)GetProcAddress(dwm, "DwmSetWindowAttribute");
    if (setAttr) {
        BOOL dark = TRUE;
        DWORD roundPref = 2; /* DWMWCP_ROUND */
        COLORREF captionCol = COL_BG;
        setAttr(hwnd, 20, &dark, sizeof(dark));
        setAttr(hwnd, 33, &roundPref, sizeof(roundPref));
        setAttr(hwnd, 35, &captionCol, sizeof(captionCol));
    }
    FreeLibrary(dwm);
}

static BOOL InitBluetoothApis(void) {
    HMODULE m = LoadLibraryW(L"BluetoothAPIs.dll");
    if (!m) m = LoadLibraryW(L"bthprops.cpl");
    if (!m) return FALSE;
    pBtFindFirstRadio  = (PFN_BtFindFirstRadio)(void *)GetProcAddress(m, "BluetoothFindFirstRadio");
    pBtFindNextRadio   = (PFN_BtFindNextRadio)(void *)GetProcAddress(m, "BluetoothFindNextRadio");
    pBtFindRadioClose  = (PFN_BtFindRadioClose)(void *)GetProcAddress(m, "BluetoothFindRadioClose");
    pBtFindFirstDevice = (PFN_BtFindFirstDevice)(void *)GetProcAddress(m, "BluetoothFindFirstDevice");
    pBtFindNextDevice  = (PFN_BtFindNextDevice)(void *)GetProcAddress(m, "BluetoothFindNextDevice");
    pBtFindDeviceClose = (PFN_BtFindDeviceClose)(void *)GetProcAddress(m, "BluetoothFindDeviceClose");
    pBtSetServiceState = (PFN_BtSetServiceState)(void *)GetProcAddress(m, "BluetoothSetServiceState");
    return (pBtFindFirstRadio && pBtFindNextRadio && pBtFindRadioClose &&
            pBtFindFirstDevice && pBtFindNextDevice && pBtFindDeviceClose && pBtSetServiceState);
}

