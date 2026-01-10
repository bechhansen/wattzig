#include "esp_zigbee_core.h"

/* Zigbee configuration */
#define INSTALLCODE_POLICY_ENABLE false /* enable the install code policy for security */
#define ENDPOINT_ID 10
#define ESP_ZB_PRIMARY_CHANNEL_MASK ESP_ZB_TRANSCEIVER_ALL_CHANNELS_MASK /* Zigbee primary channel mask use in the example */

#define ED_AGING_TIMEOUT ESP_ZB_ED_AGING_TIMEOUT_64MIN
#define ED_KEEP_ALIVE 3000 /* 3000 millisecond */

#define MANUFACTURER_NAME "\x10" \
                          "Peer Bech Hansen"
#define MODEL_IDENTIFIER "\x07" \
                         "WattZig"

// UART configuration
#define UART_NUM UART_NUM_1 // Use UART1

#define UART_TX_PIN 0 // TX pin (adjust as needed)
#define UART_RX_PIN 1 // RX pin (adjust as needed)

#define UART_BAUD_RATE 2400 // Baud rate
#define BUF_SIZE 1024       // Buffer size
#define UART_RX_BUFFER_SIZE 1024
#define UART_QUEUE_SIZE 10

#define LED_PIN 5
#define LED_PIN2 6

#define DATA_SIMULATION false

#define ESP_ZB_ZED_CONFIG()                               \
    {                                                     \
        .esp_zb_role = ESP_ZB_DEVICE_TYPE_ED,             \
        .install_code_policy = INSTALLCODE_POLICY_ENABLE, \
        .nwk_cfg.zed_cfg = {                              \
            .ed_timeout = ED_AGING_TIMEOUT,               \
            .keep_alive = ED_KEEP_ALIVE,                  \
        },                                                \
    }

#define ESP_ZB_DEFAULT_RADIO_CONFIG()       \
    {                                       \
        .radio_mode = ZB_RADIO_MODE_NATIVE, \
    }

#define ESP_ZB_DEFAULT_HOST_CONFIG()                          \
    {                                                         \
        .host_connection_mode = ZB_HOST_CONNECTION_MODE_NONE, \
    }
