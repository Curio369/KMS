#ifndef CONFIG_H
#define CONFIG_H

// --- UART Communication ---
#define COMM_RX_PIN 16
#define COMM_TX_PIN 17
#define COMM_BAUD_RATE 115200

// --- Stepper Motor (A4988) ---
#define STEPPER_DIR_PIN 25
#define STEPPER_STEP_PIN 26
#define STEPPER_EN_PIN 27

// --- Encoder ---
#define ENCODER_PIN_A 32
#define ENCODER_PIN_B 33

// --- Solenoid Relay ---
#define SOLENOID_RELAY_PIN 14

// --- Battery Monitor ---
#define VBAT_PIN 34
#define VBAT_DIVIDER_RATIO 2.0 // Adjust based on your resistor divider

#endif // CONFIG_H
