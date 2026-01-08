#include "esp_check.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "ha/esp_zigbee_ha_standard.h"
#include "main.h"
#include "driver/uart.h"
#include "kamstrup_test_data.h"
#include <esp_app_desc.h>

#include "esp_random.h"
#include "dlms_parser.h"

#include "esp_timer.h"
#include "esp_zigbee_attribute.h"

#include <stdio.h>
#include <stdint.h>
#include <time.h>
#include <inttypes.h>
#include <string.h>

#include "driver/gpio.h"
#include <iot_button.h>

#include "esp_pm.h"
#include "esp_err.h"

#include "esp_adc/adc_oneshot.h"
#include "esp_adc/adc_cali.h"
#include "esp_adc/adc_cali_scheme.h"

#ifdef CONFIG_PM_ENABLE
#include "esp_pm.h"
#include "esp_private/esp_clk.h"
#include "esp_sleep.h"
#endif

#if !defined ZB_ED_ROLE
#error Define ZB_ED_ROLE in idf.py menuconfig to compile light (End Device) source code.
#endif

#if DATA_SIMULATION
#include "kamstrup_test_data.h"
#endif

static const char *TAG = "PowerMeter";

static QueueHandle_t uart_queue;
static dlms_parser_t parser;
static int64_t lastReceived = 0;
static bool waitingForSilence = true;
static TaskHandle_t task_handle = NULL;

// static int adc_raw[2][10];
// static int voltage[2][10];

// uint32_t convert_timestamp_to_seconds_since_2000(const char* timestamp) {
//     // Parse timestamp parts
//     int year, month, day, hour, minute, second;
//     sscanf(timestamp, "%4X%2X%2X%2X%2X%2X", &year, &month, &day, &hour, &minute, &second);

//     // Create tm structure for the timestamp
//     struct tm timestamp_tm = {0};
//     timestamp_tm.tm_year = year - 1900;  // tm_year is years since 1900
//     timestamp_tm.tm_mon = month - 1;     // tm_mon is 0-11
//     timestamp_tm.tm_mday = day;
//     timestamp_tm.tm_hour = hour;
//     timestamp_tm.tm_min = minute;
//     timestamp_tm.tm_sec = second;

//     // Create tm structure for Jan 1, 2000
//     struct tm epoch_2000_tm = {0};
//     epoch_2000_tm.tm_year = 100;  // 2000 - 1900
//     epoch_2000_tm.tm_mday = 1;    // 1st day of month

//     // Convert both to time_t (seconds since Jan 1, 1970)
//     time_t timestamp_time = mktime(&timestamp_tm);
//     time_t epoch_2000_time = mktime(&epoch_2000_tm);

//     // Account for timezone differences as mktime uses local time
//     // Adjust for GMT/UTC if necessary based on your system

//     // Calculate difference
//     uint32_t seconds_since_2000 = (uint32_t)(timestamp_time - epoch_2000_time);

//     return seconds_since_2000;
// }

int8_t convert_to_int8(uint8_t *bytes)
{
    return (int8_t)(bytes[0]);
}

uint16_t convert_to_uint16(uint8_t *bytes)
{
    return (uint16_t)(bytes[0] << 8 | bytes[1]);
}

int16_t convert_to_int16(uint8_t *bytes)
{
    return (int16_t)(bytes[0] << 8 | bytes[1]);
}

uint32_t convert_to_uint32(uint8_t *bytes)
{
    return (uint32_t)(bytes[0] << 24 | bytes[1] << 16 | bytes[2] << 8 | bytes[3]);
}

