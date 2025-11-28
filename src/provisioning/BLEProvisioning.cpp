#include "BLEProvisioning.h"
#include "../config.h"

BLEProvisioning::BLEProvisioning(CredentialStore& credentialStore)
    : _credentialStore(credentialStore)
    , _pServer(nullptr)
    , _pService(nullptr)
    , _pSsidChar(nullptr)
    , _pPasswordChar(nullptr)
    , _pCommandChar(nullptr)
    , _pStatusChar(nullptr)
    , _state(ProvisioningState::IDLE)
    , _bleClientConnected(false)
    , _bleInitialized(false)
    , _wifiConnectStartTime(0)
    , _wifiConnecting(false)
    , _needsAdvertisingRestart(false)
    , _disconnectTime(0) {
}

void BLEProvisioning::begin(const char* deviceName) {
    Serial.printf("[BLE] Initializing as '%s'\n", deviceName);
    setupBLE(deviceName);
    startAdvertising();

    // Set initial state based on credentials
    if (!_credentialStore.hasCredentials()) {
        updateState(ProvisioningState::NO_CREDENTIALS);
    }
}

void BLEProvisioning::stop() {
    if (_bleInitialized) {
        NimBLEDevice::deinit(true);
        _bleInitialized = false;
        Serial.println("[BLE] Stopped");
    }
}

void BLEProvisioning::setupBLE(const char* deviceName) {
    // Initialize NimBLE
    NimBLEDevice::init(deviceName);

    // Disable security/bonding - this prevents Windows from trying to pair
    // and avoids issues with stale pairing state
    NimBLEDevice::setSecurityAuth(false, false, false);  // no bonding, no MITM, no SC
    NimBLEDevice::setSecurityIOCap(BLE_HS_IO_NO_INPUT_OUTPUT);

    // Set power level
    NimBLEDevice::setPower(ESP_PWR_LVL_P9);

    // Create server
    _pServer = NimBLEDevice::createServer();
    _pServer->setCallbacks(this);

    // Create WiFi provisioning service
    _pService = _pServer->createService(SERVICE_UUID_WIFI_PROV);

    // SSID characteristic - Read/Write
    _pSsidChar = _pService->createCharacteristic(
        CHAR_UUID_SSID,
        NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::WRITE
    );
    _pSsidChar->setCallbacks(this);

    // Password characteristic - Write only (security: can't read back)
    _pPasswordChar = _pService->createCharacteristic(
        CHAR_UUID_PASSWORD,
        NIMBLE_PROPERTY::WRITE
    );
    _pPasswordChar->setCallbacks(this);

    // Command characteristic - Write only
    _pCommandChar = _pService->createCharacteristic(
        CHAR_UUID_COMMAND,
        NIMBLE_PROPERTY::WRITE
    );
    _pCommandChar->setCallbacks(this);

    // Status characteristic - Read/Notify
    _pStatusChar = _pService->createCharacteristic(
        CHAR_UUID_STATUS,
        NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::NOTIFY
    );
    _pStatusChar->setCallbacks(this);

    // Initialize characteristics with empty values to prevent garbage data
    _pSsidChar->setValue("");
    _pStatusChar->setValue((uint8_t)ProvisioningState::IDLE);

    // Start the service FIRST
    _pService->start();

    // Configure advertising
    NimBLEAdvertising* pAdvertising = NimBLEDevice::getAdvertising();
    pAdvertising->addServiceUUID(SERVICE_UUID_WIFI_PROV);
    pAdvertising->setScanResponse(false);  // Keep it simple

    _bleInitialized = true;
    Serial.println("[BLE] Service started");
}

void BLEProvisioning::startAdvertising() {
    NimBLEAdvertising* pAdvertising = NimBLEDevice::getAdvertising();

    // Always stop first to avoid "already advertising" issues
    if (pAdvertising->isAdvertising()) {
        pAdvertising->stop();
        delay(50);
    }

    pAdvertising->start();
    Serial.println("[BLE] Advertising started");
}

void BLEProvisioning::poll() {
    // Handle BLE advertising restart (deferred from onDisconnect callback)
    if (_needsAdvertisingRestart && (millis() - _disconnectTime > 200)) {
        _needsAdvertisingRestart = false;
        Serial.println("[BLE] Restarting advertising after disconnect");
        startAdvertising();
    }

    // Handle WiFi connection state machine
    if (_wifiConnecting) {
        wl_status_t status = WiFi.status();

        if (status == WL_CONNECTED) {
            // Success!
            _wifiConnecting = false;
            Serial.printf("[WiFi] Connected! IP: %s\n", WiFi.localIP().toString().c_str());
            updateState(ProvisioningState::CONNECTED);
        }
        else if (status == WL_CONNECT_FAILED ||
                 status == WL_NO_SSID_AVAIL ||
                 (millis() - _wifiConnectStartTime > WIFI_CONNECT_TIMEOUT_MS)) {
            // Failed or timeout
            _wifiConnecting = false;
            WiFi.disconnect();
            Serial.println("[WiFi] Connection failed");
            updateState(ProvisioningState::FAILED);
        }
    }

    // Check if WiFi was disconnected externally
    if (_state == ProvisioningState::CONNECTED && WiFi.status() != WL_CONNECTED) {
        Serial.println("[WiFi] Connection lost");
        updateState(ProvisioningState::DISCONNECTED);
    }
}

