#ifndef STEPPER_CONTROLLER_H
#define STEPPER_CONTROLLER_H

#include <Arduino.h>

class StepperController {
public:
  StepperController();

  void begin();

  // Commands the stepper to move to a target encoder position
  void moveTo(long targetPosition);

  // Must be called in the main loop to keep the motor moving
  void update();

  // Checks if the motor has reached its target
  bool isMoving();

  // Returns current encoder position
  long getPosition();

  // Interrupt Service Routines for the encoder
  static void IRAM_ATTR isrA();
  static void IRAM_ATTR isrB();

private:
  void stepMotor(bool direction);

  static volatile long currentPosition;
  long targetPosition;
  bool moving;

  unsigned long lastStepTime;
  const unsigned long stepInterval = 1000; // Microseconds between steps (speed)
};

#endif // STEPPER_CONTROLLER_H
