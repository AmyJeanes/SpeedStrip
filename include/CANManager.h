// Simple wrapper around Adafruit_MCP2515 for reception
#pragma once
#include <Adafruit_MCP2515.h>
#include <Arduino.h>
#include "Configuration.h"

// Provide a sensible default for the MCP2515 chip-select pin if the board core
// does not define PIN_CAN_CS (Feather CAN boards usually do).
#ifndef PIN_CAN_CS
#define PIN_CAN_CS 9
#endif

class CANManager
{
public:
    // Decoded subset of DI_torque (CAN ID 0x108) focusing on torque + axle speed.
    struct DITorqueMsg
    {
        float torqueCommandNm = 0.0f; // DI_torqueCommand (12|13@1- scale 2, offset 0)
        float axleSpeedRpm = 0.0f;    // DI_axleSpeed (40|16@1- scale 0.1, offset 0)
        float powerW = 0.0f;          // Derived power (torque * omega)
        float powerPercent = 0.0f;    // 0..100 mapped from powerW magnitude
        bool reversing = false;       // True when power is negative (regen)
        uint32_t lastRxMs = 0;        // millis() timestamp when received
    };

    explicit CANManager(uint8_t csPin = PIN_CAN_CS) : _mcp(csPin) {}

    bool begin(uint32_t bitrate = CAN_BAUDRATE)
    {
        if (!_mcp.begin(bitrate))
            return false;

        // Filter for exactly 0x108 (DI_torque) and ignore everything else.
        if (!_mcp.setFilterMask(0, false, 0x7FF) ||
            !_mcp.setFilter(0, false, 0x108) ||
            !_mcp.setFilter(1, false, 0x108))
        {
            return false;
        }

        // Block all messages on the second mask.
        if (!_mcp.setFilterMask(1, false, 0x7FF) ||
            !_mcp.setFilter(2, false, 0x7FF) ||
            !_mcp.setFilter(3, false, 0x7FF) ||
            !_mcp.setFilter(4, false, 0x7FF) ||
            !_mcp.setFilter(5, false, 0x7FF))
        {
            return false;
        }

        return true;
    }

    void setDebugRaw(bool enabled) { _debugRaw = enabled; }
    void setDebugDecoded(bool enabled) { _debugDecoded = enabled; }

    bool hasNewDITorque() const { return _diTorqueNew; }
    DITorqueMsg getDITorque(bool clear = true)
    {
        DITorqueMsg c = _diTorque;
        if (clear)
            _diTorqueNew = false;
        return c;
    }

    // Poll and drain all pending frames; returns true if at least one processed.
    bool poll()
    {
        bool any = false;
        while (true)
        {
            int packetSize = _mcp.parsePacket();
            if (!packetSize)
                break;
            any = true;

            uint32_t id = _mcp.packetId();
            bool isRtr = _mcp.packetRtr();
            uint8_t data[8] = {0};
            int len = 0;
            if (!isRtr)
            {
                while (_mcp.available() && len < 8)
                {
                    data[len++] = _mcp.read();
                }
            }

            if (_debugRaw)
            {
                Serial.print(F("CAN: id=0x"));
                Serial.print(id, HEX);
                if (_mcp.packetExtended())
                    Serial.print(F(" ext"));
                if (isRtr)
                    Serial.print(F(" RTR"));
                Serial.print(F(" len="));
                Serial.print(packetSize);
                Serial.print(F(" data="));
                if (isRtr)
                {
                    Serial.print(F("<RTR>"));
                }
                else
                {
                    for (int i = 0; i < len; ++i)
                    {
                        if (data[i] < 0x10)
                            Serial.print('0');
                        Serial.print(data[i], HEX);
                        Serial.print(' ');
                    }
                }
                Serial.println();
            }

            // Decode DI_torque (standard ID 0x108, DLC 8)
            if (id == 0x108 && !isRtr && len >= 7)
            {
                // DI_torqueCommand : 12|13@1- scale 2, offset 0
                uint16_t rawTorqueU = ((uint16_t)(data[1] >> 4) & 0x0F) |
                                      ((uint16_t)data[2] << 4) |
                                      ((uint16_t)(data[3] & 0x01) << 12);
                int16_t rawTorque = (int16_t)signExtend(rawTorqueU, 13);
                float torqueNm = (float)rawTorque * 2.0f;
                if (torqueNm == -4096.0f)
                    continue; // SNA

                // DI_axleSpeed : 40|16@1- scale 0.1, offset 0
                int16_t rawRpm = (int16_t)((uint16_t)data[5] | ((uint16_t)data[6] << 8));
                if (rawRpm == (int16_t)0x8000)
                    continue; // SNA
                float axleRpm = (float)rawRpm * 0.1f;

                const float rpmToRad = 0.10471976f; // 2*pi/60
                float omega = axleRpm * rpmToRad;
                float powerW = torqueNm * omega;

                bool reversing = powerW < 0.0f;
                float powerForDisplay = reversing ? -powerW : powerW;

                float powerPercent = 0.0f;
                if (powerForDisplay >= POWER_MIN_W && POWER_MAX_W > POWER_MIN_W)
                {
                    float clamped = powerForDisplay;
                    if (clamped > POWER_MAX_W)
                        clamped = POWER_MAX_W;
                    powerPercent = ((clamped - POWER_MIN_W) / (POWER_MAX_W - POWER_MIN_W)) * 100.0f;
                }

                uint32_t nowMs = millis();
                DITorqueMsg msg;
                msg.torqueCommandNm = torqueNm;
                msg.axleSpeedRpm = axleRpm;
                msg.powerW = powerW;
                msg.powerPercent = powerPercent;
                msg.reversing = reversing;
                msg.lastRxMs = nowMs;
                _diTorque = msg;
                _diTorqueNew = true;

                if (_debugDecoded)
                {
                    Serial.print(F("DI_torque: torqueCmd="));
                    Serial.print(torqueNm, 0);
                    Serial.print(F("Nm rpm="));
                    Serial.print(axleRpm, 0);
                    Serial.print(F(" power="));
                    Serial.print(powerW, 0);
                    Serial.print(F("W ("));
                    Serial.print(powerPercent, 0);
                    Serial.println(F("%)"));
                }
            }
        }
        return any;
    }

private:
    static int32_t signExtend(uint32_t value, uint8_t bits)
    {
        uint32_t mask = 1u << (bits - 1);
        if (value & mask)
            value |= ~((1u << bits) - 1u);
        return (int32_t)value;
    }

    Adafruit_MCP2515 _mcp;
    bool _debugRaw = false;
    bool _debugDecoded = true; // default show decoded message when present
    DITorqueMsg _diTorque{};
    bool _diTorqueNew = false;
};
