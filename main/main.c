#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include <math.h>
#include <sys/param.h>

#include "esp_wifi.h"
#include "esp_http_server.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "driver/gpio.h"
#include "driver/rmt.h"
#include "tinyusb.h"
#include "tinyusb_default_config.h"
#include "cJSON.h"
#include "index.h"
#include "freertos/queue.h"
#include "freertos/event_groups.h"
#include "esp_system.h"
#include "esp_random.h"
#include "esp_mac.h"
#include "esp_timer.h"

// --- ADC Headers ---
#include "esp_adc/adc_oneshot.h"

// --- BLE Headers ---
#include "esp_bt.h"
#include "esp_bt_main.h"
#include "esp_gap_ble_api.h"
#include "esp_gatts_api.h"
#include "esp_bt_defs.h"

static const char *TAG = "MIDI_PEDAL";

// --- CONFIGURATION ---
#define SWITCH_COUNT 8
#define BANK_COUNT 4
#define LONG_PRESS_MS 1200
#define MIDI_NOTE_OFF 128
#define MIDI_NOTE_ON  144
#define MIDI_CC       176
#define MIDI_PC       192

#define LED_DATA_PIN 48
#define LED_COUNT 9
#define BATTERY_LED_INDEX 0

// --- EXPRESSION PEDAL CONFIG ---
#define EX_PEDAL_CHANNEL ADC_CHANNEL_2 
#define EX_SMOOTH_SIZE   10

// --- BATTERY CONFIG (GPIO 7 / ADC1) ---
#define BAT_ADC_CHANNEL ADC_CHANNEL_6 
#define BAT_ADC_UNIT    ADC_UNIT_1    
#define BAT_DIVIDER_RATIO 2.0f 
#define BAT_MAX_V 4.2f
#define BAT_MIN_V 3.0f

#define WIFI_CONNECTED_BIT BIT0
#define WIFI_FAIL_BIT      BIT1
#define COMBO_HOLD_MS 2000

// --- MATRIX CONFIG ---
#define NUM_ROWS 2
#define NUM_COLS 4

const gpio_num_t ROW_PINS[NUM_ROWS] = { GPIO_NUM_12, GPIO_NUM_13 };
const gpio_num_t COL_PINS[NUM_COLS] = { GPIO_NUM_4, GPIO_NUM_5, GPIO_NUM_6, GPIO_NUM_8};

const int MATRIX_MAP[NUM_ROWS][NUM_COLS] = {
    {0, 1, 2, 3}, 
    {4, 5, 6, 7}  
};

// --- BLE MIDI UUIDs ---
#define MIDI_SERVICE_UUID        0x03, 0xB8, 0x0E, 0x5A, 0xED, 0xE8, 0x4B, 0x33, 0xA7, 0x51, 0x6C, 0xE3, 0x4E, 0xC4, 0xC7, 0x00
#define MIDI_CHARACTERISTIC_UUID 0x77, 0x72, 0xE5, 0xDB, 0x38, 0x68, 0x41, 0x12, 0xA1, 0xA9, 0xF2, 0x66, 0x9D, 0x10, 0x6B, 0xF3

// --- GLOBALS ---
uint8_t led_strip_pixels[LED_COUNT][3];
static float g_bat_voltage = 0.0f;
static volatile bool is_scanning = false; 
static esp_bd_addr_t current_mac_addr; 
static adc_oneshot_unit_handle_t adc_handle = NULL;
static float smoothed_bat = 0.0f;

// --- Expression Pedal Globals ---
volatile int g_exp_raw_val = 0; // GLOBAL FOR UI CALIBRATION

typedef struct { char ssid[32]; char pass[64]; } wifi_cfg_t;

// --- STRUCTS ---

typedef struct {
    uint8_t chan;
    uint8_t cc;
    uint16_t min;   // Calibration Min (ADC Value)
    uint16_t max;   // Calibration Max (ADC Value)
    uint8_t curve;  // 0=Linear, 1=Exp (Slow), 2=Log (Fast)
} exp_cfg_t;

typedef struct { 
    uint8_t p_type; uint8_t p_chan; uint8_t p_d1; uint8_t p_edge;
    uint8_t lp_type; uint8_t lp_chan; uint8_t lp_d1; 
    uint8_t l_type; uint8_t l_chan; uint8_t l_d1; 
    
    uint8_t p_excl; uint8_t p_master; 
    uint8_t lp_excl; uint8_t lp_master;
    uint8_t l_excl; uint8_t l_master;

    uint8_t incl_mask; 
    uint8_t lp_enabled; 
    uint8_t toggle_mode; 
} sw_cfg_t;

typedef struct {
    sw_cfg_t switches[SWITCH_COUNT];
    exp_cfg_t exp; // Expression settings are now PER BANK
} bank_cfg_t;

typedef struct {
    uint8_t current_bank;
    uint8_t global_brightness;
    uint8_t sw6_cycle_rev;
    uint8_t sw7_cycle_fwd;
    bank_cfg_t banks[BANK_COUNT];
} device_cfg_t; // Corrected Name

wifi_cfg_t w_cfg;
volatile device_cfg_t dev_cfg; 
static QueueHandle_t save_queue = NULL;
static EventGroupHandle_t s_wifi_event_group;
static httpd_handle_t srv = NULL;
static bool is_wifi_on = false;

const uint8_t BANK_COLORS[4][3] = { {255,0,0}, {0,255,0}, {0,0,255}, {255,0,255} };

// Forward Declarations
void midi_task(void *pv);
void restart_task(void *p);
void wifi_init_task(void *pv); 
esp_err_t get_page(httpd_req_t *req);
esp_err_t get_json(httpd_req_t *req);
esp_err_t get_status(httpd_req_t *req);
esp_err_t post_save(httpd_req_t *req);
esp_err_t post_set_bank(httpd_req_t *req);
esp_err_t post_save_wifi(httpd_req_t *req);
esp_err_t get_wifi_scan(httpd_req_t *req);
void set_pixel(int index, uint8_t r, uint8_t g, uint8_t b);
void refresh_leds();
void set_wifi_mode(bool enable);
void init_battery();
void read_battery();
void send_midi_msg(uint8_t type, uint8_t chan, uint8_t val, uint8_t velocity);
static void wifi_event_handler(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data);

// --- USB DESCRIPTORS ---
enum { ITF_NUM_MIDI = 0, ITF_NUM_MIDI_STREAMING, ITF_COUNT };
enum { EPNUM_MIDI = 1 };
#define TUSB_DESCRIPTOR_TOTAL_LEN (TUD_CONFIG_DESC_LEN + TUD_MIDI_DESC_LEN)
static const char* s_str_desc[5] = {(char[]){0x09, 0x04}, "TinyUSB", "ESP32 MIDI Pedal", "123456", "MIDI Interface"};
static const uint8_t s_midi_cfg_desc[] = {
    TUD_CONFIG_DESCRIPTOR(1, ITF_COUNT, 0, TUSB_DESCRIPTOR_TOTAL_LEN, 0x00, 100),
    TUD_MIDI_DESCRIPTOR(ITF_NUM_MIDI, 4, EPNUM_MIDI, 0x80 | EPNUM_MIDI, 64),
};

// ==========================================================
//             BATTERY & EXPRESSION LOGIC (ADC1)
// ==========================================================
void init_battery() {
    adc_oneshot_unit_init_cfg_t init_config = { .unit_id = BAT_ADC_UNIT };
    if (adc_handle == NULL) {
        adc_oneshot_new_unit(&init_config, &adc_handle);
    }
    if (adc_handle) {
        adc_oneshot_chan_cfg_t config = { .bitwidth = ADC_BITWIDTH_DEFAULT, .atten = ADC_ATTEN_DB_12 };
        adc_oneshot_config_channel(adc_handle, BAT_ADC_CHANNEL, &config);
        
        int raw = 0;
        if (adc_oneshot_read(adc_handle, BAT_ADC_CHANNEL, &raw) == ESP_OK) {
            float pin_voltage = (raw / 4095.0f) * 3.3f; 
            smoothed_bat = pin_voltage * BAT_DIVIDER_RATIO;
        }
    }
}

void init_expression_pedal() {
    if (adc_handle == NULL) {
        adc_oneshot_unit_init_cfg_t init_config = { .unit_id = BAT_ADC_UNIT };
        adc_oneshot_new_unit(&init_config, &adc_handle);
    }
    
    if (adc_handle) {
        adc_oneshot_chan_cfg_t config = {
            .bitwidth = ADC_BITWIDTH_12, 
            .atten = ADC_ATTEN_DB_12,    
        };
        adc_oneshot_config_channel(adc_handle, EX_PEDAL_CHANNEL, &config);
    }
}

