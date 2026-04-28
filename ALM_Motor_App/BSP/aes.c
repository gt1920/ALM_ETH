/**
 * aes.c - software AES-128/192/256 with CBC chaining via IV
 *         (ported from ALM_CIC_Bootloader/BSP/aes.c, fresh KEY/IV for Motor track)
 *
 * The KEY+IV defined here MUST stay byte-identical to the copies in
 *   ALM_Motor_Bootloader/BSP/aes.c
 *   Copy_Bin_Motor_Project/Copy_Bin/Program.cs
 * Otherwise the .mot upgrade pipeline breaks. Keep all three in sync.
 */
#include "aes.h"
#include <string.h>

/* IV (16B) — randomly generated for the Motor pipeline */
unsigned char IV[16] = {
    0xCD, 0x11, 0xC5, 0xE4, 0xF8, 0xFB, 0xA2, 0xDC,
    0x39, 0x83, 0xD8, 0x4C, 0x0F, 0x78, 0xAC, 0xE4
};

/* AES-256 Key (32B) — randomly generated for the Motor pipeline */
unsigned char Key[32] = {
    0xD1, 0x16, 0x0B, 0xF7, 0xB6, 0xBA, 0xA6, 0x3D,
    0x8B, 0x96, 0x5F, 0x45, 0x62, 0x24, 0xEC, 0xBA,
    0x1C, 0x80, 0x82, 0x00, 0x94, 0xA9, 0x66, 0x49,
    0x07, 0x71, 0x52, 0xBF, 0x33, 0xDC, 0xE2, 0x2E
};

/* Round-key buffer (orig key + expanded round keys, up to 14 rounds * 16B + 16B) */
static unsigned char Round_Key[256];

/* AES S-box */
static const unsigned char aes_s_box[16][16] = {
    {0x63,0x7c,0x77,0x7b,0xf2,0x6b,0x6f,0xc5,0x30,0x01,0x67,0x2b,0xfe,0xd7,0xab,0x76},
    {0xca,0x82,0xc9,0x7d,0xfa,0x59,0x47,0xf0,0xad,0xd4,0xa2,0xaf,0x9c,0xa4,0x72,0xc0},
    {0xb7,0xfd,0x93,0x26,0x36,0x3f,0xf7,0xcc,0x34,0xa5,0xe5,0xf1,0x71,0xd8,0x31,0x15},
    {0x04,0xc7,0x23,0xc3,0x18,0x96,0x05,0x9a,0x07,0x12,0x80,0xe2,0xeb,0x27,0xb2,0x75},
    {0x09,0x83,0x2c,0x1a,0x1b,0x6e,0x5a,0xa0,0x52,0x3b,0xd6,0xb3,0x29,0xe3,0x2f,0x84},
    {0x53,0xd1,0x00,0xed,0x20,0xfc,0xb1,0x5b,0x6a,0xcb,0xbe,0x39,0x4a,0x4c,0x58,0xcf},
    {0xd0,0xef,0xaa,0xfb,0x43,0x4d,0x33,0x85,0x45,0xf9,0x02,0x7f,0x50,0x3c,0x9f,0xa8},
    {0x51,0xa3,0x40,0x8f,0x92,0x9d,0x38,0xf5,0xbc,0xb6,0xda,0x21,0x10,0xff,0xf3,0xd2},
    {0xcd,0x0c,0x13,0xec,0x5f,0x97,0x44,0x17,0xc4,0xa7,0x7e,0x3d,0x64,0x5d,0x19,0x73},
    {0x60,0x81,0x4f,0xdc,0x22,0x2a,0x90,0x88,0x46,0xee,0xb8,0x14,0xde,0x5e,0x0b,0xdb},
    {0xe0,0x32,0x3a,0x0a,0x49,0x06,0x24,0x5c,0xc2,0xd3,0xac,0x62,0x91,0x95,0xe4,0x79},
    {0xe7,0xc8,0x37,0x6d,0x8d,0xd5,0x4e,0xa9,0x6c,0x56,0xf4,0xea,0x65,0x7a,0xae,0x08},
    {0xba,0x78,0x25,0x2e,0x1c,0xa6,0xb4,0xc6,0xe8,0xdd,0x74,0x1f,0x4b,0xbd,0x8b,0x8a},
    {0x70,0x3e,0xb5,0x66,0x48,0x03,0xf6,0x0e,0x61,0x35,0x57,0xb9,0x86,0xc1,0x1d,0x9e},
    {0xe1,0xf8,0x98,0x11,0x69,0xd9,0x8e,0x94,0x9b,0x1e,0x87,0xe9,0xce,0x55,0x28,0xdf},
    {0x8c,0xa1,0x89,0x0d,0xbf,0xe6,0x42,0x68,0x41,0x99,0x2d,0x0f,0xb0,0x54,0xbb,0x16}
};

