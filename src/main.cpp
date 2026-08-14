#include <Arduino.h>
#include "Config.h"
#include "Protocol.h"
#include "StepperController.h"
#include "SolenoidController.h"
#include "BatteryMonitor.h"

StepperController stepper;
SolenoidController solenoid;
BatteryMonitor battery;

SystemState currentState = STATE_IDLE;

void processCommand(String cmd) {
    cmd.trim();
    if (cmd.length() == 0) return;
    
    int separatorIndex = cmd.indexOf(':');
    String command = cmd;
    String param = "";
    
    if (separatorIndex != -1) {
        command = cmd.substring(0, separatorIndex);
        param = cmd.substring(separatorIndex + 1);
    }
    
    if (command == "GOTO") {
        long target = param.toInt();
        stepper.moveTo(target);
        currentState = STATE_MOVING;
        Serial1.println("ACK:GOTO");
        Serial.print("Moving to: "); Serial.println(target);
    } 
    else if (command == "ACTUATE") {
        bool engage = (param.toInt() == 1);
        solenoid.actuate(engage);
        Serial1.println("ACK:ACTUATE");
        Serial.print("Solenoid Actuated: "); Serial.println(engage);
    }
    else if (command == "BATT") {
        int percent = battery.getPercentage();
        Serial1.print("BATT:");
        Serial1.println(percent);
    }
    else if (command == "STATUS") {
        Serial1.print("STATUS:");
        if (currentState == STATE_IDLE) Serial1.println("IDLE");
        else if (currentState == STATE_MOVING) Serial1.println("MOVING");
        else Serial1.println("ERROR");
    }
    else {
        Serial1.print("ERR:UNKNOWN_CMD_");
        Serial1.println(command);
    }
}

void setup() {
    // Initialize debug serial (USB CDC on ESP32-S3)
    Serial.begin(115200);
    
    // Initialize communication with High-Level ESP32
    Serial1.begin(COMM_BAUD_RATE, SERIAL_8N1, COMM_RX_PIN, COMM_TX_PIN);
    
    // Initialize hardware controllers
    stepper.begin();
    solenoid.begin();
    battery.begin();
    
    Serial.println("Low-Level ESP32-S3 KMS Controller Initialized.");
}

void loop() {
    // 1. Process incoming commands from High-Level ESP32
    if (Serial1.available()) {
        String incomingCmd = Serial1.readStringUntil('\n');
        processCommand(incomingCmd);
    }
    
    // 2. Update hardware controllers
    stepper.update();
    solenoid.update();
    
    // 3. State Management
    if (currentState == STATE_MOVING && !stepper.isMoving()) {
        // Just finished moving
        currentState = STATE_IDLE;
        Serial1.println("DONE:GOTO");
        Serial.print("Reached target position: ");
        Serial.println(stepper.getPosition());
    }
}