static void handle_dlms_field(dlms_field_t *field)
{

    if (field == NULL)
    {
        ESP_LOGW(TAG, "Received null field");
        return;
    }

    switch (field->type)
    {

    case START:
        ESP_LOGI(TAG, "Start received. Acquiring lock");
        esp_zb_lock_acquire(portMAX_DELAY);
        gpio_set_level(LED_PIN, 0);
        gpio_set_level(LED_PIN2, 1); // Green LED on
        break;

    case END:
        ESP_LOGI(TAG, "End received. Releasing lock");
        esp_zb_lock_release();
        gpio_set_level(LED_PIN, 0);
        gpio_set_level(LED_PIN2, 0);
        break;

    case RMS_VOLTAGE_A:
        uint16_t valueA = convert_to_uint16(field->data);
        ESP_LOGI(TAG, "Received RMS Voltage A: %d", valueA);
        esp_zb_zcl_set_attribute_val(ENDPOINT_ID, ESP_ZB_ZCL_CLUSTER_ID_ELECTRICAL_MEASUREMENT, ESP_ZB_ZCL_CLUSTER_SERVER_ROLE, ESP_ZB_ZCL_ATTR_ELECTRICAL_MEASUREMENT_RMSVOLTAGE_ID, &valueA, false); //!< Represents the most recent RMS voltage reading in @e Volts (V).
        break;

    case RMS_VOLTAGE_B:
        uint16_t valueB = convert_to_uint16(field->data);
        ESP_LOGI(TAG, "Received RMS Voltage B: %d", valueB);
        esp_zb_zcl_set_attribute_val(ENDPOINT_ID, ESP_ZB_ZCL_CLUSTER_ID_ELECTRICAL_MEASUREMENT, ESP_ZB_ZCL_CLUSTER_SERVER_ROLE, ESP_ZB_ZCL_ATTR_ELECTRICAL_MEASUREMENT_RMSVOLTAGE_PHB_ID, &valueB, false); //!< Represents the most recent RMS voltage reading in @e Volts (V).
        break;

    case RMS_VOLTAGE_C:
        uint16_t valueC = convert_to_uint16(field->data);
        ESP_LOGI(TAG, "Received RMS Voltage C: %d", valueC);
        esp_zb_zcl_set_attribute_val(ENDPOINT_ID, ESP_ZB_ZCL_CLUSTER_ID_ELECTRICAL_MEASUREMENT, ESP_ZB_ZCL_CLUSTER_SERVER_ROLE, ESP_ZB_ZCL_ATTR_ELECTRICAL_MEASUREMENT_RMSVOLTAGE_PHC_ID, &valueC, false); //!< Represents the most recent RMS voltage reading in @e Volts (V).
        break;

    case POWER_FACTOR_A:
        uint16_t factorA = convert_to_uint16(field->data);
        uint8_t factorA_8 = (uint8_t)(factorA);
        ESP_LOGI(TAG, "Received factor A: %d", factorA);
        esp_zb_zcl_set_attribute_val(ENDPOINT_ID, ESP_ZB_ZCL_CLUSTER_ID_ELECTRICAL_MEASUREMENT, ESP_ZB_ZCL_CLUSTER_SERVER_ROLE, ESP_ZB_ZCL_ATTR_ELECTRICAL_MEASUREMENT_POWER_FACTOR_ID, &factorA_8, false);
        break;

    case POWER_FACTOR_B:
        uint16_t factorB = convert_to_uint16(field->data);
        uint8_t factorB_8 = (uint8_t)(factorB);
        ESP_LOGI(TAG, "Received factor B: %d", factorB);
        esp_zb_zcl_set_attribute_val(ENDPOINT_ID, ESP_ZB_ZCL_CLUSTER_ID_ELECTRICAL_MEASUREMENT, ESP_ZB_ZCL_CLUSTER_SERVER_ROLE, ESP_ZB_ZCL_ATTR_ELECTRICAL_MEASUREMENT_POWER_FACTOR_PH_B_ID, &factorB_8, false);
        break;

    case POWER_FACTOR_C:
        uint16_t factorC = convert_to_uint16(field->data);
        uint8_t factorC_8 = (uint8_t)(factorC);
        ESP_LOGI(TAG, "Received factor C: %d", factorC);
        esp_zb_zcl_set_attribute_val(ENDPOINT_ID, ESP_ZB_ZCL_CLUSTER_ID_ELECTRICAL_MEASUREMENT, ESP_ZB_ZCL_CLUSTER_SERVER_ROLE, ESP_ZB_ZCL_ATTR_ELECTRICAL_MEASUREMENT_POWER_FACTOR_PH_C_ID, &factorC_8, false);
        break;

    case RMS_CURRENT_A:
        uint32_t currentA = convert_to_uint32(field->data);
        uint16_t currentA_16 = (uint16_t)(currentA);
        ESP_LOGI(TAG, "Received RMS current A: %u", currentA);
        esp_zb_zcl_set_attribute_val(ENDPOINT_ID, ESP_ZB_ZCL_CLUSTER_ID_ELECTRICAL_MEASUREMENT, ESP_ZB_ZCL_CLUSTER_SERVER_ROLE, ESP_ZB_ZCL_ATTR_ELECTRICAL_MEASUREMENT_RMSCURRENT_ID, &currentA_16, false);
        break;

    case RMS_CURRENT_B:
        uint32_t currentB = convert_to_uint32(field->data);
        uint16_t currentB_16 = (uint16_t)(currentB);
        ESP_LOGI(TAG, "Received RMS current B: %u", currentB);
        esp_zb_zcl_set_attribute_val(ENDPOINT_ID, ESP_ZB_ZCL_CLUSTER_ID_ELECTRICAL_MEASUREMENT, ESP_ZB_ZCL_CLUSTER_SERVER_ROLE, ESP_ZB_ZCL_ATTR_ELECTRICAL_MEASUREMENT_RMSCURRENT_PHB_ID, &currentB_16, false);
        break;

    case RMS_CURRENT_C:
        uint32_t currentC = convert_to_uint32(field->data);
        uint16_t currentC_16 = (uint16_t)(currentC);
        ESP_LOGI(TAG, "Received RMS current C: %u", currentC);
        esp_zb_zcl_set_attribute_val(ENDPOINT_ID, ESP_ZB_ZCL_CLUSTER_ID_ELECTRICAL_MEASUREMENT, ESP_ZB_ZCL_CLUSTER_SERVER_ROLE, ESP_ZB_ZCL_ATTR_ELECTRICAL_MEASUREMENT_RMSCURRENT_PHC_ID, &currentC_16, false);
        break;

    case ACTIVE_POWER_A:
        uint32_t powerA = convert_to_uint32(field->data);
        int16_t powerA_16 = (int16_t)(powerA);
        ESP_LOGI(TAG, "Received ACTIVE_POWER_A: %u", powerA);
        esp_zb_zcl_set_attribute_val(ENDPOINT_ID, ESP_ZB_ZCL_CLUSTER_ID_ELECTRICAL_MEASUREMENT, ESP_ZB_ZCL_CLUSTER_SERVER_ROLE, ESP_ZB_ZCL_ATTR_ELECTRICAL_MEASUREMENT_ACTIVE_POWER_ID, &powerA_16, false);
        break;

    case ACTIVE_POWER_B:
        uint32_t powerB = convert_to_uint32(field->data);
        int16_t powerB_16 = (int16_t)(powerB);
        ESP_LOGI(TAG, "Received ACTIVE_POWER_B: %u", powerB);
        esp_zb_zcl_set_attribute_val(ENDPOINT_ID, ESP_ZB_ZCL_CLUSTER_ID_ELECTRICAL_MEASUREMENT, ESP_ZB_ZCL_CLUSTER_SERVER_ROLE, ESP_ZB_ZCL_ATTR_ELECTRICAL_MEASUREMENT_ACTIVE_POWER_PHB_ID, &powerB_16, false);
        break;

    case ACTIVE_POWER_C:
        uint32_t powerC = convert_to_uint32(field->data);
        int16_t powerC_16 = (int16_t)(powerC);
        ESP_LOGI(TAG, "Received ACTIVE_POWER_C: %u", powerC);
        esp_zb_zcl_set_attribute_val(ENDPOINT_ID, ESP_ZB_ZCL_CLUSTER_ID_ELECTRICAL_MEASUREMENT, ESP_ZB_ZCL_CLUSTER_SERVER_ROLE, ESP_ZB_ZCL_ATTR_ELECTRICAL_MEASUREMENT_ACTIVE_POWER_PHC_ID, &powerC_16, false);
        break;

    case REACTIVE_POWER_A:
        uint32_t rePowerA = convert_to_uint32(field->data);
        int16_t rePowerA_16 = (int16_t)(rePowerA);
        // ESP_LOGI(TAG, "Received REACTIVE A: %u", rePowerA);
        esp_zb_zcl_set_attribute_val(ENDPOINT_ID, ESP_ZB_ZCL_CLUSTER_ID_ELECTRICAL_MEASUREMENT, ESP_ZB_ZCL_CLUSTER_SERVER_ROLE, ESP_ZB_ZCL_ATTR_ELECTRICAL_MEASUREMENT_REACTIVE_POWER_ID, &rePowerA_16, false);
        break;

    case REACTIVE_POWER_B:
        uint32_t rePowerB = convert_to_uint32(field->data);
        int16_t rePowerB_16 = (int16_t)(rePowerB);
        // ESP_LOGI(TAG, "Received REACTIVE B: %u", rePowerB);
        esp_zb_zcl_set_attribute_val(ENDPOINT_ID, ESP_ZB_ZCL_CLUSTER_ID_ELECTRICAL_MEASUREMENT, ESP_ZB_ZCL_CLUSTER_SERVER_ROLE, ESP_ZB_ZCL_ATTR_ELECTRICAL_MEASUREMENT_REACTIVE_POWER_PH_B_ID, &rePowerB_16, false);
        break;

    case REACTIVE_POWER_C:
        uint32_t rePowerC = convert_to_uint32(field->data);
        int16_t rePowerC_16 = (int16_t)(rePowerC);
        // ESP_LOGI(TAG, "Received REACTIVE C: %u", rePowerC);
        esp_zb_zcl_set_attribute_val(ENDPOINT_ID, ESP_ZB_ZCL_CLUSTER_ID_ELECTRICAL_MEASUREMENT, ESP_ZB_ZCL_CLUSTER_SERVER_ROLE, ESP_ZB_ZCL_ATTR_ELECTRICAL_MEASUREMENT_REACTIVE_POWER_PH_C_ID, &rePowerC_16, false);
        break;

    case ACTIVE_ENERGY_IMPORT:
        uint64_t powerImport = convert_to_uint32(field->data);
        ESP_LOGI(TAG, "Received ACTIVE_ENERGY_IMPORT: %" PRIu64, powerImport);

        esp_zb_zcl_set_attribute_val(ENDPOINT_ID, ESP_ZB_ZCL_CLUSTER_ID_METERING, ESP_ZB_ZCL_CLUSTER_SERVER_ROLE, ESP_ZB_ZCL_ATTR_METERING_CURRENT_SUMMATION_DELIVERED_ID, &powerImport, false);

        esp_zb_zcl_report_attr_cmd_t report_attr_cmd = {0};
        report_attr_cmd.address_mode = ESP_ZB_APS_ADDR_MODE_DST_ADDR_ENDP_NOT_PRESENT;
        report_attr_cmd.attributeID = ESP_ZB_ZCL_ATTR_METERING_CURRENT_SUMMATION_DELIVERED_ID;
        report_attr_cmd.direction = ESP_ZB_ZCL_CMD_DIRECTION_TO_CLI;
        report_attr_cmd.clusterID = ESP_ZB_ZCL_CLUSTER_ID_METERING;
        // report_attr_cmd.cluster_role = ESP_ZB_ZCL_CLUSTER_SERVER_ROLE;
        report_attr_cmd.zcl_basic_cmd.src_endpoint = ENDPOINT_ID;

        esp_zb_zcl_report_attr_cmd_req(&report_attr_cmd);

        break;

    case ACTIVE_ENERGY_EXPORT:
        uint64_t powerExport = convert_to_uint32(field->data);
        ESP_LOGI(TAG, "Received ACTIVE_ENERGY_EXPORT: %" PRIu64, powerExport);

        esp_zb_zcl_set_attribute_val(ENDPOINT_ID, ESP_ZB_ZCL_CLUSTER_ID_METERING, ESP_ZB_ZCL_CLUSTER_SERVER_ROLE, ESP_ZB_ZCL_ATTR_METERING_CURRENT_SUMMATION_RECEIVED_ID, &powerExport, false);

        esp_zb_zcl_report_attr_cmd_t report_attr_cmd_export = {0};
        report_attr_cmd_export.address_mode = ESP_ZB_APS_ADDR_MODE_DST_ADDR_ENDP_NOT_PRESENT;
        report_attr_cmd_export.attributeID = ESP_ZB_ZCL_ATTR_METERING_CURRENT_SUMMATION_RECEIVED_ID;
        report_attr_cmd_export.direction = ESP_ZB_ZCL_CMD_DIRECTION_TO_CLI;
        report_attr_cmd_export.clusterID = ESP_ZB_ZCL_CLUSTER_ID_METERING;
        // report_attr_cmd.cluster_role = ESP_ZB_ZCL_CLUSTER_SERVER_ROLE;
        report_attr_cmd_export.zcl_basic_cmd.src_endpoint = ENDPOINT_ID;

        esp_zb_zcl_report_attr_cmd_req(&report_attr_cmd_export);

        break;

    case DLMS_FIELD_TIMESTAMP:
        ESP_LOGI(TAG, "Received timestamp");

        ESP_LOGI(TAG, "Timestamp: %s", (const char *)field->data);

        // uint32_t timestamp = convert_timestamp_to_seconds_since_2000((const char *)field->data);
        // ESP_LOGI(TAG, "Timestamp: %u", timestamp);
        break;

    case SERIAL_NUMBER:
        uint32_t serial = (field->data[0] << 24) |
                          (field->data[1] << 16) |
                          (field->data[2] << 8) |
                          field->data[3];

        ESP_LOGI(TAG, "Serial Number: %u", serial);

        esp_zb_zcl_attr_t *attr = esp_zb_zcl_get_attribute(ENDPOINT_ID, ESP_ZB_ZCL_CLUSTER_ID_METERING, ESP_ZB_ZCL_CLUSTER_SERVER_ROLE, ESP_ZB_ZCL_ATTR_METERING_METER_SERIAL_NUMBER_ID);
        bool should_set = true;
        if (attr && attr->data_p != NULL)
        {
            uint8_t *cur = (uint8_t *)attr->data_p;
            if (cur[0] != 0)
            {
                ESP_LOGI(TAG, "Meter serial already set, skipping update");
                should_set = false;
            }
        }

        if (should_set)
        {
            uint8_t serial_octstr[5] = {
                4, // Length byte
                field->data[0],
                field->data[1],
                field->data[2],
                field->data[3]};

            esp_zb_zcl_set_attribute_val(ENDPOINT_ID, ESP_ZB_ZCL_CLUSTER_ID_METERING, ESP_ZB_ZCL_CLUSTER_SERVER_ROLE, ESP_ZB_ZCL_ATTR_METERING_METER_SERIAL_NUMBER_ID, &serial_octstr, false);
            ESP_LOGI(TAG, "Meter serial attribute set");
        }
        break;

    default:
        break;
    }
}

