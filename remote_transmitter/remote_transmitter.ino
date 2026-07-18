/*
 * ARDUINO QUADCOPTER - REMOTE TRANSMITTER FIRMWARE
 * 
 * Microcontroller: Arduino Nano (5V)
 * RF Module: NRF24L01 2.4GHz Transceiver
 * 
 * Input Controls:
 * - Dual-Axis Joystick: Pitch (Y-axis) & Roll (X-axis)
 * - Linear Potentiometer: Throttle control
 * 
 * Communication Protocol:
 * - Transmits control packet every 50ms
 * - SPI interface to NRF24L01
 */

#include <SPI.h>
#include <RF24.h>

// ============ NRF24L01 CONFIGURATION ============
RF24 radio(9, 10);  // CE pin 9, CSN pin 10
const byte address[6] = "QUAD1";
const uint32_t SEND_INTERVAL = 50;  // 50ms = 20Hz

// ============ PIN DEFINITIONS ============
#define JOYSTICK_X A0      // Roll control
#define JOYSTICK_Y A1      // Pitch control
#define POTENTIOMETER A2   // Throttle control

// ============ CONTROL PACKET STRUCTURE ============
struct ControlPacket {
  uint8_t pitch;      // 0-255 (Forward/Backward)
  uint8_t roll;       // 0-255 (Left/Right)
  uint8_t throttle;   // 0-255 (Height)
  uint8_t checksum;   // Simple CRC
};

ControlPacket ctrlPacket;

// ============ JOYSTICK CALIBRATION ============
const int JOYSTICK_CENTER_X = 512;   // ADC center value
const int JOYSTICK_CENTER_Y = 512;
const int JOYSTICK_DEADZONE = 30;    // Deadzone ±30

unsigned long lastSendTime = 0;

// ============ SETUP ============
void setup() {
  Serial.begin(9600);
  
  // Initialize SPI and RF24
  if (!radio.begin()) {
    Serial.println("NRF24 Init Failed!");
    while (1);  // Halt
  }
  
  // RF Configuration
  radio.openWritingPipe(address);
  radio.setPALevel(RF24_PA_MIN);      // Lowest power for safety
  radio.setDataRate(RF24_250KBPS);    // Slower for reliability
  radio.stopListening();              // Transmit only
  
  Serial.println("Remote Transmitter Ready!");
  Serial.println("Waiting for joystick calibration...");
  delay(2000);
  
  // Initialize packet
  ctrlPacket.pitch = 127;
  ctrlPacket.roll = 127;
  ctrlPacket.throttle = 0;
}

// ============ MAIN LOOP ============
void loop() {
  // Read control inputs
  readControls();
  
  // Send at fixed interval
  if (millis() - lastSendTime >= SEND_INTERVAL) {
    sendPacket();
    lastSendTime = millis();
  }
  
  // Debug output every 500ms
  static unsigned long lastDebug = 0;
  if (millis() - lastDebug >= 500) {
    printDebugInfo();
    lastDebug = millis();
  }
  
  delay(10);
}

// ============ READ CONTROL INPUTS ============
void readControls() {
  // Read joystick (10-bit ADC: 0-1023)
  int rawX = analogRead(JOYSTICK_X);
  int rawY = analogRead(JOYSTICK_Y);
  int rawThrottle = analogRead(POTENTIOMETER);
  
  // Apply deadzone to joystick
  if (abs(rawX - JOYSTICK_CENTER_X) < JOYSTICK_DEADZONE) {
    rawX = JOYSTICK_CENTER_X;
  }
  if (abs(rawY - JOYSTICK_CENTER_Y) < JOYSTICK_DEADZONE) {
    rawY = JOYSTICK_CENTER_Y;
  }
  
  // Map from 0-1023 to 0-255
  ctrlPacket.roll = map(rawX, 0, 1023, 0, 255);
  ctrlPacket.pitch = map(rawY, 0, 1023, 0, 255);
  ctrlPacket.throttle = map(rawThrottle, 0, 1023, 0, 255);
  
  // Calculate checksum (simple sum)
  ctrlPacket.checksum = ctrlPacket.pitch + ctrlPacket.roll + ctrlPacket.throttle;
}

// ============ SEND CONTROL PACKET ============
void sendPacket() {
  if (radio.write(&ctrlPacket, sizeof(ctrlPacket))) {
    Serial.print("TX OK | ");
  } else {
    Serial.print("TX FAIL | ");
  }
}

// ============ DEBUG OUTPUT ============
void printDebugInfo() {
  Serial.print("Pitch: ");
  Serial.print(ctrlPacket.pitch);
  Serial.print(" | Roll: ");
  Serial.print(ctrlPacket.roll);
  Serial.print(" | Throttle: ");
  Serial.print(ctrlPacket.throttle);
  Serial.print(" | Checksum: ");
  Serial.println(ctrlPacket.checksum);
}
