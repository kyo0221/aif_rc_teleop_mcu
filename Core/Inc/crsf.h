#ifndef CRSF_H
#define CRSF_H

#include <stdint.h>

typedef struct {
    uint8_t buf[64];
    uint8_t len;
} crsf_parser_t;

typedef enum { CRSF_NONE, CRSF_CHANNELS, CRSF_LINK } crsf_event_t;

uint8_t crsf_crc8(const uint8_t *data, uint8_t len);
crsf_event_t crsf_feed(crsf_parser_t *p, uint8_t byte, uint16_t ch[16], uint8_t *lq);

#endif
