#ifndef DLMS_PARSER_H
#define DLMS_PARSER_H

#include <stdint.h>
#include <stdbool.h>


// DLMS Parser States
typedef enum {
    DLMS_STATE_WAITING_START,
    DLMS_STATE_FRAME_FORMAT,
    DLMS_STATE_DESTINATION_ADDRESS,
    DLMS_STATE_SOURCE_ADDRESS,
    DLMS_STATE_CONTROL,
    DLMS_STATE_HCS,
    DLMS_STATE_ARRAY,
    DLMS_STATE_HEADER,
    DLMS_STATE_TIMESTAMP, 
    DLMS_STATE_UNKNOWN,   
    DLMS_STATE_DATA,        
    DLMS_STATE_CHECKSUM,
    DLMS_STATE_END
} dlms_parser_state_t;

// DLMS Field Types
typedef enum {
    START,
    END,
    RMS_VOLTAGE_A,
    RMS_VOLTAGE_B,
    RMS_VOLTAGE_C,
    RMS_CURRENT_A,
    RMS_CURRENT_B,
    RMS_CURRENT_C,
    ACTIVE_POWER_A,
    ACTIVE_POWER_B,
    ACTIVE_POWER_C,
    REACTIVE_POWER_A,
    REACTIVE_POWER_B,
    REACTIVE_POWER_C,
    POWER_FACTOR_A,
    POWER_FACTOR_B,
    POWER_FACTOR_C,
    ACTIVE_ENERGY_IMPORT,
    ACTIVE_ENERGY_EXPORT,
    DLMS_FIELD_TIMESTAMP,
    SERIAL_NUMBER
} dlms_field_type_t;

// Structure to hold parsed field data
typedef struct {
    dlms_field_type_t type;
    uint8_t *data;
    uint16_t length;
} dlms_field_t;

// Single callback function type
typedef void (*dlms_field_callback_t)(dlms_field_t *field);

// DLMS Parser context
typedef struct {
    dlms_parser_state_t state;
    uint8_t buffer[512];
    uint16_t frame_pos;
    uint16_t state_pos;
    uint16_t frame_length;
    uint16_t checksum;
    bool escape_next;           
    
    // Single callback
    dlms_field_callback_t callback;
    
} dlms_parser_t;

typedef struct {
    uint8_t address[6];
    uint8_t *data;
    uint16_t length;
} dlms_data_t;

// Initialize the DLMS parser
void dlms_parser_init(dlms_parser_t *parser);

// Process a single byte
bool dlms_parser_process_byte(dlms_parser_t *parser, uint8_t byte);

// Set the callback function
void dlms_parser_set_callback(dlms_parser_t *parser, dlms_field_callback_t callback);

#endif // DLMS_PARSER_H