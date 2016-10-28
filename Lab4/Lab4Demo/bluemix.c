/*
 * ZentriOS SDK LICENSE AGREEMENT | Zentri.com, 2016.
 *
 * Use of source code and/or libraries contained in the ZentriOS SDK is
 * subject to the Zentri Operating System SDK license agreement and
 * applicable open source license agreements.
 *
 */
/* Documentation for this app is available online.
 * See https://docs.zentri.com/wifi/sdk/latest/examples/cloud/bluemix
 */

#include "zos.h"
#include "platform_common.h"
#include "common.h"

/** @file
 *
 * IBM Bluemix Example App using the MQTT protocol
 *
 * The application connects to the Bluemix Quickstart broker:
 * quickstart.messaging.internetofthings.ibmcloud.com
 *
 * After a connection with the broker is established, the following sensor
 * readings are published at 5 second intervals. Both readings are
 * combined into a JSON formatted message
 *   - Signal strength (RSSI) of the Wi-Fi access point the board is
 *     connected with
 *   - Thermistor value converted to temperature using ZentriOS ADC APIs
 *     (see note below about the thermistor)
 *
 * The sensor readings can be viewed in real-time on the IBM Watson IoT
 * Platform at https://quickstart.internetofthings.ibmcloud.com
 *
 * The Device ID is zentri_XXXX where XXXX is the least significant
 * 4 bytes of the WLAN MAC address. The ID is printed to a ZentriOS terminal.
 *
 * NOTES:
 * 1. Thermistor. If your hardware platform does not have a thermistor connected
 *    to the standard ADC, the periodic_publish() function must be modified
 *    to use another sensor
 * 2. The ZAP may not work with a debug build if TLS is enabled on platforms
 *    with memory footprints less than 128kB
 *
 */

/******************************************************
 *                      Macros
 ******************************************************/

#if PLATFORM_ADC_MAX_RESOLUTION == 12
#define LUT_FILENAME  "lut_12bit.csv"
#else
#define LUT_FILENAME  "lut_10bit.csv"
#endif

/******************************************************
 *                    Constants
 ******************************************************/

/******************************************************
 *                   Enumerations
 ******************************************************/

/******************************************************
 *                 Type Definitions
 ******************************************************/

/******************************************************
 *                    Structures
 ******************************************************/
const zos_adc_config_t ADC_CONFIG =
{
    .resolution = PLATFORM_ADC_MAX_RESOLUTION,
    .sampling_cycle = 12,
    .gain = 1
};

/******************************************************
 *               Function Declarations
 ******************************************************/
static zos_result_t mqtt_connection_event_cb( mqtt_event_info_t *event );

/******************************************************
 *               Variable Definitions
 ******************************************************/
mqtt_settings_t settings;
mqtt_connection_t* mqtt_connection;
static mqtt_callback_t callback = mqtt_connection_event_cb;
zos_adc_lut_t adc_lut;

/******************************************************
 *               Function Definitions
 ******************************************************/
