# ESP32-C6 Zigbee Power Supply Issue - Documentation Summary

## Issue Description
ESP32-C6 brownout resets during Zigbee network initialization due to inadequate power delivery from supercapacitor with excessive ESR (Equivalent Series Resistance).

## Root Cause Analysis

### Current Hardware Configuration
- **Supercapacitor**: CE5R5105HF-ZJ (CDA Cap CE Series)
  - Capacitance: 1.0F
  - Voltage Rating: 5.5V
  - **ESR: 30Ω** ← PRIMARY PROBLEM
  - Height: 6.0mm
  - **Status**: Previously shorted, likely damaged (ESR may be higher than 30Ω)

- **Power Supply**: USB limited to 45mA average
- **LDO Regulator**: MCP1700T-3302E/MB (250mA max, 3.3V output)
- **Supply Voltage**: 4.1V at LDO input
- **Decoupling Network**: 470µF electrolytic (2cm from ESP32) + 10µF + 100nF ceramics

### Power Requirements
- **Zigbee TX Burst**: 100-150mA for milliseconds
- **Normal Operation**: ~30-50mA
- **Light Sleep**: ~1mA
- **Data Interval**: Every 10 seconds via UART

### Why 30Ω ESR Fails

**Voltage Drop Calculation:**
```
At 150mA Zigbee TX:
Voltage drop = 150mA × 30Ω = 4.5V

Available voltage = 4.1V - 4.5V = NEGATIVE (complete voltage collapse)
```

Even at 100mA:
```
Voltage drop = 100mA × 30Ω = 3.0V
Available voltage = 4.1V - 3.0V = 1.1V (insufficient for 3.3V LDO regulation)
```

The LDO requires minimum ~3.6V input to maintain 3.3V output. With 30Ω ESR, the voltage collapses below this threshold during Zigbee transmission, causing brownout resets.

## Solution Requirements

### Supercapacitor Specifications Needed
- **ESR**: <5Ω (preferably <3.5Ω)
- **Capacitance**: ≥0.47F (1.0F preferred)
- **Voltage Rating**: ≥5.5V
- **Height**: ≤10mm (ideally ≤13mm if flexibility possible)
- **Status**: Undamaged (never shorted)

### Recommended Supercapacitor Options

#### Option 1: KEMET FT Series (BEST PERFORMANCE)
**Part Number**: FT0H105ZF
- Capacitance: 1.0F
- **ESR: 3.5Ω** ✓ (8.5× better than current)
- Voltage: 5.5V
- Height: **13.0mm** (3mm over target)
- Max Current (30 min): 1.5mA

**Voltage drop at 150mA**: 150mA × 3.5Ω = **525mV** (acceptable)
**Available voltage**: 4.1V - 0.525V = **3.575V** ✓ (sufficient for LDO)

**Trade-off**: Exceeds height constraint by 3mm but provides excellent performance.

#### Option 2: Alternative Search Required
No supercapacitors found that meet BOTH <10mm height AND <5Ω ESR requirements. This is a fundamental physics limitation - low ESR requires larger electrode surface area.

### Alternative Power Solutions

#### LiPo Battery (RECOMMENDED IF HEIGHT CRITICAL)
- **Voltage**: 3.7V, 150-250mAh
- **ESR**: <0.1Ω (35× better than FT0H105ZF, 300× better than current)
- **Thickness**: 4-6mm ✓
- **Requires**: Charge management IC (e.g., MCP73831)
- **Advantages**: 
  - Ideal for pulse current applications
  - Fits height constraint
  - Much lower ESR
  - Designed for exactly this use case

## Working Solution: Light Sleep Mode

### Implementation Strategy
Given 10-second data intervals, implement light sleep with UART wake-up:
```c
// Power consumption in light sleep: ~1mA
// Voltage drop: 1mA × 30Ω = 30mV (acceptable)
// Recharge time: 9.9 seconds per cycle (sufficient)

esp_sleep_enable_uart_wakeup(UART_NUM);
esp_light_sleep_start();
```

### Additional Optimizations Required

1. **Reduce Zigbee TX Power**
```c
   esp_zb_set_tx_power(0);  // 0 dBm instead of maximum
   // Further reduction: -3 dBm or -6 dBm if range permits
```

2. **Lower CPU Frequency**
```c
   esp_pm_config_t pm_config = {
       .max_freq_mhz = 80,  // Reduce from 160MHz
       .min_freq_mhz = 40,
       .light_sleep_enable = true
   };
```

