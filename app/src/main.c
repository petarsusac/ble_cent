#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/hci.h>
#include <zephyr/bluetooth/conn.h>
#include <zephyr/bluetooth/uuid.h>
#include <zephyr/bluetooth/gatt.h>
#include <zephyr/kernel.h>

#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(main, LOG_LEVEL_DBG);

static void start_scan(void);

static struct bt_conn *p_default_conn = NULL;

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
		LOG_DBG("We're only interested in connectable events");
		return;
	}

	bt_addr_le_to_str(addr, addr_str, sizeof(addr_str));
	LOG_INF("Device found: %s, RSSI: %d", addr_str, rssi);

	bt_le_scan_stop();

	err = bt_conn_le_create(addr, BT_CONN_LE_CREATE_CONN, BT_LE_CONN_PARAM_DEFAULT, &p_default_conn);

	if (err)
	{
		LOG_ERR("Failed to connect to %s", addr_str);
		start_scan();
	}
}

static void start_scan(void)
{
	int err;

	/* This demo doesn't require active scan */
	err = bt_le_scan_start(BT_LE_SCAN_PASSIVE, device_found);
	if (err) {
		LOG_ERR("Scanning failed to start (err %d)\n", err);
		return;
	}

	LOG_INF("Scanning successfully started\n");
}

static void subscribe_cb(struct bt_conn *conn, uint8_t err, 
							 struct bt_gatt_subscribe_params *params)
{
	LOG_INF("Subscription returned %d", BT_GATT_ERR(err));
}

static uint8_t notify_cb(struct bt_conn *conn,
				      struct bt_gatt_subscribe_params *params,
				      const void *data, uint16_t length)
{
	if (length > 0)
	{
		uint8_t* value = (uint8_t *) data;
		LOG_INF("Received notification value %d", *value); 
	}

	return BT_GATT_ITER_CONTINUE;
}

static struct bt_gatt_subscribe_params sub_params = {
	.subscribe = subscribe_cb,
	.notify = notify_cb,
};

static struct bt_uuid_128 vnd_svc_uuid = BT_UUID_INIT_128(
	BT_UUID_128_ENCODE(0x12345678, 0x1234, 0x5678, 0x1234, 0x56789abcdef0));
static struct bt_uuid_128 vnd_chr_uuid = BT_UUID_INIT_128(
	BT_UUID_128_ENCODE(0x12345678, 0x1234, 0x5678, 0x1234, 0x56789abcdef1));
static struct bt_uuid_16 ccc_uuid = BT_UUID_INIT_16(BT_UUID_GATT_CCC_VAL);

static struct bt_gatt_discover_params disc_params;

static uint8_t discover_cb(struct bt_conn *conn,
			     const struct bt_gatt_attr *attr,
			     struct bt_gatt_discover_params *params)
{
	int err;

	if (!attr) {
		LOG_INF("Discover complete\n");
		(void)memset(params, 0, sizeof(*params));
		return BT_GATT_ITER_STOP;
	}

	LOG_INF("[ATTRIBUTE] handle %u\n", attr->handle);

	if (!bt_uuid_cmp(disc_params.uuid, &vnd_svc_uuid.uuid)) 
	{
		disc_params.uuid = &vnd_chr_uuid.uuid;
		disc_params.start_handle = attr->handle + 1;
		disc_params.type = BT_GATT_DISCOVER_CHARACTERISTIC;

		err = bt_gatt_discover(conn, &disc_params);
		if (err) 
		{
			LOG_ERR("Discover char failed (err %d)\n", err);
		}
	} else if (!bt_uuid_cmp(disc_params.uuid, &vnd_chr_uuid.uuid))
	{
		disc_params.uuid = &ccc_uuid.uuid;
		disc_params.start_handle = attr->handle + 2;
		disc_params.type = BT_GATT_DISCOVER_DESCRIPTOR;
		sub_params.value_handle = bt_gatt_attr_value_handle(attr);

		err = bt_gatt_discover(conn, &disc_params);
		err = bt_gatt_discover(conn, &disc_params);
		if (err) 
		{
			LOG_ERR("Discover descriptor failed (err %d)\n", err);
		}
	}
	else
	{
		sub_params.value = BT_GATT_CCC_NOTIFY;
		sub_params.ccc_handle = attr->handle;

		err = bt_gatt_subscribe(conn, &sub_params);
		if (err && err != -EALREADY) {
			LOG_ERR("Subscribe failed (err %d)\n", err);
		} else {
			LOG_ERR("[SUBSCRIBED]\n");
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
		LOG_ERR("Failed to connect to %s (%u)\n", addr, err);

		bt_conn_unref(p_default_conn);
		p_default_conn = NULL;

		start_scan();
		return;
	}

	// if (conn != p_default_conn) {
	// 	return;
	// }

	LOG_INF("Connected: %s\n", addr);

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

	LOG_INF("Disconnected: %s (reason 0x%02x)\n", addr, reason);

	bt_conn_unref(p_default_conn);
	p_default_conn = NULL;

	start_scan();
}

BT_CONN_CB_DEFINE(conn_callbacks) = {
	.connected = connected,
	.disconnected = disconnected,
};

int main(void)
{
	int err;

	err = bt_enable(NULL);

	if (err)
	{
		LOG_ERR("Failed to enable bluetooth");
		return 0;
	}

	LOG_INF("Bluetooth started");

	start_scan();

	for (;;)
	{
		k_sleep(K_FOREVER);
	}
}
