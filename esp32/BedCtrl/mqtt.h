#ifndef __MQTT__
#define __MQTT__

#include <WiFi.h>
#include <MQTT.h>
#include "globals.h"

typedef void (*mqtt_on_message_callback)(void* context, String& sub_topic, String& message);

class Mqtt {

public:
    Mqtt(mqtt_on_message_callback on_message_callback, void* context);
    virtual ~Mqtt(void) {};

private:
    WiFiClient* wifi_client;
    MQTTClient* mqtt;
    String sub_topic;
    String pub_topic;
    String username;
    String password;
    String broker_address;
    mqtt_on_message_callback on_message_callback;
    void* context;

private:
    bool check_connection(void);

public:
    void* get_context(void) { return(this->context); }
    mqtt_on_message_callback get_callback(void) { return(this->on_message_callback); }
    String get_sub_topic(String topic);
    bool publish(String& message, String* sub_topic=NULL);
    void loop(void);
};

Mqtt* mqtt_create(mqtt_on_message_callback on_message_callback, void* context);
#endif // __MQTT__