static void initLedFlash(void *pvParameters)
{
    gpio_set_level(LED_PIN, 0);
    gpio_set_level(LED_PIN2, 0);

    bool on = false;
    while (1)
    {
        if (on)
        {
            gpio_set_level(LED_PIN, 1);
            vTaskDelay(50 / portTICK_PERIOD_MS);
        }
        else
        {
            gpio_set_level(LED_PIN, 0);
            vTaskDelay(1000 / portTICK_PERIOD_MS);
        }

        on = !on;
    }
}

static void uart_event_task(void *pvParameters)
{

    lastReceived = esp_timer_get_time();

    uart_event_t event;
    uint8_t *data = (uint8_t *)malloc(UART_RX_BUFFER_SIZE);

    while (1)
    {
        if (xQueueReceive(uart_queue, (void *)&event, portMAX_DELAY))
        {
            switch (event.type)
            {
            case UART_DATA:

                // Read the received data
                int length = uart_read_bytes(UART_NUM, data, event.size, portMAX_DELAY);
                if (length > 0)
                {

                    int64_t currentTime = esp_timer_get_time();

                    if (waitingForSilence && (currentTime - lastReceived) < 3000000)
                    {
                        ESP_LOGI(TAG, "Waiting for silence...");
                        lastReceived = currentTime;
                        continue;
                    }

                    waitingForSilence = false;

                    for (int i = 0; i < length; i++)
                    {
                        if (!dlms_parser_process_byte(&parser, data[i]))
                        {
                            ESP_LOGW(TAG, "Parser error at byte %d", i);
                            dlms_parser_init(&parser);
                        }
                    }
                }
                break;
            case UART_FIFO_OVF:
                ESP_LOGW(TAG, "UART FIFO Overflow");
                uart_flush_input(UART_NUM);
                xQueueReset(uart_queue);
                break;
            case UART_BUFFER_FULL:
                ESP_LOGW(TAG, "UART Ring Buffer Full");
                uart_flush_input(UART_NUM);
                xQueueReset(uart_queue);
                break;
            case UART_BREAK:
                ESP_LOGW(TAG, "UART Break");
                break;
            case UART_PARITY_ERR:
                ESP_LOGW(TAG, "UART Parity Error");
                break;
            case UART_FRAME_ERR:
                ESP_LOGW(TAG, "UART Frame Error");
                break;
            default:
                break;
            }
        }
    }
    free(data);
}

