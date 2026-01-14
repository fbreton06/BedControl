#include "mqtt.h"


static Mqtt* mqtt;

static void on_message(String &topic, String &message) {

    String sub_topic = mqtt->get_sub_topic(topic);
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
    this->pub_topic = String(MQTT_PUB_TOPIC);
    this->sub_topic = String(MQTT_SUB_TOPIC);
    this->username = String(MQTT_USERNAME);
    this->password = String(MQTT_PASSWORD);
    this->broker_address = String(MQTT_BROKER_ADDRESS);
    // Initialisation du MQTT
    this->mqtt->begin(this->broker_address.c_str(), *this->wifi_client);
    this->mqtt->onMessage(on_message);
}

String Mqtt::get_sub_topic(String topic) {

    String result = "#";
    if (topic == this->sub_topic) {

        result = "";
    } else if (topic.startsWith(this->sub_topic + "/")) {

        result = topic.substring(this->sub_topic.length() + 1, topic.length()) ;
    }
    return result;
}

bool Mqtt::check_connection(void) {

    if (!this->mqtt->connected()) {

        // Broker connection (or reconnection)
        const char connect_id[] = MQTT_ID;
        if (this->mqtt->connect(connect_id, this->username.c_str(), this->password.c_str())) {

            if (this->sub_topic != "") {
                
                this->mqtt->subscribe(this->sub_topic + "/#");
            }
            Serial.println("MQTT connected");
        }
    }
    return this->mqtt->connected();
}

bool Mqtt::publish(String& message, String* sub_topic) {

    bool done = false;
    if (this->check_connection() &&  (this->pub_topic != "")) {

        String topic = this->pub_topic;
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
