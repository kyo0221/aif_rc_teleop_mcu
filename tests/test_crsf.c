#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "crsf.h"

static void pack_channels(const uint16_t ch[16], uint8_t out[22]) {
    memset(out, 0, 22);
    for (int i = 0; i < 16; i++) {
        for (int b = 0; b < 11; b++) {
            if (ch[i] & (1u << b)) {
                int bit = i * 11 + b;
                out[bit / 8] |= (uint8_t)(1u << (bit % 8));
            }
        }
    }
}

static uint8_t make_frame(uint8_t type, const uint8_t *payload, uint8_t n, uint8_t *out) {
    out[0] = 0xC8;
    out[1] = (uint8_t)(n + 2);
    out[2] = type;
    memcpy(&out[3], payload, n);
    out[3 + n] = crsf_crc8(&out[2], (uint8_t)(n + 1));
    return (uint8_t)(n + 4);
}

static crsf_event_t feed_all(crsf_parser_t *p, const uint8_t *data, int n, uint16_t ch[16], uint8_t *lq) {
    crsf_event_t last = CRSF_NONE;
    for (int i = 0; i < n; i++) {
        crsf_event_t e = crsf_feed(p, data[i], ch, lq);
        if (e != CRSF_NONE) last = e;
    }
    return last;
}

static uint8_t channel_frame(const uint16_t in[16], uint8_t *frame) {
    uint8_t payload[22];
    pack_channels(in, payload);
    return make_frame(0x16, payload, 22, frame);
}

static void crc_check_value(void) {
    assert(crsf_crc8((const uint8_t *)"123456789", 9) == 0xBC);
}

static void parses_channels(void) {
    crsf_parser_t p = {0};
    uint16_t in[16], out[16] = {0};
    uint8_t lq = 0, frame[64];
    for (int i = 0; i < 16; i++) in[i] = (uint16_t)(172 + i * 100);
    uint8_t n = channel_frame(in, frame);
    assert(feed_all(&p, frame, n, out, &lq) == CRSF_CHANNELS);
    for (int i = 0; i < 16; i++) assert(out[i] == in[i]);
}

static void parses_link_quality(void) {
    crsf_parser_t p = {0};
    uint16_t ch[16] = {0};
    uint8_t lq = 0, frame[64];
    uint8_t payload[10] = {0, 0, 87, 0, 0, 0, 0, 0, 0, 0};
    uint8_t n = make_frame(0x14, payload, 10, frame);
    crsf_event_t event = feed_all(&p, frame, n, ch, &lq);
    assert(event == CRSF_LINK && lq == 87);
}

static void rejects_bad_crc(void) {
    crsf_parser_t p = {0};
    uint16_t in[16], out[16] = {0};
    uint8_t lq = 0, frame[64];
    for (int i = 0; i < 16; i++) in[i] = (uint16_t)(172 + i * 100);
    uint8_t n = channel_frame(in, frame);
    frame[n - 1] ^= 0x01;
    assert(feed_all(&p, frame, n, out, &lq) == CRSF_NONE);
    for (int i = 0; i < 16; i++) assert(out[i] == 0);
}

static void ignores_other_types(void) {
    crsf_parser_t p = {0};
    uint16_t in[16], out[16] = {0};
    uint8_t lq = 0, frame[64], other[64];
    uint8_t payload[8] = {1, 2, 3, 4, 5, 6, 7, 8};
    for (int i = 0; i < 16; i++) in[i] = (uint16_t)(172 + i * 100);
    uint8_t on = make_frame(0x08, payload, 8, other);
    assert(feed_all(&p, other, on, out, &lq) == CRSF_NONE);
    uint8_t n = channel_frame(in, frame);
    assert(feed_all(&p, frame, n, out, &lq) == CRSF_CHANNELS);
}

static void recovers_after_garbage(void) {
    crsf_parser_t p = {0};
    uint16_t in[16], out[16] = {0};
    uint8_t lq = 0, frame[64];
    const uint8_t garbage[] = {0x00, 0xFF, 0xC8, 0xFF, 0x12};
    for (int i = 0; i < 16; i++) in[i] = (uint16_t)(172 + i * 100);
    uint8_t n = channel_frame(in, frame);
    feed_all(&p, garbage, 5, out, &lq);
    feed_all(&p, frame, n, out, &lq);
    assert(feed_all(&p, frame, n, out, &lq) == CRSF_CHANNELS);
    for (int i = 0; i < 16; i++) assert(out[i] == in[i]);
}

static void recovers_after_truncated(void) {
    crsf_parser_t p = {0};
    uint16_t in[16], out[16] = {0};
    uint8_t lq = 0, frame[64];
    for (int i = 0; i < 16; i++) in[i] = (uint16_t)(172 + i * 100);
    uint8_t n = channel_frame(in, frame);
    feed_all(&p, frame, 10, out, &lq);
    feed_all(&p, frame, n, out, &lq);
    assert(feed_all(&p, frame, n, out, &lq) == CRSF_CHANNELS);
    for (int i = 0; i < 16; i++) assert(out[i] == in[i]);
}

static void rejects_bad_length(void) {
    crsf_parser_t p = {0};
    uint16_t in[16], out[16] = {0};
    uint8_t lq = 0, frame[64];
    const uint8_t short_len[] = {0xC8, 0x01};
    const uint8_t long_len[] = {0xC8, 0x3F};
    for (int i = 0; i < 16; i++) in[i] = (uint16_t)(172 + i * 100);
    assert(feed_all(&p, short_len, 2, out, &lq) == CRSF_NONE);
    assert(feed_all(&p, long_len, 2, out, &lq) == CRSF_NONE);
    uint8_t n = channel_frame(in, frame);
    feed_all(&p, frame, n, out, &lq);
    assert(feed_all(&p, frame, n, out, &lq) == CRSF_CHANNELS);
}

static void parses_known_frame(void) {
    static const uint8_t f[26] = {0xC8, 0x18, 0x16, 0xE0, 0x03, 0x1F, 0xF8, 0xC0, 0x07, 0x3E, 0xF0, 0x81, 0x0F,
                                  0x7C, 0xE0, 0x03, 0x1F, 0xF8, 0xC0, 0x07, 0x3E, 0xF0, 0x81, 0x0F, 0x7C, 0xAD};
    crsf_parser_t p = {0};
    uint16_t ch[16] = {0};
    uint8_t lq = 0;
    crsf_event_t e = CRSF_NONE;
    for (int i = 0; i < 26; i++) e = crsf_feed(&p, f[i], ch, &lq);
    assert(e == CRSF_CHANNELS);
    for (int i = 0; i < 16; i++) assert(ch[i] == 992);
}

int main(void) {
    parses_known_frame();
    crc_check_value();
    parses_channels();
    parses_link_quality();
    rejects_bad_crc();
    ignores_other_types();
    recovers_after_garbage();
    recovers_after_truncated();
    rejects_bad_length();
    puts("test_crsf ok");
    return 0;
}
