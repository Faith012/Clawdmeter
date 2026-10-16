#pragma once
// In-memory stand-in for ESP32 NVS Preferences (brightness, Clawd's prefs, the Clawd Jump best).
// State lasts for the process lifetime only — the sim boots with defaults.
#include <stdint.h>
#include <stddef.h>

class Preferences {
public:
    bool begin(const char* name, bool read_only = false) {
        (void)name; (void)read_only;
        return true;
    }
    void end(void) {}
    uint8_t getUChar(const char* key, uint8_t def = 0);
    size_t  putUChar(const char* key, uint8_t value);
    uint32_t getUInt(const char* key, uint32_t def = 0);
    size_t   putUInt(const char* key, uint32_t value);
};
