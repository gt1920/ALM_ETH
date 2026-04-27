/**
 * tcp_server.c - lwIP raw API TCP server on port 40000
 *
 * Single-client: accepts one connection at a time.
 * Frames are length-prefixed: [LEN_L][LEN_H][body...]
 * where body is the 64-byte protocol frame.
 */
#include "tcp_server.h"
#include "comm_protocol.h"

#include "lwip/tcp.h"
#include <string.h>

/* ---- TCP stream reassembly buffer ---- */
#define RX_BUF_SIZE  256
#define LEN_PREFIX   2       /* 2-byte LE length prefix */

/* ---- State ---- */
static struct tcp_pcb *server_pcb = NULL;
static struct tcp_pcb *client_pcb = NULL;

static uint8_t  rx_buf[RX_BUF_SIZE];
static uint16_t rx_buf_pos = 0;

/* ---- Forward declarations ---- */
static err_t accept_cb(void *arg, struct tcp_pcb *newpcb, err_t err);
static err_t recv_cb(void *arg, struct tcp_pcb *tpcb, struct pbuf *p, err_t err);
static void  err_cb(void *arg, err_t err);
static err_t sent_cb(void *arg, struct tcp_pcb *tpcb, u16_t len);

/* =========================================================
 * Init
 * ========================================================= */
void TCP_Server_Init(void)
{
    struct tcp_pcb *pcb = tcp_new();
    if (pcb == NULL)
        return;

    tcp_bind(pcb, IP_ADDR_ANY, TCP_SERVER_PORT);

    server_pcb = tcp_listen(pcb);
    if (server_pcb == NULL)
    {
        tcp_close(pcb);
        return;
    }

    tcp_accept(server_pcb, accept_cb);
}

/* =========================================================
 * Accept
 * ========================================================= */
static err_t accept_cb(void *arg, struct tcp_pcb *newpcb, err_t err)
{
    (void)arg;
    (void)err;

    /* Only allow one client at a time */
    if (client_pcb != NULL)
    {
        tcp_abort(newpcb);
        return ERR_ABRT;
    }

    client_pcb = newpcb;
    rx_buf_pos = 0;

    tcp_recv(newpcb, recv_cb);
    tcp_err(newpcb, err_cb);
    tcp_sent(newpcb, sent_cb);

    /* Enable TCP keepalive */
    newpcb->so_options |= SOF_KEEPALIVE;
    newpcb->keep_idle  = 10000;  /* 10s idle before first probe */
    newpcb->keep_intvl = 5000;   /* 5s between probes */
    newpcb->keep_cnt   = 3;      /* 3 probes before drop */

    return ERR_OK;
}

/* =========================================================
 * Receive - stream reassembly + frame extraction
 * ========================================================= */
static err_t recv_cb(void *arg, struct tcp_pcb *tpcb, struct pbuf *p, err_t err)
{
    (void)arg;
    (void)err;

    /* NULL pbuf = remote closed connection */
    if (p == NULL)
    {
        client_pcb = NULL;
        rx_buf_pos = 0;
        tcp_close(tpcb);
        return ERR_OK;
    }

    /* Accumulate data into rx_buf */
    struct pbuf *q;
    for (q = p; q != NULL; q = q->next)
    {
        uint16_t copy_len = q->len;
        if (rx_buf_pos + copy_len > RX_BUF_SIZE)
            copy_len = RX_BUF_SIZE - rx_buf_pos;   /* prevent overflow */

        if (copy_len > 0)
        {
            memcpy(&rx_buf[rx_buf_pos], q->payload, copy_len);
            rx_buf_pos += copy_len;
        }
    }

    tcp_recved(tpcb, p->tot_len);
    pbuf_free(p);

    /* Extract complete frames: [LEN_L][LEN_H][body...] */
    while (rx_buf_pos >= LEN_PREFIX)
    {
        uint16_t frame_len = (uint16_t)rx_buf[0] | ((uint16_t)rx_buf[1] << 8);

        /* Sanity check: reject absurdly large frames */
        if (frame_len > (RX_BUF_SIZE - LEN_PREFIX))
        {
            /* Protocol error -- discard all buffered data */
            rx_buf_pos = 0;
            break;
        }

        uint16_t total = LEN_PREFIX + frame_len;
        if (rx_buf_pos < total)
            break;   /* not enough data yet */

        /* Process the frame body */
        Process_ETH_Command(&rx_buf[LEN_PREFIX], frame_len);

        /* Shift remaining bytes to front */
        uint16_t remaining = rx_buf_pos - total;
        if (remaining > 0)
            memmove(rx_buf, &rx_buf[total], remaining);
        rx_buf_pos = remaining;
    }

    return ERR_OK;
}

/* =========================================================
 * Error callback
 * ========================================================= */
static void err_cb(void *arg, err_t err)
{
    (void)arg;
    (void)err;

    /* lwIP has already freed the PCB */
    client_pcb = NULL;
    rx_buf_pos = 0;
}

/* =========================================================
 * Sent callback (back-pressure awareness, currently unused)
 * ========================================================= */
static err_t sent_cb(void *arg, struct tcp_pcb *tpcb, u16_t len)
{
    (void)arg;
    (void)tpcb;
    (void)len;
    return ERR_OK;
}

/* =========================================================
 * Public API
 * ========================================================= */
bool TCP_Server_IsConnected(void)
{
    return (client_pcb != NULL);
}

bool TCP_Server_Send(const uint8_t *data, uint16_t len)
{
    if (client_pcb == NULL)
        return false;

    /* Check send buffer space: need 2 (prefix) + len */
    uint16_t needed = LEN_PREFIX + len;
    if (tcp_sndbuf(client_pcb) < needed)
        return false;

    /* Write 2-byte LE length prefix */
    uint8_t prefix[2];
    prefix[0] = (uint8_t)(len & 0xFF);
    prefix[1] = (uint8_t)((len >> 8) & 0xFF);

    err_t err = tcp_write(client_pcb, prefix, 2,
                          TCP_WRITE_FLAG_COPY | TCP_WRITE_FLAG_MORE);
    if (err != ERR_OK)
        return false;

    /* Write frame body */
    err = tcp_write(client_pcb, data, len, TCP_WRITE_FLAG_COPY);
    if (err != ERR_OK)
        return false;

    /* Flush */
    tcp_output(client_pcb);
    return true;
}
