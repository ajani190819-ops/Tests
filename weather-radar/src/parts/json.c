/* ---------------- json: minimal in-place parser ----------------

   The three feeds this app reads (Open-Meteo geocoding, forecast, ensemble,
   plus the RainViewer catalog) are all flat-ish JSON whose only awkward part
   is the ensemble hourly object: dozens of member arrays that have to be
   enumerated by key prefix, with nulls inside them where a member's forecast
   horizon has run out. A real DOM parser is overkill; regex would lie. This
   is a strict recursive-descent parser that reuses the source buffer:
   strings are unescaped IN PLACE (an escape can only ever shrink: "\u00e9"
   is 6 source bytes for 2 decoded bytes), numbers stay doubles, and
   null becomes a node whose type says null so a caller can tell
   "reported 0.0 mm" from "reported nothing". */

#define J_NULL 0
#define J_BOOL 1
#define J_NUM  2
#define J_STR  3
#define J_ARR  4
#define J_OBJ  5

typedef struct JNode {
    int type;
    double num;
    char *str;          /* decoded, NUL-terminated in place */
    char *key;          /* member key, decoded in place (NULL for array items) */
    struct JNode *child, *next;
} JNode;

/* The ensemble response is the big one: ~31 member arrays x 73 hours plus
   keys. 16k nodes covers ~64 members x (1 array + 73 items) + everything
   else with headroom; a truncated parse just yields fewer members. */
#define JPOOL 20000
static JNode g_jpool[JPOOL];
static int   g_jused = 0;

static JNode *JNew(void) {
    if (g_jused >= JPOOL) return NULL;
    JNode *n = &g_jpool[g_jused++];
    n->type = J_NULL; n->num = 0; n->str = NULL; n->key = NULL;
    n->child = NULL; n->next = NULL;
    return n;
}

static void JSkipWs(char **p) {
    while (**p == ' ' || **p == '\t' || **p == '\r' || **p == '\n') (*p)++;
}

/* Decode escapes in place. src points at the first char after the opening
   quote; returns the position after the closing quote, or NULL. */
static char *JDecodeString(char *src, char **outStr) {
    char *r = src, *w = src;
    while (*r && *r != '"') {
        if (*r != '\\') { *w++ = *r++; continue; }
        r++;
        switch (*r) {
        case '"':  *w++ = '"';  r++; break;
        case '\\': *w++ = '\\'; r++; break;
        case '/':  *w++ = '/';  r++; break;
        case 'b':  *w++ = '\b'; r++; break;
        case 'f':  *w++ = '\f'; r++; break;
        case 'n':  *w++ = '\n'; r++; break;
        case 'r':  *w++ = '\r'; r++; break;
        case 't':  *w++ = '\t'; r++; break;
        case 'u': {
            unsigned cp = 0;
            for (int i = 0; i < 4 && r[1 + i]; i++) {
                char c = r[1 + i];
                cp <<= 4;
                if (c >= '0' && c <= '9') cp |= (unsigned)(c - '0');
                else if (c >= 'a' && c <= 'f') cp |= (unsigned)(c - 'a' + 10);
                else if (c >= 'A' && c <= 'F') cp |= (unsigned)(c - 'A' + 10);
            }
            r += 5;
            /* surrogate pair -> one code point */
            if (cp >= 0xD800 && cp <= 0xDBFF && r[0] == '\\' && r[1] == 'u') {
                unsigned lo = 0;
                for (int i = 0; i < 4 && r[2 + i]; i++) {
                    char c = r[2 + i];
                    lo <<= 4;
                    if (c >= '0' && c <= '9') lo |= (unsigned)(c - '0');
                    else if (c >= 'a' && c <= 'f') lo |= (unsigned)(c - 'a' + 10);
                    else if (c >= 'A' && c <= 'F') lo |= (unsigned)(c - 'A' + 10);
                }
                if (lo >= 0xDC00 && lo <= 0xDFFF) {
                    cp = 0x10000 + ((cp - 0xD800) << 10) + (lo - 0xDC00);
                    r += 6;
                }
            }
            /* encode as UTF-8 (the whole buffer is fed to MultiByteToWideChar) */
            if (cp < 0x80) {
                *w++ = (char)cp;
            } else if (cp < 0x800) {
                *w++ = (char)(0xC0 | (cp >> 6));
                *w++ = (char)(0x80 | (cp & 63));
            } else if (cp < 0x10000) {
                *w++ = (char)(0xE0 | (cp >> 12));
                *w++ = (char)(0x80 | ((cp >> 6) & 63));
                *w++ = (char)(0x80 | (cp & 63));
            } else {
                *w++ = (char)(0xF0 | (cp >> 18));
                *w++ = (char)(0x80 | ((cp >> 12) & 63));
                *w++ = (char)(0x80 | ((cp >> 6) & 63));
                *w++ = (char)(0x80 | (cp & 63));
            }
            break;
        }
        default: *w++ = *r++; break;
        }
    }
    if (*r != '"') return NULL;
    *w = '\0';
    *outStr = src;
    return r + 1;
}