void read_battery() {
    if (adc_handle == NULL) return;
    int raw = 0;
    if (adc_oneshot_read(adc_handle, BAT_ADC_CHANNEL, &raw) == ESP_OK) {
        float pin_voltage = (raw / 4095.0f) * 3.3f; 
        float instant_voltage = pin_voltage * BAT_DIVIDER_RATIO;
        if (smoothed_bat < 0.1f) smoothed_bat = instant_voltage; 
        else smoothed_bat = (smoothed_bat * 0.98f) + (instant_voltage * 0.02f);
        g_bat_voltage = smoothed_bat;
    }
}

// Helper for Curve Math
uint8_t apply_curve(float normalized, uint8_t type) {
    float val = normalized;
    if (val < 0.0f) val = 0.0f;
    if (val > 1.0f) val = 1.0f;

    switch(type) {
        case 1: // Exp (Slow Start / Volume Swell) -> y = x^2
            val = val * val;
            break;
        case 2: // Log (Fast Start) -> y = sqrt(x)
            val = sqrtf(val); // Requires #include <math.h>
            break;
        case 0: // Linear -> y = x
        default:
            break;
    }
    return (uint8_t)(val * 127.0f);
}

// Noise threshold (Adjust 10-30 if still jittery)
#define EXP_HYSTERESIS 25

void process_expression_pedal() {
    // 1. Get Config
    volatile exp_cfg_t *exp = &dev_cfg.banks[dev_cfg.current_bank].exp;

    // 2. Read ADC (Oversampling)
    uint32_t sum = 0;
    int sample = 0;
    for(int i=0; i<4; i++) {
        adc_oneshot_read(adc_handle, EX_PEDAL_CHANNEL, &sample);
        sum += sample;
    }
    int raw = sum / 4;

    // 3. Hysteresis
    static int stable_raw = 0;
    if (raw > (stable_raw + EXP_HYSTERESIS)) {
        stable_raw = raw - EXP_HYSTERESIS; 
    } 
    else if (raw < (stable_raw - EXP_HYSTERESIS)) {
        stable_raw = raw + EXP_HYSTERESIS; 
    }

    // 4. Smoothing
    static float smooth_raw = 0;
    smooth_raw = (smooth_raw * 0.90f) + ((float)stable_raw * 0.10f);
    int current_val = (int)smooth_raw;

    // --- FIX: Update UI Global continuously (Outside the MIDI logic) ---
    // This ensures you see the REAL raw value even if you go past Min/Max
    g_exp_raw_val = current_val; 
    // ------------------------------------------------------------------

    // 5. Normalize
    float norm = 0.0f;
    if (exp->max > exp->min) {
        norm = (float)(current_val - exp->min) / (float)(exp->max - exp->min);
    } else {
        norm = (float)(exp->min - current_val) / (float)(exp->min - exp->max);
    }
    
    if (norm < 0.0f) norm = 0.0f;
    if (norm > 1.0f) norm = 1.0f;

    // 6. Curve
    uint8_t midi_val = apply_curve(norm, exp->curve);

    // 7. Send MIDI (Only if changed)
    static uint8_t last_exp_val = 255;
    static uint8_t last_bank_idx = 255; 

    if (midi_val != last_exp_val || dev_cfg.current_bank != last_bank_idx) {
        send_midi_msg(176, exp->chan, exp->cc, midi_val); 
        last_exp_val = midi_val;
        last_bank_idx = dev_cfg.current_bank;
    }
}

// ==========================================================
//             BLE MIDI STACK
// ==========================================================
static bool is_ble_connected = false;
static uint16_t current_conn_id = 0;        
static esp_gatt_if_t current_gatts_if = 0;
static uint16_t midi_char_handle = 0; 

static const uint8_t midi_service_uuid[16] = { 0x00, 0xC7, 0xC4, 0x4E, 0xE3, 0x6C, 0x51, 0xA7, 0x33, 0x4B, 0xE8, 0xED, 0x5A, 0x0E, 0xB8, 0x03 };
static const uint8_t midi_char_uuid[16]    = { 0xF3, 0x6B, 0x10, 0x9D, 0x66, 0xF2, 0xA9, 0xA1, 0x12, 0x41, 0x68, 0x38, 0xDB, 0xE5, 0x72, 0x77 };

enum { MIDI_IDX_SVC, MIDI_IDX_CHAR, MIDI_IDX_VAL, MIDI_IDX_CFG, MIDI_IDX_NB };

static esp_ble_adv_params_t adv_params = {
    .adv_int_min        = 0x20,
    .adv_int_max        = 0x40,
    .adv_type           = ADV_TYPE_IND,
    .own_addr_type      = BLE_ADDR_TYPE_RANDOM, 
    .channel_map        = ADV_CHNL_ALL,
    .adv_filter_policy  = ADV_FILTER_ALLOW_SCAN_ANY_CON_ANY,
};

static uint8_t raw_adv_data[3 + 18] = { 0x02, 0x01, 0x06, 0x11, 0x07 }; 
static uint8_t raw_scan_rsp_data[] = { 0x0B, 0x09, 'E', 'S', 'P', '3', '2', '_', 'M', 'I', 'D', 'I' };

#define CHAR_DECLARATION_SIZE   (sizeof(uint8_t))
static const uint16_t primary_service_uuid = ESP_GATT_UUID_PRI_SERVICE;
static const uint16_t character_declaration_uuid = ESP_GATT_UUID_CHAR_DECLARE;
static const uint16_t character_client_config_uuid = ESP_GATT_UUID_CHAR_CLIENT_CONFIG;
static const uint8_t char_prop_read_notify_write_nr = ESP_GATT_CHAR_PROP_BIT_READ | ESP_GATT_CHAR_PROP_BIT_NOTIFY | ESP_GATT_CHAR_PROP_BIT_WRITE_NR;
static const uint8_t ccc_value[2] = {0x00, 0x00}; 
static const uint8_t char_value[5] = {0x80, 0x80, 0x00, 0x00, 0x00}; 

static const esp_gatts_attr_db_t midi_gatt_db[MIDI_IDX_NB] = {
    [MIDI_IDX_SVC] = {{ESP_GATT_AUTO_RSP}, {ESP_UUID_LEN_16, (uint8_t *)&primary_service_uuid, ESP_GATT_PERM_READ, ESP_UUID_LEN_128, ESP_UUID_LEN_128, (uint8_t *)midi_service_uuid}},
    [MIDI_IDX_CHAR] = {{ESP_GATT_AUTO_RSP}, {ESP_UUID_LEN_16, (uint8_t *)&character_declaration_uuid, ESP_GATT_PERM_READ, CHAR_DECLARATION_SIZE, CHAR_DECLARATION_SIZE, (uint8_t *)&char_prop_read_notify_write_nr}},
    [MIDI_IDX_VAL] = {{ESP_GATT_AUTO_RSP}, {ESP_UUID_LEN_128, (uint8_t *)midi_char_uuid, ESP_GATT_PERM_READ | ESP_GATT_PERM_WRITE, 512, sizeof(char_value), (uint8_t *)char_value}},
    [MIDI_IDX_CFG] = {{ESP_GATT_AUTO_RSP}, {ESP_UUID_LEN_16, (uint8_t *)&character_client_config_uuid, ESP_GATT_PERM_READ | ESP_GATT_PERM_WRITE, sizeof(uint16_t), sizeof(ccc_value), (uint8_t *)ccc_value}},
};

static void gap_event_handler(esp_gap_ble_cb_event_t event, esp_ble_gap_cb_param_t *param) {
    switch (event) {
        case ESP_GAP_BLE_ADV_DATA_RAW_SET_COMPLETE_EVT: esp_ble_gap_start_advertising(&adv_params); break;
        case ESP_GAP_BLE_ADV_START_COMPLETE_EVT: if(param->adv_start_cmpl.status == ESP_BT_STATUS_SUCCESS) ESP_LOGI(TAG, "Legacy Advertising Started"); break;
        case ESP_GAP_BLE_SEC_REQ_EVT: esp_ble_gap_security_rsp(param->ble_security.ble_req.bd_addr, true); break;
        case ESP_GAP_BLE_AUTH_CMPL_EVT: ESP_LOGI(TAG, "Auth Complete. Success: %d", param->ble_security.auth_cmpl.success); break;
        default: break;
    }
}

