#include "SolenoidController.h"
#include "Config.h"

SolenoidController::SolenoidController() {
    isEngaged = false;
    engagedTime = 0;
}

void SolenoidController::begin() {
    pinMode(SOLENOID_RELAY_PIN, OUTPUT);
    digitalWrite(SOLENOID_RELAY_PIN, LOW); // Assumes LOW = relay off
}

void SolenoidController::actuate(bool engage) {
    isEngaged = engage;
    if (engage) {
        digitalWrite(SOLENOID_RELAY_PIN, HIGH);
        engagedTime = millis();
    } else {
        digitalWrite(SOLENOID_RELAY_PIN, LOW);
    }
}

void SolenoidController::update() {
    if (isEngaged && (millis() - engagedTime > maxEngageDuration)) {
        // Safety timeout reached, disengage automatically
        actuate(false);
    }
}
