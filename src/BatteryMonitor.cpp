#include "BatteryMonitor.h"
#include "Config.h"

BatteryMonitor::BatteryMonitor() {
}

void BatteryMonitor::begin() {
    pinMode(VBAT_PIN, INPUT);
}

float BatteryMonitor::getVoltage() {
    // Read raw ADC value (12-bit on ESP32, 0-4095)
    int raw = analogRead(VBAT_PIN);
    
    // Convert to voltage (ESP32 ADC reference is roughly 3.3V)
    // Needs calibration for accurate reading
    float pinVoltage = (raw / 4095.0) * 3.3;
    
    // Calculate actual battery voltage using the divider ratio
    float vBat = pinVoltage * VBAT_DIVIDER_RATIO;
    
    return vBat;
}

int BatteryMonitor::getPercentage() {
    float voltage = getVoltage();
    
    // Rough estimate for a 12V Lead Acid or 3S LiPo battery
    // Assuming 3S LiPo: 12.6V max, 9.6V min
    float maxV = 12.6;
    float minV = 9.6;
    
    if (voltage >= maxV) return 100;
    if (voltage <= minV) return 0;
    
    int percent = (int)(((voltage - minV) / (maxV - minV)) * 100);
    return percent;
}
