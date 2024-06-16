#include "bt.h"

#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/hci.h>
#include <zephyr/bluetooth/conn.h>
#include <zephyr/bluetooth/uuid.h>
#include <zephyr/bluetooth/gatt.h>
#include <zephyr/kernel.h>

#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(bt, LOG_LEVEL_DBG);

static void start_scan(void);
static void device_found(const bt_addr_le_t *addr, int8_t rssi, uint8_t type, struct net_buf_simple *ad);
bool ad_data_parse_cb(struct bt_data *data, void *user_data);
static void subscribe_cb(struct bt_conn *conn, uint8_t err, struct bt_gatt_subscribe_params *params);
static uint8_t notify_cb(struct bt_conn *conn, struct bt_gatt_subscribe_params *params, const void *data, uint16_t length);
static uint8_t discover_cb(struct bt_conn *conn, const struct bt_gatt_attr *attr, struct bt_gatt_discover_params *params);
static void connected(struct bt_conn *conn, uint8_t err);
static void disconnected(struct bt_conn *conn, uint8_t reason);

static struct bt_gatt_subscribe_params sub_params = {
	.subscribe = subscribe_cb,
	.notify = notify_cb,
};

BT_CONN_CB_DEFINE(conn_callbacks) = {
	.connected = connected,
	.disconnected = disconnected,
};

static struct bt_uuid_128 vnd_svc_uuid = BT_UUID_INIT_128(
	BT_UUID_128_ENCODE(0x12345678, 0x1234, 0x5678, 0x1234, 0x56789abcdef0));
static struct bt_uuid_128 vnd_chr_uuid = BT_UUID_INIT_128(
	BT_UUID_128_ENCODE(0x12345678, 0x1234, 0x5678, 0x1234, 0x56789abcdef1));
static struct bt_uuid_16 ccc_uuid = BT_UUID_INIT_16(BT_UUID_GATT_CCC_VAL);

static struct bt_gatt_discover_params disc_params;

static struct bt_conn *p_default_conn = NULL;

static bt_subscribed_cb_t p_user_subscribed_cb = NULL;
static bt_notif_rcv_cb_t p_user_notif_rcv_cb = NULL;

int bt_register_notif_rcv_cb(bt_notif_rcv_cb_t p_cb)
{
    p_user_notif_rcv_cb = p_cb;
    return 0;
}

int bt_start(bt_subscribed_cb_t p_subscribed_cb)
{
	int err;

    p_user_subscribed_cb = p_subscribed_cb;

	err = bt_enable(NULL);

	if (err)
	{
		LOG_ERR("Failed to enable bluetooth");
		return -1;
	}

	LOG_DBG("Bluetooth started");

	start_scan();

    return 0;
}

static void device_found(const bt_addr_le_t *addr, int8_t rssi, uint8_t type,
			             struct net_buf_simple *ad)
{
	char addr_str[BT_ADDR_LE_STR_LEN];
	int err;

	if (p_default_conn) {
		return;
	}

	/* We're only interested in connectable events */
	if (type != BT_GAP_ADV_TYPE_ADV_IND &&
	    type != BT_GAP_ADV_TYPE_ADV_DIRECT_IND) 
	{
		return;
	}

	bt_addr_le_to_str(addr, addr_str, sizeof(addr_str));
	LOG_DBG("Device found: %s, RSSI: %d", addr_str, rssi);

	bt_data_parse(ad, ad_data_parse_cb, (void *) addr);
}

bool ad_data_parse_cb(struct bt_data *data, void *user_data)
{
	int err;
	bt_addr_le_t *p_addr = user_data;
	struct bt_uuid_128 adv_svc_uuid = BT_UUID_INIT_128(0x0);

	if (BT_DATA_UUID128_ALL == data->type)
	{
		memcpy(adv_svc_uuid.val, data->data, sizeof(adv_svc_uuid.val));

		if (0 == bt_uuid_cmp(&adv_svc_uuid.uuid, &vnd_svc_uuid.uuid))
		{
			bt_le_scan_stop();

			LOG_DBG("Connecting...");
			err = bt_conn_le_create(p_addr, BT_CONN_LE_CREATE_CONN, BT_LE_CONN_PARAM_DEFAULT, &p_default_conn);

			if (err)
			{
				LOG_ERR("Failed to connect");
				start_scan();
			}

			// Stop parsing the advertising data
			return false;
		}
	}

	// Continue parsing
	return true;
}

