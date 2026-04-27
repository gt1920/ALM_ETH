/**
 * aes.h - software AES-128/192/256 with CBC chaining via IV
 *         (ported from G0B1_SPI_Bootloader/BSP/AES.c, key/IV refreshed)
 *
 * Block size is fixed at 16 bytes. Decode/Encrypt operate on one 16-byte
 * block at a time; passing a non-NULL IV makes the call CBC-chained:
 *   - Encrypt:  state ^= IV before encrypt; IV updated to ciphertext
 *   - Decode:   plaintext after decrypt is XOR'd with caller's IV;
 *               IV is updated to the input ciphertext for the next call
 */
#ifndef __AES_H__
#define __AES_H__

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

/* IV (16 bytes) and AES-256 Key (32 bytes) — DO NOT publish */
extern unsigned char IV[16];
extern unsigned char Key[32];

/* All ops process a single 16-byte block.
 * IV_IN_OUT may be NULL for raw ECB-style; non-NULL gives CBC chain. */
void Aes_IV_key128bit_Encrypt(unsigned char *IV_IN_OUT, unsigned char *State_IN_OUT, unsigned char *key128bit);
void Aes_IV_key128bit_Decode (unsigned char *IV_IN_OUT, unsigned char *State_IN_OUT, unsigned char *key128bit);
void Aes_IV_key192bit_Encrypt(unsigned char *IV_IN_OUT, unsigned char *State_IN_OUT, unsigned char *key192bit);
void Aes_IV_key192bit_Decode (unsigned char *IV_IN_OUT, unsigned char *State_IN_OUT, unsigned char *key192bit);
void Aes_IV_key256bit_Encrypt(unsigned char *IV_IN_OUT, unsigned char *State_IN_OUT, unsigned char *key256bit);
void Aes_IV_key256bit_Decode (unsigned char *IV_IN_OUT, unsigned char *State_IN_OUT, unsigned char *key256bit);

#ifdef __cplusplus
}
#endif

#endif /* __AES_H__ */
