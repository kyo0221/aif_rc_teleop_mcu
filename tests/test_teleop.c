#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "teleop.h"

static void frame(teleop_t *t, int thr, int steer, bool sa_auto, bool restart, uint32_t now) {
    uint16_t ch[16];
    for (int i = 0; i < 16; i++) ch[i] = 992;
    ch[2] = (uint16_t)thr;
    ch[0] = (uint16_t)steer;
    ch[5] = sa_auto ? 1811 : 172;
    ch[6] = restart ? 1811 : 172;
    teleop_on_channels(t, ch, now);
}

static uint8_t flags(teleop_t *t) {
    uint8_t out[8];
    teleop_frame(t, out);
    return out[4];
}

static void axis_values(void) {
    assert(teleop_axis(992) == 0);
    assert(teleop_axis(1811) == 1000);
    assert(teleop_axis(172) == -1000);
    assert(teleop_axis(1032) == 0);
    assert(teleop_axis(1402) == 473);
    assert(teleop_axis(582) == -473);
}

static void axis_clamps(void) {
    assert(teleop_axis(0) == -1000);
    assert(teleop_axis(1984) == 1000);
    assert(teleop_axis(2047) == 1000);
}

static void boots_manual_lost(void) {
    teleop_t t = {0};
    uint8_t out[8];
    teleop_frame(&t, out);
    assert(t.throttle == 0 && t.steering == 0);
    assert(out[4] == 0x00);
}

static void drives_after_neutral(void) {
    teleop_t t = {0};
    frame(&t, 992, 992, false, false, 0);
    frame(&t, 1811, 992, false, false, 10);
    assert(t.throttle == 1000);
    assert((flags(&t) & 0x05) == 0x05);
}

static void no_drive_until_neutral(void) {
    teleop_t t = {0};
    frame(&t, 1811, 992, false, false, 0);
    assert(t.throttle == 0);
    assert((flags(&t) & 0x04) == 0);
}

static void steering_sign(void) {
    teleop_t t = {0};
    frame(&t, 992, 992, false, false, 0);
    frame(&t, 992, 1811, false, false, 10);
    assert(t.steering == -1000);
}

static teleop_t settled(void) {
    teleop_t t = {0};
    frame(&t, 992, 992, false, false, 1000);
    frame(&t, 992, 992, false, false, 1500);
    return t;
}

static void auto_needs_manual_first(void) {
    teleop_t t = {0};
    frame(&t, 992, 992, true, false, 0);
    frame(&t, 992, 992, true, false, 600);
    assert(!t.autonomous);
    assert(flags(&t) & 0x08);
}

static void auto_after_manual(void) {
    teleop_t t = settled();
    frame(&t, 992, 992, true, false, 1510);
    assert(t.autonomous);
    assert(t.throttle == 0 && t.steering == 0);
}

static void auto_ignores_sticks(void) {
    teleop_t t = settled();
    frame(&t, 992, 992, true, false, 1510);
    frame(&t, 1811, 992, true, false, 1520);
    assert(t.throttle == 0);
}

static void manual_takeover_immediate(void) {
    teleop_t t = settled();
    frame(&t, 992, 992, true, false, 1510);
    assert(t.autonomous);
    frame(&t, 1811, 992, true, false, 1515);
    assert(t.autonomous);
    assert(t.throttle == 0);
    frame(&t, 1811, 992, false, false, 1520);
    assert(!t.autonomous);
    assert(t.throttle == 1000);
}

static void link_loss_manual(void) {
    teleop_t t = settled();
    frame(&t, 1811, 172, false, false, 1510);
    assert(t.throttle == 1000 && t.steering == 1000);
    teleop_tick(&t, 1510 + 499);
    assert(t.link);
    teleop_tick(&t, 1510 + 500);
    assert(!t.link);
    assert(t.throttle == 0 && t.steering == 0);
    assert(!t.auto_ready);
}

static void recover_manual_needs_neutral(void) {
    teleop_t t = settled();
    frame(&t, 1811, 172, false, false, 1510);
    assert(t.throttle == 1000 && t.steering == 1000);
    teleop_tick(&t, 2010);
    frame(&t, 1811, 172, false, false, 3000);
    assert(t.throttle == 0 && t.steering == 0);
    frame(&t, 992, 992, false, false, 3010);
    assert(t.throttle == 0 && t.steering == 0);
    frame(&t, 1811, 172, false, false, 3020);
    assert(t.throttle == 1000 && t.steering == 1000);
}

static void recover_manual_sa_auto_stays_manual(void) {
    teleop_t t = settled();
    teleop_tick(&t, 2000);
    frame(&t, 992, 992, true, false, 3000);
    assert(!t.autonomous);
    frame(&t, 992, 992, true, false, 3600);
    assert(!t.autonomous);
    assert(flags(&t) == 0x0D);
}

static void stale_switch_on_recovery(void) {
    teleop_t t = settled();
    teleop_tick(&t, 2000);
    frame(&t, 992, 992, false, false, 3000);
    frame(&t, 992, 992, true, false, 3010);
    assert(!t.autonomous);
    frame(&t, 992, 992, true, false, 3600);
    assert(!t.autonomous);
    frame(&t, 992, 992, false, false, 3610);
    frame(&t, 992, 992, true, false, 3620);
    assert(t.autonomous);
}

static void stale_switch_at_boot(void) {
    teleop_t t = {0};
    uint16_t ch[16] = {0};
    teleop_on_channels(&t, ch, 0);
    frame(&t, 992, 992, true, false, 10);
    assert(!t.autonomous);
    frame(&t, 992, 992, true, false, 600);
    assert(!t.autonomous);
}