static void start_scan(void)
{
	int err;

	err = bt_le_scan_start(BT_LE_SCAN_PASSIVE, device_found);
	if (err) {
		LOG_ERR("Scanning failed to start (err %d)", err);
		return;
	}

	LOG_DBG("Scanning successfully started");
}

static void subscribe_cb(struct bt_conn *conn, uint8_t err, 
						 struct bt_gatt_subscribe_params *params)
{
	LOG_DBG("Subscription returned %d", BT_GATT_ERR(err));
}

static uint8_t notify_cb(struct bt_conn *conn,
				         struct bt_gatt_subscribe_params *params,
				         const void *data, uint16_t length)
{
	if (length > 0)
	{
		LOG_DBG("Received notification of length %u", length);
        if (p_user_notif_rcv_cb)
        {
            p_user_notif_rcv_cb(data, length);
        }
	}

	return BT_GATT_ITER_CONTINUE;
}


static uint8_t discover_cb(struct bt_conn *conn,
			               const struct bt_gatt_attr *attr,
			               struct bt_gatt_discover_params *params)
{
	int err;

	if (!attr) {
		LOG_DBG("Discover complete");
		(void)memset(params, 0, sizeof(*params));
		return BT_GATT_ITER_STOP;
	}

	LOG_DBG("[ATTRIBUTE] handle %u", attr->handle);

	if (!bt_uuid_cmp(disc_params.uuid, &vnd_svc_uuid.uuid)) 
	{
		disc_params.uuid = &vnd_chr_uuid.uuid;
		disc_params.start_handle = attr->handle + 1;
		disc_params.type = BT_GATT_DISCOVER_CHARACTERISTIC;

		err = bt_gatt_discover(conn, &disc_params);
		if (err) 
		{
			LOG_ERR("Discover char failed (err %d)", err);
		}
	} else if (!bt_uuid_cmp(disc_params.uuid, &vnd_chr_uuid.uuid))
	{
		disc_params.uuid = &ccc_uuid.uuid;
		disc_params.start_handle = attr->handle + 2;
		disc_params.type = BT_GATT_DISCOVER_DESCRIPTOR;
		sub_params.value_handle = bt_gatt_attr_value_handle(attr);

		err = bt_gatt_discover(conn, &disc_params);

		if (err) 
		{
			LOG_ERR("Discover descriptor failed (err %d)", err);
		}
	}
	else
	{
		sub_params.value = BT_GATT_CCC_NOTIFY;
		sub_params.ccc_handle = attr->handle;

		err = bt_gatt_subscribe(conn, &sub_params);
		if (err && err != -EALREADY) {
			LOG_ERR("Subscribe failed (err %d)", err);
		} else {
			LOG_DBG("[SUBSCRIBED]");
            if (p_user_subscribed_cb)
            {
                p_user_subscribed_cb();
            }
		}

		return BT_GATT_ITER_STOP;
	}

	return BT_GATT_ITER_STOP;
	
}

static void connected(struct bt_conn *conn, uint8_t err)
{
	char addr[BT_ADDR_LE_STR_LEN];

	bt_addr_le_to_str(bt_conn_get_dst(conn), addr, sizeof(addr));

	if (err) {
		LOG_ERR("Failed to connect to %s (%u)", addr, err);

		bt_conn_unref(p_default_conn);
		p_default_conn = NULL;

		start_scan();
		return;
	}

	// if (conn != p_default_conn) {
	// 	return;
	// }

	LOG_DBG("Connected: %s", addr);

	disc_params.uuid = &vnd_svc_uuid.uuid;
	disc_params.func = discover_cb;
	disc_params.start_handle = BT_ATT_FIRST_ATTRIBUTE_HANDLE;
	disc_params.end_handle = BT_ATT_LAST_ATTRIBUTE_HANDLE;
	disc_params.type = BT_GATT_DISCOVER_PRIMARY;

	err = bt_gatt_discover(conn, &disc_params);

	if (err)
	{
		LOG_ERR("Failed to start discovery");
	}
}

static void disconnected(struct bt_conn *conn, uint8_t reason)
{
	char addr[BT_ADDR_LE_STR_LEN];

	// if (conn != default_conn) {
	// 	return;
	// }

	bt_addr_le_to_str(bt_conn_get_dst(conn), addr, sizeof(addr));

	LOG_DBG("Disconnected: %s (reason 0x%02x)", addr, reason);

	bt_conn_unref(p_default_conn);
	p_default_conn = NULL;

	start_scan();
}
