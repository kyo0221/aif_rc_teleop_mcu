#include "teleop.h"

#define LOSS_MS 500u
#define SETTLE_MS 500u

int16_t teleop_axis(uint16_t raw) {
    int32_t n = ((int32_t)raw - 992) * 1000 / 819;
    if (n > 1000) n = 1000;
    if (n < -1000) n = -1000;
    int32_t a = n < 0 ? -n : n;
    if (a < 50) return 0;
    int32_t v = (a - 50) * 1000 / 950;
    return (int16_t)(n < 0 ? -v : v);
}

static bool hysteresis(bool level, uint16_t raw) {
    if (raw >= 1500) return true;
    if (raw <= 1300) return false;
    return level;
}

void teleop_on_channels(teleop_t *t, const uint16_t ch[16], uint32_t now_ms) {
    bool was_lost = !t->link;
    t->link = true;
    t->last_rx_ms = now_ms;
    if (was_lost) t->link_up_ms = now_ms;
    bool settled = now_ms - t->link_up_ms >= SETTLE_MS;

    t->sw_auto = hysteresis(t->sw_auto, ch[5]);
    bool level = hysteresis(t->restart_level, ch[6]);
    if (settled && !t->restart_level && level) t->restart_count++;
    t->restart_level = level;

    int16_t thr = teleop_axis(ch[2]);
    int16_t steer = (int16_t)-teleop_axis(ch[0]);
    if (thr == 0 && steer == 0) t->stick_ok = true;

    if (settled) {
        if (!t->autonomous) {
            if (!t->sw_auto) t->auto_ready = true;
            else if (t->auto_ready) t->autonomous = true;
        } else if (!t->sw_auto) {
            t->autonomous = false;
        }
    }

    bool drive = !t->autonomous && t->stick_ok;
    t->throttle = drive ? thr : 0;
    t->steering = drive ? steer : 0;
}

void teleop_on_link_quality(teleop_t *t, uint8_t lq) {
    t->lq = lq;
}

void teleop_tick(teleop_t *t, uint32_t now_ms) {
    if (t->link && now_ms - t->last_rx_ms >= LOSS_MS) {
        t->link = false;
        t->stick_ok = false;
        t->throttle = 0;
        t->steering = 0;
        t->lq = 0;
        if (!t->autonomous) t->auto_ready = false;
    }
}

void teleop_frame(teleop_t *t, uint8_t out[8]) {
    out[0] = (uint8_t)((uint16_t)t->throttle & 0xFF);
    out[1] = (uint8_t)((uint16_t)t->throttle >> 8);
    out[2] = (uint8_t)((uint16_t)t->steering & 0xFF);
    out[3] = (uint8_t)((uint16_t)t->steering >> 8);
    out[4] = (uint8_t)((t->link ? 1 : 0) | (t->autonomous ? 2 : 0) |
                       (t->stick_ok ? 4 : 0) | (t->sw_auto ? 8 : 0));
    out[5] = t->restart_count;
    out[6] = t->lq;
    out[7] = t->seq++;
}
