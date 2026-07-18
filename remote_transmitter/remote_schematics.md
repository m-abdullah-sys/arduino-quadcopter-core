# Remote Transmitter Schematic & Pin Mapping

## Overview
This document contains the complete electrical schematic and pin configuration for the Arduino Nano remote transmitter unit.

## Hardware Specifications

### Microcontroller
- **MCU**: Arduino Nano (ATmega328P)
- **Operating Voltage**: 5V
- **Clock Speed**: 16 MHz
- **Flash Memory**: 32 KB
- **SRAM**: 2 KB

### RF Communication Module
- **Module**: NRF24L01+ 2.4GHz Transceiver
- **Interface**: SPI (3-wire protocol)
- **Data Rate**: 250 kbps (for reliability over range)
- **PA Level**: Minimum (for safety in testing)
- **Operating Voltage**: 3.3V (requires voltage regulator)

### Input Devices
- **Joystick**: Dual-axis analog potentiometer (X & Y axes)
  - **Type**: Standard 10k dual-axis module
  - **Output**: 0-1023 ADC on each axis
  - **Center**: ~512 (2.5V)
  - **Supply**: 5V from Arduino

- **Potentiometer**: Linear 10k trimmer or slide pot
  - **Type**: Analog input device
  - **Output**: 0-1023 ADC
  - **Supply**: 5V from Arduino

### Power Supply
- **Primary**: 9V battery (recommended 750mAh NiMH or 6F22 alkaline)
- **Arduino Regulation**: Internal 7805 regulator (9V → 5V @ 500mA)
- **RF Module**: 3.3V regulator (LD1117-33) from 5V rail
  - **Output**: 500mA capable (NRF24 peak draw ~15mA)

## Detailed Pin Mapping

### Arduino Nano Pins
| Pin | Function | Connection | Notes |
|-----|----------|-----------|-------|
| A0 | Joystick X (Roll) | Joystick X-axis output | Analog input, 0-1023 |
| A1 | Joystick Y (Pitch) | Joystick Y-axis output | Analog input, 0-1023 |
| A2 | Potentiometer (Throttle) | Pot wiper | Analog input, 0-1023 |
| D9 | RF CE (Chip Enable) | NRF24 CE pin | SPI control line |
| D10 | RF CSN (Chip Select) | NRF24 CSN pin | SPI control line |
| D11 | RF MOSI | NRF24 MOSI pin | Hardware SPI data out |
| D12 | RF MISO | NRF24 MISO pin | Hardware SPI data in |
| D13 | RF SCK | NRF24 SCK pin | Hardware SPI clock |
| 5V | Power Rail | Arduino 5V out | Powers joystick & pot |
| 3V3 | 3.3V Rail | LD1117 output | Powers NRF24L01 |
| GND | Ground | Common ground | All devices share GND |

### NRF24L01+ Pin Details

```
NRF24L01+ DIP Pinout (Top View):
┌─────────────────────┐
│ 1(GND) 2(VCC)       │
│ 3(CE)  4(CSN)       │
│ 5(SCK) 6(MOSI)      │
│ 7(MISO) 8(IRQ)      │
└─────────────────────┘

Pin Connections:
1. GND    → Arduino GND
2. VCC    → 3.3V (via LD1117)
3. CE     → Arduino D9
4. CSN    → Arduino D10
5. SCK    → Arduino D13
6. MOSI   → Arduino D11
7. MISO   → Arduino D12
8. IRQ    → Not connected (optional)

Add 10µF capacitor between VCC and GND on RF module
```

### Joystick Connections

```
Joystick Module:
┌─────────────┐
│ GND VRx VCC │
│ Vy  +      │
└─────────────┘

Pin Connections:
- GND  → Arduino GND
- VCC  → Arduino 5V
- VRx  → Arduino A0 (Roll X-axis)
- VRy  → Arduino A1 (Pitch Y-axis)
- +    → Not used (optional indicator LED)
```

### Potentiometer Connections

