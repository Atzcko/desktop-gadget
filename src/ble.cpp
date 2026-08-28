#include "ble.h"
#include "emotion.h"
#include "settings.h"
#include <string.h>

#include <Arduino.h>
#include <NimBLEDevice.h>
#include <NimBLEHIDDevice.h>
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

/*
 * Standard BLE keyboard report descriptor.
 *
 * Its purpose here is DISCOVERABILITY, not typing. macOS System Settings >
 * Bluetooth only lists devices implementing a profile it knows how to pair
 * with — classic BT, or BLE HID/audio. A custom GATT peripheral, however
 * well it advertises, is invisible there by design. Presenting a HID
 * keyboard service is what puts "Flip Clock" in that list and makes
 * Connect work.
 *
 * No key reports are ever sent. The descriptor is the ticket in; the
 * emotion API still travels over the NUS service alongside it.
 */
static const uint8_t HID_REPORT_MAP[] = {
    0x05, 0x01,  /* Usage Page (Generic Desktop)      */
    0x09, 0x06,  /* Usage (Keyboard)                  */
    0xA1, 0x01,  /* Collection (Application)          */
    0x85, 0x01,  /*   Report ID (1)                   */
    0x05, 0x07,  /*   Usage Page (Key Codes)          */
    0x19, 0xE0,  /*   Usage Minimum (224)             */
    0x29, 0xE7,  /*   Usage Maximum (231)             */
    0x15, 0x00,  /*   Logical Minimum (0)             */
    0x25, 0x01,  /*   Logical Maximum (1)             */
    0x75, 0x01,  /*   Report Size (1)                 */
    0x95, 0x08,  /*   Report Count (8)                */
    0x81, 0x02,  /*   Input (Data,Var,Abs) modifiers  */
    0x95, 0x01,  /*   Report Count (1)                */
    0x75, 0x08,  /*   Report Size (8)                 */
    0x81, 0x01,  /*   Input (Const) reserved          */
    0x95, 0x05,  /*   Report Count (5)                */
    0x75, 0x01,  /*   Report Size (1)                 */
    0x05, 0x08,  /*   Usage Page (LEDs)               */
    0x19, 0x01,  /*   Usage Minimum (1)               */
    0x29, 0x05,  /*   Usage Maximum (5)               */
    0x91, 0x02,  /*   Output (Data,Var,Abs) LEDs      */
    0x95, 0x01,  /*   Report Count (1)                */
    0x75, 0x03,  /*   Report Size (3)                 */
    0x91, 0x01,  /*   Output (Const) padding          */
    0x95, 0x06,  /*   Report Count (6)                */
    0x75, 0x08,  /*   Report Size (8)                 */
    0x15, 0x00,  /*   Logical Minimum (0)             */
    0x25, 0x65,  /*   Logical Maximum (101)           */
    0x05, 0x07,  /*   Usage Page (Key Codes)          */
    0x19, 0x00,  /*   Usage Minimum (0)               */
    0x29, 0x65,  /*   Usage Maximum (101)             */
    0x81, 0x00,  /*   Input (Data,Array) keys         */
    0xC0,        /* End Collection                    */

    /*
     * Standard relative mouse, Report ID 2 (D051). Present in BOTH identities:
     * a pointing device does not summon Keyboard Setup Assistant, so the
     * gadget identity keeps its meaning while gaining a cursor.
     */
    0x05, 0x01,        /* Usage Page (Generic Desktop)         */
    0x09, 0x02,        /* Usage (Mouse)                        */
    0xA1, 0x01,        /* Collection (Application)             */
    0x85, 0x02,        /*   Report ID (2)                      */
    0x09, 0x01,        /*   Usage (Pointer)                    */
    0xA1, 0x00,        /*   Collection (Physical)              */
    0x05, 0x09,        /*     Usage Page (Buttons)             */
    0x19, 0x01,        /*     Usage Minimum (1)                */
    0x29, 0x03,        /*     Usage Maximum (3)                */
    0x15, 0x00,        /*     Logical Minimum (0)              */
    0x25, 0x01,        /*     Logical Maximum (1)              */
    0x95, 0x03,        /*     Report Count (3)                 */
    0x75, 0x01,        /*     Report Size (1)                  */
    0x81, 0x02,        /*     Input (Data,Var,Abs) buttons     */
    0x95, 0x01,        /*     Report Count (1)                 */
    0x75, 0x05,        /*     Report Size (5)                  */
    0x81, 0x03,        /*     Input (Const) padding            */
    0x05, 0x01,        /*     Usage Page (Generic Desktop)     */
    0x09, 0x30,        /*     Usage (X)                        */
    0x09, 0x31,        /*     Usage (Y)                        */
    0x09, 0x38,        /*     Usage (Wheel)                    */
    0x15, 0x81,        /*     Logical Minimum (-127)           */
    0x25, 0x7F,        /*     Logical Maximum (127)            */
    0x75, 0x08,        /*     Report Size (8)                  */
    0x95, 0x03,        /*     Report Count (3)                 */
    0x81, 0x06,        /*     Input (Data,Var,Rel)             */
    0xC0,              /*   End Collection                     */
    0xC0,              /* End Collection                       */

};

