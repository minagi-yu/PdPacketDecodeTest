// PdPacketDecodeTest.cpp : このファイルには 'main' 関数が含まれています。プログラム実行の開始と終了がそこで行われます。
//

#define _CRT_SECURE_NO_WARNINGS

#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <stdlib.h>

static uint32_t table[] = {
 0x00000000, 0x1db71064, 0x3b6e20c8, 0x26d930ac,
 0x76dc4190, 0x6b6b51f4, 0x4db26158, 0x5005713c,
 0xedb88320, 0xf00f9344, 0xd6d6a3e8, 0xcb61b38c,
 0x9b64c2b0, 0x86d3d2d4, 0xa00ae278, 0xbdbdf21c,
};
//static uint32_t table[] = {
//    0x00000000, 0x4C11DB70, 0x9823B6E0, 0xD4326D90,
//    0x34867077, 0x7897AB07, 0xACA5C697, 0xE0B41DE7,
//    0x690CE0EE, 0x251D3B9E, 0xF12F560E, 0xBD3E8D7E,
//    0x5D8A9099, 0x119B4BE9, 0xC5A92679, 0x89B8FD09
//};

uint32_t crc32_4bits(uint8_t* data, size_t len)
{
    uint32_t crc;
    size_t i;

    crc = 0xffffffff;

    for (i = 0; i < len; i++) {
        crc = table[(crc ^ data[i]) & 0xf] ^ (crc >> 4);
        crc = table[(crc ^ (data[i] >> 4)) & 0xf] ^ (crc >> 4);
    }

    return crc ^ 0xffffffff;
    // return crc;
}

uint32_t crcWrap(uint32_t c)
{
    uint32_t ret = 0;
    uint32_t j, bit;
    //c = ~c;
    for (size_t i = 0; i < 32; i++) {
        j = 31 - i;
        bit = (c >> i) & 1;
        ret |= bit << j;
    }
    return ret;
}

static struct pd_packet
{
    uint_fast8_t sop;
    uint8_t message[(16 + (32 * 8) + 32) / 8];
    uint32_t crc;
} pd_packet;

static bool reset_signal;
static uint_fast8_t shift = 0;

enum pd_phy_decode_state
{
    SOP_DETECT,
    SOP_DECODE,
    MSG_DECODE,
    EOP_DECODE,
};

enum k_code
{
    SYNC_1 = 0x18,
    SYNC_2 = 0x11,
    RST_1 = 0x07,
    RST_2 = 0x19,
    EOP = 0x0d,
    SYNC_3 = 0x06
};

#define ERROR5B4B 0x00
const uint8_t symbol5b4b[] = {
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

const uint8_t SOP_ORDER[7][4] = {
    {SYNC_1, SYNC_1, SYNC_1, SYNC_2},
    {SYNC_1, SYNC_1, SYNC_3, SYNC_3},
    {SYNC_1, SYNC_3, SYNC_1, SYNC_3},
    {SYNC_1, RST_2, RST_2, SYNC_3},
    {SYNC_1, RST_2, SYNC_3, SYNC_2},
    {RST_1, RST_1, RST_1, RST_2},
    {RST_1, SYNC_1, RST_1, SYNC_3}};

static enum pd_phy_decode_state state;

static uint_fast8_t count;

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

    if (d == EOP) {
        printf(", d=%02x, count=%2d, crc=%08x ", symbol5b4b[d], count, pd_packet.crc);
        puts("EOP");
        return;
    }

    if (count < 4)
    {
        soptemp[count] = d;
        if (count == 3)
        {
            pd_packet.sop = validate_sop(soptemp);
            msg = pd_packet.message;
            pd_packet.crc = 0xffffffff;
        }
    }
    else
    {
        d = symbol5b4b[d];
        pd_packet.crc = table[(pd_packet.crc ^ d) & 0xf] ^ (pd_packet.crc >> 4);
        // pd_packet.crc = table[((pd_packet.crc >> (32 - 4)) ^ d) & 0xf] ^ (pd_packet.crc << 4);
        if (count % 2)
        {
            *msg++ |= (d << 4);
        }
        else
        {
            *msg = d;
        }
    }
    printf(", d=%02x, count=%2d, crc=%08x, invcrc=%08x\n", d, count, pd_packet.crc, ~pd_packet.crc);
    count++;
}

