# Flight Controller Schematic & Pin Mapping

## Overview
This document contains the complete electrical schematic and pin configuration for the Arduino Nano 33 BLE Sense flight controller unit.

## Hardware Specifications

### Microcontroller
- **MCU**: Arduino Nano 33 BLE Sense
- **Processor**: ARM Cortex-M4 (64 MHz)
- **Operating Voltage**: 3.3V logic (native)
- **Flash Memory**: 256 KB
- **SRAM**: 64 KB

### Built-in Sensors (LSM9DS1 IMU)
- **Interface**: Internal I2C bus (not user configurable)
- **Accelerometer**: 3-axis (±2g to ±16g range)
- **Gyroscope**: 3-axis (±245°/s to ±2000°/s range)
- **Magnetometer**: 3-axis (±4 to ±16 Gauss range)
- **Sampling Rate**: Up to 1.66 kHz

### RF Communication Module
- **Module**: NRF24L01+ 2.4GHz Transceiver
- **Interface**: SPI (3-wire protocol)
- **Data Rate**: 250 kbps
- **PA Level**: Minimum
- **Operating Voltage**: 3.3V (direct connection)

### Motor Actuators
- **ESCs (Electronic Speed Controllers)**: 4x brushless motor controllers
- **Control Signal**: PWM (1000-2000 µs pulse width)
- **PWM Frequency**: 50 Hz (servo standard)
- **Supply Voltage**: 5V from BEC (Battery Eliminator Circuit)

### Power Supply
- **Primary**: 3S LiPo Battery (11.1V nominal, 12.6V max)
- **BEC (5V/3A)**: Supplies motor controllers and servos
- **Arduino**: Native 3.3V via on-board regulator from 5V BEC
- **RF Module**: 3.3V direct (shares Arduino 3.3V rail)

## Detailed Pin Mapping

### Arduino Nano 33 BLE Sense Pins
| Pin | Function | Connection | Notes |
|-----|----------|-----------|-------|
| D3 | Motor FL (PWM) | ESC Front-Left | 490 Hz PWM |
| D5 | Motor FR (PWM) | ESC Front-Right | 490 Hz PWM |
| D6 | Motor RL (PWM) | ESC Rear-Left | 977 Hz PWM |
| D9 | Motor RR (PWM) | ESC Rear-Right | 977 Hz PWM |
| D7 | RF CE | NRF24 Chip Enable | SPI control |
| D8 | RF CSN | NRF24 Chip Select | SPI control |
| D11 | RF MOSI | NRF24 MOSI | Hardware SPI out |
| D12 | RF MISO | NRF24 MISO | Hardware SPI in |
| D13 | RF SCK | NRF24 Clock | Hardware SPI clock |
| A4 | IMU SDA | Internal LSM9DS1 | I2C data (internal) |
| A5 | IMU SCL | Internal LSM9DS1 | I2C clock (internal) |
| GND | Ground | Common | All devices share GND |
| Vin | Input Power | 5V from BEC | Powers on-board regulator |
| 3V3 | 3.3V Rail | NRF24 VCC | Regulated output |
| 5V | 5V Rail | ESC Power (optional) | Via Vin if needed |

### ESC Connection Details

```
ESC Connector (3-pin):
┌──────────────┐
│ Signal GND VCC
└──────────────┘

Front-Left ESC (Motor 1):
- Signal → Arduino D3
- GND    → Common Ground
- VCC    → 5V from BEC

Front-Right ESC (Motor 2):
- Signal → Arduino D5
- GND    → Common Ground
- VCC    → 5V from BEC

Rear-Left ESC (Motor 3):
- Signal → Arduino D6
- GND    → Common Ground
- VCC    → 5V from BEC

Rear-Right ESC (Motor 4):
- Signal → Arduino D9
- GND    → Common Ground
- VCC    → 5V from BEC
```

### NRF24L01+ Connection

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
2. VCC    → Arduino 3.3V
3. CE     → Arduino D7
4. CSN    → Arduino D8
5. SCK    → Arduino D13
6. MOSI   → Arduino D11
7. MISO   → Arduino D12
8. IRQ    → Not connected (optional)

Capacitor: 10µF between VCC and GND
```

### IMU (Built-in LSM9DS1)
- **Interface**: Internal I2C (not user accessible)
- **I2C Address**: 0x6B (accel/gyro), 0x1E (mag)
- **Automatically initialized** by Arduino_LSM9DS1 library
- No external wiring required

## Power Distribution

```
3S LiPo Battery (11.1V)
    ↓
[BEC Regulator: 11.1V → 5V @ 3A]
    ├── 5V Rail (3A max)
    │   ├→ ESC 1 (Front-Left)
    │   ├→ ESC 2 (Front-Right)
    │   ├→ ESC 3 (Rear-Left)
    │   ├→ ESC 4 (Rear-Right)
    │   └→ Arduino Nano 33 BLE (Vin pin)
    │
    └── GND Rail (Common)
        ├→ All ESCs
        ├→ Arduino GND
        └→ NRF24 GND

[Arduino Nano 33 BLE Sense]
    ├── 3.3V Rail (500mA) [On-board regulator]
    │   ├→ NRF24L01+ VCC
    │   └→ Logic circuits
    │
    └── GND Rail
        ├→ NRF24 GND
        └→ ESC GND (common)