// Initialize UART
void uart_init(void)
{
    uart_config_t uart_config = {
        .baud_rate = UART_BAUD_RATE,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_XTAL,
    };

    // Install UART driver and set pins
    uart_driver_install(UART_NUM, BUF_SIZE * 2, BUF_SIZE * 2, 20, &uart_queue, 0);
    uart_param_config(UART_NUM, &uart_config);
    uart_set_pin(UART_NUM, UART_TX_PIN, UART_RX_PIN, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);

    ESP_LOGI(TAG, "UART initialized successfully");
}

#if DATA_SIMULATION
// Timer callback function to send DLMS data
void dlms_data_timer_callback(TimerHandle_t xTimer)
{
    // ESP_LOGI(TAG, "Sending DLMS test data...");
    uart_write_bytes(UART_NUM, (const char *)dlmsFrame, dlmsFrameSize);
    // uart_write_bytes(UART_NUM, (const char *)kamstrup_test_data, kamstrup_test_data_size);
}
#endif

static void bdb_start_top_level_commissioning_cb(uint8_t mode_mask)
{
    ESP_RETURN_ON_FALSE(esp_zb_bdb_start_top_level_commissioning(mode_mask) == ESP_OK, , TAG, "Failed to start Zigbee commissioning");
}
void esp_zb_app_signal_handler(esp_zb_app_signal_t *signal_struct)
{
    uint32_t *p_sg_p = signal_struct->p_app_signal;
    esp_err_t err_status = signal_struct->esp_err_status;
    esp_zb_app_signal_type_t sig_type = *p_sg_p;
    switch (sig_type)
    {
    case ESP_ZB_ZDO_SIGNAL_SKIP_STARTUP:
        ESP_LOGI(TAG, "Initialize Zigbee stack");
        esp_zb_bdb_start_top_level_commissioning(ESP_ZB_BDB_MODE_INITIALIZATION);
        break;
    case ESP_ZB_BDB_SIGNAL_DEVICE_FIRST_START:
    case ESP_ZB_BDB_SIGNAL_DEVICE_REBOOT:
        if (err_status == ESP_OK)
        {

            // ESP_LOGI(TAG, "Deferred driver initialization %s", deferred_driver_init() ? "failed" : "successful");

            ESP_LOGI(TAG, "Device started up in %s factory-reset mode", esp_zb_bdb_is_factory_new() ? "" : "non");
            if (esp_zb_bdb_is_factory_new())
            {
                ESP_LOGI(TAG, "Start network steering");
                esp_zb_bdb_start_top_level_commissioning(ESP_ZB_BDB_MODE_NETWORK_STEERING);

                xTaskCreate(initLedFlash, "initLedFlash", 2048, NULL, 12, &task_handle);
            }
            else
            {
                ESP_LOGI(TAG, "Device rebooted");

                xTaskCreate(uart_event_task, "uart_event_task", 2048, NULL, 12, NULL);
            }
        }
        else
        {
            ESP_LOGW(TAG, "Failed to initialize Zigbee stack (status: %s)", esp_err_to_name(err_status));
        }
        break;

    case ESP_ZB_ZDO_SIGNAL_DEVICE_ANNCE:
        ESP_LOGI(TAG, "Device announce received");
        break;

    case ESP_ZB_BDB_SIGNAL_STEERING:
        if (err_status == ESP_OK)
        {
            esp_zb_ieee_addr_t extended_pan_id;
            esp_zb_get_extended_pan_id(extended_pan_id);
            ESP_LOGI(TAG, "Joined network successfully (Extended PAN ID: %02x:%02x:%02x:%02x:%02x:%02x:%02x:%02x, PAN ID: 0x%04hx, Channel:%d, Short Address: 0x%04hx)",
                     extended_pan_id[7], extended_pan_id[6], extended_pan_id[5], extended_pan_id[4],
                     extended_pan_id[3], extended_pan_id[2], extended_pan_id[1], extended_pan_id[0],
                     esp_zb_get_pan_id(), esp_zb_get_current_channel(), esp_zb_get_short_address());

            vTaskDelete(task_handle);
            task_handle = NULL;
            xTaskCreate(uart_event_task, "uart_event_task", 2048, NULL, 12, NULL);
        }
        else
        {
            ESP_LOGI(TAG, "Network steering was not successful (status: %s)", esp_err_to_name(err_status));
            esp_zb_scheduler_alarm((esp_zb_callback_t)bdb_start_top_level_commissioning_cb, ESP_ZB_BDB_MODE_NETWORK_STEERING, 1000);
        }
        break;

    case ESP_ZB_NWK_SIGNAL_PERMIT_JOIN_STATUS:
        if (err_status == ESP_OK)
        {
            if (*(uint8_t *)esp_zb_app_signal_get_params(p_sg_p))
            {
                ESP_LOGI(TAG, "Network(0x%04hx) is open for %d seconds", esp_zb_get_pan_id(), *(uint8_t *)esp_zb_app_signal_get_params(p_sg_p));
            }
            else
            {
                ESP_LOGW(TAG, "Network(0x%04hx) closed, devices joining not allowed.", esp_zb_get_pan_id());
            }
        }
        break;

    case ESP_ZB_COMMON_SIGNAL_CAN_SLEEP:
        esp_zb_sleep_now();
        break;

    default:
        ESP_LOGI(TAG, "ZDO signal: %s (0x%x), status: %s", esp_zb_zdo_signal_to_string(sig_type), sig_type, esp_err_to_name(err_status));
        break;
    }
}