static char *JParseValue(char *p, JNode **out);

static char *JParseValue(char *p, JNode **out) {
    JSkipWs(&p);
    JNode *n = JNew();
    if (!n) { *out = NULL; return NULL; }
    *out = n;
    if (*p == '{') {
        n->type = J_OBJ;
        p++;
        JNode *tail = NULL;
        for (;;) {
            JSkipWs(&p);
            if (*p == '}') { p++; break; }
            if (*p != '"') return NULL;
            char *key = NULL;
            p = JDecodeString(p + 1, &key);
            if (!p) return NULL;
            JSkipWs(&p);
            if (*p != ':') return NULL;
            p++;
            JNode *v = NULL;
            p = JParseValue(p, &v);
            if (!p || !v) return NULL;
            v->key = key;
            if (tail) tail->next = v; else n->child = v;
            tail = v;
            JSkipWs(&p);
            if (*p == ',') { p++; continue; }
            if (*p == '}') { p++; break; }
            return NULL;
        }
        return p;
    }
    if (*p == '[') {
        n->type = J_ARR;
        p++;
        JNode *tail = NULL;
        for (;;) {
            JSkipWs(&p);
            if (*p == ']') { p++; break; }
            JNode *v = NULL;
            p = JParseValue(p, &v);
            if (!p || !v) return NULL;
            if (tail) tail->next = v; else n->child = v;
            tail = v;
            JSkipWs(&p);
            if (*p == ',') { p++; continue; }
            if (*p == ']') { p++; break; }
            return NULL;
        }
        return p;
    }
    if (*p == '"') {
        char *s = NULL;
        p = JDecodeString(p + 1, &s);
        if (!p) return NULL;
        n->type = J_STR;
        n->str = s;
        return p;
    }
    if (!strncmp(p, "true", 4))  { n->type = J_BOOL; n->num = 1; return p + 4; }
    if (!strncmp(p, "false", 5)) { n->type = J_BOOL; n->num = 0; return p + 5; }
    if (!strncmp(p, "null", 4))  { n->type = J_NULL; return p + 4; }
    char *end = NULL;
    double d = strtod(p, &end);
    if (end == p) return NULL;
    n->type = J_NUM;
    n->num = d;
    return end;
}

/* Parse `src` (a NUL-terminated, mutable buffer that must outlive the tree).
   The node pool resets, so one parse at a time - the worker thread is the
   only caller and it serialises its own jobs. */
static JNode *JsonParse(char *src) {
    g_jused = 0;
    if (!src) return NULL;
    JNode *root = NULL;
    char *end = JParseValue(src, &root);
    if (!end || !root) return NULL;
    return root;
}

static JNode *JGet(JNode *obj, const char *key) {
    if (!obj || obj->type != J_OBJ) return NULL;
    for (JNode *c = obj->child; c; c = c->next)
        if (c->key && strcmp(c->key, key) == 0) return c;
    return NULL;
}

static int JLen(JNode *arr) {
    if (!arr || arr->type != J_ARR) return 0;
    int n = 0;
    for (JNode *c = arr->child; c; c = c->next) n++;
    return n;
}

static JNode *JAt(JNode *arr, int i) {
    if (!arr || arr->type != J_ARR) return NULL;
    int k = 0;
    for (JNode *c = arr->child; c; c = c->next, k++)
        if (k == i) return c;
    return NULL;
}

/* Numeric value, or NAN when the node is missing / null / not a number.
   NAN is load-bearing: a member that reported nothing must never be read
   as "0.0 mm of rain". */
static double JNum(JNode *n) {
    if (!n || n->type != J_NUM) return NAN;
    return n->num;
}
