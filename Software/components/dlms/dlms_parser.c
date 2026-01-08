#include "include/dlms_parser.h"
#include <string.h>
#include "esp_log.h"

// Frame markers and escape characters
#define DLMS_START_MARKER 0x7E
#define DLMS_END_MARKER 0x7E
#define DLMS_ESCAPE 0x7D
#define DLMS_ESCAPE_MASK 0x20

// DLMS/COSEM data type tags
#define DLMS_TAG_OCTET_STRING         0x09  // length-prefixed raw bytes
#define DLMS_TAG_VISIBLE_STRING       0x0A  // length-prefixed ASCII
#define DLMS_TAG_DOUBLE_LONG_UNSIGNED 0x06  // 4-byte unsigned integer
#define DLMS_TAG_UNSIGNED             0x12  // 2-byte unsigned integer
#define DLMS_TAG_DATE_TIME            0x0C  // 12-byte date-time structure

// Common sizes and lengths
#define DLMS_SIZE_U32         4
#define DLMS_SIZE_U16         2
#define DLMS_SIZE_DATE_TIME   12
#define DLMS_OBIS_ADDR_LEN    6

static const char *TAG = "Parser";

static dlms_data_t data;

// Mapping of OBIS codes to field type and data length
typedef struct {
    uint8_t obis[DLMS_OBIS_ADDR_LEN];
    int type;              // dlms_field_t.type enum; use int to avoid header dependency
    uint8_t length;        // number of data bytes
    const char *label;     // optional log label
} dlms_obis_map_t;

static const dlms_obis_map_t kObisMap[] = {
    {{0x01,0x01,0x20,0x07,0x00,0xFF}, RMS_VOLTAGE_A,       DLMS_SIZE_U16, "RMS Voltage A"},
    {{0x01,0x01,0x34,0x07,0x00,0xFF}, RMS_VOLTAGE_B,       DLMS_SIZE_U16, "RMS Voltage B"},
    {{0x01,0x01,0x48,0x07,0x00,0xFF}, RMS_VOLTAGE_C,       DLMS_SIZE_U16, "RMS Voltage C"},
    {{0x01,0x01,0x21,0x07,0x00,0xFF}, POWER_FACTOR_A,      DLMS_SIZE_U16, "Power Factor A"},
    {{0x01,0x01,0x35,0x07,0x00,0xFF}, POWER_FACTOR_B,      DLMS_SIZE_U16, "Power Factor B"},
    {{0x01,0x01,0x49,0x07,0x00,0xFF}, POWER_FACTOR_C,      DLMS_SIZE_U16, "Power Factor C"},
    {{0x01,0x01,0x1F,0x07,0x00,0xFF}, RMS_CURRENT_A,       DLMS_SIZE_U32, "RMS Current A"},
    {{0x01,0x01,0x33,0x07,0x00,0xFF}, RMS_CURRENT_B,       DLMS_SIZE_U32, "RMS Current B"},
    {{0x01,0x01,0x47,0x07,0x00,0xFF}, RMS_CURRENT_C,       DLMS_SIZE_U32, "RMS Current C"},
    {{0x01,0x01,0x15,0x07,0x00,0xFF}, ACTIVE_POWER_A,      DLMS_SIZE_U32, "Active Power A"},
    {{0x01,0x01,0x29,0x07,0x00,0xFF}, ACTIVE_POWER_B,      DLMS_SIZE_U32, "Active Power B"},
    {{0x01,0x01,0x3D,0x07,0x00,0xFF}, ACTIVE_POWER_C,      DLMS_SIZE_U32, "Active Power C"},
    {{0x01,0x01,0x16,0x07,0x00,0xFF}, REACTIVE_POWER_A,    DLMS_SIZE_U32, "Reactive Power A"},
    {{0x01,0x01,0x2A,0x07,0x00,0xFF}, REACTIVE_POWER_B,    DLMS_SIZE_U32, "Reactive Power B"},
    {{0x01,0x01,0x3E,0x07,0x00,0xFF}, REACTIVE_POWER_C,    DLMS_SIZE_U32, "Reactive Power C"},
    {{0x01,0x01,0x01,0x08,0x00,0xFF}, ACTIVE_ENERGY_IMPORT, DLMS_SIZE_U32, "Active Energy Import"},
    {{0x01,0x01,0x02,0x08,0x00,0xFF}, ACTIVE_ENERGY_EXPORT, DLMS_SIZE_U32, "Active Energy Export"},
    {{0x01,0x01,0x00,0x00,0x01,0xFF}, SERIAL_NUMBER,       DLMS_SIZE_U32, "Serial Identifier"},
};

