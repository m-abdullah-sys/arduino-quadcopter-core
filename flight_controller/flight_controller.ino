/*
 * ARDUINO QUADCOPTER - FLIGHT CONTROLLER FIRMWARE
 * 
 * Microcontroller: Arduino Nano 33 BLE Sense (ARM Cortex-M4)
 * Operating Voltage: 3.3V
 * Clock Speed: 64 MHz
 * 
 * Built-in Sensors:
 * - LSM9DS1 IMU (9-axis: Accel, Gyro, Magnetometer)
 *   Interface: Internal I2C bus
 * 
 * RF Module: NRF24L01 (SPI)
 * Motor Control: 4x ESC PWM outputs
 * 
 * Core Functions:
 * - Receive wireless control packets
 * - Read IMU data (gyroscope for attitude)
 * - PID stabilization control loop
 * - Motor mixing algorithm
 * - PWM output to ESCs
 */

#include <SPI.h>
#include <RF24.h>
#include <Arduino_LSM9DS1.h>

// ============ NRF24L01 CONFIGURATION ============
RF24 radio(7, 8);  // CE pin 7, CSN pin 8
const byte address[6] = "QUAD1";

// ============ PIN DEFINITIONS ============
// ESC PWM Outputs (PWM capable pins on Nano 33 BLE Sense)
#define ESC_FRONT_LEFT 3    // Motor FL
#define ESC_FRONT_RIGHT 5   // Motor FR
#define ESC_REAR_LEFT 6     // Motor RL
#define ESC_REAR_RIGHT 9    // Motor RR

// ============ CONTROL PACKET STRUCTURE (MUST MATCH REMOTE!) ============
struct ControlPacket {
  uint8_t pitch;      // 0-255 (Forward/Backward)
  uint8_t roll;       // 0-255 (Left/Right)
  uint8_t throttle;   // 0-255 (Height)
  uint8_t checksum;   // Simple CRC
};

ControlPacket ctrlPacket;

// ============ IMU DATA STRUCTURE ============
struct IMUData {
  float accelX, accelY, accelZ;   // m/s²
  float gyroX, gyroY, gyroZ;      // °/s
  float magX, magY, magZ;         // Gauss
};

IMUData imuData;

// ============ PID CONTROLLER STRUCTURE ============
struct PIDController {
  float kp;           // Proportional gain
  float ki;           // Integral gain
  float kd;           // Derivative gain
  float prevError;    // Previous error
  float integral;     // Accumulated integral
  float output;       // PID output
};

PIDController pidRoll, pidPitch;

// ============ MOTOR CONTROL ============
uint16_t motorThrottle = 1000;  // Base throttle (µs)
uint16_t motorFL, motorFR, motorRL, motorRR;  // Individual motor commands

// ============ TIMING ============
const uint32_t CONTROL_LOOP_INTERVAL = 10;  // 10ms = 100Hz control rate
const uint32_t IMU_SAMPLE_INTERVAL = 5;     // 5ms IMU sampling
const uint32_t COMMAND_TIMEOUT = 500;       // 500ms failsafe timeout

unsigned long lastControlTime = 0;
unsigned long lastIMUTime = 0;
unsigned long lastCommandTime = 0;

// ============ PID TUNING PARAMETERS ============
#define PID_ROLL_KP 1.5
#define PID_ROLL_KI 0.05
#define PID_ROLL_KD 0.8

#define PID_PITCH_KP 1.5
#define PID_PITCH_KI 0.05
#define PID_PITCH_KD 0.8

// ============ IMU CALIBRATION OFFSETS ============
float gyroOffsetX = 0, gyroOffsetY = 0, gyroOffsetZ = 0;
float accelOffsetX = 0, accelOffsetY = 0, accelOffsetZ = 0;

// ============ ATTITUDE STATE ============
float rollAngle = 0;    // Current roll (°)
float pitchAngle = 0;   // Current pitch (°)
float rollTarget = 0;   // Target roll
float pitchTarget = 0;  // Target pitch

