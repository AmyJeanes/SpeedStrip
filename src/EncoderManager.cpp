#include "EncoderManager.h"
#include "ColorUtils.h"

EncoderManager::EncoderManager(uint8_t switchPin, uint8_t pixelPin)
    : _pixel(1, pixelPin, NEO_GRB + NEO_KHZ800), _switchPin(switchPin), _pixelPin(pixelPin) {}

bool EncoderManager::begin(uint8_t address, uint8_t brightness)
{
    if (!_ss.begin(address) || !_pixel.begin(address))
    {
        return false;
    }
    uint32_t version = ((_ss.getVersion() >> 16) & 0xFFFF);
    if (version != 4991)
    {
        Serial.print("Warning: unexpected encoder product ID: ");
        Serial.println(version);
    }
    _pixel.setBrightness(brightness);
    _pixel.show();
    _ss.pinMode(_switchPin, INPUT_PULLUP);
    _position = _ss.getEncoderPosition();
    _ss.setGPIOInterrupts((uint32_t)1 << _switchPin, 1);
    _ss.enableEncoderInterrupt();
    return true;
}

void EncoderManager::update()
{
    bool currentPressed = !_ss.digitalRead(_switchPin);
    _justPressed = (currentPressed && !_pressed);
    _justReleased = (!currentPressed && _pressed);
    _pressed = currentPressed;

    int32_t newPos = _ss.getEncoderPosition();
    bool moved = newPos != _position;
    if (moved)
    {
        _position = newPos;
        Serial.print("Encoder: ");
        Serial.println(_position);
    }

    bool needShow = false;
    if (_pressed)
    {
        _pixel.setPixelColor(0, _pixel.Color(255, 0, 0));
        needShow = true;
    }
    else if (moved || _justReleased)
    {
        _pixel.setPixelColor(0, ColorUtils::wheel(_pixel, _position & 0xFF));
        needShow = true;
    }
    if (needShow)
    {
        _pixel.show();
    }

    if (_justPressed)
    {
        Serial.println("Encoder button pressed!");
    }
}
