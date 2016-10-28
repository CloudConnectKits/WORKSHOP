/*
 * ZentriOS SDK LICENSE AGREEMENT | Zentri.com, 2015.
 *
 * Use of source code and/or libraries contained in the ZentriOS SDK is
 * subject to the Zentri Operating System SDK license agreement and
 * applicable open source license agreements.
 *
 */
/* Documentation for this app is available online.
 * See https://docs.zentri.com/wifi/sdk/latest/examples/demo/led-matrix
 */

/*
 * This is an Avnet-modified version of the standard Zentri apps\demo\led_matrix example, used as Lab1 and Lab3 in the Avnet workshops
 *  Added modifications include the servicing of a Pushbutton event for:
 *  - Initiating a scan for Wi-Fi networks
 *  - Listing scan results to the serial console
 *  - Reading from a text file on the local file system and displaying on the LED matrix
 *  In addition, the module's IP address is printed in a related console message, so that this compatible with all OSes
 */

#include "zos.h"
#include "led_matrix8x8.h"

static void scan_event_handler(void *arg);
static void print_scan_result(int index, const zos_scan_result_t* record );
static void load_file_message(void);
static zos_result_t update_text_processor(const http_server_request_t *request, const char *arg);
static zos_result_t update_blink_processor(const http_server_request_t *request, const char *arg);
static zos_result_t update_brightness_processor(const http_server_request_t *request, const char *arg);
static zos_result_t update_scroll_processor(const http_server_request_t *request, const char *arg);
static zos_result_t retrieve_all_processor(const http_server_request_t *request, const char *arg);
static void button_clicked_event_handler(void *arg);
static void button_pressed_event_handler(void *arg);
static void button_toggled_event_handler(void *arg);

HTTP_SERVER_DYNAMIC_PAGES_START
	HTTP_SERVER_DYNAMIC_PAGE("/led_matrix/update/text",          update_text_processor),
	HTTP_SERVER_DYNAMIC_PAGE("/led_matrix/update/blink",         update_blink_processor),
	HTTP_SERVER_DYNAMIC_PAGE("/led_matrix/update/brightness",    update_brightness_processor),
	HTTP_SERVER_DYNAMIC_PAGE("/led_matrix/update/scroll",        update_scroll_processor),
	HTTP_SERVER_DYNAMIC_PAGE("/led_matrix/retrieve/all",         retrieve_all_processor),
HTTP_SERVER_DYNAMIC_PAGES_END

#define BUTTON_DEBOUNCE_TIME  50 // ms
#define BUTTON_CLICK_TIME   1000 // ms
#define BUTTON_PRESS_TIME    100 // ms

static zos_bool_t initialized;


/*************************************************************************************************/
void zn_app_init(void)
{
    zos_result_t result;
    char buffer[32];

    const button_config_t config =
    {
        .active_level = BUTTON_ACTIVE_LOW,
        .debounce = BUTTON_DEBOUNCE_TIME,
        .click_time = BUTTON_CLICK_TIME,
        .press_time = BUTTON_PRESS_TIME,
        .event_handler.press = button_pressed_event_handler,
    };

    PLATFORM_ENABLE_JTAG_GPIOS();
    button_init(PLATFORM_BUTTON1, &config, (void*)1);

    ZOS_LOG("Starting Lab1: 8x8 LED Matrix Demo");

    if(zn_load_app_settings("settings.ini") != ZOS_SUCCESS)
    {
        ZOS_LOG("Failed to load settings");
        return;
    }

    if(ZOS_FAILED(result, led_matrix8x8_init(PLATFORM_STD_I2C)))
    {
        ZOS_LOG("Failed to initialize LED Matrix Library");
        return;
    }

    led_matrix8x8_set_text("Default message ... ");
    led_matrix8x8_set_scroll_rate(35);

    HTTP_SERVER_REGISTER_DYNAMIC_PAGES();

    if(ZOS_FAILED(result,zn_network_restart(ZOS_WLAN)))
    {
        ZOS_LOG("Failed to restart network: %d\r\n\r\n", result);
        ZOS_LOG("----------------------------------------------------------------------\r\n");
        ZOS_LOG("This basic app expects valid network credentials have been configured.");
        ZOS_LOG("Join a network and save credentials to non-volatile memory using the  ");
        ZOS_LOG("ZentriOS commands shown below                                         ");
        ZOS_LOG("                                                                      ");
        ZOS_LOG("> network_up -s                                                       ");
        ZOS_LOG("----------------------------------------------------------------------");
        ZOS_LOG("\r\n                                                                  ");
    }
    else
    {
        ZOS_LOG("From your browser, enter the URL: http://%s/ to control the display",  ZOS_GET_SETTING_STR("wlan.network.ip", buffer));
        ZOS_LOG("Note! If this webpage address fails, try instead using the assigned IPV4 address");
    }

    load_file_message();

    initialized = ZOS_TRUE;
}


