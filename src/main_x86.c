/* main_x86.c - main source code for x86 app */

/*
 * Copyright (c) 2015 Intel Corporation
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <errno.h>
#include <misc/printk.h>
#include <misc/byteorder.h>
#include <zephyr.h>

#include <bluetooth/bluetooth.h>
#include <bluetooth/hci.h>
#include <bluetooth/conn.h>
#include <bluetooth/uuid.h>
#include <bluetooth/gatt.h>

#include <ipm.h>
#include <ipm/ipm_quark_se.h>

QUARK_SE_IPM_DEFINE(hrs_ipm, 0, QUARK_SE_IPM_INBOUND);

#define DEVICE_NAME		"Zephyr Heartrate Monitor"
#define DEVICE_NAME_LEN		(sizeof(DEVICE_NAME) - 1)
#define HEART_RATE_APPEARANCE	0x0341
#define HRS_ID			99

static uint8_t ccc_value;
static uint16_t hrs_value = 0;
static struct bt_gatt_ccc_cfg hrmc_ccc_cfg[CONFIG_BLUETOOTH_MAX_PAIRED] = {};
struct bt_conn *default_conn;

static const struct bt_data ad[] = {
	BT_DATA_BYTES(BT_DATA_FLAGS, (BT_LE_AD_GENERAL | BT_LE_AD_NO_BREDR)),
	BT_DATA_BYTES(BT_DATA_UUID16_ALL, 0x0d, 0x18)
};

static const struct bt_data sd[] = {
	BT_DATA(BT_DATA_NAME_COMPLETE, DEVICE_NAME, DEVICE_NAME_LEN)
};

static void hrmc_ccc_cfg_changed(const struct bt_gatt_attr *attr, uint16_t value)
{
	ccc_value = (value == BT_GATT_CCC_NOTIFY) ? 1 : 0;
}

static ssize_t read_name(struct bt_conn *conn, const struct bt_gatt_attr *attr,
                         void *buf, uint16_t len, uint16_t offset)
{
        return bt_gatt_attr_read(conn, attr, buf, len, offset, DEVICE_NAME,
                                 strlen(DEVICE_NAME));
}

static ssize_t read_appearance(struct bt_conn *conn,
                               const struct bt_gatt_attr *attr, void *buf,
                               uint16_t len, uint16_t offset)
{
        uint16_t appearance = sys_cpu_to_le16(HEART_RATE_APPEARANCE);

        return bt_gatt_attr_read(conn, attr, buf, len, offset, &appearance,
                                 sizeof(appearance));
}

static struct bt_gatt_attr gap_attrs[] = {
        BT_GATT_PRIMARY_SERVICE(BT_UUID_GAP),
        BT_GATT_CHARACTERISTIC(BT_UUID_GAP_DEVICE_NAME, BT_GATT_CHRC_READ),
        BT_GATT_DESCRIPTOR(BT_UUID_GAP_DEVICE_NAME, BT_GATT_PERM_READ,
                           read_name, NULL, NULL),
        BT_GATT_CHARACTERISTIC(BT_UUID_GAP_APPEARANCE, BT_GATT_CHRC_READ),
        BT_GATT_DESCRIPTOR(BT_UUID_GAP_APPEARANCE, BT_GATT_PERM_READ,
                           read_appearance, NULL, NULL)
};

/* Heart Rate Service Declaration */
static struct bt_gatt_attr hrs_attrs[] = {
        BT_GATT_PRIMARY_SERVICE(BT_UUID_HRS),
        BT_GATT_CHARACTERISTIC(BT_UUID_HRS_MEASUREMENT, BT_GATT_CHRC_NOTIFY),
        BT_GATT_DESCRIPTOR(BT_UUID_HRS_MEASUREMENT, BT_GATT_PERM_READ,
			   NULL, NULL, NULL),
	BT_GATT_CCC(hrmc_ccc_cfg, hrmc_ccc_cfg_changed)
};

static void connected(struct bt_conn *conn, uint8_t err)
{
	if (err) {
		printk("Connection failed (err %u)\n", err);
	} else {
		default_conn = bt_conn_ref(conn);
		printk("Connected\n");
	}
}

static void disconnected(struct bt_conn *conn, uint8_t reason)
{
	printk("Disconnected (reason %u)\n", reason);

	if (default_conn) {
		bt_conn_unref(default_conn);
		default_conn = NULL;
	}
}

static struct bt_conn_cb conn_callbacks = {
	.connected = connected,
	.disconnected = disconnected,
};

static void bt_ready(int err)
{
	if (err) {
		printk("Bluetooth init failed (err %d)\n", err);
		return;
	}

	printk("Bluetooth initialized\n");

	bt_gatt_register(gap_attrs, ARRAY_SIZE(gap_attrs));
	bt_gatt_register(hrs_attrs, ARRAY_SIZE(hrs_attrs));

	err = bt_le_adv_start(BT_LE_ADV_CONN, ad, ARRAY_SIZE(ad),
					sd, ARRAY_SIZE(sd));
	if (err) {
		printk("Advertising failed to start (err %d)\n", err);
		return;
	}

	printk("Advertising successfully started\n");
}

static void auth_cancel(struct bt_conn *conn)
{
	char addr[BT_ADDR_LE_STR_LEN];

	bt_addr_le_to_str(bt_conn_get_dst(conn), addr, sizeof(addr));

	printk("Pairing cancelled: %s\n", addr);
}

static struct bt_conn_auth_cb auth_cb_display = {
	.cancel = auth_cancel,
};

void hrs_ipm_callback(void *context, uint32_t id, volatile void *data)
{
	uint8_t value = *(volatile uint8_t *) data;

	switch (id) {
	/* only accept value from defined HRS channel */
        case HRS_ID:
                hrs_value = (uint16_t) value << 8 | 0x06;
		if (default_conn) {
			bt_gatt_notify(default_conn, &hrs_attrs[2], &hrs_value, sizeof(hrs_value));
                }
		break;
	default:
		break;
	}
}

void main(void)
{
	struct device *ipm;
	int err;

        ipm = device_get_binding("hrs_ipm");
	if (!ipm) {
                printk("IPM: Device not found.\n");
                return;
        }

	ipm_set_enabled(ipm, 1);
        ipm_register_callback(ipm, hrs_ipm_callback, NULL);

	err = bt_enable(bt_ready);
	if (err) {
		printk("Bluetooth init failed (err %d)\n", err);
		return;
	}

	bt_conn_cb_register(&conn_callbacks);
	bt_conn_auth_cb_register(&auth_cb_display);

	k_sleep(TICKS_UNLIMITED);
}
