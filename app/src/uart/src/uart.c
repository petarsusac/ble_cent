#include "uart.h"

#include <zephyr/devicetree.h>
#include <zephyr/drivers/uart.h>

static const struct device *const p_uart_dev = DEVICE_DT_GET(DT_NODELABEL(uart1));

int uart_send_blocking(const uint8_t *p_data, size_t len)
{
    for (size_t i = 0; i < len; i++)
    {
        uart_poll_out(p_uart_dev, p_data[i]);
    }

    uart_poll_out(p_uart_dev, UART_END_BYTE);

    return 0;
}
