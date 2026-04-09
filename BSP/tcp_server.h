#ifndef __TCP_SERVER_H__
#define __TCP_SERVER_H__

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdbool.h>

#define TCP_SERVER_PORT  40000

void TCP_Server_Init(void);
bool TCP_Server_IsConnected(void);
bool TCP_Server_Send(const uint8_t *data, uint16_t len);

#ifdef __cplusplus
}
#endif

#endif /* __TCP_SERVER_H__ */
