#include "pd_phy.h"

static const uint32_t CRC32TABLE[] = {
    0x00000000,
    0x1db71064,
    0x3b6e20c8,
    0x26d930ac,
    0x76dc4190,
    0x6b6b51f4,
    0x4db26158,
    0x5005713c,
    0xedb88320,
    0xf00f9344,
    0xd6d6a3e8,
    0xcb61b38c,
    0x9b64c2b0,
    0x86d3d2d4,
    0xa00ae278,
    0xbdbdf21c,
};

enum K_CODE
{
    SYNC_1 = 0x18,
    SYNC_2 = 0x11,
    RST_1 = 0x07,
    RST_2 = 0x19,
    EOP = 0x0d,
    SYNC_3 = 0x06
};

#define ERROR5B4B 0x00
const uint8_t SYMBOL5B4B[] = {
    ERROR5B4B, //00
    ERROR5B4B, //01
    ERROR5B4B, //02
    ERROR5B4B, //03
    ERROR5B4B, //04
    ERROR5B4B, //05
    SYNC_3,    //06
    RST_1,     //07
    ERROR5B4B, //08
    0x01,      //09
    0x04,      //0A
    0x05,      //0B
    ERROR5B4B, //0C
    EOP,       //0D
    0x06,      //0E
    0x07,      //0F
    ERROR5B4B, //10
    SYNC_2,    //11
    0x08,      //12
    0x09,      //13
    0x02,      //14
    0x03,      //15
    0x0a,      //16
    0x0b,      //17
    SYNC_1,    //18
    RST_2,     //19
    0x0c,      //1A
    0x0d,      //1B
    0x0e,      //1C
    0x0f,      //1D
    0x00,      //1E
    ERROR5B4B  //1F
};

const uint8_t SOP_ORDER[7][4] = {
    {SYNC_1, SYNC_1, SYNC_1, SYNC_2},
    {SYNC_1, SYNC_1, SYNC_3, SYNC_3},
    {SYNC_1, SYNC_3, SYNC_1, SYNC_3},
    {SYNC_1, RST_2, RST_2, SYNC_3},
    {SYNC_1, RST_2, SYNC_3, SYNC_2},
    {RST_1, RST_1, RST_1, RST_2},
    {RST_1, SYNC_1, RST_1, SYNC_3}};

static uint_fast8_t count = 0;
static bool reset_signal = false;
static uint_fast8_t shift = 0;
struct pd_packet packet;

static uint_fast8_t validate_sop(uint8_t *d)
{
    uint_fast8_t valid[SOP_ERROR] = {0};

    for (uint_fast8_t i = 0; i < 4; i++)
    {
        for (uint_fast8_t j = 0; j < SOP_ERROR; j++)
        {
            if (*d == SOP_ORDER[j][i])
                valid[j]++;
        }
        d++;
    }

    for (uint_fast8_t i = 0; i < SOP_ERROR; i++)
    {
        if (valid[i] >= 3)
            return i;
    }
    return SOP_ERROR;
}

static void store_data(uint8_t d)
{
    static uint8_t soptemp[4];
    static uint8_t *msg;

    if ((d == EOP) || (count > sizeof(packet.message) + 4))
    {
        puts("EOP");
        return;
    }

    if (count < 4)
    {
        soptemp[count] = d;
        if (count == 3)
        {
            packet.sop = validate_sop(soptemp);
            msg = packet.message;
            packet.crc = 0xffffffff;
        }
    }
    else
    {
        d = SYMBOL5B4B[d];
        packet.crc = CRC32TABLE[(packet.crc ^ d) & 0xf] ^ (packet.crc >> 4);
        if (count % 2)
        {
            *msg++ |= (d << 4);
        }
        else
        {
            *msg = d;
        }
    }
    count++;
}

void pd_init()
{
    count = 0;
    reset_signal = false;
    shift = 0;
}

void pd_recv(uint8_t d)
{
    static uint8_t od;
    bool sop_detected = false;

    if (count == 0)
    {
        shift = 8;
        do
        {
            uint8_t tmp = (d << (8 - shift)) | (od >> shift);
            tmp &= 0x1f;
            if (tmp == SYNC_1)
            { // Sync-1
                reset_signal = false;
                sop_detected = true;
                break;
            }
            if (tmp == RST_1)
            { // RST-1
                reset_signal = true;
                sop_detected = true;
                break;
            }
        } while (--shift);
        if (!sop_detected)
            return;
    }

    printf("d=%02x, od=%02x, shift=%d, 5b=%02x", d, od, shift, ((d << (8 - shift)) | (od >> shift)) & 0x1f);
    store_data(((d << (8 - shift)) | (od >> shift)) & 0x1f);
    shift = (shift + 5) % 8;
    if (shift == 0)
        shift = 8;
    if (shift < 4)
    {
        printf("d=%02x, od=%02x, shift=%d, 5b=%02x", d, od, shift, (d >> shift) & 0x1f);
        store_data((d >> shift) & 0x1f);
        shift = (shift + 5) % 8;
        if (shift == 0)
            shift = 8;
    }
    od = d;
}
