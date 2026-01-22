#pragma once

#include <Arduino.h>
#include <seesaw_neopixel.h>
#include "Configuration.h"

class LightStripManager
{
public:
    LightStripManager(uint8_t length = NEOPIXEL_STRIP_LENGTH,
               uint8_t dataPin = NEOPIXEL_STRIP_PIN,
               uint8_t i2cAddr = NEOPIXEL_I2C_ADDR);

    bool begin();
    void setAccelPosition(float percent, bool reverse);
    void setDemoMode(bool enabled)
    {
        if (_demoMode != enabled)
        {
            _demoMode = enabled;
            _needsRefresh = true;
        }
    }
    void update(uint32_t now);

private:
    void rainbowCycleStep();
    void applyAccelPattern(uint32_t now);

    seesaw_NeoPixel _strip;
    uint8_t _i2cAddr;
    bool _demoMode{DEMO_MODE};
    float _accelPercent{0.0f};
    bool _hasAccel{false};
    bool _reverse{false};
    float _smoothedPercent{0.0f};
    uint32_t _lastSmoothMs{0};
    float _lastRenderedPercent{-1.0f};
    bool _needsRefresh{true};
    uint32_t _lastNeoPixelMs{0};
    uint16_t _rainbowStep{0};
};
