#include "config_store.h"

#include <SPIFFS.h>

bool ConfigStore::reload() {
    File file = SPIFFS.open("/keyconfig.json");
    if (!file) {
        Serial.println("ConfigStore: failed to open /keyconfig.json");
        return false;
    }

    doc_.clear();
    DeserializationError err =
        deserializeJson(doc_, file, DeserializationOption::NestingLimit(5));
    file.close();

    if (err) {
        Serial.print(F("ConfigStore: deserializeJson() failed: "));
        Serial.println(err.c_str());
        return false;
    }
    return true;
}
