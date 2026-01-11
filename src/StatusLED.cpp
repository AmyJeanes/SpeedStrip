#include "StatusLED.h"
#include <Adafruit_NeoPixel.h>

static Adafruit_NeoPixel *g_px = nullptr;

StatusLED::StatusLED(uint8_t powerPin, uint8_t dataPin, uint8_t brightness)
    : _powerPin(powerPin), _dataPin(dataPin), _brightness(brightness) {}

bool StatusLED::begin()
{
    pinMode(_powerPin, OUTPUT);
    digitalWrite(_powerPin, HIGH);

    if (!g_px)
    {
        g_px = new Adafruit_NeoPixel(1, _dataPin, NEO_GRB + NEO_KHZ800);
    }
    g_px->begin();
    g_px->setBrightness(_brightness);
    g_px->show();
    _state = State::Off;
    return true;
}

void StatusLED::setState(State s)
{
    _state = s;
    switch (s)
    {
    case State::Off:
        _showColor(0, 0, 0);
        break;
    case State::Waiting:
        _showColor(255, 180, 10);
        break;
    case State::Ok:
        _showColor(0, 255, 0);
        break;
    case State::Error:
        _showColor(255, 0, 0);
        break;
    }
}

void StatusLED::setColor(uint8_t r, uint8_t g, uint8_t b)
{
    if (r == 0 && g == 0 && b == 0)
    {
        _state = State::Off;
    }
    _showColor(r, g, b);
}

void StatusLED::_showColor(uint8_t r, uint8_t g, uint8_t b)
{
    if (!g_px)
        return;
    g_px->setPixelColor(0, g_px->Color(r, g, b));
    g_px->show();
}

void StatusLED::update()
{
    if (_state != State::Waiting)
        return;
    uint32_t now = millis();
    if (now - _lastAnimMs < 25)
        return;
    _lastAnimMs = now;

    _animPhase = (_animPhase + 1) % 48;
    uint8_t tri;
    if (_animPhase < 24)
    {
        tri = _animPhase * (255 / 23);
    }
    else
    {
        tri = (47 - _animPhase) * (255 / 23);
    }
    uint8_t r = (uint16_t)255 * tri / 255;
    uint8_t g = (uint16_t)180 * tri / 255;
    uint8_t b = (uint16_t)10 * tri / 255;
    _showColor(r, g, b);
}