static void gatts_event_handler(esp_gatts_cb_event_t event, esp_gatt_if_t gatts_if, esp_ble_gatts_cb_param_t *param) {
    if (event == ESP_GATTS_REG_EVT) {
        current_gatts_if = gatts_if;
        esp_ble_gap_set_rand_addr(current_mac_addr);
        esp_ble_gap_set_device_name("ESP32_MIDI");
        memcpy(&raw_adv_data[5], midi_service_uuid, 16);
        esp_ble_gap_config_adv_data_raw(raw_adv_data, sizeof(raw_adv_data));
        esp_ble_gap_config_scan_rsp_data_raw(raw_scan_rsp_data, sizeof(raw_scan_rsp_data));
        esp_ble_gatts_create_attr_tab(midi_gatt_db, gatts_if, MIDI_IDX_NB, 0);
    }
    else if (event == ESP_GATTS_CREAT_ATTR_TAB_EVT) {
        if (param->add_attr_tab.status == ESP_GATT_OK) {
            midi_char_handle = param->add_attr_tab.handles[MIDI_IDX_VAL]; 
            esp_ble_gatts_start_service(param->add_attr_tab.handles[MIDI_IDX_SVC]);
        }
    }
    else if (event == ESP_GATTS_CONNECT_EVT) {
        current_conn_id = param->connect.conn_id;
        is_ble_connected = true;
                esp_ble_conn_update_params_t conn_params = {0};
        memcpy(conn_params.bda, param->connect.remote_bda, sizeof(esp_bd_addr_t));
        conn_params.min_int = 0x06; // 7.5ms (1.25ms units)
        conn_params.max_int = 0x0C; // 15ms
        conn_params.latency = 0;
        conn_params.timeout = 400;  // 4s
        esp_ble_gap_update_conn_params(&conn_params);
        ESP_LOGI(TAG, "BLE Connected");
    }
    else if (event == ESP_GATTS_DISCONNECT_EVT) {
        is_ble_connected = false;
        ESP_LOGI(TAG, "BLE Disconnected - Restarting Adv");
        esp_ble_gap_start_advertising(&adv_params);
    }
}

void init_ble_midi() { 
    esp_bt_controller_config_t bt_cfg = BT_CONTROLLER_INIT_CONFIG_DEFAULT(); 
    esp_bt_controller_init(&bt_cfg); 
    esp_bt_controller_enable(ESP_BT_MODE_BLE); 
    esp_bluedroid_init(); 
    esp_bluedroid_enable(); 
    
    esp_ble_auth_req_t auth_req = ESP_LE_AUTH_BOND; 
    esp_ble_io_cap_t iocap = ESP_IO_CAP_NONE;
    uint8_t key_size = 16;
    uint8_t init_key = ESP_BLE_ENC_KEY_MASK | ESP_BLE_ID_KEY_MASK;
    uint8_t rsp_key = ESP_BLE_ENC_KEY_MASK | ESP_BLE_ID_KEY_MASK;
    esp_ble_gap_set_security_param(ESP_BLE_SM_AUTHEN_REQ_MODE, &auth_req, sizeof(uint8_t));
    esp_ble_gap_set_security_param(ESP_BLE_SM_IOCAP_MODE, &iocap, sizeof(uint8_t));
    esp_ble_gap_set_security_param(ESP_BLE_SM_MAX_KEY_SIZE, &key_size, sizeof(uint8_t));
    esp_ble_gap_set_security_param(ESP_BLE_SM_SET_INIT_KEY, &init_key, sizeof(uint8_t));
    esp_ble_gap_set_security_param(ESP_BLE_SM_SET_RSP_KEY, &rsp_key, sizeof(uint8_t));
    
    esp_ble_gatts_register_callback(gatts_event_handler); 
    esp_ble_gap_register_callback(gap_event_handler); 
    esp_ble_gatts_app_register(0); 
}

void deinit_ble_midi() { 
    esp_ble_gatts_app_unregister(0); 
    esp_bluedroid_disable(); 
    esp_bluedroid_deinit(); 
    esp_bt_controller_disable(); 
    esp_bt_controller_deinit(); 
    is_ble_connected = false; 
}

void send_ble_midi_packet(uint8_t type, uint8_t chan, uint8_t val, uint8_t velocity) {
    if (!is_ble_connected || midi_char_handle == 0) return;
    uint8_t midi_packet[5]; uint8_t status = type | (chan & 0x0F);
    midi_packet[0] = 0x80; midi_packet[1] = 0x80; midi_packet[2] = status; midi_packet[3] = val; midi_packet[4] = (type == MIDI_PC) ? 0 : velocity;
    uint16_t len = (type == MIDI_PC) ? 4 : 5; 
    esp_ble_gatts_send_indicate(current_gatts_if, current_conn_id, midi_char_handle, len, midi_packet, false); 
}

// ==========================================================
//                  CORE LOGIC
// ==========================================================

void start_webserver() {
    if (srv != NULL) return;
    httpd_config_t hcfg = HTTPD_DEFAULT_CONFIG(); 
    hcfg.stack_size = 12288; 
    if(httpd_start(&srv, &hcfg) == ESP_OK) {
        httpd_uri_t u[] = { {.uri="/",.method=HTTP_GET,.handler=get_page}, {.uri="/api/settings",.method=HTTP_GET,.handler=get_json}, {.uri="/api/status",.method=HTTP_GET,.handler=get_status}, {.uri="/api/save",.method=HTTP_POST,.handler=post_save}, {.uri="/api/set_bank",.method=HTTP_POST,.handler=post_set_bank}, {.uri="/api/save_wifi",.method=HTTP_POST,.handler=post_save_wifi}, {.uri="/api/scan",.method=HTTP_GET,.handler=get_wifi_scan} };
        for(int i=0; i < 7; i++) httpd_register_uri_handler(srv, &u[i]);
        ESP_LOGI(TAG, "Web Server Started");
    }
}

void stop_webserver() { if (srv) { httpd_stop(srv); srv = NULL; ESP_LOGI(TAG, "Web Server Stopped"); } }

static void wifi_event_handler(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data) {
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) { 
        if (!is_scanning) { esp_wifi_connect(); }
    }
    else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) { 
        wifi_event_sta_disconnected_t* event = (wifi_event_sta_disconnected_t*) event_data;
        if (!is_scanning) {
            ESP_LOGW(TAG, "WiFi Disconnected. Reason: %d", event->reason);
            xEventGroupSetBits(s_wifi_event_group, WIFI_FAIL_BIT); 
        }
    }
    else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) { 
        xEventGroupSetBits(s_wifi_event_group, WIFI_CONNECTED_BIT); 
    }
}

void wifi_init_task(void *pv) {
    xEventGroupClearBits(s_wifi_event_group, WIFI_CONNECTED_BIT | WIFI_FAIL_BIT);
    esp_wifi_stop(); 
    esp_wifi_set_mode(WIFI_MODE_STA);
    wifi_config_t sta_config = {0}; 
    memset(&sta_config, 0, sizeof(wifi_config_t));
    strlcpy((char*)sta_config.sta.ssid, w_cfg.ssid, sizeof(sta_config.sta.ssid));
    strlcpy((char*)sta_config.sta.password, w_cfg.pass, sizeof(sta_config.sta.password));
    sta_config.sta.threshold.authmode = WIFI_AUTH_WPA2_PSK; 
    sta_config.sta.pmf_cfg.capable = true;  
    sta_config.sta.pmf_cfg.required = false; 
    esp_wifi_set_config(WIFI_IF_STA, &sta_config);
    esp_wifi_start(); 
    ESP_LOGI(TAG, "Attempting connection to '%s'...", w_cfg.ssid);
    
    EventBits_t bits = xEventGroupWaitBits(s_wifi_event_group, WIFI_CONNECTED_BIT | WIFI_FAIL_BIT, pdFALSE, pdFALSE, pdMS_TO_TICKS(15000));
    if (bits & WIFI_CONNECTED_BIT) {
        ESP_LOGI(TAG, "WiFi Connected Successfully.");
        start_webserver();
    } else {
        ESP_LOGE(TAG, "WiFi Failed -> Starting Access Point");
        esp_wifi_stop();
        vTaskDelay(pdMS_TO_TICKS(100));
        wifi_config_t ap_cfg = {0};
        memset(&ap_cfg, 0, sizeof(wifi_config_t));

        // 2. Set Robust Parameters (WPA2 + Channel 6)
        strlcpy((char*)ap_cfg.ap.ssid, "MidiPedal_Config", sizeof(ap_cfg.ap.ssid));
        strlcpy((char*)ap_cfg.ap.password, "12345678", sizeof(ap_cfg.ap.password)); // Simple password
        ap_cfg.ap.channel = 6;                       // Channel 6 is often clearer than 1
        ap_cfg.ap.ssid_len = strlen("MidiPedal_Config");
        ap_cfg.ap.max_connection = 4;
        ap_cfg.ap.authmode = WIFI_AUTH_WPA2_PSK;     // Phones prefer WPA2 over Open
        ap_cfg.ap.pmf_cfg.required = false;          // Keep PMF off for compatibility

        ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_AP)); 
        ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &ap_cfg)); 
        
        wifi_country_t country_config = { .cc = "US", .schan = 1, .nchan = 11, .policy = WIFI_COUNTRY_POLICY_AUTO };
        esp_wifi_set_country(&country_config);
        
        ESP_ERROR_CHECK(esp_wifi_start());
        esp_wifi_set_bandwidth(WIFI_IF_AP, WIFI_BW_HT20);
        
        ESP_LOGI(TAG, "AP Started. Connect to 'MidiPedal_Config' (20MHz Mode)");
        start_webserver();
    }
    vTaskDelete(NULL);
}

