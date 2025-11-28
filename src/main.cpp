#include <Arduino.h>
#include "config.h"
#include "provisioning/CredentialStore.h"
#include "provisioning/BLEProvisioning.h"

// =============================================================================
// Global Objects
// =============================================================================

CredentialStore credentialStore;
BLEProvisioning bleProvisioning(credentialStore);

// =============================================================================
// Setup
// =============================================================================

void setup() {
    // Initialize serial
    Serial.begin(115200);
    delay(1000);  // Give serial time to initialize

    Serial.println();
    Serial.println("================================================");
    Serial.println("       AutoPrintFarm Hub - Starting Up");
    Serial.println("================================================");
    Serial.println();

    // Initialize credential store (NVS)
    Serial.println("[Main] Initializing credential store...");
    if (!credentialStore.begin()) {
        Serial.println("[Main] ERROR: Failed to initialize credential store!");
    }

    // Initialize BLE provisioning
    Serial.println("[Main] Starting BLE provisioning...");
    bleProvisioning.begin(BLE_DEVICE_NAME);

    // Auto-connect to WiFi if credentials are stored
    if (credentialStore.hasCredentials()) {
        Serial.println("[Main] Found stored WiFi credentials, attempting auto-connect...");
        bleProvisioning.autoConnect();
    } else {
        Serial.println("[Main] No WiFi credentials stored. Use BLE provisioning to configure.");
    }

    Serial.println();
    Serial.println("[Main] Setup complete!");
    Serial.println("================================================");
    Serial.println();
}

// =============================================================================
// Main Loop
// =============================================================================

void loop() {
    // Poll BLE provisioning (handles WiFi connection state machine)
    bleProvisioning.poll();

    // Print status periodically (every 10 seconds)
    static unsigned long lastStatusPrint = 0;
    if (millis() - lastStatusPrint > 10000) {
        lastStatusPrint = millis();

        if (bleProvisioning.isWiFiConnected()) {
            Serial.printf("[Status] WiFi: Connected | SSID: %s | IP: %s | RSSI: %d dBm\n",
                          bleProvisioning.getConnectedSSID().c_str(),
                          bleProvisioning.getIPAddress().c_str(),
                          bleProvisioning.getRSSI());
        } else {
            Serial.printf("[Status] WiFi: Not connected | State: %d\n",
                          static_cast<uint8_t>(bleProvisioning.getState()));
        }
    }

    // Small delay to prevent watchdog issues
    delay(10);
}
