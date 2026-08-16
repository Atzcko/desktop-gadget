#include "ble.h"
#include "emotion.h"
#include "settings.h"

#include <Arduino.h>
#include <NimBLEDevice.h>
#include <WiFi.h>

/*
 * Nordic UART Service UUIDs. Deliberately not a bespoke profile: NUS is
 * what every BLE tool, phone app and bleak example already understands, so
 * the device is usable from a generic scanner with no custom decoder.
 *
 *   RX  client -> device : JSON, same schema as POST /emotion
 *   TX  device -> client : "ok <state> <seconds>" or "err <reason>"
 */
#define NUS_SERVICE "6E400001-B5A3-F393-E0A9-E50E24DCCA9E"
#define NUS_RX      "6E400002-B5A3-F393-E0A9-E50E24DCCA9E"
#define NUS_TX      "6E400003-B5A3-F393-E0A9-E50E24DCCA9E"

static NimBLEServer         *server;
static NimBLECharacteristic *tx_char;
static bool running;
static bool connected;

static void notify(const char *msg)
{
    if (tx_char && connected) {
        tx_char->setValue((uint8_t *)msg, strlen(msg));
        tx_char->notify();
    }
}

class ServerCallbacks : public NimBLEServerCallbacks {
    void onConnect(NimBLEServer *, ble_gap_conn_desc *) override
    {
        connected = true;
        Serial.println("[ble] client connected");
    }
    void onDisconnect(NimBLEServer *) override
    {
        connected = false;
        Serial.println("[ble] client disconnected, advertising again");
        /* Without this the device is invisible after the first client
         * leaves, which reads as "BLE stopped working". */
        NimBLEDevice::startAdvertising();
    }
};

class RxCallbacks : public NimBLECharacteristicCallbacks {
    void onWrite(NimBLECharacteristic *c) override
    {
        std::string v = c->getValue();
        if (v.empty()) return;

        EmotionRequest req;
        char err[96];
        if (!emotion_parse(v.c_str(), &req, err, sizeof(err))) {
            Serial.printf("[ble] rejected: %s\n", err);
            char out[128];
            snprintf(out, sizeof(out), "err %s", err);
            notify(out);
            return;
        }
        if (!emotion_post(req)) {
            notify("err queue full");
            return;
        }
        char out[64];
        snprintf(out, sizeof(out), "ok %s %u",
                 emotion_name(req.state), req.duration_s);
        notify(out);
    }
};

static ServerCallbacks server_cb;
static RxCallbacks     rx_cb;

void ble_begin(void)
{
    Settings &s = settings_get();
    if (!s.ble_enabled) {
        Serial.println("[ble] disabled in settings");
        return;
    }
    if (running) return;

    NimBLEDevice::init(s.ble_name);
    /* +9 dBm: the clock sits on a desk and a laptop may be a room away.
     * Power draw is irrelevant — this device is USB-fed. */
    NimBLEDevice::setPower(ESP_PWR_LVL_P9);

    server = NimBLEDevice::createServer();
    server->setCallbacks(&server_cb);

    NimBLEService *svc = server->createService(NUS_SERVICE);

    NimBLECharacteristic *rx = svc->createCharacteristic(
        NUS_RX, NIMBLE_PROPERTY::WRITE | NIMBLE_PROPERTY::WRITE_NR);
    rx->setCallbacks(&rx_cb);

    tx_char = svc->createCharacteristic(NUS_TX, NIMBLE_PROPERTY::NOTIFY);

    svc->start();

    /*
     * Advertising payload is 31 bytes. A 128-bit service UUID eats 18 of
     * them, which leaves too little for the name — NimBLE then silently
     * demotes the name to the scan response, and a scanner doing a passive
     * scan never sees it. The device appears as "(unnamed)".
     *
     * So: NAME in the primary advertisement, service UUID in the scan
     * response. The name is what a human identifies the device by; the UUID
     * only matters once something has decided to connect.
     */
    NimBLEAdvertising *adv = NimBLEDevice::getAdvertising();

    NimBLEAdvertisementData advData;
    advData.setFlags(BLE_HS_ADV_F_DISC_GEN | BLE_HS_ADV_F_BREDR_UNSUP);
    advData.setName(s.ble_name);
    advData.setAppearance(0x0100);          /* Generic Clock */
    adv->setAdvertisementData(advData);

    NimBLEAdvertisementData scanData;
    scanData.setCompleteServices(NimBLEUUID(NUS_SERVICE));
    adv->setScanResponseData(scanData);

    adv->setScanResponse(true);
    /* Fast-ish interval: this device is mains-powered, so there is no
     * reason to make a laptop wait to discover it. 100 ms units of 0.625 ms. */
    adv->setMinInterval(160);   /* 100 ms */
    adv->setMaxInterval(320);   /* 200 ms */
    adv->start();

    running = true;

    /* If Wi-Fi is already up (BLE toggled on from Settings rather than at
     * boot), it must start yielding the radio right now. */
    if (WiFi.getMode() != WIFI_MODE_NULL) WiFi.setSleep(true);

    Serial.printf("[ble] advertising as \"%s\"\n", s.ble_name);
}

void ble_stop(void)
{
    if (!running) return;
    NimBLEDevice::stopAdvertising();
    NimBLEDevice::deinit(true);
    server   = nullptr;
    tx_char  = nullptr;
    running  = false;
    connected = false;
    if (WiFi.getMode() != WIFI_MODE_NULL) WiFi.setSleep(false);
    Serial.println("[ble] stopped");
}

void ble_apply_name(const char *name)
{
    if (!running) { ble_begin(); return; }
    /* NimBLE cannot rename a running stack cleanly, so bounce it. This is
     * only ever triggered by a human editing the name in Settings. */
    ble_stop();
    ble_begin();
}

bool ble_is_running(void)   { return running; }
bool ble_is_connected(void) { return connected; }
