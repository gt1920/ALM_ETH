
#include "adjuster_process_task.h"
#include "LED.h"
#include "adjuster_CAN_Poll.h"
#include "adjuster_update_task.h"

/* Switch-ACT-style indicator: each pulse is a fixed ON window followed by a
   forced OFF gap. The OFF gap is what makes sustained traffic (e.g. OTA
   UPG_DATA bursts) still produce visible flicker — without it, every new RX
   would re-extend the ON window and the LED would look solid ON. */
#define LED_PULSE_ON_MS   30
#define LED_PULSE_OFF_MS  30

__IO uint32_t signal_led_last_tick;        /* tick of last ON↔OFF transition */
static uint8_t  led_state    = 0;          /* 0 = OFF, 1 = ON */
static uint8_t  rx_pending   = 0;          /* RX seen but waiting for OFF gap */

void process(uint32_t now)
{
    /* Latch any new CAN-RX event from update_data() into rx_pending. We
       don't drive the LED directly here — the state machine below decides
       when the next ON pulse can start (after the forced OFF gap). */
    if (singal_led_on)
    {
        rx_pending = 1;
        singal_led_on = 0;
    }

    if (led_state)
    {
        /* Currently ON — close the pulse after LED_PULSE_ON_MS, regardless
           of whether more frames keep arriving. This is the key bit: under
           OTA we get a flash-write blocking call between iterations and the
           old code re-armed ON every time, pinning the LED solid. */
        if ((uint32_t)(now - signal_led_last_tick) >= LED_PULSE_ON_MS)
        {
            LED_Ctrl(0);
            led_state = 0;
            signal_led_last_tick = now;
        }
    }
    else
    {
        /* Currently OFF — start a new pulse only after the OFF gap has
           elapsed AND there is at least one RX event waiting. */
        if (rx_pending && (uint32_t)(now - signal_led_last_tick) >= LED_PULSE_OFF_MS)
        {
            LED_Ctrl(1);
            led_state = 1;
            signal_led_last_tick = now;
            rx_pending = 0;
        }
    }
}

/* Kept as a no-op for the header export — all timing now lives inside
   process(). Old call sites (if any) link without an edit. */
void signal_led_off(uint32_t now)
{
    (void)now;
}