// ============ SETUP ============
void setup() {
  Serial.begin(115200);
  delay(1000);
  
  Serial.println("\n=== QUADCOPTER FLIGHT CONTROLLER ===");
  Serial.println("Arduino Nano 33 BLE Sense");
  
  // Initialize pins
  pinMode(ESC_FRONT_LEFT, OUTPUT);
  pinMode(ESC_FRONT_RIGHT, OUTPUT);
  pinMode(ESC_REAR_LEFT, OUTPUT);
  pinMode(ESC_REAR_RIGHT, OUTPUT);
  
  // Initialize ESCs to minimum (1000µs)
  analogWrite(ESC_FRONT_LEFT, 0);
  analogWrite(ESC_FRONT_RIGHT, 0);
  analogWrite(ESC_REAR_LEFT, 0);
  analogWrite(ESC_REAR_RIGHT, 0);
  
  // Initialize RF24
  Serial.print("Initializing RF24... ");
  if (!radio.begin()) {
    Serial.println("FAILED!");
    while (1);
  }
  Serial.println("OK");
  
  radio.openReadingPipe(1, address);
  radio.setPALevel(RF24_PA_MIN);
  radio.setDataRate(RF24_250KBPS);
  radio.startListening();
  
  // Initialize IMU
  Serial.print("Initializing IMU... ");
  if (!IMU.begin()) {
    Serial.println("FAILED!");
    while (1);
  }
  Serial.println("OK");
  
  // Calibrate IMU (5 second static calibration)
  Serial.println("Calibrating IMU (keep quadcopter still)...");
  calibrateIMU();
  Serial.println("Calibration complete!");
  
  // Initialize PID controllers
  pidRoll.kp = PID_ROLL_KP;
  pidRoll.ki = PID_ROLL_KI;
  pidRoll.kd = PID_ROLL_KD;
  pidRoll.prevError = 0;
  pidRoll.integral = 0;
  
  pidPitch.kp = PID_PITCH_KP;
  pidPitch.ki = PID_PITCH_KI;
  pidPitch.kd = PID_PITCH_KD;
  pidPitch.prevError = 0;
  pidPitch.integral = 0;
  
  Serial.println("Flight Controller Ready!");
  Serial.println("Waiting for control signal...");
  
  lastControlTime = millis();
  lastIMUTime = millis();
  lastCommandTime = millis();
}

// ============ MAIN LOOP ============
void loop() {
  unsigned long now = millis();
  
  // Receive control packets
  receiveControl();
  
  // Read IMU data at faster rate
  if (now - lastIMUTime >= IMU_SAMPLE_INTERVAL) {
    readIMU();
    updateAttitude();
    lastIMUTime = now;
  }
  
  // Run control loop at 100Hz
  if (now - lastControlTime >= CONTROL_LOOP_INTERVAL) {
    controlLoop();
    lastControlTime = now;
  }
  
  // Check for command timeout (failsafe)
  if (now - lastCommandTime > COMMAND_TIMEOUT) {
    emergencyStop();
  }
}

// ============ RECEIVE CONTROL PACKETS ============
void receiveControl() {
  if (radio.available()) {
    byte payload[sizeof(ControlPacket)];
    radio.read(&payload, sizeof(ControlPacket));
    
    // Verify checksum
    ControlPacket temp;
    memcpy(&temp, payload, sizeof(ControlPacket));
    
    uint8_t calcChecksum = temp.pitch + temp.roll + temp.throttle;
    if (calcChecksum == temp.checksum) {
      ctrlPacket = temp;
      lastCommandTime = millis();
      
      // Debug output every 200ms
      static unsigned long lastDebug = 0;
      if (millis() - lastDebug >= 200) {
        Serial.print("RX OK | Pitch:");
        Serial.print(ctrlPacket.pitch);
        Serial.print(" Roll:");
        Serial.print(ctrlPacket.roll);
        Serial.print(" Throttle:");
        Serial.println(ctrlPacket.throttle);
        lastDebug = millis();
      }
    } else {
      Serial.println("RX CHECKSUM ERROR!");
    }
  }
}

// ============ READ IMU DATA ============
void readIMU() {
  float ax, ay, az;
  float gx, gy, gz;
  
  // Read accelerometer
  if (IMU.accelerationAvailable()) {
    IMU.readAcceleration(ax, ay, az);
    imuData.accelX = ax - accelOffsetX;
    imuData.accelY = ay - accelOffsetY;
    imuData.accelZ = az - accelOffsetZ;
  }
  
  // Read gyroscope
  if (IMU.gyroscopeAvailable()) {
    IMU.readGyroscope(gx, gy, gz);
    imuData.gyroX = gx - gyroOffsetX;
    imuData.gyroY = gy - gyroOffsetY;
    imuData.gyroZ = gz - gyroOffsetZ;
  }
  
  // Read magnetometer
  if (IMU.magneticFieldAvailable()) {
    IMU.readMagneticField(imuData.magX, imuData.magY, imuData.magZ);
  }
}

// ============ UPDATE ATTITUDE (INTEGRATION) ============
void updateAttitude() {
  // Simple integration of gyroscope for attitude estimation
  // Time delta: ~5ms = 0.005s
  const float dt = 0.005;
  
  // Integrate gyro rates to get angles
  rollAngle += imuData.gyroX * dt;
  pitchAngle += imuData.gyroY * dt;
  
  // Constrain angles to ±180°
  if (rollAngle > 180) rollAngle -= 360;
  if (rollAngle < -180) rollAngle += 360;
  if (pitchAngle > 180) pitchAngle -= 360;
  if (pitchAngle < -180) pitchAngle += 360;
}

