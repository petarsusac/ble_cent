#ifndef _UART_H_
#define _UART_H_

#include <stdint.h>
#include <stddef.h>

int uart_send_blocking(const uint8_t *p_data, size_t len);

#endif /*_UART_H_ */