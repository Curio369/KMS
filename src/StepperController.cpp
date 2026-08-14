#include "StepperController.h"
#include "Config.h"

// Define static member
volatile long StepperController::currentPosition = 0;

StepperController::StepperController() {
    targetPosition = 0;
    moving = false;
    lastStepTime = 0;
}

void StepperController::begin() {
    pinMode(STEPPER_DIR_PIN, OUTPUT);
    pinMode(STEPPER_STEP_PIN, OUTPUT);
    pinMode(STEPPER_EN_PIN, OUTPUT);
    
    digitalWrite(STEPPER_EN_PIN, LOW); // Enable A4988 (active low)
    digitalWrite(STEPPER_DIR_PIN, HIGH);
    digitalWrite(STEPPER_STEP_PIN, LOW);
    
    pinMode(ENCODER_PIN_A, INPUT_PULLUP);
    pinMode(ENCODER_PIN_B, INPUT_PULLUP);
    
    attachInterrupt(digitalPinToInterrupt(ENCODER_PIN_A), isrA, CHANGE);
    attachInterrupt(digitalPinToInterrupt(ENCODER_PIN_B), isrB, CHANGE);
}

void StepperController::moveTo(long target) {
    targetPosition = target;
    moving = true;
}

void StepperController::update() {
    if (!moving) return;
    
    // Calculate error based on encoder feedback
    long error = targetPosition - currentPosition;
    
    if (abs(error) <= 1) { 
        // Reached target within an acceptable tolerance
        moving = false;
        return;
    }
    
    // Simple constant speed control without acceleration for now
    if (micros() - lastStepTime >= stepInterval) {
        lastStepTime = micros();
        bool dir = (error > 0);
        stepMotor(dir);
    }
}

void StepperController::stepMotor(bool direction) {
    digitalWrite(STEPPER_DIR_PIN, direction ? HIGH : LOW);
    digitalWrite(STEPPER_STEP_PIN, HIGH);
    delayMicroseconds(2); // Minimum HIGH pulse width for A4988
    digitalWrite(STEPPER_STEP_PIN, LOW);
}

bool StepperController::isMoving() {
    return moving;
}

long StepperController::getPosition() {
    long pos = 0;
    noInterrupts();
    pos = currentPosition;
    interrupts();
    return pos;
}

void IRAM_ATTR StepperController::isrA() {
    bool a = digitalRead(ENCODER_PIN_A);
    bool b = digitalRead(ENCODER_PIN_B);
    if (a == b) {
        currentPosition++;
    } else {
        currentPosition--;
    }
}

void IRAM_ATTR StepperController::isrB() {
    bool a = digitalRead(ENCODER_PIN_A);
    bool b = digitalRead(ENCODER_PIN_B);
    if (a != b) {
        currentPosition++;
    } else {
        currentPosition--;
    }
}
