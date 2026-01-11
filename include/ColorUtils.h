// Small helpers for color generation
#pragma once

#include <stdint.h>
#include "seesaw_neopixel.h"

namespace ColorUtils
{

    inline uint32_t wheel(seesaw_NeoPixel &px, uint8_t WheelPos)
    {
        WheelPos = 255 - WheelPos;
        if (WheelPos < 85)
        {
            return px.Color(255 - WheelPos * 3, 0, WheelPos * 3);
        }
        if (WheelPos < 170)
        {
            WheelPos -= 85;
            return px.Color(0, WheelPos * 3, 255 - WheelPos * 3);
        }
        WheelPos -= 170;
        return px.Color(WheelPos * 3, 255 - WheelPos * 3, 0);
    }

}
