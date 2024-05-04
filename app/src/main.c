#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

#include "bt.h"
#include "uart.h"

LOG_MODULE_REGISTER(main, LOG_LEVEL_DBG);

static void notif_rcv_cb(const void *data, uint16_t len)
{
	LOG_DBG("Received data of size %u", len);
}

static void bt_connected_cb(void)
{
	LOG_INF("tHE BLuEToOTH DeVIcE IS COnNEcTED");
	uart_send_cmd(UART_CMD_CONN_OK);
}

int main(void)
{
	bt_register_notif_rcv_cb(notif_rcv_cb);
	bt_start(bt_connected_cb);

	k_sleep(K_FOREVER);
}