static void esp_zb_task(void *pvParameters)
{

    /* initialize Zigbee stack */
    esp_zb_cfg_t zb_nwk_cfg = ESP_ZB_ZED_CONFIG();
    esp_zb_sleep_enable(true);
    esp_zb_init(&zb_nwk_cfg);
    esp_zb_configuration_tool_cfg_t sensor_cfg = ESP_ZB_DEFAULT_CONFIGURATION_TOOL_CONFIG();
    esp_zb_ep_list_t *esp_zb_sensor_ep = esp_zb_ep_list_create();

    esp_zb_endpoint_config_t endpoint_config = {
        .endpoint = ENDPOINT_ID,
        .app_profile_id = ESP_ZB_AF_HA_PROFILE_ID,
        .app_device_id = ESP_ZB_HA_COMBINED_INTERFACE_DEVICE_ID,
        .app_device_version = 0};

    sensor_cfg.basic_cfg.power_source = 0x04; // DC source

    esp_zb_cluster_list_t *cluster_list = esp_zb_zcl_cluster_list_create();

    // Basic cluster
    esp_zb_attribute_list_t *basic_cluster = esp_zb_basic_cluster_create(&(sensor_cfg.basic_cfg));
    ESP_ERROR_CHECK(esp_zb_basic_cluster_add_attr(basic_cluster, ESP_ZB_ZCL_ATTR_BASIC_MANUFACTURER_NAME_ID, MANUFACTURER_NAME));
    ESP_ERROR_CHECK(esp_zb_basic_cluster_add_attr(basic_cluster, ESP_ZB_ZCL_ATTR_BASIC_MODEL_IDENTIFIER_ID, MODEL_IDENTIFIER));
    ESP_ERROR_CHECK(esp_zb_basic_cluster_add_attr(basic_cluster, ESP_ZB_ZCL_ATTR_BASIC_DATE_CODE_ID, MODEL_IDENTIFIER));
    
    // Power source attribute (DC)
    //uint8_t power_source = ESP_ZB_ZCL_BASIC_POWER_SOURCE_DC_SOURCE;
    //esp_zb_cluster_add_attr(basic_cluster, ESP_ZB_ZCL_CLUSTER_ID_BASIC, ESP_ZB_ZCL_ATTR_BASIC_POWER_SOURCE_ID, ESP_ZB_ZCL_ATTR_TYPE_8BIT_ENUM, ESP_ZB_ZCL_ATTR_ACCESS_READ_ONLY, &power_source);

    // Versioning attributes
    uint8_t app_version = 1; // Major version only

    // SW Build ID attribute (string) - limited to 15 chars payload (length byte + 15)
    const esp_app_desc_t *app_desc = esp_app_get_description();
    char sw_build_id[16] = {0};
    size_t ver_len = strnlen(app_desc->version, 15); // payload cap
    sw_build_id[0] = (char)ver_len;
    memcpy(&sw_build_id[1], app_desc->version, ver_len);

    esp_zb_cluster_add_attr(
        basic_cluster,
        ESP_ZB_ZCL_CLUSTER_ID_BASIC,
        ESP_ZB_ZCL_ATTR_BASIC_APPLICATION_VERSION_ID,
        ESP_ZB_ZCL_ATTR_TYPE_U8,
        ESP_ZB_ZCL_ATTR_ACCESS_READ_ONLY | ESP_ZB_ZCL_ATTR_ACCESS_REPORTING ,
        &app_version);

    esp_zb_cluster_add_attr(
        basic_cluster,
        ESP_ZB_ZCL_CLUSTER_ID_BASIC,
        ESP_ZB_ZCL_ATTR_BASIC_SW_BUILD_ID,
        ESP_ZB_ZCL_ATTR_TYPE_CHAR_STRING,
        ESP_ZB_ZCL_ATTR_ACCESS_READ_ONLY | ESP_ZB_ZCL_ATTR_ACCESS_REPORTING,
        sw_build_id);

    ESP_ERROR_CHECK(esp_zb_cluster_list_add_basic_cluster(cluster_list, basic_cluster, ESP_ZB_ZCL_CLUSTER_SERVER_ROLE));

    // Identity attributes
    ESP_ERROR_CHECK(esp_zb_cluster_list_add_identify_cluster(cluster_list, esp_zb_identify_cluster_create(&(sensor_cfg.identify_cfg)), ESP_ZB_ZCL_CLUSTER_SERVER_ROLE));
    ESP_ERROR_CHECK(esp_zb_cluster_list_add_identify_cluster(cluster_list, esp_zb_zcl_attr_list_create(ESP_ZB_ZCL_CLUSTER_ID_IDENTIFY), ESP_ZB_ZCL_CLUSTER_CLIENT_ROLE));

    uint8_t undefined_value_uint8 = (uint8_t)0x80;
    uint16_t undefined_value_uint16 = (uint16_t)0xFFFF;
    int16_t undefined_value_int16 = (int16_t)0x8000;
    // uint32_t undefined_value_uint32 = (uint32_t)(0xFFFFFFFF);
    uint64_t undefined_value_uint64 = (uint64_t)(0xFFFFFFFFFFFFFFFF);

    // ESP_ZB_ZCL_CLUSTER_ID_METERING
    esp_zb_attribute_list_t *metering_attr_list = esp_zb_zcl_attr_list_create(ESP_ZB_ZCL_CLUSTER_ID_ELECTRICAL_MEASUREMENT);
    // MeasurementType
    uint16_t e_meas_type = (uint16_t)0x0038;
    esp_zb_electrical_meas_cluster_add_attr(metering_attr_list, ESP_ZB_ZCL_ATTR_ELECTRICAL_MEASUREMENT_MEASUREMENT_TYPE_ID, &e_meas_type);

    esp_zb_electrical_meas_cluster_add_attr(metering_attr_list, ESP_ZB_ZCL_ATTR_ELECTRICAL_MEASUREMENT_RMSCURRENT_ID, &undefined_value_uint16);
    esp_zb_electrical_meas_cluster_add_attr(metering_attr_list, ESP_ZB_ZCL_ATTR_ELECTRICAL_MEASUREMENT_ACTIVE_POWER_ID, &undefined_value_int16); /*!< Represents the single phase or Phase A, current demand of active power delivered or received at the premises, in @e Watts (W). */
    esp_zb_electrical_meas_cluster_add_attr(metering_attr_list, ESP_ZB_ZCL_ATTR_ELECTRICAL_MEASUREMENT_REACTIVE_POWER_ID, &undefined_value_int16);
    esp_zb_electrical_meas_cluster_add_attr(metering_attr_list, ESP_ZB_ZCL_ATTR_ELECTRICAL_MEASUREMENT_RMSVOLTAGE_ID, &undefined_value_uint16); /*!< Represents the single phase or Phase A, current demand of active power delivered or received at the premises, in @e Watts (W). */
    esp_zb_electrical_meas_cluster_add_attr(metering_attr_list, ESP_ZB_ZCL_ATTR_ELECTRICAL_MEASUREMENT_POWER_FACTOR_ID, &undefined_value_uint8);

    esp_zb_electrical_meas_cluster_add_attr(metering_attr_list, ESP_ZB_ZCL_ATTR_ELECTRICAL_MEASUREMENT_RMSCURRENT_PHB_ID, &undefined_value_uint16);
    esp_zb_electrical_meas_cluster_add_attr(metering_attr_list, ESP_ZB_ZCL_ATTR_ELECTRICAL_MEASUREMENT_ACTIVE_POWER_PHB_ID, &undefined_value_int16); /*!< Represents the single phase or Phase A, current demand of active power delivered or received at the premises, in @e Watts (W). */
    esp_zb_electrical_meas_cluster_add_attr(metering_attr_list, ESP_ZB_ZCL_ATTR_ELECTRICAL_MEASUREMENT_REACTIVE_POWER_PH_B_ID, &undefined_value_int16);
    esp_zb_electrical_meas_cluster_add_attr(metering_attr_list, ESP_ZB_ZCL_ATTR_ELECTRICAL_MEASUREMENT_RMSVOLTAGE_PHB_ID, &undefined_value_uint16); /*!< Represents the single phase or Phase A, current demand of active power delivered or received at the premises, in @e Watts (W). */
    esp_zb_electrical_meas_cluster_add_attr(metering_attr_list, ESP_ZB_ZCL_ATTR_ELECTRICAL_MEASUREMENT_POWER_FACTOR_PH_B_ID, &undefined_value_uint8);

    esp_zb_electrical_meas_cluster_add_attr(metering_attr_list, ESP_ZB_ZCL_ATTR_ELECTRICAL_MEASUREMENT_RMSCURRENT_PHC_ID, &undefined_value_uint16);
    esp_zb_electrical_meas_cluster_add_attr(metering_attr_list, ESP_ZB_ZCL_ATTR_ELECTRICAL_MEASUREMENT_ACTIVE_POWER_PHC_ID, &undefined_value_int16); /*!< Represents the single phase or Phase A, current demand of active power delivered or received at the premises, in @e Watts (W). */
    esp_zb_electrical_meas_cluster_add_attr(metering_attr_list, ESP_ZB_ZCL_ATTR_ELECTRICAL_MEASUREMENT_REACTIVE_POWER_PH_C_ID, &undefined_value_int16);
    esp_zb_electrical_meas_cluster_add_attr(metering_attr_list, ESP_ZB_ZCL_ATTR_ELECTRICAL_MEASUREMENT_RMSVOLTAGE_PHC_ID, &undefined_value_uint16); /*!< Represents the single phase or Phase A, current demand of active power delivered or received at the premises, in @e Watts (W). */
    esp_zb_electrical_meas_cluster_add_attr(metering_attr_list, ESP_ZB_ZCL_ATTR_ELECTRICAL_MEASUREMENT_POWER_FACTOR_PH_C_ID, &undefined_value_uint8);

    uint16_t divisor = (uint16_t)(100);
    esp_zb_electrical_meas_cluster_add_attr(metering_attr_list, ESP_ZB_ZCL_ATTR_ELECTRICAL_MEASUREMENT_ACCURRENT_DIVISOR_ID, &divisor);

    ESP_ERROR_CHECK(esp_zb_cluster_list_add_electrical_meas_cluster(cluster_list, metering_attr_list, ESP_ZB_ZCL_CLUSTER_SERVER_ROLE));

    // Set the consumed energy counter
    esp_zb_attribute_list_t *meteringCluster = esp_zb_zcl_attr_list_create(ESP_ZB_ZCL_CLUSTER_ID_METERING);

    esp_zb_cluster_add_attr(meteringCluster, ESP_ZB_ZCL_CLUSTER_ID_METERING, ESP_ZB_ZCL_ATTR_METERING_CURRENT_SUMMATION_DELIVERED_ID, ESP_ZB_ZCL_ATTR_TYPE_U48, ESP_ZB_ZCL_ATTR_ACCESS_READ_ONLY | ESP_ZB_ZCL_ATTR_ACCESS_REPORTING, &undefined_value_uint64);
    esp_zb_cluster_add_attr(meteringCluster, ESP_ZB_ZCL_CLUSTER_ID_METERING, ESP_ZB_ZCL_ATTR_METERING_CURRENT_SUMMATION_RECEIVED_ID, ESP_ZB_ZCL_ATTR_TYPE_U48, ESP_ZB_ZCL_ATTR_ACCESS_READ_ONLY | ESP_ZB_ZCL_ATTR_ACCESS_REPORTING, &undefined_value_uint64);
    // esp_zb_cluster_add_attr(meteringCluster,ESP_ZB_ZCL_CLUSTER_ID_METERING,ESP_ZB_ZCL_ATTR_METERING_READING_SNAPSHOT_TIME_ID, ESP_ZB_ZCL_ATTR_TYPE_UTC_TIME, ESP_ZB_ZCL_ATTR_ACCESS_READ_ONLY | ESP_ZB_ZCL_ATTR_ACCESS_REPORTING, &undefined_value_uint32);

    // uint8_t status = (uint8_t)(1);
    // esp_zb_cluster_add_attr(meteringCluster, ESP_ZB_ZCL_CLUSTER_ID_METERING, ESP_ZB_ZCL_ATTR_METERING_STATUS_ID, ESP_ZB_ZCL_ATTR_TYPE_8BITMAP, ESP_ZB_ZCL_ATTR_ACCESS_READ_ONLY, &status);

    uint8_t invalid_serial[] = {0}; // Length = 0, no data bytes
    esp_zb_cluster_add_attr(meteringCluster, ESP_ZB_ZCL_CLUSTER_ID_METERING, ESP_ZB_ZCL_ATTR_METERING_METER_SERIAL_NUMBER_ID, ESP_ZB_ZCL_ATTR_TYPE_OCTET_STRING, ESP_ZB_ZCL_ATTR_ACCESS_READ_ONLY, &invalid_serial);

    uint8_t electricMeteringType = (uint8_t)(0);
    esp_zb_cluster_add_attr(meteringCluster, ESP_ZB_ZCL_CLUSTER_ID_METERING, ESP_ZB_ZCL_ATTR_METERING_METERING_DEVICE_TYPE_ID, ESP_ZB_ZCL_ATTR_TYPE_8BITMAP, ESP_ZB_ZCL_ATTR_ACCESS_READ_ONLY, &electricMeteringType);

    uint8_t unit = (uint8_t)(0);
    esp_zb_cluster_add_attr(meteringCluster, ESP_ZB_ZCL_CLUSTER_ID_METERING, ESP_ZB_ZCL_ATTR_METERING_UNIT_OF_MEASURE_ID, ESP_ZB_ZCL_ATTR_TYPE_8BIT_ENUM, ESP_ZB_ZCL_ATTR_ACCESS_READ_ONLY, &unit);

    esp_zb_cluster_add_attr(meteringCluster, ESP_ZB_ZCL_CLUSTER_ID_METERING, ESP_ZB_ZCL_ATTR_METERING_DIVISOR_ID, ESP_ZB_ZCL_ATTR_TYPE_U16, ESP_ZB_ZCL_ATTR_ACCESS_READ_ONLY, &divisor);

    ESP_ERROR_CHECK(esp_zb_cluster_list_add_metering_cluster(cluster_list, meteringCluster, ESP_ZB_ZCL_CLUSTER_SERVER_ROLE));

    //---------------------------------------------------------------------------------------------------------------------------------------
    esp_zb_ep_list_add_ep(esp_zb_sensor_ep, cluster_list, endpoint_config);
    esp_zb_device_register(esp_zb_sensor_ep);

    esp_zb_set_primary_network_channel_set(ESP_ZB_PRIMARY_CHANNEL_MASK);
    ESP_ERROR_CHECK(esp_zb_start(false));

    esp_zb_stack_main_loop();
}

