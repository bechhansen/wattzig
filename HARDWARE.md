# WattZig Hardware - Bill of Materials (BOM)

## Summary
Complete electronic component list for WattZig ESP32C6-based Zigbee power meter reader.

## Microcontroller

| Qty | Part | Description | Notes | Ref |
|-----|------|-------------|-------|-----|
| 1 | **ESP32-C6-WROOM-1** | 32-bit microcontroller with Zigbee/BLE | 2.4 GHz wireless, 160 MHz | U2 |

## Power Supply

| Qty | Part | Value | Package | Notes | Ref |
|-----|------|-------|---------|-------|-----|
| 1 | **MCP1700-3302E** | 3.3V, 150mA LDO regulator | SOT-23-3 | Low dropout, stable 3.3V output | U1 |
| 1 | **Electrolytic Capacitor** | 470µF–1000µF, 6.3V+ | Through-hole or SMD | Prevents brownout| C? |
| 1 | **PHV-5R4H155-R** (PHV-5R4H474-R)| 1.5F, 5.4V | Through-hole | Supercapacitor for supplying suffiecent power to circuit | C3 |
| 2 | **Ceramic Capacitor** | 100nF (0.1µF) | 0603 SMD | Bypass capacitors (near VCC pins) |  |
| 1 | **Ceramic Capacitor** | 10µF | 0603 SMD | Additional smoothing |  |
| 1 | **1N5817W** | 450 mV @ 1 A | SOD-123 | Schottky Barrier Diode | D2 |

## I/O & Connectors

| Qty | Part | Description | Interface | Notes | Ref |
|-----|------|-------------|-----------|-------|-----|
| 1 | **PPTC032LJBN-RC** | DLMS meter connection | 6POS (2X3) 2.54mm | https://www.digikey.at/en/products/detail/sullins-connector-solutions/PPTC032LJBN-RC/775975| J2 |
| 1 | **Programming Header** |  | 6-pin | For bootloader flashing | J1 |

## Status Indicators

| Qty | Part | Color | Package | GPIO | Function | Ref |
|-----|------|-------|---------|------|----------|-----|
| 1 | **LED** | Red/Green |  | GPIO5 | Status indicator - commissioning | D1 |
| 2 | **Current Limiting Resistor** | 220Ω–330Ω | 1/4W or 0603 SMD | — | LED series resistors | |

## Control Interface

| Qty | Part | Type | Package | GPIO | Function | Url | Ref |
|-----|------|------|---------|------|----------|-----|-----|
| 1 | **Push Button** | Tactile switch, 12mm | 4-pin THT or SMD | GPIO4 | Long press (4s) = factory reset, double-click = restart | https://www.digikey.dk/da/products/detail/cit-relay-and-switch/CS1102V5-85F100/16607637 | SW1 |
| 1 | **Pull-up Resistor** | 10kΩ | 1/4W or 0603 SMD | — | Button debouncing |  | 

## Optional: Voltage Monitoring (Commented Code - WIP)

| Qty | Part | Value | Package | Notes | Ref |
|-----|------|-------|---------|-------|-----|
| 2 | **Resistor** | 10kΩ | 1/4W or 0603 SMD | Voltage divider for VCC monitoring (ADC) |  |


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

| Parameter | Value | Notes | Ref |
|-----------|-------|-------|-----|
| **Input voltage** | 4.5V–5.5V DC | Minimum 4.5V for regulator dropout | — |
| **Recommended input** | 5V @ 500mA minimum | Power supply capability | — |
| **Regulated output** | 3.3V @ 150mA | LDO regulator limit | U2 |
| **ESP32-C6 current** | ~80mA (active), ~20µA (sleep) | Idle load much lower | U1 |
| **Sleep mode** | ~10–50µA | With Zigbee sleep enabled | — |

## Supplier References (Example)

| Component | Example Part # | Supplier | Notes | Ref |
|-----------|---------------|----------|-------|-----|
| ESP32-C6-WROOM-1 | ESP32-C6-WROOM-1 | Digi-Key, Mouser | Check stock/lead times | U1 |
| MCP1700-3302E | MCP1700-3302E/CB | Digi-Key, Mouser | SOT-23 package | U2 |
| Bulk Capacitor | EKMG160ELL471MK20S | Digi-Key (Panasonic) | 470µF 10V electrolytic | C1 |
| LED Red | LTST-C150R | Digi-Key (Lite-On) | 3mm through-hole | LED1 |
| LED Green | LTST-C150G | Digi-Key (Lite-On) | 3mm through-hole | LED2 |
| Push Button | 9453 (Omron) | Digi-Key | 12mm tactile switch | SW1 |

## Revision History

| Version | Date | Changes |
|---------|------|----------|
| v10 | 2026-01-18 | Added Ref column to all BOM tables for component tracking |
| v9 | 2025-10-01 | Fixed supercapacitor placement (parallel, not series); confirmed bulk capacitor requirement |
| v8 | 2025-07-21 | Button moved to GPIO4; UART pins confirmed GPIO0/1 |
| v7 | 2025-06-10 | MCP1700-3302E selected; power circuit redesign |

## Cost Estimate (USD)

| Category | Est. Cost | Notes | Ref |
|----------|-----------|-------|-----|
| ESP32-C6-WROOM-1 | $3–5 | Module (prices vary by volume) | U1 |
| Regulator + Caps | $1–2 | MCP1700 + bulk cap + ceramics | U2, C1–C5 |
| LEDs + Resistors | $0.50 | Basic components | LED1, LED2, R1, R2 |
| Button + Resistor | $0.50 | Tactile switch | SW1, R3 |
| PCB | $2–5 | Prototype (Jlcpcb, Oshpark) | PCB1 |
| Connectors | $1–2 | JST headers, UART connector | J1–J3 |
| Enclosure | $1–3 | 3D printed resin or SLS | — |
| **Total** | **~$10–20** | Prototype quantities (1–10 units) | — |

## Next Steps for Assembly

1. Order PCB from Gerber files in `PCB/Out v0.1/`
2. Procure components from BOM above
3. Reflow solder (if SMD) or hand-solder THT components
4. **Critical check**: Verify bulk capacitor is parallel to regulator
5. Power test: Apply 5V, check 3.3V output with multimeter
6. Program ESP32-C6 via SPI/JTAG or UART bootloader
7. Assemble into 3D-printed enclosure (`3DPrint/WattZig v9.3mf`)