void set_wifi_mode(bool enable) {
    nvs_handle_t h;
    if(nvs_open("storage", NVS_READWRITE, &h) == ESP_OK) {
        uint8_t val = enable ? 1 : 0;
        nvs_set_u8(h, "wifi_mode", val);
        nvs_commit(h);
        nvs_close(h);
    }
    if (enable && !is_wifi_on) {
        ESP_LOGI(TAG, "Disabling BLE to Enable WiFi...");
        deinit_ble_midi(); 
        vTaskDelay(pdMS_TO_TICKS(100)); 
        xTaskCreatePinnedToCore(wifi_init_task, "wifi_init", 4096, NULL, 10, NULL, 1);
        is_wifi_on = true;
    } 
    else if (!enable && is_wifi_on) {
        ESP_LOGI(TAG, "Disabling WiFi to Enable BLE...");
        stop_webserver();
        esp_wifi_disconnect();
        esp_wifi_stop();
        is_wifi_on = false;
        vTaskDelay(pdMS_TO_TICKS(100)); 
        init_ble_midi(); 
    }
}

void send_midi_msg(uint8_t type, uint8_t chan, uint8_t val, uint8_t velocity) {
    if (type == 0) return; // "None" type: Do nothing
    if (tud_midi_mounted()) { 
        uint8_t packet[4] = {0};
        uint8_t stat = type | (chan & 0x0F);
        packet[0] = (type == MIDI_PC) ? 0x0C : ((type == MIDI_NOTE_ON) ? 0x09 : 0x0B); 
        if (type==MIDI_NOTE_OFF) packet[0]=0x08;
        packet[1] = stat; packet[2] = val; packet[3] = (type==MIDI_PC)?0:velocity;
        tud_midi_packet_write(packet);
    }
    if (!is_wifi_on) { send_ble_midi_packet(type, chan, val, velocity); }
}

// --- LED FUNCTIONS ---
void set_pixel(int index, uint8_t r, uint8_t g, uint8_t b) {
    if (index >= 0 && index < LED_COUNT) {
        led_strip_pixels[index][0] = r;
        led_strip_pixels[index][1] = g;
        led_strip_pixels[index][2] = b;
    }
}

// Add this static buffer just above the function to remember the last state
static uint8_t last_led_state[LED_COUNT][3];

void refresh_leds() {
    // --- 1. SMART CHECK ---
    if (memcmp(led_strip_pixels, last_led_state, sizeof(led_strip_pixels)) == 0) {
        return; 
    }

    // --- 2. UPDATE HISTORY ---
    memcpy(last_led_state, led_strip_pixels, sizeof(led_strip_pixels));

    // --- 3. PREPARE DATA ---
    rmt_item32_t items[LED_COUNT * 24];
    
    for (int n = 0; n < LED_COUNT; n++) {
        uint8_t r = led_strip_pixels[n][0];
        uint8_t g = led_strip_pixels[n][1];
        uint8_t b = led_strip_pixels[n][2];
        
        // GRB Order
        uint32_t color = (g << 16) | (r << 8) | b; 
        
        for (int i = 0; i < 24; i++) {
            bool bit = (color >> (23 - i)) & 1;
            items[(n * 24) + i] = bit ? 
                (rmt_item32_t){{{ 16, 1, 9, 0 }}} : 
                (rmt_item32_t){{{ 8, 1, 17, 0 }}};
        }
    }
    
    // --- 4. SEND DATA (SAFE MODE) ---
    esp_rom_delay_us(60); 
    rmt_write_items(RMT_CHANNEL_0, items, LED_COUNT * 24, true);
}

void update_status_leds(float voltage, int current_bank, bool *sw_states, bool *btn_held) {
    uint32_t global_br = dev_cfg.global_brightness; 

    // --- 1. SET BATTERY LED (Index 0) ---
    if (voltage > 3.9) {
        set_pixel(0, 0, 15, 0);       // Dim Green
    } else if (voltage > 3.6) {
        set_pixel(0, 12, 8, 0);       // Dim Orange
    } else {
        set_pixel(0, 25, 0, 0);       // Dim Red
    }

    // --- 2. SET SWITCH LEDS ---
    for (int i = 0; i < SWITCH_COUNT; i++) {
        // Map Switch Index to LED Index (Snake Logic)
        int led_idx;
        if (i < 4) led_idx = i + 1;
        else led_idx = 12 - i;

        // Get config for this switch
        sw_cfg_t *cfg = (sw_cfg_t*)&dev_cfg.banks[current_bank].switches[i];
        
        uint8_t r = 0, g = 0, b = 0;
        
        // --- CASE A: DIRECT BANK SWITCH (Navigation) ---
        // We want these to show the TARGET color, but DIMMED so they don't look "Active".
        if (cfg->p_type >= 252) {
            uint8_t target_b = cfg->p_type - 252;
            
            // Scaled down to 5% brightness
            r = (BANK_COLORS[target_b][0] * global_br) / 4000; 
            g = (BANK_COLORS[target_b][1] * global_br) / 4000;
            b = (BANK_COLORS[target_b][2] * global_br) / 4000;
        }
        
        // --- CASE B: CYCLE SWITCH (Next/Prev) ---
        else if (cfg->p_type == 250 || cfg->p_type == 251) {
            // Keep cycle switches fairly bright (50%) using Current Bank Color
            r = (BANK_COLORS[current_bank][0] * global_br) / 512;
            g = (BANK_COLORS[current_bank][1] * global_br) / 512;
            b = (BANK_COLORS[current_bank][2] * global_br) / 512;
        }

        // --- CASE C: STANDARD MIDI SWITCH ---
        else {
            bool is_active = sw_states[i] || btn_held[i];

            if (is_active) {
                // ACTIVE: 100% Brightness (Current Bank Color)
                r = (BANK_COLORS[current_bank][0] * global_br) / 255;
                g = (BANK_COLORS[current_bank][1] * global_br) / 255;
                b = (BANK_COLORS[current_bank][2] * global_br) / 255;
            } else {
                // INACTIVE (Backlight): 25% Brightness (Current Bank Color)
                // This provides the visual "Glue" to tell you which bank you are in.
                r = (BANK_COLORS[current_bank][0] * global_br) / 4000; // Very dim
                g = (BANK_COLORS[current_bank][1] * global_br) / 4000;
                b = (BANK_COLORS[current_bank][2] * global_br) / 4000;
                
                // Ensure at least faint light if global brightness is high enough
                if (global_br > 50 && r==0 && g==0 && b==0) {
                      // Force minimum glow if color is non-black
                      if (BANK_COLORS[current_bank][0] > 0) r = 1;
                      if (BANK_COLORS[current_bank][1] > 0) g = 1;
                      if (BANK_COLORS[current_bank][2] > 0) b = 1;
                }
            }
        }

        set_pixel(led_idx, r, g, b);
    }
    
    refresh_leds();
}

void nvs_save_task(void *pv) {
    uint8_t trigger;
    while(1) {
        if (xQueueReceive(save_queue, &trigger, portMAX_DELAY)) {
            vTaskDelay(pdMS_TO_TICKS(500));
            nvs_handle_t h; if(nvs_open("storage", NVS_READWRITE, &h) == ESP_OK) { nvs_set_blob(h, "dev_cfg", (void*)&dev_cfg, sizeof(dev_cfg)); nvs_commit(h); nvs_close(h); ESP_LOGI(TAG, "Config Saved"); }
        }
    }
}

void restart_task(void *p) { vTaskDelay(pdMS_TO_TICKS(2000)); esp_restart(); }