static void stale_switch_keeps_autonomous(void) {
    teleop_t t = settled();
    frame(&t, 992, 992, true, false, 1510);
    teleop_tick(&t, 2010);
    frame(&t, 992, 992, false, false, 3000);
    assert(t.autonomous);
    frame(&t, 992, 992, true, false, 3010);
    frame(&t, 992, 992, true, false, 3600);
    assert(t.autonomous);
    frame(&t, 992, 992, false, false, 3610);
    assert(!t.autonomous);
}

static void link_loss_auto_stays_auto(void) {
    teleop_t t = settled();
    frame(&t, 992, 992, true, false, 1510);
    teleop_tick(&t, 2010);
    assert(t.autonomous);
    assert(!t.link);
    assert(flags(&t) == 0x0A);
}

static void recover_auto_to_manual_needs_neutral(void) {
    teleop_t t = settled();
    frame(&t, 1811, 172, true, false, 1510);
    assert(t.autonomous);
    teleop_tick(&t, 2010);
    frame(&t, 1811, 172, false, false, 3000);
    assert(t.autonomous);
    frame(&t, 1811, 172, false, false, 3500);
    assert(!t.autonomous);
    assert(t.throttle == 0 && t.steering == 0);
    frame(&t, 992, 992, false, false, 3510);
    assert(t.throttle == 0 && t.steering == 0);
    frame(&t, 1811, 172, false, false, 3520);
    assert(t.throttle == 1000 && t.steering == 1000);
}

static void restart_edge(void) {
    teleop_t t = settled();
    frame(&t, 992, 992, false, true, 1510);
    frame(&t, 992, 992, false, true, 1520);
    frame(&t, 992, 992, false, false, 1530);
    frame(&t, 992, 992, false, true, 1540);
    assert(t.restart_count == 2);
}

static void restart_in_auto(void) {
    teleop_t t = settled();
    frame(&t, 992, 992, true, false, 1510);
    assert(t.autonomous);
    frame(&t, 992, 992, true, true, 1520);
    assert(t.restart_count == 1);
}

static void restart_not_on_recovery(void) {
    teleop_t t = settled();
    teleop_tick(&t, 2000);
    frame(&t, 992, 992, false, true, 3000);
    frame(&t, 992, 992, false, true, 3600);
    assert(t.restart_count == 0);
    frame(&t, 992, 992, false, false, 3610);
    frame(&t, 992, 992, false, true, 3620);
    assert(t.restart_count == 1);
}

static void restart_stale_on_recovery(void) {
    teleop_t t = settled();
    teleop_tick(&t, 2000);
    frame(&t, 992, 992, false, false, 3000);
    frame(&t, 992, 992, false, true, 3010);
    frame(&t, 992, 992, false, true, 3600);
    assert(t.restart_count == 0);
}

static void switch_hysteresis(void) {
    teleop_t t = {0};
    frame(&t, 992, 992, false, false, 0);
    assert(!t.sw_auto);
    frame(&t, 992, 992, false, false, 10);
    uint16_t ch[16];
    for (int i = 0; i < 16; i++) ch[i] = 992;
    ch[5] = 1400;
    teleop_on_channels(&t, ch, 20);
    assert(!t.sw_auto);
    ch[5] = 1811;
    teleop_on_channels(&t, ch, 30);
    assert(t.sw_auto);
    ch[5] = 1400;
    teleop_on_channels(&t, ch, 40);
    assert(t.sw_auto);
    ch[5] = 1200;
    teleop_on_channels(&t, ch, 50);
    assert(!t.sw_auto);
}

static void link_timeout_across_wrap(void) {
    teleop_t t = {0};
    frame(&t, 992, 992, false, false, 0xFFFFFF00u);
    teleop_tick(&t, 0xFFFFFF00u + 499);
    assert(t.link);
    teleop_tick(&t, 0x000000F4u);
    assert(!t.link);
}

static void frame_layout(void) {
    teleop_t t = {0};
    frame(&t, 992, 992, false, false, 0);
    frame(&t, 1811, 172, false, false, 10);
    teleop_on_link_quality(&t, 87);
    uint8_t out[8];
    const uint8_t want[8] = {0xE8, 0x03, 0xE8, 0x03, 0x05, 0x00, 87, 0};
    teleop_frame(&t, out);
    assert(memcmp(out, want, 8) == 0);
    teleop_frame(&t, out);
    assert(out[7] == 1);
}

static void frame_negative_values(void) {
    teleop_t t = {0};
    frame(&t, 992, 992, false, false, 0);
    frame(&t, 172, 1811, false, false, 10);
    assert(t.throttle == -1000 && t.steering == -1000);
    uint8_t out[8];
    teleop_frame(&t, out);
    assert(out[0] == 0x18 && out[1] == 0xFC);
    assert(out[2] == 0x18 && out[3] == 0xFC);
}

int main(void) {
    axis_values();
    axis_clamps();
    boots_manual_lost();
    drives_after_neutral();
    no_drive_until_neutral();
    steering_sign();
    auto_needs_manual_first();
    auto_after_manual();
    auto_ignores_sticks();
    manual_takeover_immediate();
    link_loss_manual();
    recover_manual_needs_neutral();
    recover_manual_sa_auto_stays_manual();
    link_loss_auto_stays_auto();
    recover_auto_to_manual_needs_neutral();
    restart_edge();
    restart_in_auto();
    restart_not_on_recovery();
    restart_stale_on_recovery();
    stale_switch_on_recovery();
    stale_switch_at_boot();
    stale_switch_keeps_autonomous();
    switch_hysteresis();
    link_timeout_across_wrap();
    frame_layout();
    frame_negative_values();
    puts("test_teleop ok");
    return 0;
}
