/* One deterministic software-master controller. No Win32, file I/O or engine
   feedback here. Directions are identities: 0 = M4/down, 1 = M5/up. */
#ifndef TESTS_BRIGHTNESS_CONTROLLER_H
#define TESTS_BRIGHTNESS_CONTROLLER_H
#include <stdint.h>
#include <string.h>

typedef struct BrightnessController {
    int level, lastOn;
    int direction, mode, pending, active, remaining;
    uint64_t startedAt, nextAt, heldUntil;
} BrightnessController;

static inline int BrightnessClamp(int level) {
    return level < 0 ? 0 : level > 1000 ? 1000 : level;
}

static inline void BrightnessStop(BrightnessController *s) {
    s->direction = s->pending = s->active = s->remaining = 0;
}

static inline void BrightnessSet(BrightnessController *s, int level) {
    s->level = BrightnessClamp(level);
    if (s->level > 0) s->lastOn = s->level;
}

static inline void BrightnessInit(BrightnessController *s, int level) {
    memset(s, 0, sizeof(*s));
    s->lastOn = 1000;
    BrightnessSet(s, level);
}

static inline void BrightnessAbsolute(BrightnessController *s, int level) {
    BrightnessStop(s);
    BrightnessSet(s, level);
}

static inline void BrightnessMode(BrightnessController *s, int mode) {
    if (mode < 0 || mode > 2) mode = 0;
    if (s->mode != mode) BrightnessStop(s);
    s->mode = mode;
}

/* Only genuine Custom presses reach this entry point. Raw reports and timer
   ticks NEVER masquerade as another press. A tap alone has no trailing ramp. */
static inline void BrightnessPress(BrightnessController *s, int key, int mode, uint64_t now) {
    int direction = key == 0 ? -1 : 1;
    BrightnessMode(s, mode);
    if (mode == 1 && s->active && s->direction == direction) {
        BrightnessStop(s);
        return;
    }
    BrightnessStop(s);
    BrightnessSet(s, s->level + direction * 50);
    s->direction = direction;
    s->startedAt = now;
    s->nextAt = now + 30;
    s->pending = mode == 0;
    s->active = mode != 0;
    s->remaining = mode == 2 ? 150 : 0;
    if ((direction < 0 && s->level == 0) || (direction > 0 && s->level == 1000))
        BrightnessStop(s);
}

static inline void BrightnessHeld(BrightnessController *s, int key, uint64_t now) {
    if (s->mode != 0 || !s->pending || s->direction != (key == 0 ? -1 : 1)) return;
    if (now < s->startedAt || now - s->startedAt >= 10000) {
        BrightnessStop(s);
        return;
    }
    s->active = 1;
    s->heldUntil = now + 120;
}

static inline void BrightnessRelease(BrightnessController *s, int key) {
    if (s->mode == 0 && s->direction == (key == 0 ? -1 : 1)) BrightnessStop(s);
}

/* At most one fine step per due tick; no catch-up bursts after a stalled UI.
   Direction-specific rates intentionally remain 1% down / 1.5% up per 30ms. */
static inline int BrightnessTick(BrightnessController *s, uint64_t now) {
    int before = s->level;
    if (!s->active) return 0;
    if (now < s->startedAt || now - s->startedAt >= 10000 ||
        (s->mode == 0 && now >= s->heldUntil)) {
        BrightnessStop(s);
        return 0;
    }
    if (now < s->nextAt) return 0;
    int amount = s->direction > 0 ? 15 : 10;
    if (s->mode == 2 && amount > s->remaining) amount = s->remaining;
    BrightnessSet(s, s->level + s->direction * amount);
    s->nextAt = now + 30;
    if (s->mode == 2) s->remaining -= amount;
    if (s->level == 0 || s->level == 1000 || (s->mode == 2 && s->remaining <= 0))
        BrightnessStop(s);
    return s->level != before;
}
#endif
