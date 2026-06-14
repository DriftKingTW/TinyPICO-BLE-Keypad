#pragma once

#include <ArduinoJson.h>

// Loads and caches the parsed keyconfig.json. The keymap, macros and layout
// lookups all read this single in-memory document instead of re-reading the
// file and re-parsing the ~16 KB JSON on every call and every layer switch.
class ConfigStore {
   public:
    ConfigStore() : doc_(kCapacity) {}

    // (Re)read /keyconfig.json from SPIFFS and parse it into the cached
    // document. Returns false on open/parse failure (cache left unchanged on
    // open failure). Streams straight from the file, no intermediate String.
    bool reload();

    // Parsed configuration. Valid after a successful reload().
    JsonDocument &doc() { return doc_; }

   private:
    // Sized for up to ~10 layers (was the project-wide jsonDocSize).
    static const size_t kCapacity = 16384;
    DynamicJsonDocument doc_;
};
