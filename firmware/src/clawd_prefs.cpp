#include "clawd_prefs.h"
#include "clawd_director.h"
#include "clawd_policy.h"
#include <Preferences.h>
#include <Arduino.h>

void clawd_prefs_init(void) {
    Preferences prefs;
    prefs.begin("clawdmeter", true);
    const int lively = prefs.getUChar("lively", CLAWD_LIVELINESS_DEFAULT);
    const int attn = prefs.getUChar("attn_anim", CLAWD_ATTN_ANIM_DEFAULT_S);
    const bool status = prefs.getUChar("status_mode", 1) != 0;
    prefs.end();
    director_set_liveliness(clawd_liveliness_clamp(lively));
    director_set_attn_anim(clawd_attn_anim_clamp(attn));
    director_set_status_mode(status);
    Serial.printf("Clawd prefs init: liveliness %d, attention animation %d s, status mode %s\n",
                  clawd_liveliness(), clawd_attn_anim(), status ? "on" : "off");
}

static void save(const char* key, uint8_t v) {
    Preferences prefs;
    prefs.begin("clawdmeter", false);
    prefs.putUChar(key, v);
    prefs.end();
}

void liveliness_set(int level) {
    level = clawd_liveliness_clamp(level);
    if (level == clawd_liveliness()) return;
    save("lively", (uint8_t)level);
    director_set_liveliness(level);
    Serial.printf("Liveliness: %d\n", level);
}

int liveliness_get(void) { return clawd_liveliness(); }

void attn_anim_set(int seconds) {
    seconds = clawd_attn_anim_clamp(seconds);
    if (seconds == clawd_attn_anim()) return;
    save("attn_anim", (uint8_t)seconds);
    director_set_attn_anim(seconds);
    Serial.printf("Attention animation: %d s\n", seconds);
}

int attn_anim_get(void) { return clawd_attn_anim(); }

void status_mode_set(bool on) {
    if (on == director_status_mode()) return;
    save("status_mode", on ? 1 : 0);
    director_set_status_mode(on);
    Serial.printf("Status mode: %s\n", on ? "on" : "off");
}

bool status_mode_get(void) { return director_status_mode(); }
