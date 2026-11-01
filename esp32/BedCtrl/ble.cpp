#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLE2902.h>
#include "globals.h"

// -------------------------------------------------------
//  Service BLE — seul canal de commande du lit
//
//  Toujours actif. Caractéristiques (UUID dans globals.h) :
//    - COMMAND (write)       : commande JSON
//        * lit     : {"source","beds","parts","push"} (ex-format MQTT)
//        * système : {"cmd":"wifi_ap"} → portail web en mode AP
//                    {"cmd":"restart"}
//    - CONFIG  (read/write)  : lecture = config courante (sans mot de passe),
//                              écriture = JSON des champs à modifier
//                              (format /save-wifi) → sauvegarde + redémarrage
//    - STATUS  (read/notify) : {"ok":bool,"message":"..."} du dernier
//                              message CONFIG ou système traité
//
//  Les callbacks tournent dans la tâche Bluetooth : ils ne font que mettre le
//  message en file. Tout est traité dans ble_loop(), appelé depuis loop(),
//  donc sans concurrence avec la gestion des moteurs ni avec la NVS.
// -------------------------------------------------------
enum BleMsgType : uint8_t { BLE_MSG_COMMAND, BLE_MSG_CONFIG, BLE_MSG_DISCONNECT };

struct BleMsg {

    BleMsgType type;
    char       json[BLE_MSG_MAX_LEN];
};

static bool                       ble_started     = false;
static QueueHandle_t              queue           = nullptr;
static BLECharacteristic*         status_char     = nullptr;
static bool                       restart_pending = false;
static uint32_t                   restart_at_ms   = 0;
static ble_on_command_callback    on_command_cb   = nullptr;
static ble_on_disconnect_callback on_disconnect_cb = nullptr;
static void*                      cb_context      = nullptr;

// Appelé depuis la tâche Bluetooth
static void enqueue(BleMsgType type, const String& json) {

    BleMsg msg;
    msg.type = type;
    if (json.length() >= sizeof(msg.json)) {

        Serial.println("[BLE] Message trop long, ignoré");
        return;
    }
    strcpy(msg.json, json.c_str());
    if (xQueueSend(queue, &msg, 0) != pdTRUE) {

        Serial.println("[BLE] File pleine, message perdu");
    }
}

static void set_status(bool ok, const String& message) {

    JsonDocument doc;
    doc["ok"]      = ok;
    doc["message"] = message;
    String out;
    serializeJson(doc, out);
    status_char->setValue(out);
    status_char->notify();
}

static void schedule_restart(void) {

    restart_at_ms   = millis() + BLE_RESTART_DELAY_MS;
    restart_pending = true;
}

class ConfigCallbacks : public BLECharacteristicCallbacks {

    void onRead(BLECharacteristic* characteristic) override {

        JsonDocument doc;
        doc["version"]     = VERSION;
        doc["hostname"]    = g_config.hostname;
        doc["wifi_ssid"]   = g_config.wifi_ssid;
        doc["wifi_mode"]   = wifi_get_mode();
        doc["ip"]          = wifi_get_ip_address();
        doc["wdt_s"]       = g_config.wdt_s;
        doc["light_delay"] = g_config.light_delay;
        String out;
        serializeJson(doc, out);
        characteristic->setValue(out);
    }

    void onWrite(BLECharacteristic* characteristic) override {

        enqueue(BLE_MSG_CONFIG, characteristic->getValue());
        // Ne pas garder la valeur écrite (elle peut contenir un mot de passe)
        characteristic->setValue("");
    }
};

class CommandCallbacks : public BLECharacteristicCallbacks {

    void onWrite(BLECharacteristic* characteristic) override {

        enqueue(BLE_MSG_COMMAND, characteristic->getValue());
    }
};

class ServerCallbacks : public BLEServerCallbacks {

    void onConnect(BLEServer*) override {

        // Bluedroid coupe l'annonce à chaque connexion : on la relance pour
        // qu'un second téléphone puisse aussi se connecter.
        Serial.println("[BLE] Client connected");
        BLEDevice::startAdvertising();
    }

    void onDisconnect(BLEServer*) override {

        Serial.println("[BLE] Client disconnected");
        enqueue(BLE_MSG_DISCONNECT, "");
        BLEDevice::startAdvertising();
    }
};