esp_err_t get_status(httpd_req_t *req) { 
    char buf[100]; 
    snprintf(buf, sizeof(buf), "{\"bank\":%d,\"bat\":%.2f,\"exp_raw\":%d}", dev_cfg.current_bank, g_bat_voltage, g_exp_raw_val); 
    httpd_resp_set_type(req, "application/json"); 
    return httpd_resp_sendstr(req, buf); 
}

esp_err_t get_page(httpd_req_t *req) { return httpd_resp_sendstr(req, INDEX_HTML); }

esp_err_t get_json(httpd_req_t *req) {
    cJSON *root = cJSON_CreateObject();
    cJSON_AddBoolToObject(root, "cyc_rev", (dev_cfg.sw6_cycle_rev == 1)); 
    cJSON_AddBoolToObject(root, "cyc_fwd", (dev_cfg.sw7_cycle_fwd == 1)); 
    cJSON_AddNumberToObject(root, "brightness", dev_cfg.global_brightness);
    
    // Output WiFi
    cJSON *wifi = cJSON_CreateObject(); 
    cJSON_AddStringToObject(wifi, "ssid", w_cfg.ssid); 
    cJSON_AddStringToObject(wifi, "pass", w_cfg.pass); 
    cJSON_AddItemToObject(root, "wifi", wifi);
    
    // Banks
    cJSON *banks_arr = cJSON_CreateArray();
    for(int b=0; b<BANK_COUNT; b++) {
        cJSON *bank_obj = cJSON_CreateObject(); 
        
        // --- NEW: Expression Per Bank ---
        cJSON *exp_obj = cJSON_CreateObject();
        volatile exp_cfg_t *e = &dev_cfg.banks[b].exp;
        cJSON_AddNumberToObject(exp_obj, "ch", e->chan);
        cJSON_AddNumberToObject(exp_obj, "cc", e->cc);
        cJSON_AddNumberToObject(exp_obj, "min", e->min);
        cJSON_AddNumberToObject(exp_obj, "max", e->max);
        cJSON_AddNumberToObject(exp_obj, "crv", e->curve);
        cJSON_AddItemToObject(bank_obj, "exp", exp_obj);
        // --------------------------------

        cJSON *sw_arr = cJSON_CreateArray();
        for(int i=0; i<SWITCH_COUNT; i++) {
            // ... (Your existing switch JSON code) ...
            cJSON *item = cJSON_CreateObject(); 
            sw_cfg_t *sw = (sw_cfg_t*)&dev_cfg.banks[b].switches[i];
            
            // Arrays
            int p[] = {sw->p_type, sw->p_chan, sw->p_d1}; 
            int p2[] = {sw->lp_type, sw->lp_chan, sw->lp_d1}; 
            int p3[] = {sw->l_type, sw->l_chan, sw->l_d1};
            
            cJSON_AddItemToObject(item, "p", cJSON_CreateIntArray(p, 3)); 
            cJSON_AddItemToObject(item, "lp", cJSON_CreateIntArray(p2, 3)); 
            cJSON_AddItemToObject(item, "l", cJSON_CreateIntArray(p3, 3)); 
            
            cJSON_AddBoolToObject(item, "lp_en", (sw->lp_enabled == 1)); 
            cJSON_AddBoolToObject(item, "tog", (sw->toggle_mode == 1)); 
            cJSON_AddNumberToObject(item, "edge", sw->p_edge); // The Edge feature

            cJSON_AddNumberToObject(item, "pe", sw->p_excl); cJSON_AddNumberToObject(item, "pm", sw->p_master);
            cJSON_AddNumberToObject(item, "lpe", sw->lp_excl); cJSON_AddNumberToObject(item, "lpm", sw->lp_master);
            cJSON_AddNumberToObject(item, "le", sw->l_excl); cJSON_AddNumberToObject(item, "lm", sw->l_master);
            cJSON_AddNumberToObject(item, "incl", sw->incl_mask);
            
            cJSON_AddItemToArray(sw_arr, item);
        } 
        cJSON_AddItemToObject(bank_obj, "switches", sw_arr); 
        cJSON_AddItemToArray(banks_arr, bank_obj);
    } 
    cJSON_AddItemToObject(root, "banks", banks_arr); 
    
    char *out = cJSON_PrintUnformatted(root); 
    httpd_resp_set_type(req, "application/json"); 
    httpd_resp_sendstr(req, out); 
    free(out); 
    cJSON_Delete(root); 
    return ESP_OK;
}
esp_err_t post_set_bank(httpd_req_t *req) { 
    char buf[10]; 
    int ret = httpd_req_recv(req, buf, req->content_len); 
    if (ret > 0) { 
        buf[ret] = '\0'; 
        int b = atoi(buf);
        if (b >= 0 && b < BANK_COUNT) {
            dev_cfg.current_bank = b;
        }
    } 
    return httpd_resp_sendstr(req, "OK"); 
}
esp_err_t post_save_wifi(httpd_req_t *req) {
    char *buf = malloc(req->content_len + 1);
    if (!buf) return ESP_FAIL;
    size_t received = 0;
    while (received < req->content_len) {
        int ret = httpd_req_recv(req, buf + received, req->content_len - received);
        if (ret <= 0) { if (ret == HTTPD_SOCK_ERR_TIMEOUT) continue; free(buf); return ESP_FAIL; }
        received += ret;
    }
    buf[received] = '\0'; 
    cJSON *root = cJSON_Parse(buf);
    if (root) {
        cJSON *j_ssid = cJSON_GetObjectItem(root, "ssid");
        cJSON *j_pass = cJSON_GetObjectItem(root, "pass");
        if (cJSON_IsString(j_ssid) && cJSON_IsString(j_pass)) {
             memset(w_cfg.ssid, 0, sizeof(w_cfg.ssid));
             memset(w_cfg.pass, 0, sizeof(w_cfg.pass));
             strncpy(w_cfg.ssid, j_ssid->valuestring, sizeof(w_cfg.ssid) - 1);
             strncpy(w_cfg.pass, j_pass->valuestring, sizeof(w_cfg.pass) - 1);
             nvs_handle_t h;
             if (nvs_open("storage", NVS_READWRITE, &h) == ESP_OK) {
                 nvs_set_blob(h, "w_cfg", &w_cfg, sizeof(w_cfg));
                 nvs_commit(h); nvs_close(h);
                 ESP_LOGI(TAG, "WiFi Credentials Saved.");
             }
        }
        cJSON_Delete(root);
    }
    free(buf);
    httpd_resp_send(req, "Saved. Rebooting...", HTTPD_RESP_USE_STRLEN);
    xTaskCreate(restart_task, "restart_task", 2048, NULL, 5, NULL);
    return ESP_OK;
}

esp_err_t get_wifi_scan(httpd_req_t *req) {
    wifi_mode_t cur_mode;
    esp_wifi_get_mode(&cur_mode);
    bool switched_mode = false;
    if (cur_mode == WIFI_MODE_AP) {
        is_scanning = true; 
        esp_wifi_set_mode(WIFI_MODE_APSTA);
        switched_mode = true;
        esp_wifi_disconnect();
        vTaskDelay(pdMS_TO_TICKS(500)); 
    }
    wifi_scan_config_t scan_config = {0}; 
    scan_config.scan_type = WIFI_SCAN_TYPE_ACTIVE;
    esp_err_t err = esp_wifi_scan_start(&scan_config, true);
    is_scanning = false; 
    if (err != ESP_OK) { 
        if (switched_mode) esp_wifi_set_mode(WIFI_MODE_AP);
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Scan Error"); 
        return ESP_FAIL; 
    }
    uint16_t ap_count = 0; 
    esp_wifi_scan_get_ap_num(&ap_count); 
    if (ap_count > 15) ap_count = 15; 
    wifi_ap_record_t *ap_info = (wifi_ap_record_t *)malloc(sizeof(wifi_ap_record_t) * ap_count);
    if (!ap_info) { if (switched_mode) esp_wifi_set_mode(WIFI_MODE_AP); return ESP_FAIL; }
    esp_wifi_scan_get_ap_records(&ap_count, ap_info);
    if (switched_mode) esp_wifi_set_mode(WIFI_MODE_AP);
    cJSON *root = cJSON_CreateArray();
    for (int i = 0; i < ap_count; i++) { if (strlen((char*)ap_info[i].ssid) > 0) cJSON_AddItemToArray(root, cJSON_CreateString((char*)ap_info[i].ssid)); }
    free(ap_info); char *out = cJSON_PrintUnformatted(root); httpd_resp_set_type(req, "application/json"); httpd_resp_sendstr(req, out); free(out); cJSON_Delete(root); return ESP_OK;
}

