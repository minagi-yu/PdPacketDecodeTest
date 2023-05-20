// PdPacketDecodeTest.cpp : このファイルには 'main' 関数が含まれています。プログラム実行の開始と終了がそこで行われます。
//

#define _CRT_SECURE_NO_WARNINGS

#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <stdlib.h>

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

#define ERROR5B4B 0xff
const uint8_t symbol5b4b[] = {
    ERROR5B4B,
    ERROR5B4B,
    ERROR5B4B,
    ERROR5B4B,
    ERROR5B4B,
    ERROR5B4B,
    SYNC_3,
    RST_1,
    ERROR5B4B,
    0x01,
    0x04,
    0x05,
    ERROR5B4B,
    EOP,
    0x06,
    0x07,
    ERROR5B4B,
    SYNC_2,
    0x08,
    RST_2,
    0x02,
    0x03,
    0x0a,
    0x0b,
    SYNC_1,
    ERROR5B4B,
    0x0c,
    0x0d,
    0x0e,
    0x0f,
    0x00,
    ERROR5B4B};

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

    if (d == EOP)
        return;

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
        if (count % 2)
        {
            *msg = symbol5b4b[d];
        }
        else
        {
            *msg++ |= (symbol5b4b[d] << 4);
        }
    }
    count++;
}

static void
pd_recv(uint8_t d)
{
    static uint8_t od;
    static uint_fast8_t nibble;
    bool sop_detected = false;
    uint8_t d5b;

    if (count == 0)
    {
        for (shift = 8; shift > 0; shift--)
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
        }
        if (!sop_detected)
            return;
    }

    store_data(((d << (8 - shift)) | (od >> shift)) & 0x1f);
    shift = (shift + 5) % 8;
    if (shift < 4)
    {
        store_data((d >> shift) & 0x1f);
        shift = (shift + 5) % 8;
    }
    od = d;
}

int main(void)
{
    char *packet[] = {
        "AAAAAAAAAAAAAAAA18E3985CA29AA699BCF452FB4DBDF7EED139A95C8D"};

    char *p = packet[0];

    for (size_t i = 0; i < strlen(p); i += 2)
    {
        int d;
        char oct[3] = {0};
        strncpy(oct, p + i, 2);
        (void)sscanf(oct, "%x", &d);
        pd_recv((uint8_t)d);
    }
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