void BLEProvisioning::autoConnect() {
    if (!_credentialStore.hasCredentials()) {
        Serial.println("[WiFi] No stored credentials for auto-connect");
        return;
    }

    String ssid, password;
    if (_credentialStore.loadCredentials(ssid, password)) {
        Serial.printf("[WiFi] Auto-connecting to: %s\n", ssid.c_str());
        _pendingSsid = ssid;
        _pendingPassword = password;
        connectToWiFi();
    }
}

void BLEProvisioning::connectToWiFi() {
    if (_pendingSsid.length() == 0) {
        Serial.println("[WiFi] No SSID to connect to");
        updateState(ProvisioningState::NO_CREDENTIALS);
        return;
    }

    // Save credentials if they're new
    String storedSsid = _credentialStore.getStoredSSID();
    if (storedSsid != _pendingSsid || !_credentialStore.hasCredentials()) {
        _credentialStore.saveCredentials(_pendingSsid, _pendingPassword);
    }

    // Disconnect if already connected
    if (WiFi.status() == WL_CONNECTED) {
        WiFi.disconnect();
        delay(100);
    }

    // Start connection
    Serial.printf("[WiFi] Connecting to: %s\n", _pendingSsid.c_str());
    updateState(ProvisioningState::CONNECTING);

    WiFi.mode(WIFI_STA);
    WiFi.begin(_pendingSsid.c_str(), _pendingPassword.c_str());

    _wifiConnectStartTime = millis();
    _wifiConnecting = true;
}

void BLEProvisioning::disconnectWiFi() {
    Serial.println("[WiFi] Disconnecting");
    WiFi.disconnect();
    _wifiConnecting = false;
    updateState(ProvisioningState::DISCONNECTED);
}

void BLEProvisioning::handleCommand(uint8_t cmd) {
    Serial.printf("[BLE] Command received: 0x%02X\n", cmd);

    switch (cmd) {
        case CMD_CONNECT:
            // Use pending credentials if set, otherwise try stored
            if (_pendingSsid.length() == 0) {
                String ssid, password;
                if (_credentialStore.loadCredentials(ssid, password)) {
                    _pendingSsid = ssid;
                    _pendingPassword = password;
                }
            }
            connectToWiFi();
            break;

        case CMD_DISCONNECT:
            disconnectWiFi();
            break;

        case CMD_CLEAR:
            _credentialStore.clearCredentials();
            _pendingSsid = "";
            _pendingPassword = "";
            disconnectWiFi();
            updateState(ProvisioningState::NO_CREDENTIALS);
            break;

        default:
            Serial.printf("[BLE] Unknown command: 0x%02X\n", cmd);
            break;
    }
}

void BLEProvisioning::updateState(ProvisioningState newState) {
    if (_state != newState) {
        _state = newState;
        Serial.printf("[BLE] State changed to: %d\n", static_cast<uint8_t>(_state));
        notifyStatus();
    }
}

void BLEProvisioning::notifyStatus() {
    if (_pStatusChar && _bleClientConnected) {
        uint8_t status = static_cast<uint8_t>(_state);
        _pStatusChar->setValue(&status, 1);
        _pStatusChar->notify();
        Serial.printf("[BLE] Status notified: %d\n", status);
    }
}

bool BLEProvisioning::isWiFiConnected() const {
    return WiFi.status() == WL_CONNECTED;
}

String BLEProvisioning::getConnectedSSID() const {
    if (isWiFiConnected()) {
        return WiFi.SSID();
    }
    return "";
}

String BLEProvisioning::getIPAddress() const {
    if (isWiFiConnected()) {
        return WiFi.localIP().toString();
    }
    return "";
}

int BLEProvisioning::getRSSI() const {
    if (isWiFiConnected()) {
        return WiFi.RSSI();
    }
    return 0;
}

// =============================================================================
// NimBLE Callbacks (NimBLE 1.4.x signatures)
// =============================================================================

void BLEProvisioning::onConnect(NimBLEServer* pServer) {
    _bleClientConnected = true;
    Serial.println("[BLE] Client connected");
    Serial.printf("[BLE] Connected clients: %d\n", pServer->getConnectedCount());
}

void BLEProvisioning::onDisconnect(NimBLEServer* pServer) {
    _bleClientConnected = false;
    Serial.println("[BLE] Client disconnected");

    // Set flag to restart advertising from main loop
    // (avoid calling BLE functions from callback context)
    _needsAdvertisingRestart = true;
    _disconnectTime = millis();
}

void BLEProvisioning::onWrite(NimBLECharacteristic* pCharacteristic) {
    std::string uuid = pCharacteristic->getUUID().toString();
    std::string value = pCharacteristic->getValue();

    if (uuid == CHAR_UUID_SSID) {
        _pendingSsid = String(value.c_str());
        Serial.printf("[BLE] SSID received: %s\n", _pendingSsid.c_str());
    }
    else if (uuid == CHAR_UUID_PASSWORD) {
        _pendingPassword = String(value.c_str());
        Serial.println("[BLE] Password received: ****");
    }
    else if (uuid == CHAR_UUID_COMMAND) {
        if (value.length() > 0) {
            handleCommand(value[0]);
        }
    }
}

void BLEProvisioning::onRead(NimBLECharacteristic* pCharacteristic) {
    std::string uuid = pCharacteristic->getUUID().toString();

    if (uuid == CHAR_UUID_SSID) {
        // Return stored SSID or pending SSID
        String ssid = _pendingSsid.length() > 0 ? _pendingSsid : _credentialStore.getStoredSSID();
        pCharacteristic->setValue(ssid.c_str());
    }
    else if (uuid == CHAR_UUID_STATUS) {
        uint8_t status = static_cast<uint8_t>(_state);
        pCharacteristic->setValue(&status, 1);
    }
}