void zn_app_init(void)
{
    zos_result_t result;
    zos_adc_t adc;
    char mac_str[32] = {0};

    /* Get device MAC address */
    zn_network_get_mac(mac_str);

    /* Bring up the network interface */
    if (!zn_network_is_up(ZOS_WLAN))
    {
        ZOS_LOG("Network is down, restarting...");
        if(ZOS_FAILED(result, zn_network_restart(ZOS_WLAN)))
        {
            ZOS_LOG("Failed to restart network: %d", result);
            return;
        }
    }

    /* Initialize ADC */
    if(ZOS_FAILED(result, zn_adc_gpio_to_peripheral(PLATFORM_STD_ADC, &adc)))
    {
        ZOS_LOG("The specified GPIO: %d does not support ADC!", PLATFORM_STD_ADC);
    }

    if (ZOS_FAILED(result, zn_adc_direct_init(adc, &ADC_CONFIG)))
    {
        ZOS_LOG("Failed to initialize ADC!");
    }

    /* Prepare ADC lookup table */
    if (ZOS_FAILED(result, zn_adc_add_lut(ZOS_ADC_GET_MASK(adc), LUT_FILENAME, &adc_lut)))
    {
        ZOS_LOG("Failed to prepare ADC lookup table! Error code %d", result);
    }

    /* MQTT settings */
    strcpy((char*)settings.host, MQTT_HOST);
    strcpy((char*)settings.topic, MQTT_TOPIC);
    sprintf((char*)settings.client, MQTT_CLIENT_ID, mac_str[12], mac_str[13], mac_str[15], mac_str[16]);
    strcpy((char*)settings.user, MQTT_USER);
    strcpy((char*)settings.password, MQTT_PASSWORD);
    settings.port = MQTT_PORT;
    settings.keepalive = MQTT_KEEPALIVE;
    settings.qos = MQTT_QOS;
    settings.security = MQTT_SECURITY;

    /* Memory allocated for MQTT object*/
    zn_malloc((uint8_t**)&mqtt_connection, sizeof(mqtt_connection_t));
    if ( mqtt_connection == NULL )
    {
        ZOS_LOG("Failed to allocate MQTT object...\n");
        return;
    }

    /* Initialize MQTT library */
    result = mqtt_init( mqtt_connection );
    if(result != ZOS_SUCCESS)
    {
        ZOS_LOG("Error initializing");
        zn_free(mqtt_connection);
        mqtt_connection = NULL;
        return;
    }

    ZOS_LOG("\r\nIBM Bluemix MQTT Demo Application Started");
    ZOS_LOG("  - Broker: %s", settings.host);
    ZOS_LOG("  - Client ID: %s", settings.client);
    ZOS_LOG("  - Topic/Queue: %s\r\n", settings.topic);

    /* Connect to IBM broker Blumix broker */
    mqtt_app_connect(NULL);
}


/*************************************************************************************************/
void zn_app_deinit(void)
{
    /* Free MQTT object and deinit library */
    mqtt_deinit( mqtt_connection );
    zn_free(mqtt_connection);
    mqtt_connection = NULL;

    /* Remove ADC lookup table */
    zn_adc_remove_lut(adc_lut);
}

/*************************************************************************************************/
/*
 * Connect to broker (sent connect frame)
 */
void mqtt_app_connect( void *arg )
{
    mqtt_pkt_connect_t conninfo;
    zos_result_t ret = ZOS_SUCCESS;

    ZOS_LOG("Opening connection with broker %s:%u", settings.host, settings.port);
    ret = mqtt_open( mqtt_connection, (const char*)settings.host, settings.port, ZOS_WLAN, callback, settings.security );
    if ( ret != ZOS_SUCCESS )
    {
        ZOS_LOG("Error opening connection (keys and certificates are set properly?)");
        return;
    }

    ZOS_LOG("Connection established");

    /* Now, after socket is connected we can send the CONNECT frame safely */
    ZOS_LOG("Connecting...");
    memset( &conninfo, 0, sizeof( conninfo ) );

    conninfo.mqtt_version = MQTT_PROTOCOL_VER4;
    conninfo.clean_session = 1;
    conninfo.client_id = settings.client;
    conninfo.keep_alive = settings.keepalive;
    conninfo.password = strlen((char*)settings.password) > 0 ? settings.password : NULL;
    conninfo.username = strlen((char*)settings.user) > 0 ? settings.user : NULL;
    ret = mqtt_connect( mqtt_connection, &conninfo );
    if ( ret != ZOS_SUCCESS )
    {
        ZOS_LOG("Error connecting");
    }
}

/*************************************************************************************************/
/*
 * Disconnect from broker (sent disconnect frame)
 */
void mqtt_app_disconnect( void *arg )
{
    if ( mqtt_disconnect( mqtt_connection ) != ZOS_SUCCESS )
    {
        ZOS_LOG("Error disconnecting");
    }
}

/*************************************************************************************************/
/*
 * Subscribe to topic
 */
