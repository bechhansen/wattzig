# WattZig Copilot Instructions

## Project Overview
WattZig is an ESP32C6-based Zigbee End Device that reads 3-phase power meter data via DLMS protocol over UART and reports electrical measurements through Zigbee Home Automation (ZHA) standard clusters. The firmware is built on ESP-IDF v5.5.2 with FreeRTOS task management.

## Architecture & Data Flow

### Core Components
1. **Zigbee Stack** (`esp_zb_task`) - Handles device lifecycle, commissioning, and cluster management
   - ZHA-compliant endpoint with Electrical Measurement and Metering clusters
   - End Device (ZED) mode with power saving enabled
   - Network steering for commissioning new devices

2. **UART/DLMS Parser** (`uart_event_task`) - Receives meter data from intelligent meters
   - UART1: 2400 baud, 8 data bits, no parity, pins TX=0, RX=1
   - State machine parser extracts fields: voltage, current, power, energy counters
   - 3-second silence detection before parsing (prevents partial frame corruption)
   - Callback-based architecture: `handle_dlms_field()` processes parsed data

3. **Attribute Synchronization** - Updates Zigbee cluster attributes from DLMS fields
   - All 3 phases mapped: RMS voltage/current (A/B/C), active/reactive power, power factor
   - Energy counters (import/export) support reporting via ZCL reports
   - LED feedback: green LED during transmission, red LED on commissioning

### Key Files
- [Software/main/main.c](../../Software/main/main.c) - App entry point, task creation, initialization
- [Software/main/main.h](../../Software/main/main.h) - Configuration: pins, UART settings, Zigbee constants
- [Software/components/dlms](../../Software/components/dlms) - DLMS parser state machine

## Build & Deployment

### Build Command
```bash
cd wattzig
idf.py build -p /dev/ttyUSB0 -b 115200  # Configure via menuconfig first
idf.py flash monitor
```

### Critical Build Setup
- **Toolchain**: ESP-IDF v5.5.2 required (hardcoded in CI/GitHub Actions)
- **Target**: ESP32C6
- **Menuconfig Settings**:
  - Enable `CONFIG_PM_ENABLE` for power management
  - Set `ZB_ED_ROLE` (End Device role for Zigbee)
  - `CONFIG_BUTTON_SHORT_PRESS_TIME_MS` for button sensitivity
- **Exit Monitor**: `Ctrl+T` then `X`

### CI/CD
GitHub Actions workflow ([.github/workflows/esp32-build.yml](../../.github/workflows/esp32-build.yml)) triggers on changes to `wattzig/` subfolder. Build artifacts are versioned using git tags.

## Conventions & Patterns

### FreeRTOS Task Design
- **Priority 12 tasks**: UART event processing, LED flashing, Zigbee stack
- **Message Passing**: UART queue receives events (data, overflow, parity errors)
- **Lock Management**: `esp_zb_lock_acquire/release` protects Zigbee attribute writes
  ```c
  esp_zb_lock_acquire(portMAX_DELAY);
  esp_zb_zcl_set_attribute_val(ENDPOINT_ID, cluster_id, role, attr_id, value, false);
  esp_zb_lock_release();
  ```

### DLMS Parsing Pattern
1. `uart_event_task` reads bytes from UART queue
2. Each byte fed to `dlms_parser_process_byte()` state machine
3. Complete fields trigger `handle_dlms_field()` callback
4. Callback extracts data (big-endian byte arrays) and updates Zigbee attributes
5. Energy counters also trigger attribute reporting (for integration with Home Assistant)

### GPIO & Peripheral Configuration
- **LED_PIN=5, LED_PIN2=6**: Status indicators (LED_PIN2 = green, LED_PIN = red)
- **Button=GPIO4**: Long press (4s) triggers factory reset; double-click restarts device
- **Voltage Divider**: 10kΩ+10kΩ for VCC monitoring (ADC code present but commented)

## Debugging & Troubleshooting

### Log Configuration
- Tag "Parser" silenced by default: `esp_log_level_set("Parser", ESP_LOG_NONE)`
- Set `esp_log_level_set("*", ESP_LOG_DEBUG)` to diagnose communication issues
- All DLMS field receipts logged at `ESP_LOGI(TAG, ...)`

### Known Issues
- **Brownout resets**: Requires bulk capacitor (470µF–1000µF, 6.3V+) for stable power
- **GPIO8/GPIO9 during programming**: GPIO8 must be HIGH, GPIO9 must be LOW (hardware requirement)
- **Supercapacitor circuit**: Must be parallel to voltage regulator, not series (fixed in v9)
- **Power supply**: Regulator needs ≥4.5V input; MCP1700-3302E recommended

### Device Lifecycle
1. **First Start**: Factory reset → blinks LED → network steering
2. **Rejoined**: Loads stored network config → LED on briefly → UART task starts
3. **Data Flow**: UART parser → Zigbee lock → attribute update → optionally report

## Extension Points

### Adding New Meters
- Implement meter driver in `wattzig/common/` following `temp_sensor_driver` pattern
- Register callback to `uart_event_task` for meter-specific framing
- Map meter fields to ZCL attributes in `handle_dlms_field()`

### New Zigbee Attributes
- Create attribute lists in `esp_zb_task()` using `esp_zb_*_cluster_create()` helpers
- Register via `esp_zb_cluster_list_add_*_cluster()`
- Update callback to populate attribute via `esp_zb_zcl_set_attribute_val()`

### Testing
- Test data defined in [wattzig/main/kamstrup_test_data.h](../../wattzig/main/kamstrup_test_data.h)
- Simulator callback available: `dlms_data_timer_callback()` (disabled by default)
- Button press triggers restart for quick firmware reload

## Dependencies
- **espressif/esp-zigbee-lib**: ~1.6.8 (Zigbee stack)
- **espressif/esp-zboss-lib**: ~1.6.4 (ZBOSS radio)
- **espressif/button**: 2.4.1 (Button debouncing)
