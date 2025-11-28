#ifndef CONFIG_H
#define CONFIG_H

// =============================================================================
// BLE Configuration
// =============================================================================

// Device name shown in BLE scan
#define BLE_DEVICE_NAME "AutoPrintFarm Hub"

// WiFi Provisioning Service UUID
#define SERVICE_UUID_WIFI_PROV "4fafc201-1fb5-459e-8fcc-c5c9c331914b"

// Characteristic UUIDs
#define CHAR_UUID_SSID     "beb5483e-36e1-4688-b7f5-ea07361b26a8"  // Read/Write
#define CHAR_UUID_PASSWORD "beb5483f-36e1-4688-b7f5-ea07361b26a8"  // Write only
#define CHAR_UUID_COMMAND  "beb54840-36e1-4688-b7f5-ea07361b26a8"  // Write only
#define CHAR_UUID_STATUS   "beb54841-36e1-4688-b7f5-ea07361b26a8"  // Read/Notify

// =============================================================================
// WiFi Configuration
// =============================================================================

// Connection timeout in milliseconds
#define WIFI_CONNECT_TIMEOUT_MS 15000

// Maximum credential lengths
#define MAX_SSID_LENGTH     32
#define MAX_PASSWORD_LENGTH 64

// =============================================================================
// NVS Configuration
// =============================================================================

#define NVS_NAMESPACE_WIFI "wifi_creds"
#define NVS_KEY_SSID       "ssid"
#define NVS_KEY_PASSWORD   "password"
#define NVS_KEY_VALID      "valid"

// =============================================================================
// Command Values (written to Command characteristic)
// =============================================================================

#define CMD_CONNECT    0x01  // Connect to WiFi with stored credentials
#define CMD_DISCONNECT 0x02  // Disconnect from WiFi
#define CMD_CLEAR      0xFF  // Clear stored credentials

// =============================================================================
// Status Values (read from Status characteristic)
// =============================================================================

#define STATUS_IDLE           0x00  // Not connected, no operation in progress
#define STATUS_CONNECTING     0x01  // WiFi connection in progress
#define STATUS_CONNECTED      0x02  // Successfully connected to WiFi
#define STATUS_FAILED         0x03  // Connection failed
#define STATUS_DISCONNECTED   0x04  // Explicitly disconnected
#define STATUS_NO_CREDENTIALS 0x05  // No SSID/password stored

#endif // CONFIG_H
