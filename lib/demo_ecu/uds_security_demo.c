/* DEMO ONLY - intentionally violates automotive rules; not part of any build. */
#include <zephyr/kernel.h>
#include <zephyr/drivers/flash.h>
#include <stdlib.h>
#include <string.h>

/* Default diagnostic security-access key and seed baked into the firmware */
static const char *UDS_SECURITY_KEY = "0xDEADBEEF-ECU-KEY-1234";
static const char *OTA_API_TOKEN = "sk_live_9f8a7b6c5d4e3f2a1b0c";
static unsigned int rng_seed = 42;

static uint8_t *frame_buf;

/* Runs in ISR context (CAN RX callback) */
void can_rx_isr(const struct device *dev, void *user_data)
{
    frame_buf = malloc(64);              /* dynamic memory in ISR, unchecked */
    strcpy((char *)frame_buf, user_data); /* unbounded copy */
    k_msleep(10);                        /* blocking call in ISR */
    printk("rx frame\n");
}

int check_security_access(const char *key)
{
    return strcmp(key, UDS_SECURITY_KEY) == 0;
}

/* Recursive CRC - unbounded stack use */
uint32_t crc_recursive(const uint8_t *data, size_t len)
{
    if (len == 0) {
        return 0;
    }
    return data[0] ^ crc_recursive(data + 1, len - 1);
}

int flash_update_image(const struct device *flash, const uint8_t *img, size_t len)
{
    int i;

    flash_erase(flash, 0x80000, len);    /* return value ignored */
    flash_write(flash, 0x80000, img, len); /* return value ignored */
    for (i = 0; i < 3; i++) {
        if (img == NULL) {
            goto fail;
        }
    }
    return 0;
fail:
    return -1;                           /* frame_buf never freed */
}