esp_err_t post_save(httpd_req_t *req) {
    char *buf = malloc(req->content_len + 1);
    if (!buf) return ESP_FAIL;
    
    size_t received = 0;
    while (received < req->content_len) {
        int ret = httpd_req_recv(req, buf + received, req->content_len - received);
        if (ret <= 0) { if (ret == HTTPD_SOCK_ERR_TIMEOUT) continue; free(buf); return ESP_FAIL; }
        received += ret;
    }
    buf[received] = '\0'; 
    
    cJSON *root = cJSON_Parse(buf);
    if (root) {
        // Global Parsers
        cJSON *crev = cJSON_GetObjectItem(root, "cyc_rev"); 
        if(crev) dev_cfg.sw6_cycle_rev = cJSON_IsTrue(crev) ? 1 : 0; 
        
        cJSON *cfwd = cJSON_GetObjectItem(root, "cyc_fwd"); 
        if(cfwd) dev_cfg.sw7_cycle_fwd = cJSON_IsTrue(cfwd) ? 1 : 0; 
        
        cJSON *bri = cJSON_GetObjectItem(root, "brightness"); 
        if (bri) dev_cfg.global_brightness = bri->valueint;
        
        // REMOVED: Global exp parsing (now inside banks)

        // Parse Banks
        cJSON *banks = cJSON_GetObjectItem(root, "banks");
        if(banks) {
            for(int b=0; b<BANK_COUNT; b++) {
                cJSON *bk = cJSON_GetArrayItem(banks, b); 
                // --- NEW: Parse Expression Per Bank ---
                cJSON *exp = cJSON_GetObjectItem(bk, "exp");
                if(exp) {
                    volatile exp_cfg_t *e = &dev_cfg.banks[b].exp;
                    cJSON *ch = cJSON_GetObjectItem(exp, "ch"); if(ch) e->chan = ch->valueint;
                    cJSON *cc = cJSON_GetObjectItem(exp, "cc"); if(cc) e->cc = cc->valueint;
                    cJSON *mn = cJSON_GetObjectItem(exp, "min"); if(mn) e->min = mn->valueint;
                    
                    // FIX: Copy/Paste error (was setting e->max = mn->valueint)
                    cJSON *mx = cJSON_GetObjectItem(exp, "max"); if(mx) e->max = mx->valueint; 
                    
                    cJSON *cv = cJSON_GetObjectItem(exp, "crv"); if(cv) e->curve = cv->valueint;
                }
                cJSON *sws = cJSON_GetObjectItem(bk, "switches");
                if(sws) {
                    for(int i=0; i<SWITCH_COUNT; i++) {
                        cJSON *it = cJSON_GetArrayItem(sws, i); 
                        sw_cfg_t *s = (sw_cfg_t*)&dev_cfg.banks[b].switches[i];
                        
                        // MIDI Arrays
                        cJSON *p = cJSON_GetObjectItem(it, "p"); 
                        cJSON *lp = cJSON_GetObjectItem(it, "lp"); 
                        cJSON *l = cJSON_GetObjectItem(it, "l");
                        
                        // Basic Configs
                        cJSON *lpen = cJSON_GetObjectItem(it, "lp_en");
                        cJSON *tog = cJSON_GetObjectItem(it, "tog"); 
                        cJSON *incl = cJSON_GetObjectItem(it, "incl");

                        // Inside the switch loop in post_save:
                        cJSON *edge = cJSON_GetObjectItem(it, "edge");

                        // === NEW PER-ACTION GROUPING KEYS ===
                        cJSON *pe = cJSON_GetObjectItem(it, "pe");
                        cJSON *pm = cJSON_GetObjectItem(it, "pm");
                        
                        cJSON *lpe = cJSON_GetObjectItem(it, "lpe");
                        cJSON *lpm = cJSON_GetObjectItem(it, "lpm");
                        
                        cJSON *le = cJSON_GetObjectItem(it, "le");
                        cJSON *lm = cJSON_GetObjectItem(it, "lm");

                        // Assignments
                        if(p) { s->p_type=cJSON_GetArrayItem(p,0)->valueint; s->p_chan=cJSON_GetArrayItem(p,1)->valueint; s->p_d1=cJSON_GetArrayItem(p,2)->valueint; }
                        if(lp) { s->lp_type=cJSON_GetArrayItem(lp,0)->valueint; s->lp_chan=cJSON_GetArrayItem(lp,1)->valueint; s->lp_d1=cJSON_GetArrayItem(lp,2)->valueint; }
                        if(l) { s->l_type=cJSON_GetArrayItem(l,0)->valueint; s->l_chan=cJSON_GetArrayItem(l,1)->valueint; s->l_d1=cJSON_GetArrayItem(l,2)->valueint; }
                        
                        if(lpen) { s->lp_enabled = cJSON_IsTrue(lpen) ? 1 : 0; }
                        if(tog) { s->toggle_mode = cJSON_IsTrue(tog) ? 1 : 0; } 
                        if(incl) { s->incl_mask = incl->valueint; }
                        if(edge) s->p_edge = edge->valueint; // NEW
                        
                        // New Group Assignments
                        if(pe)  s->p_excl    = pe->valueint;
                        if(pm)  s->p_master  = pm->valueint;
                        if(lpe) s->lp_excl   = lpe->valueint;
                        if(lpm) s->lp_master = lpm->valueint;
                        if(le)  s->l_excl    = le->valueint;
                        if(lm)  s->l_master  = lm->valueint;
                    }
                }
            }
        }
        cJSON_Delete(root); 
        uint8_t trigger = 1; xQueueSend(save_queue, &trigger, 0);
        httpd_resp_sendstr(req, "OK");
    }
    free(buf); 
    httpd_resp_set_hdr(req, "Connection", "close"); 
    return ESP_OK;
}

