#include "LightStripManager.h"
#include "ColorUtils.h"
#include <math.h>

LightStripManager::LightStripManager(uint8_t length, uint8_t dataPin, uint8_t i2cAddr)
    : _strip(length, dataPin, NEO_GRB + NEO_KHZ800), _i2cAddr(i2cAddr) {}

bool LightStripManager::begin()
{
    return _strip.begin(_i2cAddr);
}

void LightStripManager::setAccelPosition(float percent, bool reverse)
{
    if (percent < 0.0f)
        percent = 0.0f;
    else if (percent > 100.0f)
        percent = 100.0f;

    if (fabsf(percent - _accelPercent) > 0.01f)
        _needsRefresh = true;
    _accelPercent = percent;
    if (reverse != _reverse)
        _needsRefresh = true;
    _reverse = reverse;
    _hasAccel = true;
}

void LightStripManager::update(uint32_t now)
{
    if (now - _lastNeoPixelMs < NEOPIXEL_UPDATE_INTERVAL_MS)
        return;
    _lastNeoPixelMs = now;
    // Exponential smoothing to make motion feel less stepped.
    float target = _hasAccel ? _accelPercent : 0.0f;
    if (_lastSmoothMs == 0)
    {
        _lastSmoothMs = now;
        _smoothedPercent = target;
    }
    else
    {
        uint32_t dtMs = now - _lastSmoothMs;
        _lastSmoothMs = now;
        if (STRIP_SMOOTH_TIME_MS == 0)
        {
            _smoothedPercent = target;
        }
        else
        {
            float dt = (float)dtMs;
            float tau = (float)STRIP_SMOOTH_TIME_MS;
            float alpha = 1.0f - expf(-dt / tau);
            _smoothedPercent = _smoothedPercent + (target - _smoothedPercent) * alpha;
        }
    }
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

    float percent = _hasAccel ? _smoothedPercent : 0.0f;
    if (percent < 0.0f)
        percent = 0.0f;
    else if (percent > 100.0f)
        percent = 100.0f;

    if (!_demoMode && !_needsRefresh && fabsf(percent - _lastRenderedPercent) < 0.01f)
        return; // No visible change; avoid re-sending the frame over I2C

    float ratio = percent / 100.0f;
    uint16_t total = _strip.numPixels();
    if (total == 0)
        return;

    float virtualLength = NEOPIXEL_VIRTUAL_LENGTH;
    if (virtualLength <= 0.0f)
        virtualLength = (float)total;

    float litLength = ratio * virtualLength; // how much of the virtual bar is "on"
    float segmentSize = virtualLength / (float)total; // virtual units per physical pixel

    // Color gradient shifts from cool (low throttle) to warm (high throttle).
    uint8_t r = (uint8_t)(20.0f + (235.0f * ratio));
    uint8_t g = (uint8_t)(20.0f + (120.0f * (1.0f - ratio)));
    uint8_t b = (uint8_t)(30.0f + (180.0f * (1.0f - ratio)));

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
        uint16_t pixelIndex = _reverse ? (uint16_t)(total - 1 - i) : i;
        float start = segmentSize * (float)pixelIndex;
        float end = start + segmentSize;

        float covered = 0.0f;
        if (litLength > start)
        {
            float clippedEnd = (litLength < end) ? litLength : end;
            covered = clippedEnd - start;
            if (covered < 0.0f)
                covered = 0.0f;
            if (covered > segmentSize)
                covered = segmentSize;
        }

        float coverageRatio = covered / segmentSize; // 0..1 fraction of this pixel lit
        _strip.setPixelColor(i, scaledColor(coverageRatio));
    }

    _strip.show();

    _lastRenderedPercent = percent;
    _needsRefresh = false;
}
