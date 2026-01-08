# Firmware Upload Guide

This guide describes how to upload firmware to the PowerMeter ESP32C6 hardware using the service connector and standard flashing tools.

## Hardware Setup

### Service Connector Pinout

The PowerMeter hardware features a 6-pin service connector (2.54mm pitch) for programming and debugging:

| Pin | Signal | Purpose |
|-----|--------|---------|
| 1 | GND | Ground |
| 2 | VCC 3.3V | 3.3V Power Supply |
| 3 | TX | UART TX (GPIO1) |
| 4 | RX | UART RX (GPIO0) |
| 5 | GPIO8 | Boot Mode Control (High for Bootloader) |
| 6 | GPIO9 | Boot Mode Control (Low for Bootloader) |

### Required Hardware

1. **USB to UART Serial Adapter**: CP2102 STC or similar FTDI-based module
   - Must support 3.3V logic levels
   - Provides USB connectivity for flashing

2. **Test Rack PCB Clip**: 2.54mm pitch 6-pin clip connector
   - Connects to the service connector on the PowerMeter PCB
   - Allows quick connect/disconnect without soldering

3. **Cables**: Standard USB cable and connecting wires (typically included with serial adapter)

### Connection Diagram

Connect the USB to UART adapter to the service connector:

```
USB Adapter          Service Connector
───────────          ─────────────────
GND          ────→   GND (Pin 1)
5V/3.3V      ────→   VCC 3.3V (Pin 2)
TX (out)     ────→   RX (Pin 4)
RX (in)      ←────   TX (Pin 3)
GPIO/DTR/etc ────→   GPIO8 (Pin 5) - via pull-up or direct
GPIO/RTS/etc ────→   GPIO9 (Pin 6) - via pull-up or direct
```

**Important**: Ensure the USB adapter operates at 3.3V logic levels. Do not connect 5V directly to GPIO pins.

## Boot Mode Selection

To upload firmware, the ESP32C6 must be placed in bootloader mode.

### Boot Mode Requirements

For the ESP32C6 to enter bootloader mode:
- **GPIO8**: Must be pulled **HIGH** (typically connected to VCC 3.3V)
- **GPIO9**: Must be pulled **LOW** (typically connected to GND)

### Manual Boot Mode Entry

1. **Power off** the device
2. **Connect GPIO8 to VCC 3.3V** (high)
3. **Connect GPIO9 to GND** (low)
4. **Power on** the device
5. Device enters bootloader mode and is ready for flashing
6. After successful flashing, normal operation resumes automatically

###

## Flashing Firmware

### Prerequisites

1. Install esptool:
   ```bash
   pip install esptool
   ```

2. Confirm serial port availability:
   - **Linux/macOS**: `/dev/ttyUSB0` or `/dev/ttyUSB1` (or check with `ls /dev/tty*`)
   - **Windows**: `COM3`, `COM4`, etc. (check Device Manager)

### Standard Flashing Procedure

The standard way to flash firmware using esptool is:

#### Step 1: Download Firmware Binaries

Download the latest PowerMeter firmware release from GitHub:

1. Go to the [PowerMeter Releases](../../releases) page
2. Download the single release zip (e.g., `powermeter-v1.0.0-alpha.1.zip`) which contains:
   - `bootloader.bin` (bootloader)
   - `partition-table.bin` (partition table)
   - `firmware.bin` (application firmware)
3. Extract the zip to a local directory (e.g., `~/Downloads/powermeter-firmware/`)

#### Step 2: Put Device in Bootloader Mode

Manually connect GPIO8 (high) and GPIO9 (low) as described in [Boot Mode Selection](#boot-mode-selection).

#### Step 3: Flash Firmware

From the directory containing the downloaded .bin files:

```bash
esptool.py --chip esp32c6  --before default_reset --after hard_reset write_flash \
   --flash_size 4MB \
   0x0 bootloader.bin \
   0x8000 partition-table.bin \
   0x10000 firmware.bin

```

## Troubleshooting

### Device Not Found

- Confirm USB adapter is connected and recognized: `esptool.py version`
- Check port name: `esptool.py chip_id` (will list available ports)
- On Linux/macOS: May require `sudo` or udev rule configuration

### Connection Timeout During Flash

- Verify GPIO8 and GPIO9 are correctly set for bootloader mode
- Ensure 3.3V power is supplied during flashing
- Try lower baud rate: `-b 115200` instead of 460800

### CRC Error or Invalid Image

- Verify all three .bin files exist and are uncorrupted (check file sizes are reasonable, typically >100KB)
- Ensure you're in the correct directory containing bootloader.bin, partition-table.bin, and PowerMeter.bin
- Verify correct flash offsets: 0x0 (bootloader), 0x8000 (partition table), 0x20000 (application)
- Try erasing flash first (warning: clears all data):
  ```bash
  esptool.py -p /dev/ttyUSB0 erase_flash
  esptool.py -p /dev/ttyUSB0 -b 460800 --before default_reset --after hard_reset write_flash \
    --flash_mode dio --flash_freq 80m --flash_size keep \
    0x0 bootloader.bin 0x8000 partition-table.bin 0x20000 PowerMeter.bin
  ```

### Port Permission Denied (Linux)

Add user to dialout group:

```bash
sudo usermod -a -G dialout $USER
sudo reboot
```

Then unplug/replug USB adapter.

## Next Steps

After successful flashing:

1. Device boots automatically
2. LED blinks (commissioning mode)
3. Device joins the nearest Zigbee network
4. See [README.md](README.md) for normal operation details

## Building Firmware from Source

If you want to build firmware from source code instead of using pre-built binaries:

1. Clone the repository and navigate to `PowerMeter/Software`
2. Install [ESP-IDF v5.5.2](https://docs.espressif.com/projects/esp-idf/en/v5.5.2/esp32c6/get-started/index.html)
3. Run `idf.py menuconfig` to configure build options
4. Build with `idf.py build`
5. Flash using the steps above, replacing `bootloader.bin`, `partition-table.bin`, and `PowerMeter.bin` with paths in `build/bootloader/`, `build/partition_table/`, and `build/` respectively

## References

- [esptool.py Documentation](https://github.com/espressif/esptool)
- [Espressif Boot Mode Selection](https://docs.espressif.com/projects/esptool/en/latest/esp32c6/advanced-topics/boot-mode-selection.html)
- [ESP32C6 Datasheet](https://www.espressif.com/sites/default/files/documentation/esp32-c6_datasheet_en.pdf)
