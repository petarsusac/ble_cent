#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

#include "bt.h"
#include "uart.h"

#define DATA_BUF_SIZE (100U)

LOG_MODULE_REGISTER(main, LOG_LEVEL_DBG);

K_SEM_DEFINE(bt_start_sem, 0, 1);

static void uart_rx_cb(const uint8_t *p_rx_buf, size_t len)
{
	if (p_rx_buf)
	{
		uart_cmd_t cmd = (uart_cmd_t) p_rx_buf[0];
		switch (cmd)
		{
			case UART_CMD_START:
				k_sem_give(&bt_start_sem);
			break;

			default:
				// Ignore other commands
			break;
		}
	}
}

static void notif_rcv_cb(const void *data, uint16_t len)
{
	LOG_DBG("Received data of size %u", len);
	uart_send_data((const uint8_t *) data, len);
}

static void bt_connected_cb(void)
{
	LOG_INF("tHE BLuEToOTH DeVIcE IS COnNEcTED");
	uart_send_cmd(UART_CMD_CONN_OK);
}

int main(void)
{
	uart_register_rx_cb(uart_rx_cb);

	LOG_DBG("Start");

	k_sem_take(&bt_start_sem, K_FOREVER);

	bt_register_notif_rcv_cb(notif_rcv_cb);
	bt_start(bt_connected_cb);

	return 0;
}
