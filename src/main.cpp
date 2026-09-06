#include <memory>
#include <ArduinoJson.h>
#include <SPIFFS.h>
#include "sensesp_app_builder.h"
#include "sensesp/signalk/signalk_output.h"
#include "sensesp/ui/config_item.h"
#include "sensesp/ui/ui_controls.h"

#include "services/BoatBowControlApp.h"
#include "secrets.h"

#ifndef AP_PASSWORD
#error "AP_PASSWORD not defined. Define it in src/secrets.h."
#endif

#ifndef OTA_PASSWORD
#error "OTA_PASSWORD not defined. Define it in src/secrets.h."
#endif

using namespace sensesp;

// ========================================
// Configuration Variables (persisted in SPIFFS)
// ========================================
float g_config_meters_per_pulse = 0.01f;  // Default: 1cm per pulse
String g_config_path_meters_per_pulse = "/Calibration/MetersPerPulse";

namespace {
    bool findConfigFile(const String& config_path, String& filename) {
        const String hash_path = String("/") + Base64Sha1(config_path);
        const String paths_to_check[] = {hash_path, hash_path + "\n"};

        for (const auto& path : paths_to_check) {
            if (SPIFFS.exists(path)) {
                filename = path;
                return true;
            }
        }

        if (config_path.length() < 32 && SPIFFS.exists(config_path)) {
            filename = config_path;
            return true;
        }

        return false;
    }

    void updateApPasswordIfDefault(const char* ap_password) {
        const String config_path = "/System/WiFi Settings";
        String filename;
        if (!findConfigFile(config_path, filename)) {
            return;
        }

        File file = SPIFFS.open(filename, "r");
        if (!file) {
            return;
        }

        DynamicJsonDocument doc(1024);
        auto error = deserializeJson(doc, file);
        file.close();
        if (error) {
            return;
        }

        bool updated = false;
        const String default_password = "thisisfine";

        if (doc["apSettings"].is<JsonVariant>()) {
            JsonObject ap_settings = doc["apSettings"].as<JsonObject>();
            String current = ap_settings["password"] | "";
            if (current.length() == 0 || current == default_password) {
                ap_settings["password"] = ap_password;
                updated = true;
            }
        } else if (doc["ap_mode"].is<String>()) {
            String ap_mode = doc["ap_mode"].as<String>();
            String current = doc["password"] | "";
            if ((ap_mode == "Access Point" || ap_mode == "Hotspot") &&
                (current.length() == 0 || current == default_password)) {
                doc["password"] = ap_password;
                updated = true;
            }
        }

        if (!updated) {
            return;
        }

        File out = SPIFFS.open(filename, "w");
        if (!out) {
            return;
        }
        serializeJson(doc, out);
        out.close();
    }
}

// ========================================
// Global Application Instance
// ========================================
BoatBowControlApp app;

void setup() {
    // Initialize logging (must be first)
    SetupLogging();
    debugD("=== Boat Anchor Chain Counter and Bow Control System ===");
    debugD("Build: %s @ %s", __DATE__, __TIME__);

    // Setup SensESP with network and web UI FIRST (creates sensesp_app global)
    // This must happen before app.initialize() which calls event_loop()
    SensESPAppBuilder builder;
    updateApPasswordIfDefault(AP_PASSWORD);
    sensesp_app = builder
                      .set_wifi_access_point("bow-ecu", AP_PASSWORD)
                      ->set_hostname("bow-ecu")
                      ->enable_ota(OTA_PASSWORD)
                      ->get_app();

    // Register SensESP configuration items before application initialization
    // Meters per pulse is a calibration value persisted in SPIFFS
    ConfigItem(new NumberConfig(g_config_meters_per_pulse, g_config_path_meters_per_pulse))
        ->set_title("Meters Per Pulse")
        ->set_description("Calibration: distance in meters for each chain counter pulse")
        ->set_sort_order(200);

    // Now initialize the application hardware and services (uses sensesp_app)
    app.initialize();
    
    // Load the configured value into the state manager
    app.getStateManager().setMetersPerPulse(g_config_meters_per_pulse);

    // SensESP 3.5 starts during builder setup; start application integration
    app.startSignalK();

    debugD("Setup complete - waiting for SignalK connection");
}

void loop() {
    // Process inputs and tick the event loop
    app.processInputs();
}
