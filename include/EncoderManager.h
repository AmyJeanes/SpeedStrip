// Handles rotary encoder (seesaw) position, button, and onboard NeoPixel.
#pragma once

#include <Arduino.h>
#include "Adafruit_seesaw.h"
#include "seesaw_neopixel.h"

class EncoderManager
{
public:
    EncoderManager(uint8_t switchPin, uint8_t pixelPin);
    bool begin(uint8_t address, uint8_t brightness);
    void update();

    int32_t position() const { return _position; }
    bool buttonPressed() const { return _pressed; }
    bool buttonJustPressed() const { return _justPressed; }
    bool buttonJustReleased() const { return _justReleased; }

private:
    Adafruit_seesaw _ss;
    seesaw_NeoPixel _pixel; // constructed with (n, pin, type)
    uint8_t _switchPin;
    uint8_t _pixelPin;
    int32_t _position = 0;
    bool _pressed = false;
    bool _justPressed = false;
    bool _justReleased = false;
};