/* AES inverse S-box */
static const unsigned char aes_inv_s_box[16][16] = {
    {0x52,0x09,0x6a,0xd5,0x30,0x36,0xa5,0x38,0xbf,0x40,0xa3,0x9e,0x81,0xf3,0xd7,0xfb},
    {0x7c,0xe3,0x39,0x82,0x9b,0x2f,0xff,0x87,0x34,0x8e,0x43,0x44,0xc4,0xde,0xe9,0xcb},
    {0x54,0x7b,0x94,0x32,0xa6,0xc2,0x23,0x3d,0xee,0x4c,0x95,0x0b,0x42,0xfa,0xc3,0x4e},
    {0x08,0x2e,0xa1,0x66,0x28,0xd9,0x24,0xb2,0x76,0x5b,0xa2,0x49,0x6d,0x8b,0xd1,0x25},
    {0x72,0xf8,0xf6,0x64,0x86,0x68,0x98,0x16,0xd4,0xa4,0x5c,0xcc,0x5d,0x65,0xb6,0x92},
    {0x6c,0x70,0x48,0x50,0xfd,0xed,0xb9,0xda,0x5e,0x15,0x46,0x57,0xa7,0x8d,0x9d,0x84},
    {0x90,0xd8,0xab,0x00,0x8c,0xbc,0xd3,0x0a,0xf7,0xe4,0x58,0x05,0xb8,0xb3,0x45,0x06},
    {0xd0,0x2c,0x1e,0x8f,0xca,0x3f,0x0f,0x02,0xc1,0xaf,0xbd,0x03,0x01,0x13,0x8a,0x6b},
    {0x3a,0x91,0x11,0x41,0x4f,0x67,0xdc,0xea,0x97,0xf2,0xcf,0xce,0xf0,0xb4,0xe6,0x73},
    {0x96,0xac,0x74,0x22,0xe7,0xad,0x35,0x85,0xe2,0xf9,0x37,0xe8,0x1c,0x75,0xdf,0x6e},
    {0x47,0xf1,0x1a,0x71,0x1d,0x29,0xc5,0x89,0x6f,0xb7,0x62,0x0e,0xaa,0x18,0xbe,0x1b},
    {0xfc,0x56,0x3e,0x4b,0xc6,0xd2,0x79,0x20,0x9a,0xdb,0xc0,0xfe,0x78,0xcd,0x5a,0xf4},
    {0x1f,0xdd,0xa8,0x33,0x88,0x07,0xc7,0x31,0xb1,0x12,0x10,0x59,0x27,0x80,0xec,0x5f},
    {0x60,0x51,0x7f,0xa9,0x19,0xb5,0x4a,0x0d,0x2d,0xe5,0x7a,0x9f,0x93,0xc9,0x9c,0xef},
    {0xa0,0xe0,0x3b,0x4d,0xae,0x2a,0xf5,0xb0,0xc8,0xeb,0xbb,0x3c,0x83,0x53,0x99,0x61},
    {0x17,0x2b,0x04,0x7e,0xba,0x77,0xd6,0x26,0xe1,0x69,0x14,0x63,0x55,0x21,0x0c,0x7d}
};