/*************************************************************************************************/
void zn_app_deinit(void)
{
    led_matrix8x8_deinit();
    button_deinit(PLATFORM_BUTTON1);
}


/*************************************************************************************************/
zos_bool_t zn_app_idle(void)
{
    // Return TRUE so the event loop idles
    // It will wakeup when the LED matrix needs to be updated or the HTTP server receives a request
    return initialized;
}

/*************************************************************************************************/
static void load_file_message(void)
{
	uint32_t handle;
	zos_result_t result;

	if(!ZOS_FAILED(result, zn_file_open("message.txt", &handle)))
	{
		char buffer[64];
		uint32_t bytes_read;

		if(!ZOS_FAILED(result, zn_file_read(handle, buffer, sizeof(buffer)-1, &bytes_read)))
		{
			buffer[bytes_read] = 0;
			led_matrix8x8_set_text(buffer);
			ZOS_LOG("LED matrix updated with message.txt");
		}
		else
		{
			ZOS_LOG("Failed to read message.txt");
		}
	}
	else
	{
		ZOS_LOG("message.txt not found");
	}
}

/*************************************************************************************************/
static zos_result_t update_text_processor(const http_server_request_t *request, const char *arg)
{
    const http_server_param_t *param = zn_hs_get_param(request, "data");
    led_matrix8x8_set_text(param->value);
    return zn_hs_write_reply_header(request, NULL, 0, HTTP_SERVER_HEADER_NONE);
}


/*************************************************************************************************/
static zos_result_t update_blink_processor(const http_server_request_t *request, const char *arg)
{
    const http_server_param_t *param = zn_hs_get_param(request, "data");
    const uint8_t val = str_to_uint32(param->value);
    led_matrix8x8_set_blink_rate(val);
    return zn_hs_write_reply_header(request, NULL, 0, HTTP_SERVER_HEADER_NONE);
}


/*************************************************************************************************/
static zos_result_t update_brightness_processor(const http_server_request_t *request, const char *arg)
{
    const http_server_param_t *param = zn_hs_get_param(request, "data");
    const uint8_t val = str_to_uint32(param->value);
    led_matrix8x8_set_brightness(val);
    return zn_hs_write_reply_header(request, NULL, 0, HTTP_SERVER_HEADER_NONE);
}


/*************************************************************************************************/
static zos_result_t update_scroll_processor(const http_server_request_t *request, const char *arg)
{
    const http_server_param_t *param = zn_hs_get_param(request, "data");
    const uint16_t val = str_to_uint32(param->value);
    led_matrix8x8_set_scroll_rate(val);
    return zn_hs_write_reply_header(request, NULL, 0, HTTP_SERVER_HEADER_NONE);
}


