#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

#include "bt.h"
#include "uart.h"

#define DATA_BUF_SIZE (100U)

LOG_MODULE_REGISTER(main, LOG_LEVEL_DBG);

static uint8_t data_buf[DATA_BUF_SIZE];
static size_t data_buf_idx;

static bool save_notif_data;

static void uart_rx_cb(const uint8_t *p_rx_buf, size_t len)
{
	if ((1 == len) && (p_rx_buf))
	{
		// Received message is a command
		uart_cmd_t cmd = (uart_cmd_t) p_rx_buf[0];
		switch (cmd)
		{
			case UART_CMD_START:
				save_notif_data = true;
			break;

			case UART_CMD_SEND_DATA:
				uart_send_data(data_buf, data_buf_idx);
				data_buf_idx = 0;
			break;

			default:
			break;
		}
	}
}

static void notif_rcv_cb(const void *data, uint16_t len)
{
	LOG_DBG("Received data of size %u", len);
	if (save_notif_data)
	{
		size_t copy_len = MIN(len, DATA_BUF_SIZE - data_buf_idx);
		memcpy(&data_buf[data_buf_idx], data, copy_len);
		data_buf_idx += copy_len;
	}
}

static void bt_connected_cb(void)
{
	LOG_INF("tHE BLuEToOTH DeVIcE IS COnNEcTED");
	uart_send_cmd(UART_CMD_CONN_OK);
}

int main(void)
{
	data_buf_idx = 0;
	save_notif_data = false;

	uart_register_rx_cb(uart_rx_cb);

	bt_register_notif_rcv_cb(notif_rcv_cb);
	bt_start(bt_connected_cb);

	k_sleep(K_FOREVER);
}