// -------------------------------------------------------
//  Traitement (contexte loop())
// -------------------------------------------------------
static void handle_config(const char* json) {

    String error;
    if (config_update_from_json(json, error)) {

        config_save();
        set_status(true, "✅ Saved — restart in 2s…");
        Serial.println("[BLE] Config saved → restart");
        schedule_restart();
    } else {

        set_status(false, error);
        Serial.println("[BLE] Config rejected: " + error);
    }
}

static void handle_command(const char* json) {

    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, json);
    if (error) {

        Serial.println("BLE unexpected message: " + String(json));
        Serial.println(error.f_str());
        return;
    }
    if (doc.containsKey("cmd")) {

        // Commandes système
        String cmd = doc["cmd"].as<String>();
        Serial.println("[BLE] System command: " + cmd);
        if (cmd == "wifi_ap") {

            wifi_start_ap();
            set_status(true, "Portail actif : WiFi " AP_SSID " → http://" AP_IP);
        } else if (cmd == "restart") {

            set_status(true, "Redémarrage…");
            schedule_restart();
        } else {

            set_status(false, "Commande inconnue : " + cmd);
        }
    } else if (on_command_cb) {

        on_command_cb(cb_context, doc);
    }
}

// -------------------------------------------------------
//  API
// -------------------------------------------------------
void ble_setup(ble_on_command_callback on_command, ble_on_disconnect_callback on_disconnect, void* context) {

    if (ble_started) {

        return;
    }
    ble_started      = true;
    on_command_cb    = on_command;
    on_disconnect_cb = on_disconnect;
    cb_context       = context;
    queue            = xQueueCreate(BLE_QUEUE_LEN, sizeof(BleMsg));

    BLEDevice::init(g_config.hostname.c_str());
    BLEDevice::setMTU(512); // JSON en une seule trame (l'appli demande 512)

    BLEServer* server = BLEDevice::createServer();
    server->setCallbacks(new ServerCallbacks());

    BLEService* service = server->createService(BLE_SERVICE_UUID);
    BLECharacteristic* command_char = service->createCharacteristic(
        BLE_CHAR_COMMAND_UUID,
        BLECharacteristic::PROPERTY_WRITE | BLECharacteristic::PROPERTY_WRITE_NR);
    command_char->setCallbacks(new CommandCallbacks());
    BLECharacteristic* config_char = service->createCharacteristic(
        BLE_CHAR_CONFIG_UUID,
        BLECharacteristic::PROPERTY_READ | BLECharacteristic::PROPERTY_WRITE);
    config_char->setCallbacks(new ConfigCallbacks());
    status_char = service->createCharacteristic(
        BLE_CHAR_STATUS_UUID,
        BLECharacteristic::PROPERTY_READ | BLECharacteristic::PROPERTY_NOTIFY);
    status_char->addDescriptor(new BLE2902()); // Bluedroid ne le crée pas seul
    status_char->setValue("{\"ok\":true,\"message\":\"\"}");
    service->start();

    // UUID du service dans l'annonce (filtre de scan de l'appli),
    // nom dans la réponse de scan (pas de place pour les deux dans 31 octets)
    BLEAdvertising* advertising = BLEDevice::getAdvertising();
    advertising->addServiceUUID(BLE_SERVICE_UUID);
    advertising->setScanResponse(true);
    BLEDevice::startAdvertising();
    Serial.println("[BLE] Advertising as: " + g_config.hostname);
    // WROOM-32 sans PSRAM : Bluedroid + WiFi + WebServer, à surveiller
    Serial.printf("[BLE] Free heap: %u bytes\n", ESP.getFreeHeap());
}

void ble_loop(void) {

    BleMsg msg;
    while (queue && xQueueReceive(queue, &msg, 0) == pdTRUE) {

        switch (msg.type) {

            case BLE_MSG_COMMAND:
                handle_command(msg.json);
                break;
            case BLE_MSG_CONFIG:
                handle_config(msg.json);
                break;
            case BLE_MSG_DISCONNECT:
                if (on_disconnect_cb) {

                    on_disconnect_cb(cb_context);
                }
                break;
        }
    }
    if (restart_pending && (int32_t)(millis() - restart_at_ms) >= 0) {

        ESP.restart();
    }
}
