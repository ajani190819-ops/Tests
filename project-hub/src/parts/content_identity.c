/* ---- content identity ----------------------------------------------------
 * Git blob SHA-1 of a local file, in the same hex form GitHub's trees API
 * publishes for every path. A HEAD request only yields Content-Length, and two
 * different builds can share a byte count: a size-only check then reports
 * "up to date" for a changed executable, which is exactly how a same-size
 * KeyboardLighting rebuild looked invisible to the update check. Comparing
 * blob hashes is an exact content comparison: one small JSON request for the
 * remote side, one local hash for the installed file.
 */
static BOOL Sha1BlobHexW(const wchar_t *path, char out[41]) {
    HANDLE f = CreateFileW(path, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE,
                           NULL, OPEN_EXISTING, 0, NULL);
    if (f == INVALID_HANDLE_VALUE) return FALSE;
    DWORD size = GetFileSize(f, NULL);
    BOOL ok = FALSE;
    BCRYPT_ALG_HANDLE alg = NULL;
    BCRYPT_HASH_HANDLE hash = NULL;
    BYTE *buf = (BYTE *)malloc(65536);
    BYTE digest[20];
    char hdr[32];
    int hdrLen;

    out[0] = '\0';
    if (buf && BCryptOpenAlgorithmProvider(&alg, BCRYPT_SHA1_ALGORITHM, NULL, 0) == 0
        && BCryptCreateHash(alg, &hash, NULL, 0, NULL, 0, 0) == 0) {
        hdrLen = _snprintf(hdr, sizeof(hdr), "blob %lu", size);
        if (hdrLen > 0 && hdrLen < (int)sizeof(hdr)) {
            /* git hashes "blob <len>\0" followed by the bytes, NUL included. */
            ok = BCryptHashData(hash, (PUCHAR)hdr, (ULONG)hdrLen + 1, 0) == 0;
            while (ok) {
                DWORD got = 0;
                if (!ReadFile(f, buf, 65536, &got, NULL)) { ok = FALSE; break; }
                if (got == 0) break;
                ok = BCryptHashData(hash, (PUCHAR)buf, got, 0) == 0;
            }
            if (ok) ok = BCryptFinishHash(hash, digest, sizeof(digest), 0) == 0;
            if (ok) {
                static const char hexd[] = "0123456789abcdef";
                for (int i = 0; i < 20; i++) {
                    out[i * 2]     = hexd[digest[i] >> 4];
                    out[i * 2 + 1] = hexd[digest[i] & 15];
                }
                out[40] = '\0';
            }
        }
    }
    if (hash) BCryptDestroyHash(hash);
    if (alg) BCryptCloseAlgorithmProvider(alg, 0);
    free(buf);
    CloseHandle(f);
    return ok;
}

/* The blob hash of one path inside a recursive trees response. Entries look
 * like {"path":"a/b.exe","mode":"100644","type":"blob","sha":"<40 hex>",...},
 * so the first "sha" after the path match is the one we want. */
static int TreeShaFor(const char *tree, const char *path, char out[41]) {
    char pat[256];
    const char *p, *q;
    int i = 0;
    if (!tree || !path || !out) return 0;
    out[0] = '\0';
    _snprintf(pat, sizeof(pat) - 1, "\"path\":\"%s\"", path);
    pat[sizeof(pat) - 1] = '\0';
    p = strstr(tree, pat);
    if (!p) return 0;
    q = strstr(p, "\"sha\":\"");
    if (!q || (q - p) > 400) return 0;
    q += 7;
    while (i < 40 && q[i] && q[i] != '"') { out[i] = q[i]; i++; }
    if (i != 40) { out[0] = '\0'; return 0; }
    out[40] = '\0';
    return 1;
}

/* Exact when the remote blob hash is known; the old size comparison remains
 * as the fallback for when the trees API is unreachable. */
static BOOL RemoteDiffers(const char *remSha, const wchar_t *localPath, DWORD remSize) {
    if (remSha && remSha[0]) {
        char localSha[41];
        if (Sha1BlobHexW(localPath, localSha))
            return strcmp(localSha, remSha) != 0;
        return FALSE;   /* nothing installed at this path: install, not update */
    }
    {
        DWORD loc = LocalSizeOf(localPath);
        return (remSize > 0 && loc > 0 && loc != remSize);
    }
}

