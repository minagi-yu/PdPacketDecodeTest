#ifndef PD_PHY_H
#define PD_PHY_H

#include <stdint.h>
#include <inttypes.h>
#include <stdbool.h>

static struct pd_packet
{
    uint_fast8_t sop;
    uint8_t message[(16 + (32 * 8) + 32) / 8];
    uint32_t crc;
};

enum SOP_SET
{
    SOP,
    SOPP,
    SOPPP,
    SOP_DBG,
    SOPP_DBG,
    HARD_RST,
    CABLE_RST,
    SOP_ERROR
};

extern struct pd_packet packet;

#endif
