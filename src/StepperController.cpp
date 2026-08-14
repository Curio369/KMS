#include "StepperController.h"
#include "Config.h"

StepperController::StepperController()
    : motor(AccelStepper::DRIVER, STEPPER_STEP_PIN, STEPPER_DIR_PIN) {}

void StepperController::begin() {
  // Enable pin (active-low on A4988)
  pinMode(STEPPER_EN_PIN, OUTPUT);
  digitalWrite(STEPPER_EN_PIN, LOW);

  // Configure speed and acceleration from Config.h
  motor.setMaxSpeed(STEPPER_MAX_SPEED);
  motor.setAcceleration(STEPPER_ACCELERATION);
}

void StepperController::moveTo(long targetPosition) {
  motor.moveTo(targetPosition);
}

void StepperController::update() { motor.run(); }

bool StepperController::isMoving() {
  return motor.distanceToGo() != 0;
}

long StepperController::getPosition() { return motor.currentPosition(); }