void app_main(void) {
    vTaskDelay(pdMS_TO_TICKS(3000)); 

    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) { nvs_flash_erase(); nvs_flash_init(); }

    nvs_handle_t h; 
    bool load_dev_success = false, load_wifi_success = false;

    // --- IDENTITY MANAGEMENT START ---
    if(nvs_open("storage", NVS_READWRITE, &h) == ESP_OK) {
        size_t mac_sz = sizeof(current_mac_addr);
        if (nvs_get_blob(h, "bt_mac", current_mac_addr, &mac_sz) != ESP_OK) {
            esp_fill_random(current_mac_addr, 6);
            current_mac_addr[0] |= 0xC0; 
            nvs_set_blob(h, "bt_mac", current_mac_addr, sizeof(current_mac_addr));
            nvs_commit(h);
        }
        size_t sz = 0;
        if (nvs_get_blob(h, "dev_cfg", NULL, &sz) == ESP_OK && sz == sizeof(dev_cfg)) { nvs_get_blob(h, "dev_cfg", (void*)&dev_cfg, &sz); load_dev_success = true; }
        size_t w_sz = sizeof(w_cfg);
        if (nvs_get_blob(h, "w_cfg", &w_cfg, &w_sz) == ESP_OK && w_sz == sizeof(w_cfg)) { load_wifi_success = true; }
        uint8_t mode = 0;
        if (nvs_get_u8(h, "wifi_mode", &mode) == ESP_OK) { is_wifi_on = (mode == 1); }
        nvs_close(h);
    } 
    // --- IDENTITY MANAGEMENT END ---

    if (!load_dev_success) {
        dev_cfg.current_bank = 0; dev_cfg.sw6_cycle_rev = 0; dev_cfg.sw7_cycle_fwd = 0; dev_cfg.global_brightness = 127;
        
        // Loop through all banks to init switches AND expression defaults
        for(int b=0; b<BANK_COUNT; b++) {
            // Default Switches
            for(int i=0; i<SWITCH_COUNT; i++) {
                dev_cfg.banks[b].switches[i] = (sw_cfg_t){MIDI_CC, 0, 80+i, 0, MIDI_CC, 0, 90+i, MIDI_CC, 0, 80+i, 0, 0, 0, 0, 0, 0, 0, 0, 0}; 
            }
            // Default Expression (Per Bank)
            dev_cfg.banks[b].exp.chan = 0;
            dev_cfg.banks[b].exp.cc = 11;
            dev_cfg.banks[b].exp.min = 100;
            dev_cfg.banks[b].exp.max = 4000;
            dev_cfg.banks[b].exp.curve = 0;
        }
    }
    if (!load_wifi_success) { strcpy(w_cfg.ssid, "SSID"); strcpy(w_cfg.pass, "Password"); }

    save_queue = xQueueCreate(1, sizeof(uint8_t)); xTaskCreate(nvs_save_task, "nvs_save", 3072, NULL, 1, NULL);
    
    // Init GPIO
    for(int i=0; i<NUM_ROWS; i++) {
        gpio_reset_pin(ROW_PINS[i]);
        gpio_set_direction(ROW_PINS[i], GPIO_MODE_OUTPUT);
        gpio_set_level(ROW_PINS[i], 1); // Start High
    }
    for(int i=0; i<NUM_COLS; i++) {
        gpio_reset_pin(COL_PINS[i]);
        gpio_set_direction(COL_PINS[i], GPIO_MODE_INPUT);
        gpio_set_pull_mode(COL_PINS[i], GPIO_PULLUP_ONLY);
    }
    
    // --- RESET IDENTITY (COMBO BTN 5 + BTN 8 ON BOOT) ---
    // Manually Scan Matrix
    // Btn 5 is Index 4 (Row 1, Col 0) -> Row Pin[1], Col Pin[0]
    // Btn 8 is Index 7 (Row 1, Col 3) -> Row Pin[1], Col Pin[3]
    bool btn5_pressed = false;
    bool btn8_pressed = false;
    
    gpio_set_level(ROW_PINS[1], 0); // Activate Row 2
    esp_rom_delay_us(10);
    if(gpio_get_level(COL_PINS[0]) == 0) btn5_pressed = true; // Btn 5
    if(gpio_get_level(COL_PINS[3]) == 0) btn8_pressed = true; // Btn 8
    gpio_set_level(ROW_PINS[1], 1);

    if (btn5_pressed && btn8_pressed) {
        ESP_LOGW(TAG, "COMBO HELD ON BOOT: Generating NEW Identity...");
        esp_fill_random(current_mac_addr, 6);
        current_mac_addr[0] |= 0xC0;
        if(nvs_open("storage", NVS_READWRITE, &h) == ESP_OK) {
            nvs_set_blob(h, "bt_mac", current_mac_addr, sizeof(current_mac_addr));
            nvs_commit(h); nvs_close(h);
        }
        rmt_config_t config = RMT_DEFAULT_CONFIG_TX(LED_DATA_PIN, RMT_CHANNEL_0); config.clk_div = 4; rmt_config(&config); rmt_driver_install(config.channel, 0, 0);
        // Flash Purple to confirm
        for(int k=0; k<10; k++) { 
            for(int j=0; j<LED_COUNT; j++) set_pixel(j, 255, 0, 255);
            refresh_leds();
            vTaskDelay(pdMS_TO_TICKS(100)); 
            for(int j=0; j<LED_COUNT; j++) set_pixel(j, 0, 0, 0);
            refresh_leds();
            vTaskDelay(pdMS_TO_TICKS(100)); 
        }
    }

    init_battery(); 
    init_expression_pedal();

    tinyusb_config_t tusb_cfg = TINYUSB_DEFAULT_CONFIG(); tusb_cfg.descriptor.string = s_str_desc; tusb_cfg.descriptor.string_count = 5; tusb_cfg.descriptor.full_speed_config = s_midi_cfg_desc;
    tinyusb_driver_install(&tusb_cfg);

    s_wifi_event_group = xEventGroupCreate(); esp_netif_init(); esp_event_loop_create_default();
    esp_netif_create_default_wifi_sta(); esp_netif_create_default_wifi_ap();
    wifi_init_config_t wcfg = WIFI_INIT_CONFIG_DEFAULT(); esp_wifi_init(&wcfg);
    esp_wifi_set_ps(WIFI_PS_NONE);

    esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler, NULL, NULL);
    esp_event_handler_instance_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &wifi_event_handler, NULL, NULL);
    
    // --- FORCE AP IF NOT CONFIGURED ---
    if (strcmp(w_cfg.ssid, "SSID") == 0) {
        ESP_LOGI(TAG, "Default Credentials Detected. Forcing WiFi/AP Mode.");
        is_wifi_on = true; 
    }

    if (is_wifi_on) { xTaskCreatePinnedToCore(wifi_init_task, "wifi_init", 4096, NULL, 10, NULL, 1); } else { init_ble_midi(); }

    xTaskCreatePinnedToCore(midi_task, "midi", 4096, NULL, 5, NULL, 0);
}

// ... other variables ...
    uint32_t last_debounce_time[SWITCH_COUNT];
    bool sw_toggled_on[BANK_COUNT][SWITCH_COUNT] = {false};
    // NEW: Track which switch caused the flash
    int flash_source_sw_idx = -1;

// --- RECURSIVE TRIGGER OFF FUNCTION ---
// Handles the "Chain Reaction" for turning switches OFF.
// If Master2 turns off Master1, this function ensures Master1 also turns off its Slaves.
void trigger_switch_off(uint8_t bank, uint8_t idx) {
    
    // 1. Safety Check: If already OFF, do nothing (stops infinite loops)
    if (!sw_toggled_on[bank][idx]) return;

    // 2. Turn This Switch OFF
    sw_toggled_on[bank][idx] = false;
    sw_cfg_t *cfg = (sw_cfg_t*)&dev_cfg.banks[bank].switches[idx];
    
    // Send MIDI "Release/Off" Message
    send_midi_msg(cfg->l_type, cfg->l_chan, cfg->l_d1, 0);

    // 3. Recursive Cascade (Kill Slaves)
    // We use the Release Master Mask (l_master) to find slaves to kill.
    if (cfg->l_master > 0) {
        for (int b = 0; b < BANK_COUNT; b++) {
            for (int s = 0; s < SWITCH_COUNT; s++) {
                if (b == bank && s == idx) continue;
                
                sw_cfg_t *other = (sw_cfg_t*)&dev_cfg.banks[b].switches[s];

                // Do I lead this slave?
                if ((other->incl_mask & cfg->l_master) != 0) {
                    // RECURSIVE CALL: Kill the slave!
                    // This allows the off-signal to travel down the chain (MS2 -> MS1 -> SS)
                    trigger_switch_off(b, s);
                }
            }
        }
    }
}

// --- RECURSIVE TRIGGER ON FUNCTION ---
// Updated to use trigger_switch_off for Exclusive conflicts
void trigger_switch_on(uint8_t bank, uint8_t idx, uint8_t active_excl, uint8_t active_master) {
    
    if (sw_toggled_on[bank][idx]) return;

    sw_toggled_on[bank][idx] = true;
    sw_cfg_t *cfg = (sw_cfg_t*)&dev_cfg.banks[bank].switches[idx];
    
    send_midi_msg(cfg->p_type, cfg->p_chan, cfg->p_d1, 127);

    // 1. Handle EXCLUSIVE Conflicts
    if (active_excl > 0) {
        for (int b = 0; b < BANK_COUNT; b++) {
            for (int s = 0; s < SWITCH_COUNT; s++) {
                if (b == bank && s == idx) continue; 
                
                sw_cfg_t *other = (sw_cfg_t*)&dev_cfg.banks[b].switches[s];
                if ((other->p_excl & active_excl) != 0) {
                    // NEW: Use Recursive Off!
                    // If we kill a rival Master, we want its slaves to die too.
                    trigger_switch_off(b, s);
                }
            }
        }
    }

    // 2. Handle INCLUSIVE Cascades
    if (active_master > 0) {
        for (int b = 0; b < BANK_COUNT; b++) {
            for (int s = 0; s < SWITCH_COUNT; s++) {
                if (b == bank && s == idx) continue; 
                
                sw_cfg_t *other = (sw_cfg_t*)&dev_cfg.banks[b].switches[s];
                
                if ((other->incl_mask & active_master) != 0) {
                    trigger_switch_on(b, s, other->p_excl, other->p_master);
                }
            }
        }
    }
}

