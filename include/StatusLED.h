#pragma once
#include <Arduino.h>
#include <stdint.h>

class StatusLED
{
public:
    enum class State : uint8_t
    {
        Off,
        Waiting,
        Ok,
        Error
    };

    StatusLED(uint8_t powerPin = 20, uint8_t dataPin = 21, uint8_t brightness = 32);

    bool begin();
    void setState(State s);
    State state() const { return _state; }

    // Non-blocking update; call regularly if wanting animation on "Waiting" state.
    void update();

    // Direct color override (R,G,B 0-255). Also sets state to Off if all zero.
    void setColor(uint8_t r, uint8_t g, uint8_t b);

private:
    void _showColor(uint8_t r, uint8_t g, uint8_t b);

    uint8_t _powerPin;
    uint8_t _dataPin;
    uint8_t _brightness;
    State _state{State::Off};
    uint32_t _lastAnimMs{0};
    uint8_t _animPhase{0};
};
