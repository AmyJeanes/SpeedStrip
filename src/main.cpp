#include <Arduino.h>
#include "HardwareConfig.h"
#include "EncoderManager.h"
#include "StatusLED.h"
#include "CANManager.h"

EncoderManager g_encoder(ENCODER_SWITCH_PIN, ENCODER_PIXEL_PIN);
seesaw_NeoPixel strip = seesaw_NeoPixel(NEOPIXEL_STRIP_LENGTH, NEOPIXEL_STRIP_PIN, NEO_GRB + NEO_KHZ800);
StatusLED g_statusLed;
CANManager g_can;

struct IndicatorState
{
    bool leftActive = false;
    bool rightActive = false;
};

IndicatorState g_indicatorState;
bool g_canReady = false;
bool g_demoMode = true; // enables a canned indicator demo when CAN is unavailable
constexpr uint16_t INDICATOR_FLASH_MS = 400;

void setup()
{
    g_statusLed.begin();
    g_statusLed.setState(StatusLED::State::Waiting);

    Serial.begin(115200);
    while (!Serial)
    {
        g_statusLed.update();
        delay(5);
    }
    Serial.println();
    Serial.println(F("Input system starting..."));

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

    if(!strip.begin(NEOPIXEL_I2C_ADDR)){
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

// Input a value 0 to 255 to get a color value.
// The colours are a transition r - g - b - back to r.
uint32_t Wheel(byte WheelPos) {
  WheelPos = 255 - WheelPos;
  if(WheelPos < 85) {
    return strip.Color(255 - WheelPos * 3, 0, WheelPos * 3);
  }
  if(WheelPos < 170) {
    WheelPos -= 85;
    return strip.Color(0, WheelPos * 3, 255 - WheelPos * 3);
  }
  WheelPos -= 170;
  return strip.Color(WheelPos * 3, 255 - WheelPos * 3, 0);
}

void rainbowCycleStep()
{
    static uint16_t j = 0; // advances slowly to avoid blocking loop
    for (uint16_t i = 0; i < strip.numPixels(); i++)
    {
        strip.setPixelColor(i, Wheel(((i * 256 / strip.numPixels()) + j) & 0xFF));
    }
    strip.show();
    j = (j + 1) % (256 * 5);
}

void applyIndicatorPattern(uint32_t now)
{
    // Resolve which sides should flash (CAN-driven, or demo fallback if CAN is unavailable)
    bool leftActive = g_indicatorState.leftActive;
    bool rightActive = g_indicatorState.rightActive;

    if (g_demoMode && !leftActive && !rightActive)
    {
        // Alternate left/right every 1.5s so wiring and LEDs can be checked without CAN
        bool demoLeft = ((now / 1500) & 0x01) == 0;
        leftActive = demoLeft;
        rightActive = !demoLeft;
    }

    if (!leftActive && !rightActive)
    {
        rainbowCycleStep();
        return;
    }

    static uint32_t lastFlashMs = 0;
    static bool flashOn = false;
    if (now - lastFlashMs >= INDICATOR_FLASH_MS)
    {
        lastFlashMs = now;
        flashOn = !flashOn;
    }

    uint32_t amber = strip.Color(255, 120, 0);
    uint16_t mid = strip.numPixels() / 2;

    auto fillRange = [&](uint16_t start, uint16_t end, uint32_t color)
    {
        for (uint16_t i = start; i < end; ++i)
        {
            strip.setPixelColor(i, color);
        }
    };

    // Left half
    if (leftActive && flashOn)
    {
        fillRange(0, mid, amber);
    }
    else
    {
        fillRange(0, mid, 0);
    }

    // Right half
    if (rightActive && flashOn)
    {
        fillRange(mid, strip.numPixels(), amber);
    }
    else
    {
        fillRange(mid, strip.numPixels(), 0);
    }

    strip.show();
}


void loop()
{
    static uint32_t lastEncoderMs = 0;
    static uint32_t lastNeoPixelMs = 0;
    uint32_t now = millis();

    // Encoder at configured interval
    if (now - lastEncoderMs >= ENCODER_SCAN_INTERVAL_MS)
    {
        lastEncoderMs = now;
        g_encoder.update();
    }
    // Status LED animation already self-throttles internally
    g_statusLed.update();

    if (g_canReady)
    {
        g_can.poll();
        if (g_can.hasNewFrontLighting())
        {
            auto fl = g_can.getFrontLighting();
            g_indicatorState.leftActive = (fl.indicatorLeftRequest == CANManager::IndicatorReq::ActiveLow ||
                                            fl.indicatorLeftRequest == CANManager::IndicatorReq::ActiveHigh);
            g_indicatorState.rightActive = (fl.indicatorRightRequest == CANManager::IndicatorReq::ActiveLow ||
                                             fl.indicatorRightRequest == CANManager::IndicatorReq::ActiveHigh);
        }
    }

    if (now - lastNeoPixelMs >= NEOPIXEL_UPDATE_INTERVAL_MS)
    {
      lastNeoPixelMs = now;
      applyIndicatorPattern(now);
    }
}