static void button_long_press_cb(void *arg, void *usr_data)
{
    ESP_LOGI(TAG, "LLong press detected - factory reset");
    esp_zb_factory_reset();
}

static void button_press_cb(void *arg, void *usr_data)
{
    ESP_LOGI(TAG, "Press detected");

    for (int i = 0; i < 10; i++)
    {
        gpio_set_level(LED_PIN, 1);
        vTaskDelay(100 / portTICK_PERIOD_MS);
        gpio_set_level(LED_PIN, 0);
        vTaskDelay(100 / portTICK_PERIOD_MS);
    }

    esp_restart();
}

#if DATA_SIMULATION
void simulateData()
{
    // Create a periodic timer for sending DLMS data
    TimerHandle_t dlms_timer = xTimerCreate(
        "DLMSTimer",             // Timer name
        pdMS_TO_TICKS(10000),    // 10 seconds
        pdTRUE,                  // Auto-reload timer
        (void *)0,               // Timer ID
        dlms_data_timer_callback // Callback function
    );

    if (xTimerStart(dlms_timer, 0) != pdPASS)
    {
        ESP_LOGE(TAG, "Failed to start timer");
    }
    else
    {
        ESP_LOGI(TAG, "DLMS simulation started - sending data every 10 seconds");
    }
}
#endif