/*
 * Vendor-defined HID descriptor — the "desktop gadget" identity.
 *
 * Using a vendor usage page (0xFF00) instead of Generic Desktop / Keyboard
 * means macOS enumerates this as a generic HID device rather than an input
 * keyboard. Two consequences, both wanted:
 *
 *   - Keyboard Setup Assistant never appears. It only fires because the OS
 *     currently believes an unknown keyboard just arrived.
 *   - The device cannot be mistaken for a text-input device by anything.
 *
 * Two 32-byte reports are declared. The OUTPUT report is host -> device and
 * is wired to the emotion engine, so this is a real gadget protocol rather
 * than a descriptor that exists only to satisfy the pairing UI.
 */
static const uint8_t GADGET_REPORT_MAP[] = {
    0x06, 0x00, 0xFF,  /* Usage Page (Vendor Defined 0xFF00)   */
    0x09, 0x01,        /* Usage (0x01) — desktop gadget        */
    0xA1, 0x01,        /* Collection (Application)             */
    0x85, 0x01,        /*   Report ID (1)                      */
    0x09, 0x02,        /*   Usage (0x02) — status in           */
    0x15, 0x00,        /*   Logical Minimum (0)                */
    0x26, 0xFF, 0x00,  /*   Logical Maximum (255)              */
    0x75, 0x08,        /*   Report Size (8)                    */
    0x95, 0x20,        /*   Report Count (32)                  */
    0x81, 0x02,        /*   Input (Data,Var,Abs)               */
    0x09, 0x03,        /*   Usage (0x03) — command out         */
    0x15, 0x00,
    0x26, 0xFF, 0x00,
    0x75, 0x08,
    0x95, 0x20,        /*   Report Count (32)                  */
    0x91, 0x02,        /*   Output (Data,Var,Abs)              */
    0xC0,              /* End Collection                       */

    /*
     * Standard relative mouse, Report ID 2 (D051). Present in BOTH identities:
     * a pointing device does not summon Keyboard Setup Assistant, so the
     * gadget identity keeps its meaning while gaining a cursor.
     */
    0x05, 0x01,        /* Usage Page (Generic Desktop)         */
    0x09, 0x02,        /* Usage (Mouse)                        */
    0xA1, 0x01,        /* Collection (Application)             */
    0x85, 0x02,        /*   Report ID (2)                      */
    0x09, 0x01,        /*   Usage (Pointer)                    */
    0xA1, 0x00,        /*   Collection (Physical)              */
    0x05, 0x09,        /*     Usage Page (Buttons)             */
    0x19, 0x01,        /*     Usage Minimum (1)                */
    0x29, 0x03,        /*     Usage Maximum (3)                */
    0x15, 0x00,        /*     Logical Minimum (0)              */
    0x25, 0x01,        /*     Logical Maximum (1)              */
    0x95, 0x03,        /*     Report Count (3)                 */
    0x75, 0x01,        /*     Report Size (1)                  */
    0x81, 0x02,        /*     Input (Data,Var,Abs) buttons     */
    0x95, 0x01,        /*     Report Count (1)                 */
    0x75, 0x05,        /*     Report Size (5)                  */
    0x81, 0x03,        /*     Input (Const) padding            */
    0x05, 0x01,        /*     Usage Page (Generic Desktop)     */
    0x09, 0x30,        /*     Usage (X)                        */
    0x09, 0x31,        /*     Usage (Y)                        */
    0x09, 0x38,        /*     Usage (Wheel)                    */
    0x15, 0x81,        /*     Logical Minimum (-127)           */
    0x25, 0x7F,        /*     Logical Maximum (127)            */
    0x75, 0x08,        /*     Report Size (8)                  */
    0x95, 0x03,        /*     Report Count (3)                 */
    0x81, 0x06,        /*     Input (Data,Var,Rel)             */
    0xC0,              /*   End Collection                     */
    0xC0,              /* End Collection                       */

    /*
     * Standard keyboard, Report ID 3 (D052). Both identities carry it: the
     * owner asked the gadget to type, so "cannot be mistaken for a keyboard"
     * (D022) is deliberately traded for "is one, on request". 8-byte boot
     * report: modifiers, reserved, six keycodes.
     */
    0x05, 0x01,        /* Usage Page (Generic Desktop)         */
    0x09, 0x06,        /* Usage (Keyboard)                     */
    0xA1, 0x01,        /* Collection (Application)             */
    0x85, 0x03,        /*   Report ID (3)                      */
    0x05, 0x07,        /*   Usage Page (Key Codes)             */
    0x19, 0xE0,        /*   Usage Minimum (224)                */
    0x29, 0xE7,        /*   Usage Maximum (231)                */
    0x15, 0x00,        /*   Logical Minimum (0)                */
    0x25, 0x01,        /*   Logical Maximum (1)                */
    0x75, 0x01,        /*   Report Size (1)                    */
    0x95, 0x08,        /*   Report Count (8)                   */
    0x81, 0x02,        /*   Input (Var) modifiers              */
    0x95, 0x01,        /*   Report Count (1)                   */
    0x75, 0x08,        /*   Report Size (8)                    */
    0x81, 0x01,        /*   Input (Const) reserved             */
    0x95, 0x06,        /*   Report Count (6)                   */
    0x75, 0x08,        /*   Report Size (8)                    */
    0x15, 0x00,        /*   Logical Minimum (0)                */
    0x25, 0x65,        /*   Logical Maximum (101)              */
    0x05, 0x07,        /*   Usage Page (Key Codes)             */
    0x19, 0x00,        /*   Usage Minimum (0)                  */
    0x29, 0x65,        /*   Usage Maximum (101)                */
    0x81, 0x00,        /*   Input (Array) keys                 */
    0xC0,              /* End Collection                       */
};