/* MixColumns matrix */
static const unsigned char aes_MixColumns[4][4] = {
    {0x02, 0x03, 0x01, 0x01},
    {0x01, 0x02, 0x03, 0x01},
    {0x01, 0x01, 0x02, 0x03},
    {0x03, 0x01, 0x01, 0x02}
};

/* Inverse MixColumns matrix */
static const unsigned char aes_invMixColumns[4][4] = {
    {0x0E, 0x0B, 0x0D, 0x09},
    {0x09, 0x0E, 0x0B, 0x0D},
    {0x0D, 0x09, 0x0E, 0x0B},
    {0x0B, 0x0D, 0x09, 0x0E}
};

/* Round constants */
static const unsigned char aes_Rcon[14] = {
    0x01, 0x02, 0x04, 0x08, 0x10, 0x20, 0x40, 0x80,
    0x1b, 0x36, 0x6c, 0xd8, 0xab, 0x4d
};

/* SubBytes */
static void Aes_SubBytes(unsigned char *Byte_IN_OUT, unsigned int Len)
{
    unsigned int i;
    for (i = 0; i < Len; i++)
        Byte_IN_OUT[i] = aes_s_box[Byte_IN_OUT[i] >> 4][Byte_IN_OUT[i] & 0x0f];
}

/* Inverse SubBytes */
static void Aes_invSubBytes(unsigned char *Byte_IN_OUT, unsigned int Len)
{
    unsigned int i;
    for (i = 0; i < Len; i++)
        Byte_IN_OUT[i] = aes_inv_s_box[Byte_IN_OUT[i] >> 4][Byte_IN_OUT[i] & 0x0f];
}

/* Rotate one row of the state matrix left by `Number` bytes */
static void ShiftByteNumRows(unsigned char *SrcInOut, unsigned char Rows, unsigned char Number)
{
    unsigned char Data[10];
    unsigned char i;
    if ((Number > 10) || (Number == 0)) return;
    for (i = 0; i < Number; i++)
        Data[i] = SrcInOut[Rows + 4 * i];
    for (i = 0; i < (4 - Number); i++)
        SrcInOut[Rows + 4 * i] = SrcInOut[Rows + 4 * (Number + i)];
    for (i = 0; i < Number; i++)
        SrcInOut[Rows + 4 * ((4 - Number) + i)] = Data[i];
}

static void Aes_ShiftRows(unsigned char *SrcInOut)
{
    ShiftByteNumRows(SrcInOut, 1, 1);
    ShiftByteNumRows(SrcInOut, 2, 2);
    ShiftByteNumRows(SrcInOut, 3, 3);
}

static void Aes_invShiftRows(unsigned char *SrcInOut)
{
    ShiftByteNumRows(SrcInOut, 1, 3);
    ShiftByteNumRows(SrcInOut, 2, 2);
    ShiftByteNumRows(SrcInOut, 3, 1);
}

/* GF(2^8) multiplication */
static unsigned char Get_Calculate_GF28(unsigned char a, unsigned char b)
{
    unsigned char val = 0;
    unsigned char i;
    for (i = 0; i < 8; i++)
    {
        if (b & 0x01) val ^= a;
        a = (a << 1) ^ ((a & 0x80) ? 0x1b : 0);
        b >>= 1;
    }
    return val;
}

static void Aes_MixColumns(unsigned char *S)
{
    unsigned char buf[4], out[4];
    unsigned char c;
    for (c = 0; c < 4; c++)
    {
        memcpy(buf, &S[c * 4], 4);
        out[0] = Get_Calculate_GF28(buf[0], aes_MixColumns[0][0]) ^ Get_Calculate_GF28(buf[1], aes_MixColumns[0][1]) ^
                 Get_Calculate_GF28(buf[2], aes_MixColumns[0][2]) ^ Get_Calculate_GF28(buf[3], aes_MixColumns[0][3]);
        out[1] = Get_Calculate_GF28(buf[0], aes_MixColumns[1][0]) ^ Get_Calculate_GF28(buf[1], aes_MixColumns[1][1]) ^
                 Get_Calculate_GF28(buf[2], aes_MixColumns[1][2]) ^ Get_Calculate_GF28(buf[3], aes_MixColumns[1][3]);
        out[2] = Get_Calculate_GF28(buf[0], aes_MixColumns[2][0]) ^ Get_Calculate_GF28(buf[1], aes_MixColumns[2][1]) ^
                 Get_Calculate_GF28(buf[2], aes_MixColumns[2][2]) ^ Get_Calculate_GF28(buf[3], aes_MixColumns[2][3]);
        out[3] = Get_Calculate_GF28(buf[0], aes_MixColumns[3][0]) ^ Get_Calculate_GF28(buf[1], aes_MixColumns[3][1]) ^
                 Get_Calculate_GF28(buf[2], aes_MixColumns[3][2]) ^ Get_Calculate_GF28(buf[3], aes_MixColumns[3][3]);
        memcpy(&S[c * 4], out, 4);
    }
}