static inline bool obis_eq(const uint8_t *a, const uint8_t *b)
{
    return memcmp(a, b, DLMS_OBIS_ADDR_LEN) == 0;
}

void dlms_parser_init(dlms_parser_t *parser)
{
    memset(parser, 0, sizeof(dlms_parser_t));
    parser->state = DLMS_STATE_WAITING_START;
    parser->escape_next = false;

    memset(&data, 0, sizeof(dlms_data_t));
}

static void notify_callback(dlms_parser_t *parser, dlms_field_t *field)
{
    if (parser->callback != NULL)
    {
        parser->callback(field);
    }
}

void dlms_parser_set_callback(dlms_parser_t *parser, dlms_field_callback_t callback)
{
    parser->callback = callback;    
}


void process_start(dlms_parser_t *parser)
{

    dlms_field_t field;
    field.type = START;
    field.data = NULL;
    field.length = 0;
    notify_callback(parser, &field);
}

void process_end(dlms_parser_t *parser)
{
    dlms_field_t field;
    field.type = END;
    field.data = NULL;
    field.length = 0;
    notify_callback(parser, &field);
    
}

void process_payload(dlms_parser_t *parser, dlms_data_t *data)
{

    //ESP_LOGI(TAG, "Processing payload");
    
    dlms_field_t field;

    // Lookup OBIS in table
    for (size_t i = 0; i < sizeof(kObisMap) / sizeof(kObisMap[0]); ++i)
    {
        if (obis_eq(data->address, kObisMap[i].obis))
        {
            if (kObisMap[i].label)
            {
                ESP_LOGI(TAG, "Found %s", kObisMap[i].label);
            }
            field.type = kObisMap[i].type;
            field.data = data->data;
            field.length = kObisMap[i].length;
            notify_callback(parser, &field);
            break; // OBIS codes are unique; stop after first match
        }
    }
}

