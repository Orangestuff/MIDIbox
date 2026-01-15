#include "nimble_midi.h"
#include "esp_log.h"
#include "host/ble_hs.h"
#include "host/util/util.h"
#include "services/gap/ble_svc_gap.h"
#include "services/gatt/ble_svc_gatt.h"
#include "services/dis/ble_svc_dis.h" // <--- NEW: Device Info Service
#include "nimble/ble.h"
#include "nimble/nimble_port.h"
#include "nimble/nimble_port_freertos.h"
#include "freertos/task.h" 

static const char *TAG = "BLE_MIDI";
static char *s_device_name = "BLE-MIDI"; 
static uint16_t midi_char_val_handle;
static bool s_connected = false;

// MIDI Service UUID
static const ble_uuid128_t midi_service_uuid =
    BLE_UUID128_INIT(0x00, 0xC7, 0xC4, 0x4E, 0xE3, 0x6C, 0x51, 0xA7,
                     0x33, 0x4B, 0xE8, 0xED, 0x5A, 0x0E, 0xB8, 0x03);

// MIDI Characteristic UUID
static const ble_uuid128_t midi_char_uuid =
    BLE_UUID128_INIT(0xF3, 0x6B, 0x10, 0x9D, 0x66, 0xF2, 0xA9, 0xA1,
                     0x12, 0x41, 0x68, 0x38, 0xDB, 0xE5, 0x72, 0x77);

static int midi_char_access(uint16_t conn_handle, uint16_t attr_handle,
                            struct ble_gatt_access_ctxt *ctxt, void *arg) {
    if (ctxt->op == BLE_GATT_ACCESS_OP_READ_CHR) return 0; 
    if (ctxt->op == BLE_GATT_ACCESS_OP_WRITE_CHR) return 0; 
    return 0;
}

static const struct ble_gatt_svc_def gatt_svcs[] = {
    {
        .type = BLE_GATT_SVC_TYPE_PRIMARY,
        .uuid = &midi_service_uuid.u,
        .characteristics = (struct ble_gatt_chr_def[]) {
            {
                .uuid = &midi_char_uuid.u,
                .access_cb = midi_char_access,
                .flags = BLE_GATT_CHR_F_READ | BLE_GATT_CHR_F_NOTIFY | 
                         BLE_GATT_CHR_F_WRITE_NO_RSP,
                .val_handle = &midi_char_val_handle,
            },
            {0}
        },
    },
    {0}
};

static void ble_app_advertise(void);

static int ble_gap_event(struct ble_gap_event *event, void *arg) {
    switch (event->type) {
        case BLE_GAP_EVENT_CONNECT:
            if (event->connect.status == 0) {
                ESP_LOGI(TAG, "Connected");
                s_connected = true;
            } else {
                ble_app_advertise();
            }
            break;
            
        case BLE_GAP_EVENT_DISCONNECT:
            ESP_LOGI(TAG, "Disconnected (Reason: 0x%x)", event->disconnect.reason);
            s_connected = false;
            ble_app_advertise();
            break;
            
        case BLE_GAP_EVENT_SUBSCRIBE:
            ESP_LOGI(TAG, "Subscribe Event: cur_notify=%d", event->subscribe.cur_notify);
            break;
    }
    return 0;
}

static void ble_app_advertise(void) {
    struct ble_gap_adv_params adv_params;
    struct ble_hs_adv_fields fields;
    struct ble_hs_adv_fields rsp_fields;
    
    // Packet 1: Name + Flags
    memset(&fields, 0, sizeof fields);
    fields.flags = BLE_HS_ADV_F_DISC_GEN | BLE_HS_ADV_F_BREDR_UNSUP;
    fields.name = (uint8_t *)s_device_name;
    fields.name_len = strlen(s_device_name);
    fields.name_is_complete = 1;
    fields.tx_pwr_lvl_is_present = 0; 
    ble_gap_adv_set_fields(&fields);

    // Packet 2: UUID (Scan Response)
    memset(&rsp_fields, 0, sizeof rsp_fields);
    rsp_fields.uuids128 = (ble_uuid128_t*)&midi_service_uuid;
    rsp_fields.num_uuids128 = 1;
    rsp_fields.uuids128_is_complete = 1;
    ble_gap_adv_rsp_set_fields(&rsp_fields);

    memset(&adv_params, 0, sizeof adv_params);
    adv_params.conn_mode = BLE_GAP_CONN_MODE_UND;
    adv_params.disc_mode = BLE_GAP_DISC_MODE_GEN;
    
    // Explicit Intervals (32 * 0.625ms = 20ms) - Standard for MIDI
    adv_params.itvl_min = 32;
    adv_params.itvl_max = 32;

    ble_gap_adv_start(BLE_OWN_ADDR_RANDOM, NULL, BLE_HS_FOREVER, &adv_params, ble_gap_event, NULL);
}

static void ble_app_on_sync(void) {
    ble_addr_t addr;
    
    // Generate Random Address
    ble_hs_id_gen_rnd(1, &addr);
    ble_hs_id_set_rnd(addr.val);
    
    ble_svc_gap_device_appearance_set(0); 
    
    // --- NEW: Initialize Device Information Service ---
    ble_svc_dis_init();
    ble_svc_dis_model_number_set("MIDI-Pedal-S3");
    ble_svc_dis_manufacturer_name_set("Espressif");
    // --------------------------------------------------

    ble_gatts_count_cfg(gatt_svcs);
    ble_gatts_add_svcs(gatt_svcs);
    ble_gatts_start(); 
    
    ble_app_advertise();
    ESP_LOGI(TAG, "NimBLE Started (Random Addr + DIS)");
}

void host_task(void *param) {
    nimble_port_run();
    nimble_port_freertos_deinit();
}

void nimble_midi_init(char *name) {
    s_device_name = "BLE-MIDI"; 
    nimble_port_init();
    
    ble_svc_gap_init();
    ble_svc_gatt_init();
    ble_svc_gap_device_name_set(s_device_name);

    // Disable Bonding
    ble_hs_cfg.sm_io_cap = BLE_SM_IO_CAP_NO_IO;
    ble_hs_cfg.sm_bonding = 0; 
    
    ble_hs_cfg.sync_cb = ble_app_on_sync;
    xTaskCreatePinnedToCore(host_task, "nimble_host", 8192, NULL, 5, NULL, 1);
}

void nimble_midi_send(uint8_t status, uint8_t data1, uint8_t data2) {
    if (!s_connected) return;
    uint8_t packet[5] = {0x80, 0x80, status, data1, data2};
    struct os_mbuf *om = ble_hs_mbuf_from_flat(packet, sizeof(packet));
    ble_gatts_notify_custom(0, midi_char_val_handle, om);
}