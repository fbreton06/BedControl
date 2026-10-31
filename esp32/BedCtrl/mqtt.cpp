#include "mqtt.h"

static Mqtt* mqtt;

static void on_message(String &topic, String &message) {

    String sub_topic = mqtt->get_topic(topic);
    #if DEBUG_TRACE
    Serial.print("MQTT RX(");
    Serial.print(sub_topic);
    Serial.print("): ");
    Serial.println(message);
    #endif
    mqtt_on_message_callback on_message_callback = mqtt->get_callback();
    if (on_message_callback) {

        if (sub_topic != "#") {

            on_message_callback(mqtt->get_context(), sub_topic, message);
        } else {

            Serial.println("Topic: '" + topic + "' ignored!");
        }
    }
}

Mqtt::Mqtt(mqtt_on_message_callback on_message_callback, void* context) {

    this->on_message_callback = on_message_callback;
    this->context = context;
    this->wifi_client = new WiFiClient();
    this->mqtt = new MQTTClient();
    // Initialisation du MQTT
    this->mqtt->begin(g_config.mqtt_host.c_str(), g_config.mqtt_port, *this->wifi_client);
    this->mqtt->onMessage(on_message);
}

String Mqtt::get_topic(String topic) {

    String result = "#";
    if (topic == g_config.hostname) {

        result = "";
    } else if (topic.startsWith(g_config.hostname + "/")) {

        result = topic.substring(g_config.hostname.length() + 1, topic.length()) ;
    }
    return result;
}

bool Mqtt::check_connection(void) {

    if (!this->mqtt->connected()) {

        // Broker connection (or reconnection)
        if (this->mqtt->connect(g_config.hostname.c_str(), g_config.mqtt_user.c_str(), g_config.mqtt_pass.c_str())) {

            if (g_config.hostname != "") {
                
                this->mqtt->subscribe(g_config.hostname + "/#");
            }
            Serial.println("MQTT connected");
        }
    }
    return this->mqtt->connected();
}

bool Mqtt::publish(String& message, String* sub_topic) {

    bool done = false;
    if (this->check_connection() &&  (g_config.mqtt_topic != "")) {

        String topic = g_config.mqtt_topic;
        if (sub_topic && *sub_topic) {

            topic += '/' + *sub_topic;
        }
        this->mqtt->publish(topic, message.c_str());
        #if DEBUG_TRACE
        Serial.print("MQTT TX(");
        Serial.print(topic);
        Serial.print("): ");
        Serial.println(message);
        #endif
        done = true;
    }
    return done;
}

void Mqtt::loop(void) {

    this->mqtt->loop();
    this->check_connection();
}

Mqtt* mqtt_create(mqtt_on_message_callback on_message_callback, void* context) {

    mqtt = new Mqtt(on_message_callback, context);
    return mqtt;
}
