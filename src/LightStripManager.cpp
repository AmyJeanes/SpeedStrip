#include "LightStripManager.h"
#include "ColorUtils.h"
#include <math.h>

LightStripManager::LightStripManager(uint8_t length, uint8_t dataPin, uint8_t i2cAddr)
    : _strip(length, dataPin, NEO_GRB + NEO_KHZ800), _i2cAddr(i2cAddr) {}

bool LightStripManager::begin()
{
    return _strip.begin(_i2cAddr);
}

void LightStripManager::setAccelPosition(float percent)
{
    if (percent < 0.0f)
        percent = 0.0f;
    else if (percent > 100.0f)
        percent = 100.0f;

    _accelPercent = percent;
    _hasAccel = true;
}

void LightStripManager::update(uint32_t now)
{
    if (now - _lastNeoPixelMs < NEOPIXEL_UPDATE_INTERVAL_MS)
        return;
    _lastNeoPixelMs = now;
    applyAccelPattern(now);
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

void LightStripManager::applyAccelPattern(uint32_t now)
{
    if (!_hasAccel && _demoMode)
    {
        rainbowCycleStep();
        return;
    }

    float percent = _hasAccel ? _accelPercent : 0.0f;
    if (percent < 0.0f)
        percent = 0.0f;
    else if (percent > 100.0f)
        percent = 100.0f;

    float ratio = percent / 100.0f;
    uint16_t total = _strip.numPixels();
    if (total == 0)
        return;

    float scaled = ratio * (float)total;
    uint16_t fullPixels = (uint16_t)scaled; // floor
    float fractional = scaled - (float)fullPixels;

    // Color gradient shifts from cool (low throttle) to warm (high throttle).
    uint8_t r = (uint8_t)(20.0f + (235.0f * ratio));
    uint8_t g = (uint8_t)(20.0f + (120.0f * (1.0f - ratio)));
    uint8_t b = (uint8_t)(30.0f + (180.0f * (1.0f - ratio)));
    uint32_t color = _strip.Color(r, g, b);

    auto scaledColor = [&](float scale)
    {
        if (scale <= 0.0f)
            return (uint32_t)0;
        if (scale > 1.0f)
            scale = 1.0f;
        float gammaScaled = powf(scale, NEOPIXEL_GAMMA); // perceptual smoothing
        uint8_t sr = (uint8_t)((float)r * gammaScaled);
        uint8_t sg = (uint8_t)((float)g * gammaScaled);
        uint8_t sb = (uint8_t)((float)b * gammaScaled);
        return _strip.Color(sr, sg, sb);
    };

    for (uint16_t i = 0; i < total; ++i)
    {
        if (i < fullPixels)
        {
            _strip.setPixelColor(i, color);
        }
        else if (i == fullPixels && fractional > 0.0f)
        {
            _strip.setPixelColor(i, scaledColor(fractional));
        }
        else
        {
            _strip.setPixelColor(i, 0);
        }
    }

    _strip.show();
}
