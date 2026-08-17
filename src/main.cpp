#include <Arduino.h>
#include <SoftwareSerial.h>
#include "Config.h"
#include "Protocol.h"
#include "StepperController.h"
#include "SolenoidController.h"
#include "BatteryMonitor.h"

SoftwareSerial commSerial(COMM_RX_PIN, COMM_TX_PIN);

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
        commSerial.println("ACK:GOTO");
        Serial.print("Moving to: "); Serial.println(target);
    } 
    else if (command == "ACTUATE") {
        bool engage = (param.toInt() == 1);
        solenoid.actuate(engage);
        commSerial.println("ACK:ACTUATE");
        Serial.print("Solenoid Actuated: "); Serial.println(engage);
    }
    else if (command == "BATT") {
        int percent = battery.getPercentage();
        commSerial.print("BATT:");
        commSerial.println(percent);
    }
    else if (command == "STATUS") {
        commSerial.print("STATUS:");
        if (currentState == STATE_IDLE) commSerial.println("IDLE");
        else if (currentState == STATE_MOVING) commSerial.println("MOVING");
        else commSerial.println("ERROR");
    }
    else {
        commSerial.print("ERR:UNKNOWN_CMD_");
        commSerial.println(command);
    }
}

void setup() {
    // Initialize debug serial
    Serial.begin(115200);
    
    // Initialize communication with High-Level ESP
    commSerial.begin(COMM_BAUD_RATE);
    
    // Initialize hardware controllers
    stepper.begin();
    solenoid.begin();
    battery.begin();
    
    Serial.println("Low-Level ESP8266 KMS Controller Initialized.");
}

void loop() {
    // 1. Process incoming commands from High-Level ESP32
    if (commSerial.available()) {
        String incomingCmd = commSerial.readStringUntil('\n');
        processCommand(incomingCmd);
    }
    
    // 2. Update hardware controllers
    stepper.update();
    solenoid.update();
    
    // 3. State Management
    if (currentState == STATE_MOVING && !stepper.isMoving()) {
        // Just finished moving
        currentState = STATE_IDLE;
        commSerial.println("DONE:GOTO");
        Serial.print("Reached target position: ");
        Serial.println(stepper.getPosition());
    }
}
