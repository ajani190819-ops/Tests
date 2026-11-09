/* DNS Pilot - benchmark the major public DNS resolvers and steer Windows
   through the fastest ones.

   The dorm/school problem this solves: speed-test websites are blocked or
   unreliable on managed Wi-Fi, so this app measures DNS itself - real UDP
   port-53 queries to every listed resolver, the same path Windows will use
   once the DNS is set. Both IPv4 and IPv6 are ranked separately and the
   winners (plus their secondary) are applied to the active adapter.

   Auto-Pilot re-tests on an interval and switches ONLY when the new best is
   meaningfully faster than what is applied (>=15% and >=8ms) and nothing
   full-screen is in the foreground - so it never resets the connection in
   the middle of what you are doing.

   Setting DNS needs administrator rights; the app says so plainly and can
   relaunch itself elevated for Auto-Pilot sessions. */
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <iphlpapi.h>
#include <icmpapi.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <windowsx.h>
#include <shellapi.h>
#include <winhttp.h>
#include <math.h>
#include <stdint.h>

#ifndef GGI_MARK_NONEXISTING_GLYPHS
#define GGI_MARK_NONEXISTING_GLYPHS 1
#endif

#define APP_TITLE_W   L"DNS Pilot"
#define APP_DIR_NAME  L"\\DnsPilot"
#define WM_APP_DONE   (WM_APP + 1)
#define SINGLE_MUTEX_NAME L"Local\\Tests_DnsPilot_SingleInstance"

#define COL_BG        RGB(24, 27, 36)
#define COL_CARD      RGB(34, 39, 52)
#define COL_ROW2      RGB(29, 33, 45)
#define COL_TXT       RGB(232, 236, 245)
#define COL_MUTE      RGB(151, 160, 179)
#define COL_ACCENT    RGB(78, 161, 255)
#define COL_OK        RGB(61, 220, 151)
#define COL_WARN      RGB(245, 181, 68)
#define COL_BAD       RGB(255, 107, 107)
#define COL_GOLD      RGB(232, 196, 107)
#define COL_DIV       RGB(48, 55, 72)


/* ---- modularized (unity build): each section lives in its own file under
      parts/ and is textually included here IN ORIGINAL ORDER, so dns_pilot.c
      is still ONE translation unit. Edit the part files; the build compiles
      this one file. ---- */
#include "parts/resolver_catalogue.c"
#include "parts/dns_probe.c"
#include "parts/benchmark.c"
#include "parts/adapter_discovery.c"
#include "parts/live_stats.c"
#include "parts/speed_test.c"
#include "parts/apply_dns.c"
#include "parts/elevation.c"
#include "parts/config.c"
#include "parts/auto_watch.c"
#include "parts/ui.c"