void mqtt_app_subscribe( void *arg )
{
    mqtt_msgid_t pktid;
    ZOS_LOG("Subscribing to topic '%s'", settings.topic);
    pktid = mqtt_subscribe( mqtt_connection, settings.topic, settings.qos );
    if ( pktid == 0 )
    {
        ZOS_LOG("Error subscribing: packet ID = 0");
    }
}

/*************************************************************************************************/
/*
 * Unsubscribe from topic
 */
void mqtt_app_unsubscribe( void *arg )
{
    mqtt_msgid_t pktid;
    ZOS_LOG("Unsubscribing from topic '%s'", settings.topic);
    pktid = mqtt_unsubscribe( mqtt_connection, settings.topic );

    if ( pktid == 0 )
    {
        ZOS_LOG("Error unsubscribing: packet ID = 0");
    }
}

/*************************************************************************************************/
/*
 * Publish (send) message to topic
 */
void mqtt_app_publish( void *arg )
{
    mqtt_msgid_t pktid;
    ZOS_LOG("Publishing to topic: '%s', message: '%s'", settings.topic, settings.message);
    pktid = mqtt_publish( mqtt_connection, settings.topic, settings.message, (uint32_t)strlen((char*)settings.message), settings.qos );

    if ( pktid == 0 )
    {
        ZOS_LOG("Error publishing: packet ID = 0");
    }
}

/******************************************************
 *               Static Function Definitions
 ******************************************************/
/*
 * Call back function to handle connection events.
 */
static zos_result_t mqtt_connection_event_cb( mqtt_event_info_t *event )
{
    switch ( event->type )
    {
        case MQTT_EVENT_TYPE_CONNECTED:
            ZOS_LOG("CONNECTED" );

            /* once connected, trigger event to publish a message */
            zn_event_issue(periodic_publish, NULL, 0);
            break;
        case MQTT_EVENT_TYPE_DISCONNECTED:
            ZOS_LOG("DISCONNECTED" );
            break;
        case MQTT_EVENT_TYPE_PUBLISHED:
            ZOS_LOG("MESSAGE PUBLISHED" );

            /* once previous message is published, trigger event to publish another */
            zn_event_register_timed(periodic_publish, NULL, MQTT_PUBLISH_PERIOD, 0);
            break;
        case MQTT_EVENT_TYPE_SUBCRIBED:
            ZOS_LOG("TOPIC SUBSCRIBED" );
            break;
        case MQTT_EVENT_TYPE_UNSUBSCRIBED:
            ZOS_LOG("TOPIC UNSUBSCRIBED" );
            break;
        case MQTT_EVENT_TYPE_PUBLISH_MSG_RECEIVED:
        {
            ZOS_LOG("MESSAGE RECEIVED");
        }
            break;
        default:
            break;
    }
    return ZOS_SUCCESS;
}

/*************************************************************************************************
 * Periodic publish the thermistor reading
 */
static void periodic_publish(void *arg)
{
	char light_sense_buffer[16];
    char rssi_buffer[16];
    uint32_t bytes_read;
    zos_result_t result;
    zos_adc_t adc;
    uint16_t   adc_raw = 0, light_value = 0;

    zn_issue_command_return_data(rssi_buffer, sizeof(rssi_buffer), &bytes_read, "wlan_get_rssi");
    rssi_buffer[bytes_read] = 0;

    if(ZOS_FAILED(result, zn_adc_gpio_to_peripheral(PLATFORM_STD_ADC, &adc)))
    {
        ZOS_LOG("The specified GPIO: %d does not support ADC!", PLATFORM_STD_ADC);
    }

    if (ZOS_FAILED(result, zn_adc_direct_sample(adc, &adc_raw)))
        {
            ZOS_LOG("Failed to read converted ADC sample! Error code %d", result);
        }
        else
        {
        	light_value = (4095 - adc_raw);
        	int_to_str(light_value, light_sense_buffer);

        	// Publish the converted ADC sample to MQTT queue //
        	sprintf((char*)settings.message, MQTT_MESSAGE, rssi_buffer, light_sense_buffer);
        	mqtt_app_publish(NULL);
        }
}

