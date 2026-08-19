#include "BatteryMonitor.h"
#include "Config.h"

BatteryMonitor::BatteryMonitor() {
}

void BatteryMonitor::begin() {
    pinMode(VBAT_PIN, INPUT);
}

float BatteryMonitor::getVoltage() {
    // Read raw ADC value (10-bit on ESP8266, 0-1023)
    int raw = analogRead(VBAT_PIN);
    
    // Convert to voltage (ESP8266 ADC reference is usually 1.0V or scaled to 3.3V on dev boards)
    // Assuming a dev board like NodeMCU/D1 Mini with built-in voltage divider for 3.3V
    float pinVoltage = (raw / 1023.0) * 3.3;
    
    // Calculate actual battery voltage using the external divider ratio
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
