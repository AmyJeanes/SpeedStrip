#include "LightStripManager.h"
#include "ColorUtils.h"

LightStripManager::LightStripManager(uint8_t length, uint8_t dataPin, uint8_t i2cAddr)
    : _strip(length, dataPin, NEO_GRB + NEO_KHZ800), _i2cAddr(i2cAddr) {}

bool LightStripManager::begin()
{
    return _strip.begin(_i2cAddr);
}

void LightStripManager::setIndicators(bool leftActive, bool rightActive)
{
    _leftActive = leftActive;
    _rightActive = rightActive;
}

void LightStripManager::update(uint32_t now)
{
    if (now - _lastNeoPixelMs < NEOPIXEL_UPDATE_INTERVAL_MS)
        return;
    _lastNeoPixelMs = now;
    applyIndicatorPattern(now);
}

void LightStripManager::rainbowCycleStep()
{
    for (uint16_t i = 0; i < _strip.numPixels(); i++)
    {
        _strip.setPixelColor(i, ColorUtils::wheel(_strip, ((i * 256 / _strip.numPixels()) + _rainbowStep) & 0xFF));
    }
    _strip.show();
    _rainbowStep = (_rainbowStep + 1) % (256 * 5);
}

void LightStripManager::applyIndicatorPattern(uint32_t now)
{
    bool leftActive = _leftActive;
    bool rightActive = _rightActive;

    if (_demoMode && !leftActive && !rightActive)
    {
        bool demoLeft = ((now / 1500) & 0x01) == 0;
        leftActive = demoLeft;
        rightActive = !demoLeft;
    }

    if (!leftActive && !rightActive)
    {
        rainbowCycleStep();
        return;
    }

    if (now - _lastFlashMs >= INDICATOR_FLASH_MS)
    {
        _lastFlashMs = now;
        _flashOn = !_flashOn;
    }

    uint32_t amber = _strip.Color(255, 120, 0);
    uint16_t mid = _strip.numPixels() / 2;

    auto fillRange = [&](uint16_t start, uint16_t end, uint32_t color)
    {
        for (uint16_t i = start; i < end; ++i)
        {
            _strip.setPixelColor(i, color);
        }
    };

    if (leftActive && _flashOn)
    {
        fillRange(0, mid, amber);
    }
    else
    {
        fillRange(0, mid, 0);
    }

    if (rightActive && _flashOn)
    {
        fillRange(mid, _strip.numPixels(), amber);
    }
    else
    {
        fillRange(mid, _strip.numPixels(), 0);
    }

    _strip.show();
}
