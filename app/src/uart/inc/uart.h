#ifndef _UART_H_
#define _UART_H_

#include <stdint.h>
#include <stddef.h>
#include <string.h>

#define UART_RX_BUF_LEN (20U)

#define UART_END_BYTE (0x00)

#define UART_MSG_CONN_OK "CONNOK"
#define UART_MSG_START "START"

typedef void (*uart_rx_cb_t)(const uint8_t *p_data, size_t len);

int uart_register_rx_cb(uart_rx_cb_t p_cb);
int uart_send_blocking(const uint8_t *p_data, size_t len);

#endif /*_UART_H_ */