static void Aes_invMixColumns(unsigned char *S)
{
    unsigned char buf[4], out[4];
    unsigned char c;
    for (c = 0; c < 4; c++)
    {
        memcpy(buf, &S[c * 4], 4);
        out[0] = Get_Calculate_GF28(buf[0], aes_invMixColumns[0][0]) ^ Get_Calculate_GF28(buf[1], aes_invMixColumns[0][1]) ^
                 Get_Calculate_GF28(buf[2], aes_invMixColumns[0][2]) ^ Get_Calculate_GF28(buf[3], aes_invMixColumns[0][3]);
        out[1] = Get_Calculate_GF28(buf[0], aes_invMixColumns[1][0]) ^ Get_Calculate_GF28(buf[1], aes_invMixColumns[1][1]) ^
                 Get_Calculate_GF28(buf[2], aes_invMixColumns[1][2]) ^ Get_Calculate_GF28(buf[3], aes_invMixColumns[1][3]);
        out[2] = Get_Calculate_GF28(buf[0], aes_invMixColumns[2][0]) ^ Get_Calculate_GF28(buf[1], aes_invMixColumns[2][1]) ^
                 Get_Calculate_GF28(buf[2], aes_invMixColumns[2][2]) ^ Get_Calculate_GF28(buf[3], aes_invMixColumns[2][3]);
        out[3] = Get_Calculate_GF28(buf[0], aes_invMixColumns[3][0]) ^ Get_Calculate_GF28(buf[1], aes_invMixColumns[3][1]) ^
                 Get_Calculate_GF28(buf[2], aes_invMixColumns[3][2]) ^ Get_Calculate_GF28(buf[3], aes_invMixColumns[3][3]);
        memcpy(&S[c * 4], out, 4);
    }
}

/* G() in key expansion */
static void Aes_G_function(unsigned char *W, unsigned char round_j)
{
    unsigned char tmp = W[0];
    memcpy(W, &W[1], 3);
    W[3] = tmp;
    Aes_SubBytes(W, 4);
    W[0] ^= aes_Rcon[round_j];
}

static void Aes_Bytes_Xor(unsigned char *Byte_IO, unsigned char *Byte_I, unsigned int Len)
{
    while (Len--)
    {
        *Byte_IO++ ^= *Byte_I++;
    }
}

/* Generate full round-key schedule from raw key */
static void Key_Schedule(unsigned char *KeyData_In, unsigned char KeySize)
{
    unsigned char Datatemp[4];
    unsigned char Datatemp0[4];
    unsigned char Count;
    unsigned char Round_Count;

    if      (KeySize == 16) Round_Count = 10;
    else if (KeySize == 24) Round_Count = 12;
    else if (KeySize == 32) Round_Count = 14;
    else return;

    memcpy(Datatemp0, &KeyData_In[KeySize - 4], 4);

    for (Count = 0; Count < (Round_Count * 4 - ((KeySize - 16) / 8) * 2); Count++)
    {
        if ((Count % (KeySize / 4)) == 0)
        {
            Aes_G_function(Datatemp0, Count / (KeySize / 4));
        }
        else
        {
            if (((Count % (KeySize / 4)) == 4) && (KeySize == 32))
                Aes_SubBytes(Datatemp0, 4);
        }
        memcpy(Datatemp, &KeyData_In[Count * 4], 4);
        Aes_Bytes_Xor(Datatemp0, Datatemp, 4);
        memcpy(&KeyData_In[KeySize + Count * 4], Datatemp0, 4);
    }
}

