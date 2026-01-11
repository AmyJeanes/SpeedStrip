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
    // Decoded subset of DI_systemStatus (CAN ID 0x118) focusing on DI_accelPedalPos.
    struct DISystemStatusMsg
    {
        float accelPedalPosPercent = 0.0f; // DI_accelPedalPos (32|8@1+ scale 0.4, offset 0)
        uint32_t lastRxMs = 0;             // millis() timestamp when received
    };

    explicit CANManager(uint8_t csPin = PIN_CAN_CS) : _mcp(csPin) {}

    bool begin(uint32_t bitrate = CAN_BAUDRATE)
    {
        if (!_mcp.begin(bitrate))
            return false;

        // Filter for exactly 0x118 (DI_systemStatus) and ignore everything else.
        if (!_mcp.setFilterMask(0, false, 0x7FF) ||
            !_mcp.setFilter(0, false, 0x118) ||
            !_mcp.setFilter(1, false, 0x118))
        {
            return false;
        }

        // Block all messages on the second mask.
        if (!_mcp.setFilterMask(1, false, 0x7FF) ||
            !_mcp.setFilter(2, false, 0x118) ||
            !_mcp.setFilter(3, false, 0x118) ||
            !_mcp.setFilter(4, false, 0x118) ||
            !_mcp.setFilter(5, false, 0x118))
        {
            return false;
        }

        return true;
    }

    void setDebugRaw(bool enabled) { _debugRaw = enabled; }
    void setDebugDecoded(bool enabled) { _debugDecoded = enabled; }

    bool hasNewDISystemStatus() const { return _diSystemStatusNew; }
    DISystemStatusMsg getDISystemStatus(bool clear = true)
    {
        DISystemStatusMsg c = _diSystemStatus;
        if (clear)
            _diSystemStatusNew = false;
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

            // Decode DI_systemStatus (standard ID 0x118, DLC 8)
            if (id == 0x118 && !isRtr && len >= 5)
            {
                // DI_accelPedalPos : 32|8@1+ scale 0.4, offset 0 => byte 4, unsigned
                uint8_t rawAccel = data[4];
                float accelPercent = (float)rawAccel * 0.4f;

                DISystemStatusMsg msg;
                msg.accelPedalPosPercent = accelPercent;
                msg.lastRxMs = millis();
                _diSystemStatus = msg;
                _diSystemStatusNew = true;

                if (_debugDecoded)
                {
                    Serial.print(F("DI_systemStatus: accelPedalPos="));
                    Serial.print(accelPercent, 1);
                    Serial.println(F("%"));
                }
            }
        }
        return any;
    }

private:
    Adafruit_MCP2515 _mcp;
    bool _debugRaw = false;
    bool _debugDecoded = true; // default show decoded message when present
    DISystemStatusMsg _diSystemStatus{};
    bool _diSystemStatusNew = false;
};