//TODO: Clean up this awful mess of a function
bool dlms_parser_process_byte(dlms_parser_t *parser, uint8_t byte)
{
    // // Handle escape sequences except in START and END states
    // if (parser->state != DLMS_STATE_WAITING_START &&
    //     parser->state != DLMS_STATE_END)
    // {

    //     if (parser->escape_next)
    //     {
    //         byte ^= DLMS_ESCAPE_MASK; // Un-escape the byte
    //         ESP_LOGE(TAG, "#################### Unescapted value %02X  ##########################", byte);
    //         parser->escape_next = false;
    //     }
    //     else if (byte == DLMS_ESCAPE)
    //     {
    //         ESP_LOGE(TAG, "#################### Escape character found ##########################");
    //         parser->escape_next = true;
    //         parser->frame_pos++;
    //         return true; // Wait for the next byte
    //     }
    // }

    switch (parser->state)
    {
    case DLMS_STATE_WAITING_START: // OK
        if (byte == DLMS_START_MARKER)
        {
            parser->frame_pos = 0;
            parser->state_pos = 0;
            parser->checksum = 0;
            parser->frame_length = 0;
            parser->escape_next = false;
            parser->state = DLMS_STATE_FRAME_FORMAT;
            ESP_LOGI(TAG, "Change state to: DLMS_STATE_FRAME_FORMAT");

            parser->frame_pos++;

            process_start(parser);
        }
        break;

    case DLMS_STATE_FRAME_FORMAT:

        if (parser->state_pos == 0)
        {
            parser->buffer[parser->state_pos++] = byte;
        }
        else
        {
            uint8_t byte0 = parser->buffer[0];
            uint8_t byte1 = byte;

            parser->frame_length = ((byte0 & 0x0F) << 8) | byte1;

            parser->state = DLMS_STATE_DESTINATION_ADDRESS;
            parser->state_pos = 0;
            //ESP_LOGI(TAG, "Change state to: DLMS_STATE_DESTINATION_ADDRESS - Frame length: %d", parser->frame_length);
        }

        parser->frame_pos++;

        break;

    case DLMS_STATE_DESTINATION_ADDRESS:

        // TODO: Evaluate length of destination address
        parser->state = DLMS_STATE_SOURCE_ADDRESS;
        // ESP_LOGI(TAG, "Change state to: DLMS_STATE_SOURCE_ADDRESS");
        parser->state_pos = 0;
        parser->frame_pos++;

        break;

    case DLMS_STATE_SOURCE_ADDRESS:
        // TODO: Evaluate length of source address
        parser->state = DLMS_STATE_CONTROL;
        // ESP_LOGI(TAG, "Change state to: DLMS_STATE_CONTROL");
        parser->state_pos = 0;
        parser->frame_pos++;
        break;

    case DLMS_STATE_CONTROL:
        parser->state = DLMS_STATE_HCS;
        // ESP_LOGI(TAG, "Change state to: DLMS_STATE_HCS");
        parser->state_pos = 0;
        parser->frame_pos++;
        break;

    case DLMS_STATE_HCS:
        if (parser->state_pos == 0)
        {
            parser->buffer[parser->state_pos++] = byte;
        }
        else
        {
            parser->state = DLMS_STATE_HEADER;
            parser->state_pos = 0;
            //    ESP_LOGI(TAG, "Change state to: DLMS_STATE_HEADER");
        }
        parser->frame_pos++;
        break;

    case DLMS_STATE_ARRAY:
        if (parser->state_pos == 4)
        { // Array length
            parser->state = DLMS_STATE_TIMESTAMP;
            parser->state_pos = 0;
            //  ESP_LOGI(TAG, "Change state to: DLMS_STATE_TIMESTAMP");
        }
        else
        {
            parser->state_pos++;
        }

        parser->frame_pos++;
        break;

    case DLMS_STATE_TIMESTAMP:

        if (parser->state_pos < DLMS_SIZE_DATE_TIME)
        {
            parser->buffer[parser->state_pos++] = byte;
        }
        else
        {
            parser->state = DLMS_STATE_UNKNOWN;
            parser->state_pos = 0;
            // ESP_LOGI(TAG, "Change state to: DLMS_STATE_UNKNOWN");
        }
        parser->frame_pos++;
        break;

    case DLMS_STATE_HEADER:

        if (parser->state_pos == 2)
        { // Header length
            parser->state = DLMS_STATE_ARRAY;
            parser->state_pos = 0;
            //  ESP_LOGI(TAG, "Change state to: DLMS_STATE_ARRAY");
        }
        else
        {
            parser->state_pos++;
        }

        parser->frame_pos++;
        break;

    case DLMS_STATE_UNKNOWN:
        if (parser->state_pos == 1)
        { // Array length
            parser->state = DLMS_STATE_DATA;
            parser->state_pos = 0;
            //    ESP_LOGI(TAG, "Change state to: DLMS_STATE_DATA");
        }
        else
        {
            parser->state_pos++;
        }

        parser->frame_pos++;
        break;

    case DLMS_STATE_DATA:

        //ESP_LOGI(TAG, "%02x", byte);

        parser->buffer[parser->state_pos++] = byte;

        if (parser->state_pos > 1)
        {
            uint8_t type = parser->buffer[0];
            uint8_t length = parser->buffer[1];

            /*if (byte == DLMS_END_MARKER)
            {
                ESP_LOGE(TAG, "XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX");
            }*/

            switch (type)
            {
            case DLMS_TAG_OCTET_STRING:

                if (parser->state_pos >= 2 + length)
                {

                    uint8_t dataType = parser->buffer[2 + length];

                    switch (dataType)
                    {
                    case DLMS_TAG_VISIBLE_STRING:

                        if (parser->state_pos >= 3 + length)
                        {
                            uint8_t dataLength = parser->buffer[3 + length];

                            if (parser->state_pos >= 4 + length + dataLength)
                            {
                                //ESP_LOGI(TAG, "String with address found with lenght %d", dataLength);
                                parser->state_pos = 0;
                            }
                        }

                        break;

                    case DLMS_TAG_OCTET_STRING:

                        if (parser->state_pos >= 3 + length)
                        {
                            uint8_t dataLength = parser->buffer[3 + length];

                            if (parser->state_pos >= 4 + length + dataLength)
                            {
                                //ESP_LOGI(TAG, "Octet String with address found with lenght %d", dataLength);

                                parser->state_pos = 0;
                            }
                        }

                        break;
                    case DLMS_TAG_DOUBLE_LONG_UNSIGNED:
                        if (parser->state_pos >= 3 + length + DLMS_SIZE_U32)
                        {
                            //ESP_LOGI(TAG, "Data with address found with lenght %d", 4);
                            //ESP_LOGI(TAG, "Current %02x%02x%02x%02x", parser->buffer[9], parser->buffer[10], parser->buffer[11], parser->buffer[12]);

                            dlms_data_t data;
                            memcpy(data.address, &parser->buffer[2], DLMS_OBIS_ADDR_LEN);
                            
                            data.data = &parser->buffer[2 + length + 1];
                            data.length = DLMS_SIZE_U32;

                            process_payload(parser, &data);
                            parser->state_pos = 0;
                        }

                        break;
                    case DLMS_TAG_UNSIGNED:

                        if (parser->state_pos >= 3 + length + DLMS_SIZE_U16)
                        {
                            //ESP_LOGI(TAG, "Data with address found with lenght %d", 2);
                            //ESP_LOGI(TAG, "Data %02x%02x", parser->buffer[9], parser->buffer[10]);

                            dlms_data_t data;
                            memcpy(data.address, &parser->buffer[2], DLMS_OBIS_ADDR_LEN);
                            
                            data.data = &parser->buffer[2 + length + 1];
                            data.length = DLMS_SIZE_U16;

                            process_payload(parser, &data);
                            parser->state_pos = 0;
                        }
                        break;
                    }
                }

                break;

            case DLMS_TAG_VISIBLE_STRING:

                if (parser->state_pos > 1 + length)
                {
                    ESP_LOGI(TAG, "String found with lenght %d", length);
                    parser->state_pos = 0;
                }

                break;

            default:
                break;
            }
        }

        parser->frame_pos++;

        if (parser->frame_pos >= parser->frame_length - 1)
        {
            parser->state = DLMS_STATE_CHECKSUM;
            parser->state_pos = 0;
            //ESP_LOGI(TAG, "Change state to: DLMS_STATE_CHECKSUM");
        }

        break;

    case DLMS_STATE_CHECKSUM:

        //ESP_LOGI(TAG, "CRC %02X", byte);

        if (parser->state_pos == 0)
        {
            parser->buffer[parser->state_pos++] = byte;
        }
        else
        {
            parser->state = DLMS_STATE_END;
            parser->state_pos = 0;
            ESP_LOGI(TAG, "Change state to: DLMS_STATE_END");
        }
        parser->frame_pos++;

        /*if (parser->checksum == byte) {
            parser->state = DLMS_STATE_END;
            //process_payload(parser);
        } else {
            parser->state = DLMS_STATE_WAITING_START;
            ESP_LOGI(TAG, "Change state to: DLMS_STATE_WAITING_START");
            return false;
        }*/
        break;

    case DLMS_STATE_END:
        parser->state = DLMS_STATE_WAITING_START;
        ESP_LOGI(TAG, "Change state to: DLMS_STATE_WAITING_START");

        process_end(parser);
        parser->frame_pos = 0;
        ESP_LOGI(TAG, "---------------------------------------------------------");
        

        
        if (byte != DLMS_END_MARKER)
        {
            ESP_LOGE(TAG, "store_byte failed - DLMS_STATE_WAITING_START");
            return false;
        }
        
        
        break;

        
    }

    return true;
}