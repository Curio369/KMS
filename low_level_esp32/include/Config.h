#ifndef CONFIG_H
#define CONFIG_H

// --- UART Communication (SoftwareSerial) ---
#define COMM_RX_PIN 4  // D2
#define COMM_TX_PIN 5  // D1
#define COMM_BAUD_RATE 9600 // Recommend lower baud for SoftwareSerial

// --- Stepper Motor (A4988) ---
#define STEPPER_STEP_PIN 12 // D6
#define STEPPER_DIR_PIN 13  // D7
#define STEPPER_EN_PIN 14   // D5
#define STEPPER_MAX_SPEED 1000.0   // steps/sec
#define STEPPER_ACCELERATION 500.0 // steps/sec^2

// --- Solenoid Relay ---
#define SOLENOID_RELAY_PIN 16 // D0

// --- Battery Monitor ---
#define VBAT_PIN A0
#define VBAT_DIVIDER_RATIO 2.0 // Adjust based on your resistor divider

#endif // CONFIG_H
