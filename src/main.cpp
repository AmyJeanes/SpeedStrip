#include <Arduino.h>
#include <Wire.h>
#include "Configuration.h"
#include "ColorUtils.h"
#include "EncoderManager.h"
#include "StatusLED.h"
#include "LightStripManager.h"
#include "CANManager.h"

EncoderManager g_encoder(ENCODER_SWITCH_PIN, ENCODER_PIXEL_PIN);
StatusLED g_statusLed;
LightStripManager g_lightStrip;
CANManager g_can;

void setup()
{
    g_statusLed.begin();
    g_statusLed.setState(StatusLED::State::Waiting);

    Serial.begin(115200);
    if (SERIAL_WAIT)
    {
        uint32_t now = millis();
        while (!Serial && (SERIAL_WAIT_TIMEOUT_MS == 0 || (millis() - now) < SERIAL_WAIT_TIMEOUT_MS))
        {
            g_statusLed.update();
            delay(5);
        }
    }

    Serial.println();
    Serial.println(F("Input system starting..."));

    Wire.begin();
    Wire.setClock(I2C_CLOCK_HZ);

    if (!g_encoder.begin(ENCODER_I2C_ADDR, ENCODER_PIXEL_BRIGHTNESS))
    {
        Serial.println(F("ERROR: Encoder init failed."));
        g_statusLed.setState(StatusLED::State::Error);
        while (true)
        {
            g_statusLed.update();
            delay(100);
        }
    }

    if (!g_lightStrip.begin())
    {
        Serial.println("ERROR: NeoPixel strip not found!");
        g_statusLed.setState(StatusLED::State::Error);
        while (true)
        {
            g_statusLed.update();
            delay(100);
        }
    }

    // Initialize CAN controller
    if (!g_can.begin(CAN_BAUDRATE))
    {
        Serial.println(F("ERROR: MCP2515 init failed."));
        g_statusLed.setState(StatusLED::State::Error);
        while (true)
        {
            g_statusLed.update();
            delay(100);
        }
    }
    else
    {
        Serial.println(F("MCP2515 CAN controller initialized."));
        // Enable decoded output by default; raw traffic can be toggled later.
        g_can.setDebugDecoded(false);
        g_can.setDebugRaw(false);
    }

    Serial.println(F("Setup complete."));
    g_statusLed.setState(StatusLED::State::Ok);
}
void loop()
{
    static uint32_t lastEncoderMs = 0;
    uint32_t now = millis();

    // Encoder at configured interval
    if (now - lastEncoderMs >= ENCODER_SCAN_INTERVAL_MS)
    {
        lastEncoderMs = now;
        g_encoder.update();
    }
    // Status LED animation already self-throttles internally
    g_statusLed.update();

    g_can.poll();
    if (g_can.hasNewDISystemStatus())
    {
        auto di = g_can.getDISystemStatus();
        g_lightStrip.setAccelPosition(di.accelPedalPosPercent);
    }

    g_lightStrip.update(now);
}
