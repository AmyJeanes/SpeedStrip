// Simple wrapper around Adafruit_MCP2515 for reception (and future transmit)
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
    // Enum for VCFRONT_indicatorRequest
    enum class IndicatorReq : uint8_t
    {
        Off = 0,        // TURN_SIGNAL_OFF
        ActiveLow = 1,  // TURN_SIGNAL_ACTIVE_LOW
        ActiveHigh = 2, // TURN_SIGNAL_ACTIVE_HIGH
        Unknown = 3     // SNA / invalid (value 3 not defined in VAL table)
    };

    // Enum for SCCM_highBeamStalkStatus
    enum class HighBeamStalkStatus : uint8_t
    {
        Idle = 0,
        Pull = 1,
        Push = 2,
        SNA = 3
    };

    // Enum for SCCM_turnIndicatorStalkStatus
    enum class TurnIndicatorStalkStatus : uint8_t
    {
        Idle = 0,
        Up1 = 1,
        Up2 = 2,
        Down1 = 3,
        Down2 = 4,
        SNA = 5
    };

    // Enum for SCCM_washWipeButtonStatus
    enum class WashWipeButtonStatus : uint8_t
    {
        NotPressed = 0,
        FirstDetent = 1,
        SecondDetent = 2,
        SNA = 3
    };

    // Decoded subset of VCRIGHT_doorStatus (CAN ID 0x103) needed for LED binding.
    struct RightDoorStatusMsg
    {
        bool rearIntSwitchPressed = false; // VCRIGHT_rearIntSwitchPressed (32|1@1+)
        uint32_t lastRxMs = 0;             // millis() timestamp when received
    };

    // Decoded subset of VCFRONT_lighting (CAN ID 0x3F5) focusing on indicator left request.
    // Signal definition: VCFRONT_indicatorLeftRequest : 0|2@1+ (1,0) [0|2]
    // 0 = TURN_SIGNAL_OFF, 1 = TURN_SIGNAL_ACTIVE_LOW, 2 = TURN_SIGNAL_ACTIVE_HIGH (treat 1/2 as active)
    struct FrontLightingMsg
    {
        IndicatorReq indicatorLeftRequest = IndicatorReq::Off;
        IndicatorReq indicatorRightRequest = IndicatorReq::Off;
        uint32_t lastRxMs = 0;
    };

    // Decoded subset of ID249SCCMLeftStalk (CAN ID 0x249)
    struct SCCMLeftStalkMsg
    {
        HighBeamStalkStatus highBeamStalkStatus = HighBeamStalkStatus::Idle;
        uint8_t leftStalkCounter = 0;
        uint8_t leftStalkCrc = 0;
        TurnIndicatorStalkStatus turnIndicatorStalkStatus = TurnIndicatorStalkStatus::Idle;
        WashWipeButtonStatus washWipeButtonStatus = WashWipeButtonStatus::NotPressed;
        uint32_t lastRxMs = 0;
    };

    explicit CANManager(uint8_t csPin = PIN_CAN_CS) : _mcp(csPin) {}

    bool begin(uint32_t bitrate = CAN_BAUDRATE)
    {
        if (!_mcp.begin(bitrate))
            return false;

        // Filter for exactly 0x103 (door status) and 0x3F5 (front lighting)
        // All other messages are ignored on the first mask
        if (!_mcp.setFilterMask(0, false, 0x7FF) ||
            !_mcp.setFilter(0, false, 0x103) ||
            !_mcp.setFilter(1, false, 0x3F5))
        {
            return false;
        }

        // Filter out all messages on the second mask
        if (!_mcp.setFilterMask(1, false, 0x7FF) ||
            !_mcp.setFilter(2, false, 0x249) ||
            !_mcp.setFilter(3, false, 0x000) ||
            !_mcp.setFilter(4, false, 0x000) ||
            !_mcp.setFilter(5, false, 0x000))
        {
            return false;
        }

        return true;
    }

    void setDebugRaw(bool enabled) { _debugRaw = enabled; }
    void setDebugDecoded(bool enabled) { _debugDecoded = enabled; }

    bool hasNewRightDoorStatus() const { return _rightDoorNew; }
    RightDoorStatusMsg getRightDoorStatus(bool clear = true)
    {
        RightDoorStatusMsg c = _rightDoor;
        if (clear)
            _rightDoorNew = false;
        return c;
    }

    bool hasNewFrontLighting() const { return _frontLightingNew; }
    FrontLightingMsg getFrontLighting(bool clear = true)
    {
        FrontLightingMsg c = _frontLighting;
        if (clear)
            _frontLightingNew = false;
        return c;
    }

    bool hasNewSCCMLeftStalk() const { return _sccmLeftStalkNew; }
    SCCMLeftStalkMsg getSCCMLeftStalk(bool clear = true)
    {
        SCCMLeftStalkMsg c = _sccmLeftStalk;
        if (clear)
            _sccmLeftStalkNew = false;
        return c;
    }

    // Poll and drain all pending frames; returns true if at least one processed.
    bool poll()
    {
        bool any = false;
        // Drain loop: parsePacket() returns 0 when no more frames.
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

            // Decode VCRIGHT_doorStatus (standard ID 0x103, DLC 8)
            if (id == 0x103 && !isRtr && len >= 5)
            { // need at least byte 4 for rearIntSwitchPressed
                RightDoorStatusMsg msg;
                // Signal VCRIGHT_rearIntSwitchPressed : 32|1@1+ (little-endian) -> bit0 (LSB) of byte 4
                msg.rearIntSwitchPressed = (data[4] & 0x01) != 0;
                msg.lastRxMs = millis();
                _rightDoor = msg;
                _rightDoorNew = true;
                if (_debugDecoded)
                {
                    Serial.print(F("DoorStatus: rearIntSwitchPressed="));
                    Serial.print(msg.rearIntSwitchPressed);
                    Serial.println();
                }
            }

            // Decode VCFRONT_lighting (standard ID 0x3F5, DLC 8) - only need first byte for indicator requests
            if (id == 0x3F5 && !isRtr && len >= 1)
            {
                FrontLightingMsg msg;
                // Left indicator: bits 0-1
                uint8_t rawLeft = (uint8_t)(data[0] & 0x03);
                switch (rawLeft)
                {
                case 0:
                    msg.indicatorLeftRequest = IndicatorReq::Off;
                    break;
                case 1:
                    msg.indicatorLeftRequest = IndicatorReq::ActiveLow;
                    break;
                case 2:
                    msg.indicatorLeftRequest = IndicatorReq::ActiveHigh;
                    break;
                default:
                    msg.indicatorLeftRequest = IndicatorReq::Unknown;
                    break;
                }
                // Right indicator: bits 2-3
                uint8_t rawRight = (uint8_t)((data[0] >> 2) & 0x03);
                switch (rawRight)
                {
                case 0:
                    msg.indicatorRightRequest = IndicatorReq::Off;
                    break;
                case 1:
                    msg.indicatorRightRequest = IndicatorReq::ActiveLow;
                    break;
                case 2:
                    msg.indicatorRightRequest = IndicatorReq::ActiveHigh;
                    break;
                default:
                    msg.indicatorRightRequest = IndicatorReq::Unknown;
                    break;
                }
                msg.lastRxMs = millis();
                _frontLighting = msg;
                _frontLightingNew = true;
                if (_debugDecoded)
                {
                    Serial.print(F("FrontLighting: left="));
                    switch (msg.indicatorLeftRequest)
                    {
                    case IndicatorReq::Off:
                        Serial.print(F("OFF"));
                        break;
                    case IndicatorReq::ActiveLow:
                        Serial.print(F("ACTIVE_LOW"));
                        break;
                    case IndicatorReq::ActiveHigh:
                        Serial.print(F("ACTIVE_HIGH"));
                        break;
                    case IndicatorReq::Unknown:
                        Serial.print(F("UNKNOWN"));
                        break;
                    }
                    Serial.print(F(" right="));
                    switch (msg.indicatorRightRequest)
                    {
                    case IndicatorReq::Off:
                        Serial.println(F("OFF"));
                        break;
                    case IndicatorReq::ActiveLow:
                        Serial.println(F("ACTIVE_LOW"));
                        break;
                    case IndicatorReq::ActiveHigh:
                        Serial.println(F("ACTIVE_HIGH"));
                        break;
                    case IndicatorReq::Unknown:
                        Serial.println(F("UNKNOWN"));
                        break;
                    }
                }
            }

            // Decode ID249SCCMLeftStalk (standard ID 0x249, DLC 4)
            if (id == 0x249 && !isRtr && len >= 3)
            {
                SCCMLeftStalkMsg msg;
                msg.leftStalkCrc = data[0];
                msg.leftStalkCounter = (data[1] >> 0) & 0x0F;
                msg.highBeamStalkStatus = (HighBeamStalkStatus)((data[1] >> 4) & 0x03);
                msg.washWipeButtonStatus = (WashWipeButtonStatus)((data[1] >> 6) & 0x03);
                msg.turnIndicatorStalkStatus = (TurnIndicatorStalkStatus)((data[2] >> 0) & 0x07);
                msg.lastRxMs = millis();
                _sccmLeftStalk = msg;
                _sccmLeftStalkNew = true;
                if (_debugDecoded)
                {
                    Serial.print(F("SCCMLeftStalk: highBeam="));
                    Serial.print((uint8_t)msg.highBeamStalkStatus);
                    Serial.print(F(" turn="));
                    Serial.print((uint8_t)msg.turnIndicatorStalkStatus);
                    Serial.print(F(" washWipe="));
                    Serial.print((uint8_t)msg.washWipeButtonStatus);
                    Serial.print(F(" counter="));
                    Serial.print(msg.leftStalkCounter);
                    Serial.print(F(" crc="));
                    Serial.print(msg.leftStalkCrc, HEX);
                    Serial.println();
                }
            }
        }
        return any;
    }


    void sendTurnSignalCommand(TurnIndicatorStalkStatus turnStatus,
                               HighBeamStalkStatus highBeamStatus = HighBeamStalkStatus::Idle,
                               WashWipeButtonStatus washWipeStatus = WashWipeButtonStatus::NotPressed,
                               uint8_t reserved = 0)
    {
        // ID 0x249, DLC 3, Motorola (big-endian) bit layout
        // Byte 0: CRC (placeholder, set to 0)
        // Byte 1: [counter(7:4)] [highBeam(3:2)] [washWipe(1:0)]
        // Byte 2: [turnStatus(7:5)] [reserved(4:0)]

        static uint8_t counter = 0;
        uint8_t data[3] = {0};

        // Byte 1
        data[1] = ((counter & 0x0F) << 4) |
                  (((uint8_t)highBeamStatus & 0x03) << 2) |
                  ((uint8_t)washWipeStatus & 0x03);

        // Byte 2
        data[2] = (((uint8_t)turnStatus & 0x07) << 5) |
                  (reserved & 0x1F);

        // Byte 0: CRC (unknown polynomial, set to 0 for now)
        data[0] = 0;

        _mcp.beginPacket(0x249);
        _mcp.write(data, 3);
        _mcp.endPacket();

        counter = (counter + 1) % 16;
    }

    Adafruit_MCP2515 &mcp() { return _mcp; }

private:
    // CRC computation for stalk messages (ID 0x249, 0x24A).
    // It's a CRC-8 with polynomial 0x1D (AUTOSAR), initial value 0xFF, final XOR 0xFF.
    // The CRC is calculated over bytes 1 and 2 of the CAN frame (after packing all fields).
    uint8_t _compute_crc(const uint8_t *data, uint8_t len)
    {
        uint8_t crc = 0xFF;
        for (uint8_t i = 1; i < len; ++i) // Loop over bytes 1 and 2
        {
            crc ^= data[i];
            for (uint8_t j = 0; j < 8; ++j)
            {
                if (crc & 0x80)
                {
                    crc = (crc << 1) ^ 0x1D;
                }
                else
                {
                    crc <<= 1;
                }
            }
        }
        return crc ^ 0xFF;
    }

    Adafruit_MCP2515 _mcp;
    bool _debugRaw = false;
    bool _debugDecoded = true; // default show decoded message when present
    RightDoorStatusMsg _rightDoor{};
    bool _rightDoorNew = false;
    FrontLightingMsg _frontLighting{};
    bool _frontLightingNew = false;
    SCCMLeftStalkMsg _sccmLeftStalk{};
    bool _sccmLeftStalkNew = false;
};