/*************************************************************************************************/
static zos_result_t retrieve_all_processor(const http_server_request_t *request, const char *arg)
{
    zos_result_t result;
    char buffer[128];
    char *ptr = buffer;
    const led_matrix8x8_context_t *context = led_matrix8x8_get_context();

    ptr += sprintf(ptr, "{\"brightness\":%d,", context->brightness);
    ptr += sprintf(ptr, "\"blink\":%d,", context->blink_rate);
    ptr += sprintf(ptr, "\"scroll\":%d,", context->scroll.rate);
    ptr += sprintf(ptr, "\"msg\":\"");

    if(ZOS_FAILED(result, zn_hs_write_reply_header(request, "application/json", -1, HTTP_SERVER_HEADER_NONE)))
    {
    }
    else if(ZOS_FAILED(result, zn_hs_write_chunked_data(request, buffer, ptr - buffer, ZOS_FALSE)))
    {
    }
    else if(ZOS_FAILED(result, zn_hs_write_chunked_data(request, context->text, strlen(context->text), ZOS_FALSE)))
    {
    }
    else if(ZOS_FAILED(result, zn_hs_write_chunked_data(request, "\"}", 2, ZOS_TRUE)))
    {
    }

    return result;
}

/*************************************************************************************************/
static void button_pressed_event_handler(void *arg)
{
    ZOS_LOG("USER BUTTON Pressed!");
    zn_event_issue(scan_event_handler, NULL, EVENT_FLAGS1(RUN_NOW));
}

/*************************************************************************************************/
static void scan_event_handler(void *arg)
{
    zos_result_t ret;
    zos_scan_result_t *scan_results;
    const uint32_t start_time = zn_rtos_get_time();
    int i = 0;

    ZOS_LOG( "Scanning for Wi-Fi networks ..." );
    if(ZOS_FAILED(ret, zn_network_scan(&scan_results, 0, NULL)))
    {
        ZOS_LOG("Failed to issue scan: %d", ret);
        return;
    }

    ZOS_LOG("  # Type  BSSID             RSSI  Rate Chan  Security    SSID" );
    ZOS_LOG("--------------------------------------------------------------------" );

    for(const zos_scan_result_t *result = scan_results; result != NULL; result = result->next, ++i)
    {
        print_scan_result(i, result);
    }

    ZOS_LOG("Scan complete in %lu milliseconds\r\n", zn_rtos_get_time() - start_time);
    zn_network_scan_destroy_results();
}


/*************************************************************************************************/
static void print_scan_result(int index, const zos_scan_result_t* record )
{
    char buffer[128], mac[20];
    fpi_str_buffer_t data_rate_str;
    char *ptr = buffer;
    fpi_int_to_str(data_rate_str, record->max_data_rate, 1000.0, 3);

    ptr += sprintf(ptr,    " %2d", index);
    ptr += sprintf(ptr,    " %5s", ( record->bss_type == ZOS_BSS_TYPE_ADHOC ) ? "Adhoc" : "Infra");
    ptr += sprintf(ptr,     " %s", mac_to_str(&record->BSSID, mac));
    ptr += sprintf(ptr,   "  %3d", record->signal_strength);
    ptr += sprintf(ptr,  "  %-5s", data_rate_str);
    ptr += sprintf(ptr,    " %3d", record->channel);
    ptr += sprintf(ptr, "  %-10s", ( record->security == ZOS_SECURITY_OPEN )           ? "Open" :
                                   ( record->security == ZOS_SECURITY_WEP_PSK )        ? "WEP" :
                                   ( record->security == ZOS_SECURITY_WPA_TKIP_PSK )   ? "WPA TKIP" :
                                   ( record->security == ZOS_SECURITY_WPA_AES_PSK )    ? "WPA AES" :
                                   ( record->security == ZOS_SECURITY_WPA2_AES_PSK )   ? "WPA2 AES" :
                                   ( record->security == ZOS_SECURITY_WPA2_TKIP_PSK )  ? "WPA2 TKIP" :
                                   ( record->security == ZOS_SECURITY_WPA2_MIXED_PSK ) ? "WPA2 Mixed" :
                                   "Unknown" );
    ptr += sprintf(ptr, "  ");

    if(record->SSID.len > 0)
    {
        ssid_to_str(ptr, &record->SSID);
    }
    else
    {
        strcpy(ptr, "<ssid hidden>");
    }

    ZOS_LOG("%s", buffer);
}
