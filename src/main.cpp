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
    static uint32_t benchLastMs = 0;
    static uint32_t loopCount = 0;
    static uint32_t loopAccumUs = 0;
    static uint32_t loopMaxUs = 0;
    static uint32_t stripCount = 0;
    static uint32_t stripAccumUs = 0;
    static uint32_t stripMaxUs = 0;

    uint32_t loopStartUs = micros();
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
    if (g_can.hasNewDITorque())
    {
        auto di = g_can.getDITorque();
        g_lightStrip.setAccelPercent(di.powerPercentSigned);
    }

    uint32_t stripStartUs = micros();
    g_lightStrip.update(now);
    uint32_t stripDurUs = micros() - stripStartUs;
    ++stripCount;
    stripAccumUs += stripDurUs;
    if (stripDurUs > stripMaxUs)
        stripMaxUs = stripDurUs;

    ++loopCount;
    uint32_t loopDurUs = micros() - loopStartUs;
    loopAccumUs += loopDurUs;
    if (loopDurUs > loopMaxUs)
        loopMaxUs = loopDurUs;

    if (PERF_LOG_ENABLED)
    {
        if (benchLastMs == 0)
            benchLastMs = now;

        uint32_t elapsedMs = now - benchLastMs;
        if (elapsedMs >= PERF_LOG_INTERVAL_MS && elapsedMs > 0)
        {
            uint32_t loopsPerSec = (loopCount * 1000UL) / elapsedMs;
            uint32_t loopAvgUs = loopCount ? (loopAccumUs / loopCount) : 0;
            uint32_t stripAvgUs = stripCount ? (stripAccumUs / stripCount) : 0;

            Serial.print(F("[perf] loop "));
            Serial.print(loopsPerSec);
            Serial.print(F("/s avg "));
            Serial.print(loopAvgUs);
            Serial.print(F("us max "));
            Serial.print(loopMaxUs);
            Serial.print(F(" | strip avg "));
            Serial.print(stripAvgUs);
            Serial.print(F("us max "));
            Serial.println(stripMaxUs);

            benchLastMs = now;
            loopCount = 0;
            loopAccumUs = 0;
            loopMaxUs = 0;
            stripCount = 0;
            stripAccumUs = 0;
            stripMaxUs = 0;
        }
    }

}