```

## Motor Mixing Algorithm (X-Configuration)

```
Quadcopter Frame Layout (X-mode):
      Front
    FL(1) --- FR(2)
      \       /
       \     /
        \   /
        /   \
       /     \
      /       \
    RL(4) --- RR(3)
      Back

Motor Speed Calculation:
FL = Base + Pitch_Correction - Roll_Correction
FR = Base + Pitch_Correction + Roll_Correction
RL = Base - Pitch_Correction - Roll_Correction
RR = Base - Pitch_Correction + Roll_Correction

Where:
- Base = Throttle setting (1000-2000 µs)
- Pitch_Correction = PID output for forward/back attitude
- Roll_Correction = PID output for left/right attitude
```

## PWM Timing Details

| Channel | Arduino Pin | PWM Frequency | Frequency Note |
|---------|------------|---------------|-----------------|
| ESC FL | D3 | 490 Hz | Timer2 (8-bit) |
| ESC FR | D5 | 490 Hz | Timer0 (8-bit) |
| ESC RL | D6 | 977 Hz | Timer4 (8-bit) |
| ESC RR | D9 | 977 Hz | Timer1 (16-bit) |

**Note**: PWM resolution is limited by Arduino. For smooth ESC control, frequencies should be 50-100 Hz for servo-type ESCs. Use `setPwmFrequency()` function to adjust if needed.

## Control Loop Timing

```
Main Loop:
├── 100 Hz Control Loop (10ms)
│   ├── Read IMU (5ms sampling interval)
│   ├── Calculate PID corrections
│   ├── Update motor speeds via PWM
│   └── Check RF timeout
│
├── RF Packet Reception (event-driven)
│   └── Parse throttle, pitch, roll commands
│
└── Failsafe (500ms timeout)
    └── Cut motors if no command received
```

## Voltage Specifications

| Rail | Voltage | Current | Devices |
|------|---------|---------|---------|
| 5V (BEC) | 5.0V | 3A | ESCs, Arduino Vin |
| 3.3V (Arduino) | 3.3V | 500mA | NRF24L01+, Logic |
| LiPo Battery | 11.1V nominal | Peak 50-100A | Main power |

## PID Tuning Parameters

Located in `flight_controller.ino`:

```cpp
#define PID_ROLL_KP 1.5    // Roll proportional
#define PID_ROLL_KI 0.05   // Roll integral
#define PID_ROLL_KD 0.8    // Roll derivative

#define PID_PITCH_KP 1.5   // Pitch proportional
#define PID_PITCH_KI 0.05  // Pitch integral
#define PID_PITCH_KD 0.8   // Pitch derivative
```

**Tuning Guide**:
- **Kp**: Higher = faster response, too high = oscillation
- **Ki**: Reduces steady-state error, too high = windup
- **Kd**: Dampens overshoot, too high = sluggish

## Safety Features

1. **Command Timeout**: 500ms without RF signal → Motor cutoff
2. **ESC Arming**: Motors only spin when throttle > 10%
3. **Sensor Calibration**: 5-second static calibration on boot
4. **Watchdog**: Continuous monitoring of sensor health
5. **Current Limiting**: PWM values constrained to safe range

## Wiring Diagram

```
┌─────────────────────────────────────────┐
│   Arduino Nano 33 BLE Sense FC          │
│   3.3V Processor, 64MHz ARM             │
│                                         │
│  MOTOR OUTPUTS (PWM):                  │
│  D3  ────→ ESC 1 (Front-Left)          │
│  D5  ────→ ESC 2 (Front-Right)         │
│  D6  ────→ ESC 3 (Rear-Left)           │
│  D9  ────→ ESC 4 (Rear-Right)          │
│           [5V signals with GND return] │
│                                         │
│  RF COMMUNICATION (SPI):                │
│  D7  ────→ NRF24 CE                    │
│  D8  ────→ NRF24 CSN                   │
│  D11 ────→ NRF24 MOSI                  │
│  D12 ←──── NRF24 MISO                  │
│  D13 ────→ NRF24 SCK                   │
│           [3.3V SPI + 10µF cap]        │
│                                         │
│  IMU SENSOR (Internal):                 │
│  I2C (A4/A5) ←→ LSM9DS1                │
│             [Built-in, no wiring]      │
│                                         │
│  POWER:                                 │
│  Vin  ←──── 5V from BEC                │
│  GND  ────→ Common Ground              │
│  3V3  ────→ NRF24 VCC                  │
│             [From on-board regulator]  │
│                                         │
│  3S LiPo ←── Battery (11.1V)           │
│  ↓                                      │
│  [BEC: 11.1V → 5V @ 3A]               │
│  ↓                                      │
│  ESC Power Rail                        │
└─────────────────────────────────────────┘
```

## Troubleshooting

| Issue | Cause | Solution |
|-------|-------|----------|
| Motors won't spin | ESC not armed | Reduce throttle to zero first |
| Unstable flight | PID tuning wrong | Increase Kd (damping) |
| RF connection lost | Antenna issue | Check NRF24 antenna orientation |
| IMU reading errors | Sensor not calibrated | Restart with quad on level surface |
| Motor jerks | ESC calibration needed | Run ESC calibration routine |
| Insufficient thrust | Battery low/ESC weak | Check battery voltage & ESC current rating |

## Performance Specs

- **Control Rate**: 100 Hz (10ms loop)
- **IMU Sampling**: 5ms
- **RF Latency**: ~3ms (at 250 kbps)
- **Total Latency**: ~13ms (acceptable for manual flight)
- **Motor Response**: <20ms from command to thrust change