static esp_err_t esp_zb_power_save_init(void)
{
    esp_err_t rc = ESP_OK;
#ifdef CONFIG_PM_ENABLE
    int cur_cpu_freq_mhz = CONFIG_ESP_DEFAULT_CPU_FREQ_MHZ;
    esp_pm_config_t pm_config = {
        .max_freq_mhz = cur_cpu_freq_mhz,
        .min_freq_mhz = cur_cpu_freq_mhz,
#if CONFIG_FREERTOS_USE_TICKLESS_IDLE
        .light_sleep_enable = true
#endif
    };
    rc = esp_pm_configure(&pm_config);
#endif
    return rc;
}

void app_main(void)
{
    const esp_app_desc_t *app_desc = esp_app_get_description();
    
    ESP_LOGI(TAG, "========================================");
    ESP_LOGI(TAG, "PowerMeter Application Started");
    ESP_LOGI(TAG, "Version: %s", app_desc->version);
    ESP_LOGI(TAG, "Build Date: %s", app_desc->date);
    ESP_LOGI(TAG, "Build Time: %s", app_desc->time);
    ESP_LOGI(TAG, "Project Name: %s", app_desc->project_name);
    ESP_LOGI(TAG, "========================================");

    esp_log_level_set("Parser", ESP_LOG_NONE);

    gpio_reset_pin(LED_PIN);
    gpio_set_direction(LED_PIN, GPIO_MODE_OUTPUT);
    gpio_reset_pin(LED_PIN2);
    gpio_set_direction(LED_PIN2, GPIO_MODE_OUTPUT);
    gpio_set_level(LED_PIN, 0);
    gpio_set_level(LED_PIN2, 0);

    button_config_t gpio_btn_cfg = {
        .type = BUTTON_TYPE_GPIO,
        .long_press_time = 4000,
        .short_press_time = CONFIG_BUTTON_SHORT_PRESS_TIME_MS,
        .gpio_button_config = {
            .gpio_num = 4,
            .active_level = 1,
        },
    };
    button_handle_t gpio_btn = iot_button_create(&gpio_btn_cfg);
    if (NULL == gpio_btn)
    {
        ESP_LOGE(TAG, "Button create failed");
    }

    iot_button_register_cb(gpio_btn, BUTTON_LONG_PRESS_START, button_long_press_cb, NULL);
    iot_button_register_cb(gpio_btn, BUTTON_DOUBLE_CLICK, button_press_cb, NULL);

    // //-------------ADC1 Init---------------//
    // adc_oneshot_unit_handle_t adc1_handle;
    // adc_oneshot_unit_init_cfg_t init_config1 = {
    //     .unit_id = ADC_UNIT_1,
    // };
    // ESP_ERROR_CHECK(adc_oneshot_new_unit(&init_config1, &adc1_handle));

    // //-------------ADC1 Config---------------//
    // adc_oneshot_chan_cfg_t adcConfig = {
    //     .atten = ADC_ATTEN_DB_12,
    //     .bitwidth = ADC_BITWIDTH_DEFAULT,
    // };

    // ESP_ERROR_CHECK(adc_oneshot_config_channel(adc1_handle, ADC_CHANNEL_3, &adcConfig));

    // adc_cali_handle_t handle = NULL;
    // ESP_LOGI(TAG, "calibration scheme version is %s", "Curve Fitting");
    // adc_cali_curve_fitting_config_t cali_config = {
    //     .unit_id = ADC_UNIT_1,
    //     .atten = ADC_ATTEN_DB_12,
    //     .bitwidth = ADC_BITWIDTH_DEFAULT,
    // };
    // ESP_ERROR_CHECK(adc_cali_create_scheme_curve_fitting(&cali_config, &handle));

    // bool power_on = false;

    // while (!power_on)
    // {
    //     ESP_ERROR_CHECK(adc_oneshot_read(adc1_handle, ADC_CHANNEL_3, &adc_raw[0][0]));
    //     ESP_LOGI(TAG, "ADC%d Channel[%d] Raw Data: %d", ADC_UNIT_1 + 1, ADC_CHANNEL_3, adc_raw[0][0]);

    //     ESP_ERROR_CHECK(adc_cali_raw_to_voltage(handle, adc_raw[0][0], &voltage[0][0]));
    //     ESP_LOGI(TAG, "ADC%d Channel[%d] Cali Voltage: %d mV", ADC_UNIT_1 + 1, ADC_CHANNEL_3, voltage[0][0]);

    //     if (voltage[0][0] > 1600)
    //     {
    //         power_on = true;

    //         gpio_set_level(LED_PIN, 1);
    //         vTaskDelay(1000 / portTICK_PERIOD_MS);
    //         gpio_set_level(LED_PIN, 0);

    //         ESP_LOGI(TAG, "Power detected - starting application");
    //         vTaskDelay(pdMS_TO_TICKS(10000));
    //         ESP_LOGI(TAG, "Power detected - started");

    //         gpio_set_level(LED_PIN2, 1);
    //         vTaskDelay(2000 / portTICK_PERIOD_MS);
    //         gpio_set_level(LED_PIN2, 0);
    //     }
    //     else
    //     {
    //         ESP_LOGI(TAG, "No power detected - waiting...");
    //         vTaskDelay(pdMS_TO_TICKS(1000));
    //     }
    // }

    // adc_digi_stop(void);

    // Initialize UART
    uart_init();

    // Initialize DLMS parser
    dlms_parser_init(&parser);
    dlms_parser_set_callback(&parser, handle_dlms_field);

#if DATA_SIMULATION
    simulateData();
#endif

    esp_zb_platform_config_t config = {
        .radio_config = ESP_ZB_DEFAULT_RADIO_CONFIG(),
        .host_config = ESP_ZB_DEFAULT_HOST_CONFIG(),
    };
    ESP_ERROR_CHECK(nvs_flash_init());
    ESP_ERROR_CHECK(esp_zb_power_save_init());
    ESP_ERROR_CHECK(esp_zb_platform_config(&config));

    gpio_set_level(LED_PIN, 1);
    vTaskDelay(1000 / portTICK_PERIOD_MS);
    gpio_set_level(LED_PIN, 0);

    ESP_LOGI(TAG, "Power detected - starting application");
    vTaskDelay(pdMS_TO_TICKS(10000));
    ESP_LOGI(TAG, "Power detected - started");

    gpio_set_level(LED_PIN2, 1);
    vTaskDelay(2000 / portTICK_PERIOD_MS);
    gpio_set_level(LED_PIN2, 0);

    xTaskCreate(esp_zb_task, "Zigbee_main", 4096, NULL, 5, NULL);
}
