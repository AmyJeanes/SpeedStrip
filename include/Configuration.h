// Centralized hardware-related constants
#pragma once

#include <stdint.h>

// General configuration

constexpr bool SERIAL_WAIT = true; // Wait for Serial connection on startup
constexpr uint16_t SERIAL_WAIT_TIMEOUT_MS = 10000; // Max wait time (0 = infinite)
constexpr bool DEMO_MODE = false; // Enable demo mode if CAN is unavailable

// I2C addresses (default Adafruit breakout values)
constexpr uint8_t ENCODER_I2C_ADDR = 0x36; // Rotary encoder w/ NeoPixel (seesaw)
constexpr uint8_t NEOPIXEL_I2C_ADDR = 0x60; // NeoPixel driver (seesaw)

// Encoder seesaw pin assignments
constexpr uint8_t ENCODER_SWITCH_PIN = 24; // GPIO for push switch (active low)
constexpr uint8_t ENCODER_PIXEL_PIN = 6;   // NeoPixel data pin on encoder board

// Pixel brightness levels
constexpr uint8_t ENCODER_PIXEL_BRIGHTNESS = 20; // range 0-255

// NeoPixel strip configuration
constexpr uint8_t NEOPIXEL_STRIP_LENGTH = 75; // Number of pixels in the strip
constexpr uint8_t NEOPIXEL_STRIP_PIN = 15; // GPIO for NeoPixel strip data
constexpr uint8_t NEOPIXEL_UPDATE_INTERVAL_MS = 50; // How often to update the strip
constexpr float NEOPIXEL_GAMMA = 2.0f; // Gamma for fractional pixel blending

// Timing

// Individual component polling intervals (ms). Tuned to balance responsiveness & CPU.
constexpr uint16_t ENCODER_SCAN_INTERVAL_MS = 10; // Fast enough for quick spins

// CAN bus configuration
constexpr uint32_t CAN_BAUDRATE = 500000; // bits per second
