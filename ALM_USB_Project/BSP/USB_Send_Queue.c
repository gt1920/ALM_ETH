#include "USB_Send_Queue.h"

#define USB_TX_QUEUE_DEPTH   8
#define USB_FRAME_LEN        64

uint16_t usb_drop_cnt = 0;

typedef struct
{
    uint8_t buf[USB_FRAME_LEN];
    uint8_t len;
} USB_TxFrame_t;

static USB_TxFrame_t usb_tx_queue[USB_TX_QUEUE_DEPTH];

static volatile uint8_t usb_tx_head  = 0;
static volatile uint8_t usb_tx_tail  = 0;
static volatile uint8_t usb_tx_count = 0;

/* =========================
 * 入队（非阻塞）
 * ========================= */
bool USB_Send_Queue(const uint8_t *data, uint8_t len)
{
    if (len > USB_FRAME_LEN)
        return false;

    __disable_irq();

    if (usb_tx_count >= USB_TX_QUEUE_DEPTH)
    {
        __enable_irq();
        return false;
    }

    memcpy(usb_tx_queue[usb_tx_head].buf, data, len);
    usb_tx_queue[usb_tx_head].len = len;

    usb_tx_head = (usb_tx_head + 1) % USB_TX_QUEUE_DEPTH;
    usb_tx_count++;

    __enable_irq();

    return true;
}

/* =========================
 * 10ms 发送任务
 * ========================= */
void USB_Tx_Task_10ms(void)
{
    if (usb_tx_count == 0)
        return;

    if (hUsbDeviceFS.dev_state != USBD_STATE_CONFIGURED)
        return;

    uint8_t tail = usb_tx_tail;

    USBD_StatusTypeDef ret = (USBD_StatusTypeDef)USBD_CUSTOM_HID_SendReport(&hUsbDeviceFS,
																																						usb_tx_queue[tail].buf,
																																						usb_tx_queue[tail].len);

    if (ret == USBD_OK)
    {
        __disable_irq();

        usb_tx_tail = (usb_tx_tail + 1) % USB_TX_QUEUE_DEPTH;
        usb_tx_count--;

        __enable_irq();
    }
    /* BUSY → 下一个 10ms 再试 */
}