3. **Move 470µF Bulk Capacitor Closer**
   - **Current**: 2cm from ESP32-C6
   - **Target**: <5mm from ESP32-C6 power pins
   - Reduces trace inductance and resistance

4. **Verify Decoupling Network Integrity**
   - 470µF electrolytic on 3.3V rail
   - 10µF ceramic at ESP32-C6
   - 100nF ceramic at ESP32-C6 power pins (<5mm)

## Critical Issue: Zigbee Pairing

### Pairing Power Requirements
- **Duration**: 10-30+ seconds continuous
- **Current**: 80-150mA sustained (not just bursts)
- **Total Energy**: ~10 Watt-seconds

### Why Current Supercap CANNOT Handle Pairing
```
Sustained 100mA for 30 seconds:
Voltage drop = 100mA × 30Ω = 3V
Available voltage = 4.1V - 3V = 1.1V ← INSUFFICIENT

Energy stored in 1F @ 4.1V = 8.4 joules
Energy needed for pairing = ~10 joules
Result: Supercap fully discharges, system crashes
```

### Pairing Solution Options

**Option A: External Power During Pairing (RECOMMENDED)**
- Use unrestricted USB power supply during initial pairing
- After pairing complete, switch to 45mA-limited supply
- Network credentials stored in flash, device rejoins automatically

**Option B: Bypass Jumper**
- Add physical jumper that bypasses current limit during pairing
- Remove after pairing complete
- Document in user manual

**Option C: Field Pairing Procedure**
- Provide external power supply for re-pairing scenarios
- Or implement "pairing mode" button that requires external power

### Normal Rejoin vs. Initial Pairing
- **Rejoining** (after power cycle): 1-5 seconds, stored credentials
- **Initial pairing**: 10-30+ seconds, full network discovery
- Supercap may handle rejoin, but NOT initial pairing

## Recommended Action Plan

### Immediate Actions (Software Fixes)
1. ✅ Implement light sleep with UART wake-up
2. ✅ Reduce Zigbee TX power to 0 dBm or lower
3. ✅ Set CPU frequency to 80MHz
4. ✅ Move 470µF capacitor closer to ESP32-C6
5. ✅ Use external power for initial Zigbee pairing

### Hardware Upgrade Options

#### Option 1: Accept 13mm Height Limit
- **Replace with**: KEMET FT0H105ZF
- **ESR**: 3.5Ω (excellent performance)
- **Height**: 13mm (3mm over budget)
- **Result**: Reliable operation with proper power margin

#### Option 2: Redesign with LiPo Battery
- **Battery**: 3.7V 150-250mAh
- **Add**: MCP73831 charge controller
- **ESR**: <0.1Ω (best performance)
- **Height**: 4-6mm ✓
- **Result**: Ideal solution for pulse current applications

#### Option 3: Continue with Current Hardware
- Implement all software optimizations above
- Test if light sleep + reduced TX power eliminates brownouts
- Accept that initial pairing requires external power
- **Risk**: May still experience intermittent brownouts during TX

## Technical Background

### ESR (Equivalent Series Resistance)
Internal resistance of capacitor that causes voltage drop when current flows through it.
- **Formula**: V_drop = Current × ESR
- **Impact**: Higher ESR = greater voltage collapse under load
- **Physics**: Low ESR requires larger electrode surface area (more physical volume)

### Why This Matters for Zigbee
Zigbee transmission creates brief high-current pulses (100-150mA). With high ESR, these pulses cause instantaneous voltage drops that trigger brownout protection, resetting the system.

## Conclusion

The CE5R5105HF-ZJ supercapacitor with 30Ω ESR is fundamentally unsuitable for ESP32-C6 Zigbee applications requiring 100-150mA transmission bursts. The primary issue is excessive ESR causing voltage collapse, not insufficient capacitance.

**Best long-term solution**: Replace with KEMET FT0H105ZF (3.5Ω ESR) if 13mm height is acceptable, or redesign with LiPo battery for optimal performance within height constraint.

**Interim solution**: Implement light sleep mode with reduced TX power and optimized hardware placement. This may provide acceptable operation for the 10-second data interval use case, but initial Zigbee pairing will still require external power.

---

**Document Date**: 2024
**Hardware**: ESP32-C6, CE5R5105HF-ZJ supercapacitor, MCP1700 LDO
**Application**: Zigbee end device with 10-second UART data intervals