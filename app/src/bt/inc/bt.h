#ifndef _BT_H_
#define _BT_H_

#include <stdint.h>

typedef void (*bt_subscribed_cb_t)(void);
typedef void (*bt_notif_rcv_cb_t)(const void *data, uint16_t len);

int bt_register_notif_rcv_cb(bt_notif_rcv_cb_t p_cb);
int bt_start(bt_subscribed_cb_t p_subscribed_cb);

#endif /* _BT_H_ */