static void Aes_Key_Schedule_Create(unsigned char *KeyData, unsigned char KeyByteSize)
{
    memcpy(Round_Key, KeyData, KeyByteSize);
    Key_Schedule(Round_Key, KeyByteSize);
}

/* ===== AES-128 ===== */
void Aes_IV_key128bit_Encrypt(unsigned char *IV_IN_OUT, unsigned char *State_IN_OUT, unsigned char *key128bit)
{
    unsigned char Count;
    if (key128bit == NULL) return;
    if (IV_IN_OUT != NULL) Aes_Bytes_Xor(State_IN_OUT, IV_IN_OUT, 16);
    Aes_Key_Schedule_Create(key128bit, 16);
    Aes_Bytes_Xor(State_IN_OUT, &Round_Key[0], 16);
    for (Count = 1; Count < 10; Count++)
    {
        Aes_SubBytes (State_IN_OUT, 16);
        Aes_ShiftRows(State_IN_OUT);
        Aes_MixColumns(State_IN_OUT);
        Aes_Bytes_Xor(State_IN_OUT, &Round_Key[Count * 16], 16);
    }
    Aes_SubBytes (State_IN_OUT, 16);
    Aes_ShiftRows(State_IN_OUT);
    Aes_Bytes_Xor(State_IN_OUT, &Round_Key[16 * Count], 16);
    if (IV_IN_OUT != NULL) memcpy(IV_IN_OUT, State_IN_OUT, 16);
}

void Aes_IV_key128bit_Decode(unsigned char *IV_IN_OUT, unsigned char *State_IN_OUT, unsigned char *key128bit)
{
    unsigned char Count;
    unsigned char Temp[16];
    if (key128bit == NULL) return;
    if (IV_IN_OUT != NULL) memcpy(Temp, State_IN_OUT, 16);
    Aes_Key_Schedule_Create(key128bit, 16);
    Aes_Bytes_Xor(State_IN_OUT, &Round_Key[10 * 16], 16);
    Aes_invShiftRows(State_IN_OUT);
    Aes_invSubBytes(State_IN_OUT, 16);
    for (Count = 9; Count > 0; Count--)
    {
        Aes_Bytes_Xor (State_IN_OUT, &Round_Key[Count * 16], 16);
        Aes_invMixColumns(State_IN_OUT);
        Aes_invShiftRows (State_IN_OUT);
        Aes_invSubBytes  (State_IN_OUT, 16);
    }
    Aes_Bytes_Xor(State_IN_OUT, &Round_Key[16 * Count], 16);
    if (IV_IN_OUT != NULL)
    {
        Aes_Bytes_Xor(State_IN_OUT, IV_IN_OUT, 16);
        memcpy(IV_IN_OUT, Temp, 16);
    }
}

/* ===== AES-192 ===== */
void Aes_IV_key192bit_Encrypt(unsigned char *IV_IN_OUT, unsigned char *State_IN_OUT, unsigned char *key192bit)
{
    unsigned char Count;
    if (key192bit == NULL) return;
    if (IV_IN_OUT != NULL) Aes_Bytes_Xor(State_IN_OUT, IV_IN_OUT, 16);
    Aes_Key_Schedule_Create(key192bit, 24);
    Aes_Bytes_Xor(State_IN_OUT, &Round_Key[0], 16);
    for (Count = 1; Count < 12; Count++)
    {
        Aes_SubBytes (State_IN_OUT, 16);
        Aes_ShiftRows(State_IN_OUT);
        Aes_MixColumns(State_IN_OUT);
        Aes_Bytes_Xor(State_IN_OUT, &Round_Key[Count * 16], 16);
    }
    Aes_SubBytes (State_IN_OUT, 16);
    Aes_ShiftRows(State_IN_OUT);
    Aes_Bytes_Xor(State_IN_OUT, &Round_Key[Count * 16], 16);
    if (IV_IN_OUT != NULL) memcpy(IV_IN_OUT, State_IN_OUT, 16);
}

