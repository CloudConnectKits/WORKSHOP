/*
 * ZentriOS SDK LICENSE AGREEMENT | Zentri.com, 2015.
 *
 * Use of source code and/or libraries contained in the ZentriOS SDK is
 * subject to the Zentri Operating System SDK license agreement and
 * applicable open source license agreements.
 *
 */
#pragma once


#include "mqtt_api.h"

#define MQTT_PUBLISH_PERIOD         5000

#define MQTT_MESSAGE                "{\"d\":{\"signal strength\": %s, \"light_sensor\": %s}}"
#define MQTT_TOPIC                  "iot-2/evt/zentri/fmt/json"
#define MQTT_CLIENT_ID              "d:quickstart:type:zentri_%c%c%c%c"
#define MQTT_USER                   ""
#define MQTT_PASSWORD               ""
#define MQTT_HOST                   "quickstart.messaging.internetofthings.ibmcloud.com"
#define MQTT_PORT                   1883
#define MQTT_KEEPALIVE              60
#define MQTT_QOS                    MQTT_QOS_DELIVER_AT_MOST_ONCE
#define MQTT_SECURITY               ZOS_FALSE

#define MAX_TOPIC_STRING_SIZE       80
#define MAX_MESSAGE_STRING_SIZE     100
#define MAX_CLIENT_STRING_SIZE      80
#define MAX_USER_STRING_SIZE        20
#define MAX_PASSWORD_STRING_SIZE    20
#define MAX_HOST_STRING_SIZE        80

typedef struct
{
    uint8_t message	 [MAX_MESSAGE_STRING_SIZE];
    uint8_t topic    [MAX_TOPIC_STRING_SIZE];
    uint8_t client	 [MAX_CLIENT_STRING_SIZE];
    uint8_t user	 [MAX_USER_STRING_SIZE];
    uint8_t password [MAX_PASSWORD_STRING_SIZE];
    uint8_t host	 [MAX_HOST_STRING_SIZE];
    uint16_t port;
    uint16_t keepalive;
    uint8_t qos;
    zos_bool_t security;
} mqtt_settings_t;

void commands_init(void);
void commands_deinit(void);

void mqtt_app_connect( void *arg );
void mqtt_app_disconnect( void *arg );
void mqtt_app_subscribe( void *arg );
void mqtt_app_unsubscribe( void *arg );
void mqtt_app_publish( void *arg );