// ============ CALIBRATE IMU ============
void calibrateIMU() {
  float ax_sum = 0, ay_sum = 0, az_sum = 0;
  float gx_sum = 0, gy_sum = 0, gz_sum = 0;
  
  const int samples = 100;
  
  for (int i = 0; i < samples; i++) {
    float ax, ay, az, gx, gy, gz;
    
    if (IMU.accelerationAvailable()) {
      IMU.readAcceleration(ax, ay, az);
      ax_sum += ax;
      ay_sum += ay;
      az_sum += az;
    }
    
    if (IMU.gyroscopeAvailable()) {
      IMU.readGyroscope(gx, gy, gz);
      gx_sum += gx;
      gy_sum += gy;
      gz_sum += gz;
    }
    
    delay(50);
  }
  
  accelOffsetX = ax_sum / samples;
  accelOffsetY = ay_sum / samples;
  accelOffsetZ = az_sum / samples;
  
  gyroOffsetX = gx_sum / samples;
  gyroOffsetY = gy_sum / samples;
  gyroOffsetZ = gz_sum / samples;
  
  Serial.println("IMU Offsets Calculated:");
  Serial.print("Gyro: ");
  Serial.print(gyroOffsetX); Serial.print(" ");
  Serial.print(gyroOffsetY); Serial.print(" ");
  Serial.println(gyroOffsetZ);
}

// ============ CONTROL LOOP (100Hz) ============
void controlLoop() {
  // Convert received control values (0-255) to target angles (±45°)
  rollTarget = map(ctrlPacket.roll, 0, 255, -45, 45);
  pitchTarget = map(ctrlPacket.pitch, 0, 255, -45, 45);
  motorThrottle = map(ctrlPacket.throttle, 0, 255, 1000, 2000);  // 1000-2000µs
  
  // PID control for Roll
  float rollError = rollTarget - rollAngle;
  pidRoll.integral += rollError * 0.01;  // dt = 10ms = 0.01s
  pidRoll.integral = constrain(pidRoll.integral, -50, 50);  // Anti-windup
  float rollPIDOutput = (pidRoll.kp * rollError) +
                        (pidRoll.ki * pidRoll.integral) +
                        (pidRoll.kd * (rollError - pidRoll.prevError));
  pidRoll.prevError = rollError;
  pidRoll.output = constrain(rollPIDOutput, -500, 500);
  
  // PID control for Pitch
  float pitchError = pitchTarget - pitchAngle;
  pidPitch.integral += pitchError * 0.01;
  pidPitch.integral = constrain(pidPitch.integral, -50, 50);
  float pitchPIDOutput = (pidPitch.kp * pitchError) +
                         (pidPitch.ki * pidPitch.integral) +
                         (pidPitch.kd * (pitchError - pidPitch.prevError));
  pidPitch.prevError = pitchError;
  pidPitch.output = constrain(pitchPIDOutput, -500, 500);
  
  // Motor mixing algorithm
  // Quadcopter configuration (X-mode):
  //   FL(1) --- FR(2)
  //    \       /
  //     \     /
  //      \   /
  //      /   \
  //     /     \
  //    /       \
  //   RL(4) --- RR(3)
  
  int baseThrottle = motorThrottle;
  
  // Roll correction: +Roll = increase right, decrease left
  // Pitch correction: +Pitch = increase front, decrease rear
  
  motorFL = baseThrottle + pidPitch.output - pidRoll.output;  // Front-Left
  motorFR = baseThrottle + pidPitch.output + pidRoll.output;  // Front-Right
  motorRL = baseThrottle - pidPitch.output - pidRoll.output;  // Rear-Left
  motorRR = baseThrottle - pidPitch.output + pidRoll.output;  // Rear-Right
  
  // Constrain motor commands to safe range (1000-2000µs)
  motorFL = constrain(motorFL, 1000, 2000);
  motorFR = constrain(motorFR, 1000, 2000);
  motorRL = constrain(motorRL, 1000, 2000);
  motorRR = constrain(motorRR, 1000, 2000);
  
  // Convert PWM microseconds to 0-255 analog values
  // ESCs typically expect 1000µs = 0%, 2000µs = 100%
  // Arduino PWM: 0-255 represents 0-100%
  // Map: 1000µs → 0, 2000µs → 255
  analogWrite(ESC_FRONT_LEFT, map(motorFL, 1000, 2000, 0, 255));
  analogWrite(ESC_FRONT_RIGHT, map(motorFR, 1000, 2000, 0, 255));
  analogWrite(ESC_REAR_LEFT, map(motorRL, 1000, 2000, 0, 255));
  analogWrite(ESC_REAR_RIGHT, map(motorRR, 1000, 2000, 0, 255));
  
  // Debug output every 500ms
  static unsigned long lastDebug = 0;
  if (millis() - lastDebug >= 500) {
    Serial.print("Roll:");
    Serial.print(rollAngle);
    Serial.print("° Pitch:");
    Serial.print(pitchAngle);
    Serial.print("° Throttle:");
    Serial.println(motorThrottle);
    
    lastDebug = millis();
  }
}

// ============ EMERGENCY STOP ============
void emergencyStop() {
  Serial.println("FAILSAFE: No command signal!");
  analogWrite(ESC_FRONT_LEFT, 0);
  analogWrite(ESC_FRONT_RIGHT, 0);
  analogWrite(ESC_REAR_LEFT, 0);
  analogWrite(ESC_REAR_RIGHT, 0);
  
  // Halt execution
  while (1);
}