static BOOL DirExistsW(const wchar_t *p) {
    DWORD a = GetFileAttributesW(p);
    return (a != INVALID_FILE_ATTRIBUTES && (a & FILE_ATTRIBUTE_DIRECTORY));
}

static void EnsureDirTree(const wchar_t *dir) {
    wchar_t tmp[MAX_PATH];
    wcsncpy(tmp, dir, MAX_PATH - 1);
    tmp[MAX_PATH - 1] = L'\0';
    for (wchar_t *p = tmp + 3; *p; p++) {
        if (*p == L'\\' || *p == L'/') {
            wchar_t ch = *p;
            *p = L'\0';
            CreateDirectoryW(tmp, NULL);
            *p = ch;
        }
    }
    CreateDirectoryW(tmp, NULL);
}

static volatile LONG g_checkActive = 0;
static DWORD g_remBtgSize = 0, g_remKblSize = 0, g_remKblPs1Size = 0, g_remDnsSize = 0, g_remNotesSize = 0, g_remAudioSwitcherSize = 0;
/* Git blob SHA-1 of each remote build, from one recursive trees request.
   Empty means unknown (API unreachable), which falls back to the sizes. */
static char g_remBtgSha[41], g_remKblSha[41], g_remKblPs1Sha[41], g_remDnsSha[41],
            g_remNotesSha[41], g_remAudioSwitcherSha[41];
static RECT           g_rcFooterDot = { 0, 0, 0, 0 };

/* Quiet update-check phase: the periodic version check gets a small
   pulsing footer dot instead of the full-width progress sweep and
   step-by-step status churn. */
static void SetCheckPhase(BOOL on) {
    InterlockedExchange(&g_checkActive, on ? 1 : 0);
    if (g_hwnd) PostMessageW(g_hwnd, WM_HUB_STATUS, 0, 0);
}

static void SetFooterStatus(const wchar_t *msg, int pct) {
    EnterCriticalSection(&g_cs);
    wcsncpy(g_statusMsg, msg, 511);
    g_statusMsg[511] = L'\0';
    g_progressVal = pct;
    LeaveCriticalSection(&g_cs);
    if (g_hwnd) PostMessageW(g_hwnd, WM_HUB_STATUS, 0, 0);
}

static char *ReadTextFileUtf8(const wchar_t *path, DWORD *outLen) {
    HANDLE hf = CreateFileW(path, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                            NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (hf == INVALID_HANDLE_VALUE) return NULL;
    DWORD sz = GetFileSize(hf, NULL);
    if (sz == INVALID_FILE_SIZE || sz > 8 * 1024 * 1024) { CloseHandle(hf); return NULL; }
    char *buf = (char *)malloc(sz + 1);
    if (!buf) { CloseHandle(hf); return NULL; }
    DWORD rd = 0;
    if (!ReadFile(hf, buf, sz, &rd, NULL)) { free(buf); CloseHandle(hf); return NULL; }
    buf[rd] = '\0';
    CloseHandle(hf);
    if (outLen) *outLen = rd;
    return buf;
}

static BOOL WriteBinaryFileAtomic(const wchar_t *dest, const BYTE *data, DWORD len) {
    wchar_t dir[MAX_PATH];
    wcsncpy(dir, dest, MAX_PATH - 1);
    dir[MAX_PATH - 1] = L'\0';
    wchar_t *slash = wcsrchr(dir, L'\\');
    if (slash) { *slash = L'\0'; EnsureDirTree(dir); }

    wchar_t tmp[MAX_PATH];
    _snwprintf(tmp, MAX_PATH, L"%s.tmp", dest);
    HANDLE hf = CreateFileW(tmp, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (hf == INVALID_HANDLE_VALUE) return FALSE;
    DWORD wr = 0;
    BOOL ok = WriteFile(hf, data, len, &wr, NULL) && (wr == len);
    CloseHandle(hf);
    if (!ok) { DeleteFileW(tmp); return FALSE; }
    if (!MoveFileExW(tmp, dest, MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) {
        DeleteFileW(tmp);
        return FALSE;
    }
    /* Strip the Mark of the Web so Windows does not re-prompt SmartScreen
       every time the user opens a freshly updated app. */
    {
        wchar_t ads[MAX_PATH + 24];
        _snwprintf(ads, MAX_PATH + 23, L"%s:Zone.Identifier", dest);
        DeleteFileW(ads);
    }
    return TRUE;
}