static void
pd_recv(uint8_t d)
{
    static uint8_t od;
    bool sop_detected = false;

    if (count == 0)
    {
        shift = 8;
        do {
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

int main(void)
{
    char *packet[] = {
        "AAAAAAAAAAAAAAAA18E3986C5A9AA699FCF47CFB4DBDF7CE7A5DBDF7BA26EA9ED79226AAB7D4DDD9B735758D",
        "AAAAAAAAAAAAAAAA18E39894F7F76A77AFB40D",
        "AAAAAAAAAAAAAAAA18E348A54F9AA6ABE44B77F7CD937F8D",
        "AAAAAAAAAAAAAAAA18E3985CF25DCA27AB550D",
        "AAAAAAAAAAAAAAAA18E3586DF5DD696BFB5E8D",
        "AAAAAAAAAAAAAAAA18E39814F56F6EBFAE530D",
        "AAAAAAAAAAAAAAAA18E3E8ECF23DEDCEF9D40D",
        "AAAAAAAAAAAAAAAA18E39894F2D44AEB76B78D",
        "AAAAAAAAAAAAAAAA18E39826F5524ABDE5D30D",
        "AAAAAAAAAAAAAAAA18E3985CF5D5CEE4AAB28D",
        "AAAAAAAAAAAAAAAA18E3E8EFF38A4F2ADFAD0D",
        "AAAAAAAAAAAAAAAA18E39894F35C4E2D77528D",
        "AAAAAAAAAAAAAAAA18E348A7F2FCF15AF5F20D",
        "AAAAAAAAAAAAAAAA18E398DCF26EEABBD6560D",
        "AAAAAAAAAAAAAAAA18E3E8EFF4952BEFB5DE0D",
        "AAAAAAAAAAAAAAAA18E39894F4532AEE9DB30D",
        "AAAAAAAAAAAAAAAA18E3E8A493D47BEFBDF77DDFADE4E60D",
        "AAAAAAAAAAAAAAAA18E398DCF3F6EE7D57B50D",
        "AAAAAAAAAAAAAAAA18E3E8EFF55D2FC735AD8D",
        "AAAAAAAAAAAAAAAA18E39894F5CB2EC99D548D",
        "AAAAAAAAAAAAAAAA18E3D8A54CC97BEB7DEFCB714F1F558D",
        "AAAAAAAAAAAAAAAA18E398DCF4FBFA5EB9528D",
        "AAAAAAAAAAAAAAAA18E3E8EFF6B67BBDEDDD0D",
        "AAAAAAAAAAAAAAAA18E39894F67E7ABA7DB20D",
        "AAAAAAAAAAAAAAAA18E348A54DCECA4965AD3B75DBE5E20D",
        "AAAAAAAAAAAAAAAA18E398DCF569A6B9B9B58D",
        "AAAAAAAAAAAAAAAA18E3586DF74A79AE955A0D",
        "AAAAAAAAAAAAAAAA18E39814F7FA26557D570D",
        "AAAAAAAAAAAAAAAA18E3E86CF2CECF5A25D78D",
        "AAAAAAAAAAAAAAAA18E39894F7F76A77AFB40D",
    };

    char *p = packet[0];

    count = 0;
    for (size_t i = 0; i < strlen(p); i += 2)
    {
        int d;
        char oct[3] = {0};
        strncpy(oct, p + i, 2);
        (void)sscanf(oct, "%x", &d);
        pd_recv((uint8_t)d);
    }

    printf("SOP = %d\n", pd_packet.sop);
    printf("MESSAGE = ");
    for (size_t i = 0; i < (count - 4) / 2; i++)
    {
        printf("%02X", pd_packet.message[i]);
    }
    puts("");

    printf("crc=%08x\n", *((uint32_t *)&pd_packet.message[((count - 4) / 2) - 4]));
    printf("crc residue shoud be 0xc704dd7b %s", crcWrap(pd_packet.crc) == 0xc704dd7b ? "OK!" : "NG!");
}

// プログラムの実行: Ctrl + F5 または [デバッグ] > [デバッグなしで開始] メニュー
// プログラムのデバッグ: F5 または [デバッグ] > [デバッグの開始] メニュー

// 作業を開始するためのヒント:
//    1. ソリューション エクスプローラー ウィンドウを使用してファイルを追加/管理します
//   2. チーム エクスプローラー ウィンドウを使用してソース管理に接続します
//   3. 出力ウィンドウを使用して、ビルド出力とその他のメッセージを表示します
//   4. エラー一覧ウィンドウを使用してエラーを表示します
//   5. [プロジェクト] > [新しい項目の追加] と移動して新しいコード ファイルを作成するか、[プロジェクト] > [既存の項目の追加] と移動して既存のコード ファイルをプロジェクトに追加します
//   6. 後ほどこのプロジェクトを再び開く場合、[ファイル] > [開く] > [プロジェクト] と移動して .sln ファイルを選択します
