// Centralized hardware-related constants
#pragma once

#include <stdint.h>

// General configuration

constexpr bool SERIAL_WAIT = true; // Wait for Serial connection on startup
constexpr uint16_t SERIAL_WAIT_TIMEOUT_MS = 10000; // Max wait time (0 = infinite)
constexpr bool DEMO_MODE = false; // Enable demo mode if CAN is unavailable

// Benchmarking / diagnostics
constexpr bool PERF_LOG_ENABLED = false; // Set false to disable Serial perf logs
constexpr uint16_t PERF_LOG_INTERVAL_MS = 2000; // How often to print perf metrics

// I2C addresses (default Adafruit breakout values)
constexpr uint8_t ENCODER_I2C_ADDR = 0x36; // Rotary encoder w/ NeoPixel (seesaw)
constexpr uint8_t NEOPIXEL_I2C_ADDR = 0x60; // NeoPixel driver (seesaw)
constexpr uint32_t I2C_CLOCK_HZ = 400000; // Use fast-mode I2C for quicker pixel uploads

// Encoder seesaw pin assignments
constexpr uint8_t ENCODER_SWITCH_PIN = 24; // GPIO for push switch (active low)
constexpr uint8_t ENCODER_PIXEL_PIN = 6;   // NeoPixel data pin on encoder board

// Pixel brightness levels
constexpr uint8_t ENCODER_PIXEL_BRIGHTNESS = 20; // range 0-255

// NeoPixel strip configuration
constexpr uint8_t NEOPIXEL_STRIP_LENGTH = 75; // Physical pixels on the strip
constexpr uint8_t NEOPIXEL_STRIP_PIN = 15; // GPIO for NeoPixel strip data
constexpr float NEOPIXEL_VIRTUAL_LENGTH = 255.0f; // Logical pixels for sub-pixel blending
constexpr uint8_t NEOPIXEL_UPDATE_INTERVAL_MS = 50; // How often to update the strip
constexpr float NEOPIXEL_GAMMA = 2.0f; // Gamma for fractional pixel blending

// Timing

// Individual component polling intervals (ms). Tuned to balance responsiveness & CPU.
constexpr uint16_t ENCODER_SCAN_INTERVAL_MS = 10; // Fast enough for quick spins

// CAN bus configuration
constexpr uint32_t CAN_BAUDRATE = 500000; // bits per second

// Acceleration estimation (derived from DI_vehicleSpeed)
constexpr uint16_t ACCEL_WINDOW_MS = 300; // Time window for speed regression smoothing
constexpr float ACCEL_MIN_MPS2 = 0.3f; // Minimum accel magnitude to show anything
constexpr float ACCEL_MAX_MPS2 = 5.0f; // Accel mapped to 100% for the light strip

// Power estimation (derived from DI_torqueCommand + DI_axleSpeed)
constexpr float POWER_MIN_W = 2000.0f; // Minimum power magnitude to show anything
constexpr float POWER_MAX_W = 80000.0f; // Power mapped to 100% for the light strip

// Visual smoothing
constexpr uint16_t STRIP_SMOOTH_TIME_MS = 250; // Time constant for smoothing display changes
