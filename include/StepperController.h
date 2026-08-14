#ifndef STEPPER_CONTROLLER_H
#define STEPPER_CONTROLLER_H

#include <AccelStepper.h>
#include <Arduino.h>

class StepperController {
public:
  StepperController();

  void begin();

  // Commands the stepper to move to an absolute step position
  void moveTo(long targetPosition);

  // Must be called in the main loop — drives the AccelStepper state machine
  void update();

  // True while the motor is still traveling to the target
  bool isMoving();

  // Returns the current step-counted position
  long getPosition();

private:
  AccelStepper motor;
};

#endif // STEPPER_CONTROLLER_H