#define APPEARANCE_GENERIC_HID 0x03C0
#define APPEARANCE_KEYBOARD    0x03C1
#define APPEARANCE_CLOCK       0x0100
#define UUID_HID_SERVICE       ((uint16_t)0x1812)
#define UUID_MODEL_NUMBER      ((uint16_t)0x2A24)

/* Compact binary emotion command over the HID output report:
 *   [0] 0xE0 magic   [1] state 1..6   [2..3] duration_s LE   [4..] message */
#define GADGET_CMD_MAGIC 0xE0

static NimBLEServer         *server;
static NimBLECharacteristic *tx_char;
static NimBLEHIDDevice      *hid;
static NimBLECharacteristic *mouse_input;
static NimBLECharacteristic *key_input;
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

/* Host -> device on the vendor HID output report. Same validation and the
 * same queue as BLE NUS and HTTP — see D018. */
class HidOutCallbacks : public NimBLECharacteristicCallbacks {
    void onWrite(NimBLECharacteristic *c) override
    {
        std::string v = c->getValue();
        if (v.size() < 4 || (uint8_t)v[0] != GADGET_CMD_MAGIC) return;

        EmotionRequest req;
        memset(&req, 0, sizeof(req));
        req.state = (uint8_t)v[1];
        if (req.state < 1 || req.state >= EMOTION_COUNT) return;

        uint16_t dur = (uint8_t)v[2] | ((uint16_t)(uint8_t)v[3] << 8);
        if (dur < 1) dur = 5;
        if (dur > EMOTION_MAX_SECONDS) dur = EMOTION_MAX_SECONDS;
        req.duration_s = dur;

        if (v.size() > 4) {
            size_t n = v.size() - 4;
            if (n > EMOTION_MSG_MAX) n = EMOTION_MSG_MAX;
            memcpy(req.message, v.data() + 4, n);
            req.message[n] = '\0';
        }
        emotion_post(req);
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

static ServerCallbacks  server_cb;
static RxCallbacks      rx_cb;
static HidOutCallbacks  hid_out_cb;

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

    if (s.ble_hid) {
        /* HID characteristics must be encrypted, so pairing is mandatory.
         * Just Works (no passkey): the clock has a touchscreen but no way to
         * show a 6-digit code mid-pairing without hijacking the display, and
         * MITM protection buys nothing for a device that sends no keystrokes. */
        NimBLEDevice::setSecurityAuth(/*bond=*/true, /*mitm=*/false, /*sc=*/true);
        NimBLEDevice::setSecurityIOCap(BLE_HS_IO_NO_INPUT_OUTPUT);
    }

    server = NimBLEDevice::createServer();
    server->setCallbacks(&server_cb);

    if (s.ble_hid) {
        hid = new NimBLEHIDDevice(server);
        hid->manufacturer()->setValue("LilyGO");

        /* Vendor ID source 0x02 = USB-IF. 0x303A is Espressif's real VID and
         * this genuinely is an Espressif part, so this is not VID squatting.
         * The product ID is ours. */
        hid->pnp(0x02, 0x303A, 0x4001, 0x0100);
        hid->hidInfo(0x00, 0x01);

        /* A model string so the host has something better than a raw PID to
         * show in device details. */
        NimBLECharacteristic *model = hid->deviceInfo()->createCharacteristic(
            UUID_MODEL_NUMBER, NIMBLE_PROPERTY::READ);
        model->setValue(s.ble_as_keyboard ? "Flip Clock (HID keyboard)"
                                          : "Flip Clock Desktop Gadget");

        if (s.ble_as_keyboard) {
            hid->reportMap((uint8_t *)HID_REPORT_MAP, sizeof(HID_REPORT_MAP));
            /* The fallback identity IS a keyboard on ID 1 — typing uses
             * that native report; a second keyboard collection would be a
             * malformed map (v1.30.1). */
            key_input = hid->inputReport(1);
        } else {
            hid->reportMap((uint8_t *)GADGET_REPORT_MAP, sizeof(GADGET_REPORT_MAP));
            hid->inputReport(1);
            NimBLECharacteristic *out = hid->outputReport(1);
            out->setCallbacks(&hid_out_cb);
            key_input = hid->inputReport(3);    /* gadget identity: ID 3   */
        }
        mouse_input = hid->inputReport(2);      /* both identities (D051) */
        hid->startServices();
    }

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
    /*
     * The appearance and the 16-bit HID service UUID are what macOS reads to
     * decide this is a pairable input device. Without both, it stays out of
     * System Settings > Bluetooth no matter how strong the signal.
     *
     * Budget check, 31-byte limit: flags 3 + appearance 4 + 16-bit service
     * list 4 + name (2 + len). "Flip Clock" -> 23 bytes. Fits.
     */
    if (s.ble_hid) {
        /* Generic HID rather than Keyboard: same "pairable input device"
         * classification, without claiming to be something that types. */
        advData.setAppearance(s.ble_as_keyboard ? APPEARANCE_KEYBOARD
                                                : APPEARANCE_GENERIC_HID);
        advData.setCompleteServices(NimBLEUUID(UUID_HID_SERVICE));
    } else {
        advData.setAppearance(APPEARANCE_CLOCK);
    }
    adv->setAdvertisementData(advData);

    /* The 128-bit NUS UUID is 18 bytes on its own — it only fits in the
     * scan response once HID has claimed the primary packet. */
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

    Serial.printf("[ble] advertising as \"%s\" — %s\n", s.ble_name,
                  !s.ble_hid       ? "GATT only, not listed in Bluetooth settings"
                  : s.ble_as_keyboard ? "HID keyboard (compatibility mode)"
                                      : "custom HID desktop gadget");
}

void ble_stop(void)
{
    if (!running) return;
    NimBLEDevice::stopAdvertising();
    NimBLEDevice::deinit(true);
    server   = nullptr;
    tx_char  = nullptr;
    hid      = nullptr;
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

void ble_clear_bonds(void)
{
    NimBLEDevice::deleteAllBonds();
    Serial.println("[ble] cleared all bonds — remove the device on the host too");
}

bool ble_is_running(void)   { return running; }
bool ble_is_connected(void) { return connected; }

/*
 * The trackpad app's whole transport (D051): a 4-byte relative report.
 * Safe to call from the LVGL task - NimBLE's notify is task-safe, and this
 * sends nothing unless a host is connected and subscribed.
 */
bool ble_mouse(uint8_t buttons, int8_t dx, int8_t dy, int8_t wheel)
{
    if (!mouse_input || !ble_is_connected()) return false;
    uint8_t r[4] = { buttons, (uint8_t)dx, (uint8_t)dy, (uint8_t)wheel };
    mouse_input->setValue(r, sizeof(r));
    mouse_input->notify();
    return true;
}

/*
 * One keystroke: press with modifiers, then all-up. Back-to-back notifies
 * are fine at human typing rates; a stuck key needs the release to be
 * unconditional, so it is not a separate call anyone can forget.
 */
bool ble_key(uint8_t modifiers, uint8_t keycode)
{
    if (!key_input || !ble_is_connected()) return false;
    uint8_t press[8] = { modifiers, 0, keycode, 0, 0, 0, 0, 0 };
    key_input->setValue(press, sizeof(press));
    key_input->notify();
    uint8_t up[8] = { 0 };
    key_input->setValue(up, sizeof(up));
    key_input->notify();
    return true;
}

/* Subscription truth for /health: a bonded host that paired against an older
 * report map subscribes to nothing new, and that silence is otherwise
 * indistinguishable from every other keyboard failure (v1.30.1). */
int ble_mouse_subs(void) { return mouse_input ? (int)mouse_input->getSubscribedCount() : -1; }
int ble_key_subs(void)   { return key_input   ? (int)key_input->getSubscribedCount()   : -1; }
