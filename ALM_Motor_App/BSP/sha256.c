#include "sha256.h"
#include <string.h>  // 引入string.h头文件，解决memset的警告

// 常量表
static const uint32_t k[64] = {
    0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5,
    0x3956c25b, 0x59f111f1, 0x923f82a4, 0xab1c5ed5,
    0xd807aa98, 0x12835b01, 0x243185be, 0x550c7dc3,
    0x72be5d74, 0x80deb1fe, 0x9bdc06a7, 0xc19bf174,
    0xe49b69c1, 0xefbe4786, 0x0fc19dc6, 0x240ca1cc,
    0x2de92c6f, 0x4a7484aa, 0x5cb0a9dc, 0x76f988da,
    0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7,
    0xc6e00bf3, 0xd5a79147, 0x06ca6351, 0x14292967,
    0x27b70a85, 0x2e1b2138, 0x4d2c6dfc, 0x53380d13,
    0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85,
    0xa2bfe8a1, 0xa81a664b, 0xc24b8b70, 0xc76c51a3,
    0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070,
    0x19a4c116, 0x1e376c08, 0x2e0b8d32, 0x4db1e7d9,
    0x53380d13, 0x650a7354, 0x766a0abb, 0x81c2c92e
};

// 循环左移
#define ROTATE_LEFT(a, b) (((a) << (b)) | ((a) >> (32 - (b))))  // 左循环移位

// 初始化 SHA256 上下文
void SHA256_Init(SHA256_CTX *ctx)
{
    ctx->datalen = 0;
    ctx->bitlen = 0;

    // 初始哈希值（常数值）
    ctx->state[0] = 0x6a09e667;
    ctx->state[1] = 0xbb67ae85;
    ctx->state[2] = 0x3c6ef372;
    ctx->state[3] = 0xa54ff53a;
    ctx->state[4] = 0x510e527f;
    ctx->state[5] = 0x9b05688c;
    ctx->state[6] = 0x1f83d9ab;
    ctx->state[7] = 0x5be0cd19;
}

// SHA256 更新处理数据
void SHA256_Update(SHA256_CTX *ctx, const uint8_t *data, size_t len)
{
    for (size_t i = 0; i < len; i++) {
        ctx->data[ctx->datalen] = data[i];
        ctx->datalen++;

        if (ctx->datalen == 64) {
            // 处理 512 位块
            SHA256_Transform(ctx, (const uint8_t *)ctx->data);  // 强制转换为 uint8_t[]
            ctx->bitlen += 512;
            ctx->datalen = 0;
        }
    }
}

// SHA256 计算最终结果
void SHA256_Final(SHA256_CTX *ctx, uint8_t *hash)
{
    // 填充数据，使得数据长度符合要求
    uint32_t i = ctx->datalen;

    // 填充信息
    if (ctx->datalen < 56) {
        ctx->data[i++] = 0x80;
        while (i < 56) {
            ctx->data[i++] = 0x00;
        }
    } else {
        ctx->data[i++] = 0x80;
        while (i < 64) {
            ctx->data[i++] = 0x00;
        }
        SHA256_Transform(ctx, (const uint8_t *)ctx->data);  // 强制转换为 uint8_t[]
        memset(ctx->data, 0, 56);
    }

    // 最后填充长度信息
    ctx->bitlen += ctx->datalen * 8;
    ctx->data[56] = (ctx->bitlen >> 56) & 0xFF;
    ctx->data[57] = (ctx->bitlen >> 48) & 0xFF;
    ctx->data[58] = (ctx->bitlen >> 40) & 0xFF;
    ctx->data[59] = (ctx->bitlen >> 32) & 0xFF;
    ctx->data[60] = (ctx->bitlen >> 24) & 0xFF;
    ctx->data[61] = (ctx->bitlen >> 16) & 0xFF;
    ctx->data[62] = (ctx->bitlen >> 8) & 0xFF;
    ctx->data[63] = (ctx->bitlen) & 0xFF;
    
    SHA256_Transform(ctx, (const uint8_t *)ctx->data);  // 强制转换为 uint8_t[]

    // 将计算结果复制到 hash
    for (int i = 0; i < 4; i++) {
        hash[i] = (ctx->state[0] >> (24 - i * 8)) & 0xFF;
    }
}

// SHA256 变换函数
void SHA256_Transform(SHA256_CTX *ctx, const uint8_t data[])
{
    uint32_t a, b, c, d, e, f, g, h, t1, t2;
    uint32_t m[64];

    // 填充消息块
    for (int i = 0; i < 16; i++) {
				m[i] = (uint32_t)data[i * 4] << 24 | (uint32_t)data[i * 4 + 1] << 16 |
							 (uint32_t)data[i * 4 + 2] << 8 | (uint32_t)data[i * 4 + 3];
    }

    for (int i = 16; i < 64; i++) {
        m[i] = m[i - 16] + (ROTATE_LEFT(m[i - 15], 7) ^ ROTATE_LEFT(m[i - 15], 18) ^ (m[i - 15] >> 3)) +
               m[i - 7] + (ROTATE_LEFT(m[i - 2], 17) ^ ROTATE_LEFT(m[i - 2], 19) ^ (m[i - 2] >> 10));
    }

    // 初始化哈希变量
    a = ctx->state[0];
    b = ctx->state[1];
    c = ctx->state[2];
    d = ctx->state[3];
    e = ctx->state[4];
    f = ctx->state[5];
    g = ctx->state[6];
    h = ctx->state[7];

    // 压缩函数
    for (int i = 0; i < 64; i++) {
        t1 = h + (ROTATE_LEFT(e, 6) ^ ROTATE_LEFT(e, 11) ^ ROTATE_LEFT(e, 25)) + ((e & f) ^ ((~e) & g)) + k[i] + m[i];
        t2 = (ROTATE_LEFT(a, 2) ^ ROTATE_LEFT(a, 13) ^ ROTATE_LEFT(a, 22)) + ((a & b) ^ (a & c) ^ (b & c));
        h = g;
        g = f;
        f = e;
        e = d + t1;
        d = c;
        c = b;
        b = a;
        a = t1 + t2;
    }

    // 更新哈希值
    ctx->state[0] += a;
    ctx->state[1] += b;
    ctx->state[2] += c;
    ctx->state[3] += d;
    ctx->state[4] += e;
    ctx->state[5] += f;
    ctx->state[6] += g;
    ctx->state[7] += h;
}
