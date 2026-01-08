# PowerMeter Hardware - Bill of Materials (BOM)

## Summary
Complete electronic component list for PowerMeter ESP32C6-based Zigbee power meter reader.

## Microcontroller & Core

| Qty | Part | Description | Notes |
|-----|------|-------------|-------|
| 1 | **ESP32-C6-WROOM-1** | 32-bit microcontroller with Zigbee/BLE | 2.4 GHz wireless, 160 MHz |
| 1 | **SiP PMIC** | Integrated power management (on-module) | Part of module |

## Power Supply

| Qty | Part | Value | Package | Notes |
|-----|------|-------|---------|-------|
| 1 | **MCP1700-3302E** | 3.3V, 150mA LDO regulator | SOT-23-3 | Low dropout, stable 3.3V output |
| 1 | **Electrolytic Capacitor** | 470µF–1000µF, 6.3V+ | Through-hole or SMD | **Critical: prevents brownout** - mount parallel to regulator |
| 1 | **Supercapacitor** | 10F, 3.3V (optional) | Through-hole | For backup power during Zigbee sleep |
| 2 | **Ceramic Capacitor** | 100nF (0.1µF) | 0603 SMD | Bypass capacitors (near VCC pins) |
| 1 | **Ceramic Capacitor** | 10µF | 0603 SMD | Additional smoothing |

## I/O & Connectors

| Qty | Part | Description | Interface | Notes |
|-----|------|-------------|-----------|-------|
| 1 | **JST PH 2-Pin Connector** | Power input | 2.54mm pitch | ≥4.5V DC input (recommend 5V supply) |
| 1 | **JST PH 3-Pin Connector** (or DB9/UART breakout) | DLMS meter connection | UART 3-pin (TX, RX, GND) | 2400 baud protocol |
| 1 | **Programming Header** | SPI/JTAG (optional) | 6-pin or 4-pin | For bootloader flashing |

## Status Indicators

| Qty | Part | Color | Package | GPIO | Function |
|-----|------|-------|---------|------|----------|
| 1 | **LED** | Red (620nm) | 3mm or SMD 0603 | GPIO5 | Status indicator - commissioning |
| 1 | **LED** | Green (525nm) | 3mm or SMD 0603 | GPIO6 | Status indicator - data transmission |
| 2 | **Current Limiting Resistor** | 220Ω–330Ω | 1/4W or 0603 SMD | — | LED series resistors |

## Control Interface

| Qty | Part | Type | Package | GPIO | Function |
|-----|------|------|---------|------|----------|
| 1 | **Push Button** | Tactile switch, 12mm | 4-pin THT or SMD | GPIO4 | Long press (4s) = factory reset, double-click = restart |
| 1 | **Pull-up Resistor** | 10kΩ | 1/4W or 0603 SMD | — | Button debouncing |

## Optional: Voltage Monitoring (Commented Code - WIP)

| Qty | Part | Value | Package | Notes |
|-----|------|-------|---------|-------|
| 2 | **Resistor** | 10kΩ | 1/4W or 0603 SMD | Voltage divider for VCC monitoring (ADC) |
| 1 | **Ceramic Capacitor** | 100nF | 0603 SMD | ADC input filter |

## Mechanical

| Qty | Part | Description | Notes |
|-----|------|-------------|-------|
| 1 | **PCB** | Custom designed (KiCad project) | See `PCB/PowerMeter.kicad_pcb` |
| 4 | **Standoff** | M3 nylon or brass | PCB mounting |
| 1 | **Enclosure** | 3D printed case | See `3DPrint/PowerMeter v9.3mf` |

## Assembly Notes

### Critical Power Circuit
- **Bulk capacitor placement**: Must be **parallel** to voltage regulator output (not series)
- **Capacitor voltage rating**: Minimum 6.3V (10V+ recommended)
- **Capacitor type**: Electrolytic, 470µF–1000µF minimum
- **Why**: Prevents brownout resets during transient power demand

### UART Pinout (Meter Connection)
```
Pin 1: GND
Pin 2: TX (GPIO0) - from ESP32 to meter (output)
Pin 3: RX (GPIO1) - from meter to ESP32 (input)
```
Baud rate: 2400, 8 bits, no parity, 1 stop bit

### GPIO Restrictions During Programming
- **GPIO8**: Must be HIGH during flash
- **GPIO9**: Must be LOW during flash
- These pins are not used in firmware

### LED Circuit
```
GPIO5 (Red LED)   ---|[220Ω]---|>|-----|GND
GPIO6 (Green LED) ---|[220Ω]---|>|-----|GND
```

### Button Circuit
```
GPIO4 ---|[10kΩ pull-up]---VCC
       |
       └──[Button]──GND
```

## Power Specifications

| Parameter | Value | Notes |
|-----------|-------|-------|
| **Input voltage** | 4.5V–5.5V DC | Minimum 4.5V for regulator dropout |
| **Recommended input** | 5V @ 500mA minimum | Power supply capability |
| **Regulated output** | 3.3V @ 150mA | LDO regulator limit |
| **ESP32-C6 current** | ~80mA (active), ~20µA (sleep) | Idle load much lower |
| **Sleep mode** | ~10–50µA | With Zigbee sleep enabled |

## Supplier References (Example)

| Component | Example Part # | Supplier | Notes |
|-----------|---------------|----------|-------|
| ESP32-C6-WROOM-1 | ESP32-C6-WROOM-1 | Digi-Key, Mouser | Check stock/lead times |
| MCP1700-3302E | MCP1700-3302E/CB | Digi-Key, Mouser | SOT-23 package |
| Bulk Capacitor | EKMG160ELL471MK20S | Digi-Key (Panasonic) | 470µF 10V electrolytic |
| LED Red | LTST-C150R | Digi-Key (Lite-On) | 3mm through-hole |
| LED Green | LTST-C150G | Digi-Key (Lite-On) | 3mm through-hole |
| Push Button | 9453 (Omron) | Digi-Key | 12mm tactile switch |

## Revision History

| Version | Date | Changes |
|---------|------|---------|
| v9 | 2025-10-01 | Fixed supercapacitor placement (parallel, not series); confirmed bulk capacitor requirement |
| v8 | 2025-07-21 | Button moved to GPIO4; UART pins confirmed GPIO0/1 |
| v7 | 2025-06-10 | MCP1700-3302E selected; power circuit redesign |

## Cost Estimate (USD)

| Category | Est. Cost | Notes |
|----------|-----------|-------|
| ESP32-C6-WROOM-1 | $3–5 | Module (prices vary by volume) |
| Regulator + Caps | $1–2 | MCP1700 + bulk cap + ceramics |
| LEDs + Resistors | $0.50 | Basic components |
| Button + Resistor | $0.50 | Tactile switch |
| PCB | $2–5 | Prototype (Jlcpcb, Oshpark) |
| Connectors | $1–2 | JST headers, UART connector |
| Enclosure | $1–3 | 3D printed resin or SLS |
| **Total** | **~$10–20** | Prototype quantities (1–10 units) |

## Next Steps for Assembly

1. Order PCB from Gerber files in `PCB/Out v0.1/`
2. Procure components from BOM above
3. Reflow solder (if SMD) or hand-solder THT components
4. **Critical check**: Verify bulk capacitor is parallel to regulator
5. Power test: Apply 5V, check 3.3V output with multimeter
6. Program ESP32-C6 via SPI/JTAG or UART bootloader
7. Assemble into 3D-printed enclosure (`3DPrint/PowerMeter v9.3mf`)
