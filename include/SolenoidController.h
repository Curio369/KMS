#ifndef SOLENOID_CONTROLLER_H
#define SOLENOID_CONTROLLER_H

#include <Arduino.h>

class SolenoidController {
public:
  SolenoidController();

  void begin();

  // Engages or disengages the solenoid relay
  void actuate(bool engage);

  // Checks the safety timeout
  void update();

private:
  bool isEngaged;
  unsigned long engagedTime;
  const unsigned long maxEngageDuration = 5000; // 5 seconds max to prevent burning out the coil
};

#endif // SOLENOID_CONTROLLER_H
