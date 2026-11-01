#include <ArduinoJson.h>
#include <WebServer.h>
#include <Update.h>
#include <esp_task_wdt.h>
#include <esp_ota_ops.h>
#include "globals.h"

// -------------------------------------------------------
//  CSS + JS partagés
// -------------------------------------------------------
static const char STYLE[] PROGMEM = R"rawliteral(
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<style>
    *{box-sizing:border-box;margin:0;padding:0}
    body{font-family:sans-serif;background:#1a1a2e;color:#e0e0e0;min-height:100vh;display:flex;align-items:center;justify-content:center;padding:16px}
    .card{background:#16213e;border-radius:12px;padding:28px 32px;width:100%;max-width:480px;box-shadow:0 4px 24px #0004}
    h1{font-size:1.3rem;font-weight:600;margin-bottom:4px;color:#fff}
    .sub{font-size:.85rem;color:#7b8fa1;margin-bottom:24px}
    .badge{display:inline-block;font-size:.75rem;padding:2px 10px;border-radius:20px;margin-bottom:20px}
    .badge.ap{background:#3d1a00;color:#ff7043}
    .badge.sta{background:#0a2e1a;color:#4caf50}
    label{display:block;font-size:.82rem;color:#9aa5b4;margin-bottom:4px;margin-top:14px}
    input[type=text],input[type=password],input[type=number]{width:100%;padding:9px 12px;border-radius:8px;border:1px solid #2a3a50;background:#0f3460;color:#e0e0e0;font-size:.95rem}
    input[type=text]:focus,input[type=password]:focus,input[type=number]:focus{outline:none;border-color:#4a90e2}
    .btn{display:block;width:100%;padding:11px;margin-top:20px;border-radius:8px;border:none;font-size:1rem;font-weight:600;cursor:pointer;transition:opacity .2s}
    .btn:hover{opacity:.85}
    .btn-primary{background:#1565c0;color:#fff}
    .btn-danger{background:#b71c1c;color:#fff}
    .btn-success{background:#1b5e20;color:#fff}
    .section{margin-top:24px;padding-top:20px;border-top:1px solid #1e3050}
    .row{display:flex;gap:12px}
    .row>*{flex:1}
    #progress-bar{height:6px;border-radius:3px;background:#1565c0;width:0%;transition:width .3s;margin-top:10px}
    .version{font-size:.78rem;color:#7b8fa1;margin-top:6px}
    a{color:#4a90e2;text-decoration:none}
    /* Password with eye */
    .pw-wrap{position:relative;margin-top:0}
    .pw-wrap input{padding-right:42px}
    .pw-eye{position:absolute;right:0;top:0;height:100%;width:40px;background:none;border:none;cursor:pointer;color:#7b8fa1;display:flex;align-items:center;justify-content:center;font-size:1.1rem;transition:color .2s}
    .pw-eye:hover{color:#e0e0e0}
</style>
<script>
function togglePw(eyeBtn) {
    var wrap = eyeBtn.closest('.pw-wrap');
    var inp  = wrap.querySelector('input');
    var show = inp.type === 'password';
    inp.type = show ? 'text' : 'password';
    eyeBtn.textContent = show ? '🙈' : '👁️';
    eyeBtn.title = show ? 'Masquer' : 'Afficher';
}
</script>
)rawliteral";

// -------------------------------------------------------
//  Page 1 — Mode AP : configuration WiFi
// -------------------------------------------------------
static const char PAGE_AP[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
    %STYLE%
    <title>AP BedCtrl</title>
</head>
<body>
    <div class="card">
        <h1>⚙️ Configuration WiFi</h1>
        <p class="sub">Connectez l'ESP32 à votre réseau</p>
        <span class="badge ap">Mode Access Point</span>

        <label>Nom du réseau (SSID)</label>
        <input type="text" id="wifi_ssid" placeholder="MonReseau" required>

        <label>Mot de passe WiFi</label>
        <div class="pw-wrap">
            <input type="password" id="wifi_pass" placeholder="••••••••">
            <button type="button" class="pw-eye" onclick="togglePw(this)" title="Afficher">👁️</button>
        </div>

        <label>Nom du device (hostname)</label>
        <input type="text" id="hostname" placeholder="warm_test" value="%HOSTNAME%">

        <!-- Watchdog -->
        <div class="section">
            <h2 style="font-size:1rem;color:#90caf9;margin-bottom:4px">⏱️ Watchdog</h2>
            <label>Timeout (secondes, min 3)</label>
            <input type="number" id="wdt_s" min="3" max="300" value="%WDT_S%">
        </div>

        <button class="btn btn-primary" onclick="saveWifi()">💾 Save and restart</button>
        <div id="cfg-status" style="font-size:.82rem;color:#4caf50;margin-top:8px;min-height:18px"></div>

    </div>
    <script>
    function saveWifi() {
        var data = {
            wifi_ssid: document.getElementById('wifi_ssid').value,
            wifi_pass: document.getElementById('wifi_pass').value,
            hostname: document.getElementById('hostname').value,
            wdt_s: parseInt(document.getElementById('wdt_s').value)
        };
        let status = document.getElementById('cfg-status');
        fetch('/save-wifi', {   method: 'POST',
                                headers: {'Content-Type':'application/json'},
                                body: JSON.stringify(data)
                            }
        )
        .then(result => result.text().then(text => {
            status.textContent = text;
            if (result.redirected) {
              window.location.href = result.url;    // suit la redirection
            } else {
              window.location.href = '/';           // force le rechargement de la racine
            }
        }))
        .catch(() => { status.textContent = 'Erreur réseau'; });
    }
    </script>
</body>
</html>
)rawliteral";

// -------------------------------------------------------
//  Page 2 — Mode connecté : portail avancé
// -------------------------------------------------------
static const char PAGE_PORTAL[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
    %STYLE%
    <title>BedCtrl config</title>
</head>
<body>
    <div class="card">
        <h1>🛌🏻 %HOSTNAME%</h1>
        <p class="sub">Portail de configuration</p>
        <span class="badge sta">WiFi connecté — %IP_ADDR%</span>
        <p class="version">Firmware : <strong>%VERSION%</strong></p>

        <!-- OTA -->
        <div class="section">
            <h2 style="font-size:1rem;color:#90caf9;margin-bottom:12px">🔄 Mise à jour firmware (OTA)</h2>
            <p style="font-size:.78rem;color:#ffb74d;margin-bottom:10px">⚠️ Utilisez <strong>BedCtrl.ino.bin</strong> uniquement — jamais bootloader / partitions / merged.</p>
            <input type="file" id="fw" accept=".bin" style="color:#9aa5b4;font-size:.85rem">
            <div id="progress-bar"></div>
            <div id="ota-status" style="font-size:.82rem;color:#4caf50;margin-top:8px;min-height:18px"></div>
            <button class="btn btn-success" onclick="uploadFirmware()">⬆️ Flasher</button>
        </div>

        <!-- Lumière -->
        <div class="section">
            <h2 style="font-size:1rem;color:#90caf9;margin-bottom:4px">💡 Lumière</h2>
            <label>Délai allumage LEDs (cycles de 100 ms)</label>
            <input type="number" id="light_delay" min="0" max="600" value="%LIGHT_DELAY%">
        </div>

        <button class="btn btn-primary" onclick="saveConfig()">💾 Enregistrer la configuration</button>
        <div id="cfg-status" style="font-size:.82rem;color:#4caf50;margin-top:8px;min-height:18px"></div>

        <!-- Reset -->
        <div class="section">
            <h2 style="font-size:1rem;color:#ef9a9a;margin-bottom:8px">⚠️ Réinitialisation</h2>
            <p style="font-size:.82rem;color:#7b8fa1;margin-bottom:12px">Efface tous les paramètres en flash et redémarre (WiFi coupé, reconfiguration via l'appli en Bluetooth).</p>
            <button class="btn btn-danger" onclick="confirmReset()">🗑️ Effacer et réinitialiser</button>
        </div>
    </div>

    <script>
    function uploadFirmware() {
        var fw = document.getElementById('fw').files[0];
        if (!fw) {
            document.getElementById('ota-status').textContent='Sélectionnez un fichier .bin';
            return;
        }
        var fd = new FormData();
        fd.append('update', fw);
        var xhr = new XMLHttpRequest();
        xhr.upload.addEventListener('progress',function(e){
            if(e.lengthComputable){
                var pct = Math.round(e.loaded/e.total*100);
                document.getElementById('progress-bar').style.width=pct+'%';
                document.getElementById('ota-status').textContent='Upload : '+pct+'%';
            }
        });
        xhr.onload = function() {
            var t=(xhr.responseText||'').trim();
            document.getElementById('ota-status').textContent=(xhr.status===200&&t==='OK')?'✅ Flash réussi — redémarrage…':'❌ '+(t||('Erreur '+xhr.status));
        };
        xhr.onerror = function() {
            document.getElementById('ota-status').textContent='❌ Erreur réseau';
        };
        xhr.open('POST','/update');
        xhr.send(fd);
    }

    function saveConfig() {
        var data = {
              light_delay: parseInt(document.getElementById('light_delay').value)
        };
        let status = document.getElementById('cfg-status');
        fetch('/save-config', {   method: 'POST',
                                  headers: {'Content-Type':'application/json'},
                                  body: JSON.stringify(data)
                              }
        )
        .then(result => result.text()).then(text => { status.textContent=text; })
        .catch(() => { status.textContent='Erreur réseau'; });
    }

    function confirmReset() {
        if (confirm('Effacer toute la configuration et redémarrer ?')) {
            fetch('/reset',{method:'POST'}).then(() => {
                document.getElementById('cfg-status').textContent='Réinitialisation en cours…';
            });
        }
    }
    </script>
</body>
</html>
)rawliteral";

// -------------------------------------------------------
//  Helpers
// -------------------------------------------------------
static WebServer* server = nullptr;

static String build_ap_page(void) {

    String page = FPSTR(STYLE);
    String full = FPSTR(PAGE_AP);
    full.replace("%STYLE%", page);
    full.replace("%HOSTNAME%", g_config.hostname);
    full.replace("%WDT_S%",     String(g_config.wdt_s));
    return full;
}

static String build_portal_page(void) {

    String page = FPSTR(STYLE);
    String full = FPSTR(PAGE_PORTAL);
    full.replace("%STYLE%",     page);
    full.replace("%HOSTNAME%",  g_config.hostname);
    full.replace("%IP_ADDR%",   wifi_get_ip_address());
    full.replace("%VERSION%",   VERSION);
    full.replace("%LIGHT_DELAY%", String(g_config.light_delay));
    return full;
}

// -------------------------------------------------------
//  OTA upload
//
//  Sécurité : on valide l'en-tête de l'image AVANT de committer, pour rejeter
//  les mauvais binaires (bootloader / partitions / merged / firmware d'un autre
//  projet) qui, sans ce contrôle, passeraient le simple test du magic 0xE9 et
//  briqueraient le device (pas de rollback automatique).
//    - octet 0            : magic image ESP  = 0xE9
//    - offset 0x20 (LE)   : magic esp_app_desc_t = 0xABCD5432 (absent d'un
//                           bootloader → distingue une image applicative)
//    - offset 0x50 (32 o) : project_name → doit correspondre au firmware courant
//  Le basculement de partition de boot n'a lieu que dans Update.end(true) ;
//  toute image rejetée est annulée par Update.abort() (rien n'est committé).
// -------------------------------------------------------
static const size_t OTA_HDR_NEEDED = 0x70;   // jusqu'à la fin de project_name
static uint8_t  ota_hdr[OTA_HDR_NEEDED];
static size_t   ota_hdr_len = 0;
static bool     ota_checked = false;
static bool     ota_bad     = false;
static String   ota_message;

// Renvoie nullptr si l'image est valide, sinon un message d'erreur.
static const char* ota_validate(const uint8_t* h) {

    if (h[0] != 0xE9) {

        return "Fichier invalide (magic image 0xE9 absent)";
    }
    uint32_t desc_magic = (uint32_t)h[0x20] | ((uint32_t)h[0x21] << 8) |
                          ((uint32_t)h[0x22] << 16) | ((uint32_t)h[0x23] << 24);
    if (desc_magic != 0xABCD5432) {

        return "Pas une image applicative (bootloader/partitions/merged ?)";
    }
    // project_name du firmware en cours d'exécution
    esp_app_desc_t cur;
    const esp_partition_t* run = esp_ota_get_running_partition();
    if (run && esp_ota_get_partition_description(run, &cur) == ESP_OK &&
        cur.project_name[0] != '\0') {

        char name[33];
        memcpy(name, h + 0x50, 32);
        name[32] = '\0';
        if (strncmp(name, cur.project_name, 32) != 0) {

            return "Firmware d'un autre projet";
        }
    }
    return nullptr;
}

static void route_ota_post(void) {

    server->sendHeader("Connection", "close");
    const bool ok = !ota_bad && !Update.hasError();
    server->send(200, "text/plain", ok ? "OK" : (ota_message.isEmpty()
                                                      ? "FAIL"
                                                      : ota_message));
    if (ok) {

        ESP.restart();   // ne redémarre QUE si le flash a réussi et est validé
    }
}

static void route_ota_upload(void) {

    HTTPUpload& upload = server->upload();
    if (upload.status == UPLOAD_FILE_START) {

        esp_task_wdt_delete(NULL);
        ota_hdr_len = 0;
        ota_checked = false;
        ota_bad     = false;
        ota_message = "";
        Serial.printf("[OTA] Start: %s\n", upload.filename.c_str());
        if (!Update.begin(UPDATE_SIZE_UNKNOWN)) {

            Update.printError(Serial);
            ota_bad     = true;
            ota_message = "Update.begin a échoué (partition OTA ?)";
        }

    } else if (upload.status == UPLOAD_FILE_WRITE) {

        if (ota_bad) { return; }   // image déjà rejetée : on ne flashe plus rien

        // Accumule l'en-tête puis valide dès qu'on en a assez.
        if (!ota_checked) {

            size_t take = upload.currentSize;
            if (take > OTA_HDR_NEEDED - ota_hdr_len) {

                take = OTA_HDR_NEEDED - ota_hdr_len;
            }
            memcpy(ota_hdr + ota_hdr_len, upload.buf, take);
            ota_hdr_len += take;

            if (ota_hdr_len >= OTA_HDR_NEEDED) {

                ota_checked = true;
                const char* err = ota_validate(ota_hdr);
                if (err) {

                    ota_bad     = true;
                    ota_message = err;
                    Serial.printf("[OTA] Rejeté : %s\n", err);
                    Update.abort();   // rien n'est committé
                    return;
                }
            }
        }

        if (Update.write(upload.buf, upload.currentSize) != upload.currentSize) {

            Update.printError(Serial);
            ota_bad     = true;
            ota_message = "Erreur d'écriture flash";
            Update.abort();
        }

    } else if (upload.status == UPLOAD_FILE_END) {

        if (!ota_bad) {

            if (Update.end(true)) {

                Serial.printf("[OTA] Success: %u bytes\n", upload.totalSize);
            } else {

                Update.printError(Serial);
                ota_message = "Image incomplète ou corrompue";
            }
        }
        esp_task_wdt_add(NULL);   // ré-arme le watchdog

    } else if (upload.status == UPLOAD_FILE_ABORTED) {

        // Connexion interrompue : on annule proprement, l'ancien firmware reste
        // actif (la partition de boot n'a pas été modifiée).
        Update.abort();
        ota_bad     = true;
        ota_message = "Upload interrompu";
        Serial.println("[OTA] Aborted");
        esp_task_wdt_add(NULL);   // ré-arme le watchdog (sinon resté désactivé)
    }
}

// -------------------------------------------------------
//  Routes mode AP
// -------------------------------------------------------
static void setup_ap_routes(void) {

    // Main page
    server->on("/", HTTP_GET, []() {

        server->send(200, "text/html", build_ap_page());
    });

    // Save WIFI station SSID / WIFI station password / hostname and reboot
    server->on("/save-wifi", HTTP_POST, []() {

        if (!server->hasArg("plain")) {

            server->send(400, "text/plain", "Body manquant");
            return;
        }
        // Même parseur que le service BLE (config.cpp)
        String error;
        if (!config_update_from_json(server->arg("plain"), error)) {

            server->send(400, "text/plain", error);
            return;
        }
        config_save();
        server->send(200, "text/plain", "✅ Saved — restart in 2s…");
        delay(2000);
        ESP.restart();
    });

    // Favicon silencieux
    server->on("/favicon.ico", HTTP_GET, []() {

        server->send(204);
    });
}

// -------------------------------------------------------
//  Routes mode connecté
// -------------------------------------------------------
static void setup_portal_routes(void) {

    // Page principale
    server->on("/", HTTP_GET, []() {

        server->send(200, "text/html", build_portal_page());
    });

    // OTA
    server->on("/update", HTTP_POST, route_ota_post, route_ota_upload);

    // Sauvegarde config (lumière, mot de passe WiFi)
    server->on("/save-config", HTTP_POST, []() {

        if (!server->hasArg("plain")) {

            server->send(400, "text/plain", "Body manquant");
            return;
        }
        String body = server->arg("plain");

        JsonDocument doc;
        //Serial.println("body=" + body);
        DeserializationError err = deserializeJson(doc, body);
        if (err) {

            server->send(400, "text/plain", "JSON invalide");
            return;
        }

        bool changed = false;
        // Password update only if non-empty
        if (doc.containsKey("wifi_pass") && !doc["wifi_pass"].as<String>().isEmpty()) {

            g_config.wifi_pass = doc["wifi_pass"].as<String>();
            changed = true;
        }
        if (doc.containsKey("light_delay")) {

            uint value = doc["light_delay"].as<uint>();
            if (value <= 600) {

                g_config.light_delay = value;
                changed = true;
            }
        }

        if (changed) {

            config_save();
            server->send(200, "text/plain", "✅ Configuration enregistrée");
        } else {

            server->send(200, "text/plain", "Aucun changement valide");
        }
    });

    // Reset NVS
    server->on("/reset", HTTP_POST, []() {

        server->send(200, "text/plain", "Réinitialisation en cours…");
        delay(500);
        config_reset(); // efface NVS et reboot
    });

    // Favicon silencieux
    server->on("/favicon.ico", HTTP_GET, []() {

        server->send(204);
    });
}

// -------------------------------------------------------
//  API publique
// -------------------------------------------------------
void webportal_setup(bool ap_mode) {

    // Tear down any previous server (e.g. on a runtime STA→AP switch) so its
    // listening socket on port 80 is released before we bind a new one.
    if (server != nullptr) {

        server->stop();
        delete server;
        server = nullptr;
    }
    server = new WebServer(80);
    if (ap_mode) {

        setup_ap_routes();
        Serial.println("[WebPortal] AP mode — http://" AP_IP);
    } else {

        setup_portal_routes();
        Serial.println("[WebPortal] STA mode — http://" + wifi_get_ip_address());
    }
    server->begin();
}

void webportal_loop(void) {

    if (server != nullptr) {

        server->handleClient();
    }
}