void Aes_IV_key192bit_Decode(unsigned char *IV_IN_OUT, unsigned char *State_IN_OUT, unsigned char *key192bit)
{
    unsigned char Count;
    unsigned char Temp[16];
    if (key192bit == NULL) return;
    if (IV_IN_OUT != NULL) memcpy(Temp, State_IN_OUT, 16);
    Aes_Key_Schedule_Create(key192bit, 24);
    Aes_Bytes_Xor(State_IN_OUT, &Round_Key[12 * 16], 16);
    Aes_invShiftRows(State_IN_OUT);
    Aes_invSubBytes (State_IN_OUT, 16);
    for (Count = 11; Count > 0; Count--)
    {
        Aes_Bytes_Xor (State_IN_OUT, &Round_Key[Count * 16], 16);
        Aes_invMixColumns(State_IN_OUT);
        Aes_invShiftRows (State_IN_OUT);
        Aes_invSubBytes  (State_IN_OUT, 16);
    }
    Aes_Bytes_Xor(State_IN_OUT, &Round_Key[16 * Count], 16);
    if (IV_IN_OUT != NULL)
    {
        Aes_Bytes_Xor(State_IN_OUT, IV_IN_OUT, 16);
        memcpy(IV_IN_OUT, Temp, 16);
    }
}

/* ===== AES-256 ===== */
void Aes_IV_key256bit_Encrypt(unsigned char *IV_IN_OUT, unsigned char *State_IN_OUT, unsigned char *key256bit)
{
    unsigned char Count;
    if (key256bit == NULL) return;
    if (IV_IN_OUT != NULL) Aes_Bytes_Xor(State_IN_OUT, IV_IN_OUT, 16);
    Aes_Key_Schedule_Create(key256bit, 32);
    Aes_Bytes_Xor(State_IN_OUT, &Round_Key[0], 16);
    for (Count = 1; Count < 14; Count++)
    {
        Aes_SubBytes (State_IN_OUT, 16);
        Aes_ShiftRows(State_IN_OUT);
        Aes_MixColumns(State_IN_OUT);
        Aes_Bytes_Xor(State_IN_OUT, &Round_Key[Count * 16], 16);
    }
    Aes_SubBytes (State_IN_OUT, 16);
    Aes_ShiftRows(State_IN_OUT);
    Aes_Bytes_Xor(State_IN_OUT, &Round_Key[Count * 16], 16);
    if (IV_IN_OUT != NULL) memcpy(IV_IN_OUT, State_IN_OUT, 16);
}

void Aes_IV_key256bit_Decode(unsigned char *IV_IN_OUT, unsigned char *State_IN_OUT, unsigned char *key256bit)
{
    unsigned char Count;
    unsigned char Temp[16];
    if (key256bit == NULL) return;
    if (IV_IN_OUT != NULL) memcpy(Temp, State_IN_OUT, 16);
    Aes_Key_Schedule_Create(key256bit, 32);
    Aes_Bytes_Xor(State_IN_OUT, &Round_Key[14 * 16], 16);
    Aes_invShiftRows(State_IN_OUT);
    Aes_invSubBytes (State_IN_OUT, 16);
    for (Count = 13; Count > 0; Count--)
    {
        Aes_Bytes_Xor (State_IN_OUT, &Round_Key[Count * 16], 16);
        Aes_invMixColumns(State_IN_OUT);
        Aes_invShiftRows (State_IN_OUT);
        Aes_invSubBytes  (State_IN_OUT, 16);
    }
    Aes_Bytes_Xor(State_IN_OUT, &Round_Key[16 * Count], 16);
    if (IV_IN_OUT != NULL)
    {
        Aes_Bytes_Xor(State_IN_OUT, IV_IN_OUT, 16);
        memcpy(IV_IN_OUT, Temp, 16);
    }
}
