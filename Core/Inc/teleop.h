#ifndef TELEOP_H
#define TELEOP_H

#include <stdint.h>
#include <stdbool.h>

typedef struct {
    bool link, autonomous, auto_ready, stick_ok, sw_auto, restart_level;
    uint8_t restart_count, lq, seq;
    int16_t throttle, steering;
    uint32_t last_rx_ms, link_up_ms;
} teleop_t;

int16_t teleop_axis(uint16_t raw);
void teleop_on_channels(teleop_t *t, const uint16_t ch[16], uint32_t now_ms);
void teleop_on_link_quality(teleop_t *t, uint8_t lq);
void teleop_tick(teleop_t *t, uint32_t now_ms);
void teleop_frame(teleop_t *t, uint8_t out[8]);

#endif
