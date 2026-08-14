#ifndef BATTERY_MONITOR_H
#define BATTERY_MONITOR_H

#include <Arduino.h>

class BatteryMonitor {
public:
  BatteryMonitor();

  void begin();

  // Returns the battery voltage
  float getVoltage();

  // Returns an estimated battery percentage (0-100)
  int getPercentage();
};

#endif // BATTERY_MONITOR_H
