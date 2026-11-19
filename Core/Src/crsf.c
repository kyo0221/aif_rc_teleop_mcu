#include "crsf.h"

#define CRSF_SYNC 0xC8
#define CRSF_LEN_MIN 2
#define CRSF_LEN_MAX 62
#define CRSF_TYPE_CHANNELS 0x16
#define CRSF_TYPE_LINK 0x14

uint8_t crsf_crc8(const uint8_t *data, uint8_t len) {
    uint8_t crc = 0;
    for (uint8_t i = 0; i < len; i++) {
        crc ^= data[i];
        for (int b = 0; b < 8; b++) {
            crc = (crc & 0x80) ? (uint8_t)((crc << 1) ^ 0xD5) : (uint8_t)(crc << 1);
        }
    }
    return crc;
}

static void unpack_channels(const uint8_t *payload, uint16_t ch[16]) {
    for (int i = 0; i < 16; i++) {
        uint16_t v = 0;
        for (int b = 0; b < 11; b++) {
            int bit = i * 11 + b;
            if (payload[bit / 8] & (1u << (bit % 8))) v |= (uint16_t)(1u << b);
        }
        ch[i] = v;
    }
}

crsf_event_t crsf_feed(crsf_parser_t *p, uint8_t byte, uint16_t ch[16], uint8_t *lq) {
    if (p->len == 0 && byte != CRSF_SYNC) return CRSF_NONE;
    p->buf[p->len++] = byte;
    if (p->len == 2 && (byte < CRSF_LEN_MIN || byte > CRSF_LEN_MAX)) {
        p->len = 0;
        return CRSF_NONE;
    }
    if (p->len < 2 || p->len != p->buf[1] + 2) return CRSF_NONE;

    uint8_t total = p->len;
    p->len = 0;
    if (crsf_crc8(&p->buf[2], (uint8_t)(total - 3)) != p->buf[total - 1]) return CRSF_NONE;

    uint8_t type = p->buf[2];
    uint8_t payload_len = (uint8_t)(total - 4);
    if (type == CRSF_TYPE_CHANNELS && payload_len == 22) {
        unpack_channels(&p->buf[3], ch);
        return CRSF_CHANNELS;
    }
    if (type == CRSF_TYPE_LINK && payload_len == 10) {
        *lq = p->buf[3 + 2];
        return CRSF_LINK;
    }
    return CRSF_NONE;
}