void midi_task(void *pv) {
    // --- SETUP RMT (LEDs) ---
    rmt_config_t config = RMT_DEFAULT_CONFIG_TX(LED_DATA_PIN, RMT_CHANNEL_0); 
    config.clk_div = 4; 
    rmt_config(&config); 
    rmt_driver_install(config.channel, 0, 0);
    
    // --- LOCAL STATE VARIABLES ---
    bool button_state[SWITCH_COUNT]; 
    bool last_button_state[SWITCH_COUNT]; 
    uint32_t press_start[SWITCH_COUNT]; 
    bool lp_triggered[SWITCH_COUNT]; 
    uint32_t last_debounce_time[SWITCH_COUNT];
    
    // Initialize Arrays
    for(int b=0; b<BANK_COUNT; b++) {
        for(int i=0; i<SWITCH_COUNT; i++) { 
            button_state[i] = false; 
            last_button_state[i] = false; 
            press_start[i] = 0; 
            lp_triggered[i] = false; 
            last_debounce_time[i] = 0; 
        }
    }
    
    // Timers & Triggers
    uint32_t combo_timer = 0;
    bool combo_triggered = false;
    uint32_t last_slow_task_time = 0; 
    
    // LED Flash Logic
    uint32_t flash_end_time = 0;      
    int flash_source_sw_idx = -1; 

    while(1) {
        int64_t now_us = esp_timer_get_time();
        uint32_t now_ms = (uint32_t)(now_us / 1000);

        // ============================================================
        //  FAST LOOP
        // ============================================================

        // 1. SCAN MATRIX
        for (int r = 0; r < NUM_ROWS; r++) {
            gpio_set_level(ROW_PINS[r], 0); 
            esp_rom_delay_us(10); 
            for (int c = 0; c < NUM_COLS; c++) {
                int sw_idx = MATRIX_MAP[r][c];
                bool pressed = (gpio_get_level(COL_PINS[c]) == 0); 
                
                if (pressed != button_state[sw_idx]) {
                    if ((now_ms - last_debounce_time[sw_idx]) > 50) { 
                        button_state[sw_idx] = pressed;
                        last_debounce_time[sw_idx] = now_ms;
                    }
                }
            }
            gpio_set_level(ROW_PINS[r], 1); 
        }

        // 2. COMBO CHECK
        if (button_state[4] && button_state[7]) {
            if (combo_timer == 0) combo_timer = now_ms;
            if (now_ms - combo_timer > COMBO_HOLD_MS && !combo_triggered) {
                combo_triggered = true;
                bool new_wifi_mode = !is_wifi_on;
                set_wifi_mode(new_wifi_mode);
                uint8_t r = new_wifi_mode ? dev_cfg.global_brightness : 0;
                uint8_t g = new_wifi_mode ? dev_cfg.global_brightness : 0;
                uint8_t b = dev_cfg.global_brightness;
                for(int j=0; j<LED_COUNT; j++) set_pixel(j, r, g, b);
                refresh_leds();
                vTaskDelay(pdMS_TO_TICKS(500)); 
            }
        } else {
            combo_timer = 0; combo_triggered = false;
        }

        // 3. SWITCH LOGIC PROCESSOR
        for(int i=0; i<SWITCH_COUNT; i++) {
            bool s = button_state[i];
            bool last_s = last_button_state[i];
            sw_cfg_t *cfg = (sw_cfg_t*)&dev_cfg.banks[dev_cfg.current_bank].switches[i];
            
            bool trigger_action = false; // Flag: Should we fire the "Press" action now?

            // Detect Edge
            if (s != last_s) { 
                
                // === PHYSICAL PRESS ===
                if (s == true) { 
                    press_start[i] = now_ms; 
                    lp_triggered[i] = false; 

                    // If Leading Edge (0), Trigger NOW
                    if (cfg->p_edge == 0) trigger_action = true;
                } 
                // === PHYSICAL RELEASE ===
                else { 
                    // If Trailing Edge (1) AND No LP happened, Trigger NOW
                    if (cfg->p_edge == 1 && !lp_triggered[i]) trigger_action = true;
                }
                
                // -----------------------------------------------------
                //  ACTION EXECUTION (Runs on Press OR Release based on Config)
                // -----------------------------------------------------
                if (trigger_action) {
                    flash_end_time = now_ms + 50; 
                    flash_source_sw_idx = i;
                    bool is_cycling = false;

                    // A. Bank Cycle / Direct Bank
                    if (cfg->p_type >= 250) { 
                        if (cfg->p_type == 250) dev_cfg.current_bank = (dev_cfg.current_bank - 1 + BANK_COUNT) % BANK_COUNT; 
                        else if (cfg->p_type == 251) dev_cfg.current_bank = (dev_cfg.current_bank + 1) % BANK_COUNT; 
                        else {
                            uint8_t target_bank = cfg->p_type - 252;
                            if (target_bank < BANK_COUNT) dev_cfg.current_bank = target_bank;
                        }
                        uint8_t trigger = 1; xQueueSend(save_queue, &trigger, 0); 
                        is_cycling = true; 
                    }

                    // B. Standard Logic
                    if (!is_cycling) { 
                        if (cfg->toggle_mode) {
                            if (!sw_toggled_on[dev_cfg.current_bank][i]) {
                                // Turning ON
                                trigger_switch_on(dev_cfg.current_bank, i, cfg->p_excl, cfg->p_master);
                            } else {
                                // Turning OFF
                                trigger_switch_off(dev_cfg.current_bank, i);
                            }
                        } else {
                            // Momentary Press
                            if (sw_toggled_on[dev_cfg.current_bank][i]) sw_toggled_on[dev_cfg.current_bank][i] = false;
                            trigger_switch_on(dev_cfg.current_bank, i, cfg->p_excl, cfg->p_master);
                        }
                    }
                }
                
                // -----------------------------------------------------
                //  RELEASE CLEANUP (Always runs on Physical Release)
                // -----------------------------------------------------
                if (s == false && cfg->p_type < 250) {
                    // Momentary Release Logic
                    if (!cfg->toggle_mode) { 
                        trigger_switch_off(dev_cfg.current_bank, i);
                    }
                }
                last_button_state[i] = s; 
            }
            
            // === LONG PRESS CHECK ===
            if(button_state[i] && !lp_triggered[i] && (now_ms - press_start[i] > LONG_PRESS_MS)) {
                // Only allow LP if we aren't waiting for a Trailing Edge trigger?
                // Actually, standard behavior is: Hold > LP fires > Release (Short Press skipped because LP triggered)
                // This works perfectly with the logic above.
                if(cfg->lp_enabled == 1 && cfg->p_type < 250) { 
                    lp_triggered[i] = true; 
                    send_midi_msg(cfg->lp_type, cfg->lp_chan, cfg->lp_d1, 127); 

                    // LP Grouping
                    if (cfg->lp_excl > 0) {
                         for (int b = 0; b < BANK_COUNT; b++) {
                            for (int s = 0; s < SWITCH_COUNT; s++) {
                                if (b == dev_cfg.current_bank && s == i) continue; 
                                sw_cfg_t *other = (sw_cfg_t*)&dev_cfg.banks[b].switches[s];
                                if ((other->p_excl & cfg->lp_excl) != 0) trigger_switch_off(b, s);
                            }
                        }
                    }
                    if (cfg->lp_master > 0) {
                        for (int b = 0; b < BANK_COUNT; b++) {
                            for (int s = 0; s < SWITCH_COUNT; s++) {
                                if (b == dev_cfg.current_bank && s == i) continue; 
                                sw_cfg_t *other = (sw_cfg_t*)&dev_cfg.banks[b].switches[s];
                                if ((other->incl_mask & cfg->lp_master) != 0) trigger_switch_on(b, s, other->p_excl, other->p_master);
                            }
                        }
                    }
                    flash_end_time = now_ms + 50; 
                    flash_source_sw_idx = i; 
                }
            }
        }

        // ============================================================
        //  SLOW LOOP
        // ============================================================
        if (now_ms - last_slow_task_time > 10) {
            last_slow_task_time = now_ms;
            process_expression_pedal();
            read_battery();

            if (now_ms < flash_end_time) {
                uint8_t fw = dev_cfg.global_brightness / 3; 
                uint8_t br = BANK_COLORS[dev_cfg.current_bank][0] * dev_cfg.global_brightness / 255;
                uint8_t bg = BANK_COLORS[dev_cfg.current_bank][1] * dev_cfg.global_brightness / 255;
                uint8_t bb = BANK_COLORS[dev_cfg.current_bank][2] * dev_cfg.global_brightness / 255;

                for(int k=0; k<LED_COUNT; k++) {
                    bool is_source = false;
                    if (flash_source_sw_idx != -1) {
                        int target_led = (flash_source_sw_idx < 4) ? flash_source_sw_idx + 1 : 12 - flash_source_sw_idx;
                        if (k == target_led) is_source = true;
                    }
                    if (is_source) set_pixel(k, br, bg, bb); 
                    else set_pixel(k, fw, fw, fw); 
                }
                refresh_leds();
            } 
            else {
                flash_source_sw_idx = -1; 
                update_status_leds(g_bat_voltage, dev_cfg.current_bank, sw_toggled_on[dev_cfg.current_bank], button_state);
            }
        }
        const TickType_t delay = pdMS_TO_TICKS(1);
        vTaskDelay(delay == 0 ? 1 : delay);
    }
}