```
Linear Potentiometer (10k):
┌────────────────┐
│ 1    2    3    │
└────────────────┘

Pin Connections:
- Pin 1 (Left)   → Arduino GND
- Pin 2 (Wiper)  → Arduino A2 (Throttle)
- Pin 3 (Right)  → Arduino 5V
```

## Power Distribution

```
9V Battery
    ↓
[9V Connector]
    ↓
[Arduino Nano - Built-in 7805 Regulator]
    ├── 5V Rail (500mA)
    │   ├→ Joystick VCC
    │   ├→ Potentiometer Pin 3
    │   └→ LD1117 Input
    │
    └── GND Rail
        ├→ Joystick GND
        ├→ Potentiometer Pin 1
        └→ LD1117 GND

[LD1117-3.3 Regulator]
    ├── Input: 5V from Arduino
    ├── Output: 3.3V @ 500mA
    └── 10µF cap on output → NRF24L01+ VCC

NRF24L01+:
    ├── VCC: 3.3V
    ├── GND: Common
    └── SPI: To Arduino (D9-D13)
```

## Wiring Diagram

```
┌────────────────────────────────────────┐
│       Arduino Nano Remote              │
│       5V Controller                    │
│                                        │
│  ANALOG INPUTS:                        │
│  A0 ←──── Joystick X (Roll)           │
│  A1 ←──── Joystick Y (Pitch)          │
│  A2 ←──── Potentiometer (Throttle)    │
│           (5V divider network)         │
│                                        │
│  SPI INTERFACE:                        │
│  D9  ────→ NRF24 CE                   │
│  D10 ────→ NRF24 CSN                  │
│  D11 ────→ NRF24 MOSI                 │
│  D12 ←──── NRF24 MISO                 │
│  D13 ────→ NRF24 SCK                  │
│                                        │
│  POWER:                                │
│  5V  ────→ Joystick, Pot, LD1117      │
│  GND ────→ Common ground              │
│  3V3 ────→ NRF24L01+ (via LD1117)    │
│           [10µF cap]                   │
│                                        │
│  BATTERY: 9V                           │
│  Vin ←──── 9V battery connector        │
└────────────────────────────────────────┘
```

## Control Packet Format

```
Byte Structure (4 bytes total):
┌─────────┬─────────┬──────────┬──────────┐
│ Pitch   │ Roll    │ Throttle │ Checksum │
│ (0-255) │ (0-255) │ (0-255)  │ Sum CRC  │
└─────────┴─────────┴──────────┴──────────┘

Transmission Rate: 20 Hz (50ms interval)
Data Rate: 250 kbps
Range: ~100m line of sight
```

## Calibration

1. **Joystick Center**: Should read ~512 (2.5V) on both axes when untouched
2. **Potentiometer Range**: Should vary from 0-1023 across full travel
3. **Deadzone**: ±30 ADC units around center (prevents drift)

## Voltage Specifications

| Rail | Voltage | Current | Devices |
|------|---------|---------|---------|
| 5V | 5.0V | ~100mA | Arduino, Joystick, Potentiometer, LD1117 |
| 3.3V | 3.3V | ~15mA | NRF24L01+ (with 10µF cap for stability) |

## PCB Layout Tips

- Keep RF module traces short and direct
- Use ground plane for stability
- Place 10µF capacitor close to NRF24 VCC pin
- Separate analog (joystick) and digital (SPI) grounds if possible
- Use shielded USB cable if testing via serial (reduces RF interference)

## Troubleshooting

| Issue | Cause | Solution |
|-------|-------|----------|
| RF module not initializing | No power | Check 3.3V regulator output |
| Unstable joystick readings | Noise | Add 100nF caps across joystick outputs |
| Packet transmission fails | SPI conflict | Check pin D11-D13 are not used elsewhere |
| Short RF range | Low PA level | Increase PA level in code (caution: power consumption) |
| Joystick doesn't respond | Inverted axis | Swap A0/A1 or reverse map() parameters |
