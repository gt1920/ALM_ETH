#ifndef __SHA256_H
#define __SHA256_H

#include <stdint.h>
#include <string.h>

// 手动定义 size_t（如果编译器没有定义）
#ifndef size_t
typedef unsigned int size_t;
#endif

#define SHA256_BLOCK_SIZE 32  // SHA256 输出长度为 32 字节

// SHA-256 上下文结构体
typedef struct {
    uint32_t data[64];
    uint32_t state[8];
    uint32_t datalen;
    uint64_t bitlen;
} SHA256_CTX;

// SHA256 函数声明
void SHA256_Init(SHA256_CTX *ctx);
void SHA256_Update(SHA256_CTX *ctx, const uint8_t *data, size_t len);
void SHA256_Final(SHA256_CTX *ctx, uint8_t *hash);
void SHA256_Transform(SHA256_CTX *ctx, const uint8_t data[]);

#endif // __SHA256_H
