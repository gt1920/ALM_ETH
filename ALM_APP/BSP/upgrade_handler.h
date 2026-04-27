/**
 * upgrade_handler.h - OTA firmware upgrade over TCP
 *
 * PC sends an .alm file in three phases:
 *   1. SUBCMD_UPG_START  (0x01) — file size; device erases W25Q sectors then ACKs
 *   2. SUBCMD_UPG_DATA   (0x02) — 128-byte chunks with offset; device writes W25Q, ACKs each
 *   3. SUBCMD_UPG_END    (0x03) — total size; device verifies, ACKs, then reboots
 *
 * Device responses use SUBCMD_UPG_RESP (0x81) with a 1-byte status code.
 * All frames use the standard [A5][CMD][DIR][SEQ_L][SEQ_H][LEN_BODY][SUBCMD][payload] layout.
 */
#ifndef __UPGRADE_HANDLER_H__
#define __UPGRADE_HANDLER_H__

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

/* Called from Process_ETH_Command when cmd == CMD_UPGRADE */
void UPG_HandleCommand(const uint8_t *buf, uint16_t len);

/* Call from main while(1) — triggers deferred reboot after successful END */
void UPG_PollReboot(void);

#ifdef __cplusplus
}
#endif

#endif /* __UPGRADE_HANDLER_H__ */
