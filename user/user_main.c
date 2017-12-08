/*
 * ESPRESSIF MIT License
 *
 * Copyright (c) 2015 <ESPRESSIF SYSTEMS (SHANGHAI) PTE LTD>
 *
 * Permission is hereby granted for use on ESPRESSIF SYSTEMS ESP8266 only, in which case,
 * it is free of charge, to any person obtaining a copy of this software and associated
 * documentation files (the "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the Software is furnished
 * to do so, subject to the following conditions:
 * 
 * The above copyright notice and this permission notice shall be included in all copies or
 * substantial portions of the Software.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS
 * FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR
 * COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
 * IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 *
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_common.h"
#include "esp_wifi.h"
#include "esp_system.h"
#include "esp_common.h"

#include "esp8266/ets_sys.h"
#include "apps/sntp.h"

#include "iot_import.h"
#include "iot_export.h"
#include "cJSON.h"

#include "aliyun_config.h"
#include "aliyun_ota.h"

#include "uart.h"


extern int got_ip_flag;
extern uint8 fifo_tmp[255];
static int binary_file_length = 0;
LOCAL os_timer_t ota_timer;
uint8 wifi_ssid[50]={0};
uint8 wifi_pass[50]={0};
uint8 key[50]={0};
uint8 name[50]={0};
uint8 secret[50]={0};
uint8 weight[50]={0};
uint8 high[50]={0};


uint8 *fifo = NULL;
int fifo_i;


/******************************************************************************
 * FunctionName : user_rf_cal_sector_set
 * Description  : SDK just reversed 4 sectors, used for rf init data and paramters.
 *                We add this function to force users to set rf cal sector, since
 *                we don't know which sector is free in user's application.
 *                sector map for last several sectors : ABCCC
 *                A : rf cal
 *                B : rf init data
 *                C : sdk parameters
 * Parameters   : none
 * Returns      : rf cal sector
*******************************************************************************/
uint32 user_rf_cal_sector_set(void)
{
    flash_size_map size_map = system_get_flash_size_map();
    uint32 rf_cal_sec = 0;

    switch (size_map) {
        case FLASH_SIZE_4M_MAP_256_256:
            rf_cal_sec = 128 - 5;
            break;

        case FLASH_SIZE_8M_MAP_512_512:
            rf_cal_sec = 256 - 5;
            break;

        case FLASH_SIZE_16M_MAP_512_512:
        case FLASH_SIZE_16M_MAP_1024_1024:
            rf_cal_sec = 512 - 5;
            break;

        case FLASH_SIZE_32M_MAP_512_512:
        case FLASH_SIZE_32M_MAP_1024_1024:
            rf_cal_sec = 1024 - 5;
            break;
        case FLASH_SIZE_64M_MAP_1024_1024:
            rf_cal_sec = 2048 - 5;
            break;
        case FLASH_SIZE_128M_MAP_1024_1024:
            rf_cal_sec = 4096 - 5;
            break;
        default:
            rf_cal_sec = 0;
            break;
    }

    return rf_cal_sec;
}

void event_handle(void *pcontext, void *pclient, iotx_mqtt_event_msg_pt msg)
{
    uintptr_t packet_id = (uintptr_t)msg->msg;
    iotx_mqtt_topic_info_pt topic_info = (iotx_mqtt_topic_info_pt)msg->msg;

    switch (msg->event_type) {
    case IOTX_MQTT_EVENT_UNDEF:
        EXAMPLE_TRACE("undefined event occur.");
        break;

    case IOTX_MQTT_EVENT_DISCONNECT:
        EXAMPLE_TRACE("MQTT disconnect.");
        break;

    case IOTX_MQTT_EVENT_RECONNECT:
        EXAMPLE_TRACE("MQTT reconnect.");
        break;

    case IOTX_MQTT_EVENT_SUBCRIBE_SUCCESS:
        EXAMPLE_TRACE("subscribe success, packet-id=%u", (unsigned int)packet_id);
        break;

    case IOTX_MQTT_EVENT_SUBCRIBE_TIMEOUT:
        EXAMPLE_TRACE("subscribe wait ack timeout, packet-id=%u", (unsigned int)packet_id);
        break;

    case IOTX_MQTT_EVENT_SUBCRIBE_NACK:
        EXAMPLE_TRACE("subscribe nack, packet-id=%u", (unsigned int)packet_id);
        break;

    case IOTX_MQTT_EVENT_UNSUBCRIBE_SUCCESS:
        EXAMPLE_TRACE("unsubscribe success, packet-id=%u", (unsigned int)packet_id);
        break;

    case IOTX_MQTT_EVENT_UNSUBCRIBE_TIMEOUT:
        EXAMPLE_TRACE("unsubscribe timeout, packet-id=%u", (unsigned int)packet_id);
        break;

    case IOTX_MQTT_EVENT_UNSUBCRIBE_NACK:
        EXAMPLE_TRACE("unsubscribe nack, packet-id=%u", (unsigned int)packet_id);
        break;

    case IOTX_MQTT_EVENT_PUBLISH_SUCCESS:
        EXAMPLE_TRACE("publish success, packet-id=%u", (unsigned int)packet_id);
        break;

    case IOTX_MQTT_EVENT_PUBLISH_TIMEOUT:
        EXAMPLE_TRACE("publish timeout, packet-id=%u", (unsigned int)packet_id);
        break;

    case IOTX_MQTT_EVENT_PUBLISH_NACK:
        EXAMPLE_TRACE("publish nack, packet-id=%u", (unsigned int)packet_id);
        break;

    case IOTX_MQTT_EVENT_PUBLISH_RECVEIVED:
        EXAMPLE_TRACE("topic message arrived but without any related handle: topic=%.*s, topic_msg=%.*s",
                      topic_info->topic_len,
                      topic_info->ptopic,
                      topic_info->payload_len,
                      topic_info->payload);
        break;

    default:
        EXAMPLE_TRACE("Should NOT arrive here.");
        break;
    }
}

static void _demo_message_arrive(void *pcontext, void *pclient, iotx_mqtt_event_msg_pt msg)
{
    char *fifo_topic = NULL;
    iotx_mqtt_topic_info_pt ptopic_info = (iotx_mqtt_topic_info_pt) msg->msg;

#if 0
    // print topic name and topic message
    EXAMPLE_TRACE("----");
    EXAMPLE_TRACE("Topic: '%.*s' (Length: %d)",
                  ptopic_info->topic_len,
                  ptopic_info->ptopic,
                  ptopic_info->topic_len);
    print_debug(ptopic_info->ptopic, ptopic_info->topic_len, "topic");
    print_debug(ptopic_info->payload,ptopic_info->payload_len, "payload");
    EXAMPLE_TRACE("Payload: '%.*s' (Length: %d)",
                  ptopic_info->payload_len,
                  ptopic_info->payload,
                  ptopic_info->payload_len);
    EXAMPLE_TRACE("----");
	
#endif
    fifo_topic = (char *)ptopic_info->payload;
    *(fifo_topic+ptopic_info->payload_len) = '\0';
    EXAMPLE_TRACE("[yebin]---topic get data %s---",fifo_topic);
    


}


int mqtt_client(void)
{
    int rc = 0, msg_len, cnt = 0;
	uint32 ts;
    void *pclient;
    iotx_conn_info_pt pconn_info;
    iotx_mqtt_param_t mqtt_params;
    iotx_mqtt_topic_info_t topic_msg;
    char msg_pub[128];
    char *msg_buf = NULL, *msg_readbuf = NULL;
    
    char *topic_data = (char *)os_malloc(sizeof(char)*50);
	char *topic_relay = (char *)os_malloc(sizeof(char)*50);
	if(topic_data== NULL||topic_relay==NULL)
	{
		EXAMPLE_TRACE("not enough memory");
		  rc = -1;
		  goto do_exit;
	}
	memset(topic_data,0x0,sizeof(char)*50);
	memset(topic_data,0x0,sizeof(char)*50);
	sprintf(topic_data,"/%s/%s/data",key,name);
    sprintf(topic_relay,"/%s/%s/relay",key,name);




	
	
    if (NULL == (msg_buf = (char *)HAL_Malloc(MSG_LEN_MAX))) {
        EXAMPLE_TRACE("not enough memory");
        rc = -1;
        goto do_exit;
    }

    if (NULL == (msg_readbuf = (char *)HAL_Malloc(MSG_LEN_MAX))) {
        EXAMPLE_TRACE("not enough memory");
        rc = -1;
        goto do_exit;
    }

    /* Device AUTH */

    if (0 != IOT_SetupConnInfo(key, name, secret, (void **)&pconn_info)) {
        EXAMPLE_TRACE("AUTH request failed!");
        rc = -1;
        goto do_exit;
    }

    /* Initialize MQTT parameter */
    memset(&mqtt_params, 0x0, sizeof(mqtt_params));

    mqtt_params.port = pconn_info->port;
    mqtt_params.host = pconn_info->host_name;
    mqtt_params.client_id = pconn_info->client_id;
    mqtt_params.username = pconn_info->username;
    mqtt_params.password = pconn_info->password;
    mqtt_params.pub_key = pconn_info->pub_key;

    mqtt_params.request_timeout_ms = 1000;
    mqtt_params.clean_session = 0;
    mqtt_params.keepalive_interval_ms = 1000;
    mqtt_params.pread_buf = msg_readbuf;
    mqtt_params.read_buf_size = MSG_LEN_MAX;
    mqtt_params.pwrite_buf = msg_buf;
    mqtt_params.write_buf_size = MSG_LEN_MAX;

    mqtt_params.handle_event.h_fp = event_handle;
    mqtt_params.handle_event.pcontext = NULL;

    /* Construct a MQTT client with specify parameter */

    pclient = IOT_MQTT_Construct(&mqtt_params);
    if (NULL == pclient) {
        EXAMPLE_TRACE("MQTT construct failed");
        rc = -1;
        goto do_exit;
    }

    /* Subscribe the specific topic */
//    rc = IOT_MQTT_Subscribe(pclient, TOPIC_DATA, IOTX_MQTT_QOS1, _demo_message_arrive, NULL);
    rc = IOT_MQTT_Subscribe(pclient, topic_data, IOTX_MQTT_QOS1, _demo_message_arrive, NULL);

    if (rc < 0) {
        IOT_MQTT_Destroy(&pclient);
        EXAMPLE_TRACE("IOT_MQTT_Subscribe() failed, rc = %d", rc);
        rc = -1;
        goto do_exit;
    }

    HAL_SleepMs(1000);

    /* Initialize topic information */
    memset(&topic_msg, 0x0, sizeof(iotx_mqtt_topic_info_t));
    strcpy(msg_pub, "message: hello! start!");

    topic_msg.qos = IOTX_MQTT_QOS1;
    topic_msg.retain = 0;
    topic_msg.dup = 0;
    topic_msg.payload = (void *)msg_pub;
    topic_msg.payload_len = strlen(msg_pub);

    while (1) {
        if(got_ip_flag){

            msg_len = snprintf(msg_pub, sizeof(msg_pub), "weight:%shigh:%stime:",weight,high);
           // msg_len = snprintf(msg_pub, sizeof(msg_pub), "{\"attr_name\":\"weight\", \"attr_value\":\"%d\"}",\"attr_name\":\"high\",\"attr_value\":\"%d\"}", 17,56);
            if (msg_len < 0) {
                EXAMPLE_TRACE("Error occur! Exit program");
                rc = -1;
                break;
            }
            ts = sntp_get_current_timestamp();
			strcat(msg_pub,(const char *)sntp_get_real_time(ts));
		    msg_len += (int)strlen((const char *)sntp_get_real_time(ts));
            topic_msg.payload = (void *)msg_pub;
            topic_msg.payload_len = msg_len;

            rc = IOT_MQTT_Publish(pclient,topic_data, &topic_msg);

            if (rc < 0) {
                EXAMPLE_TRACE("error occur when publish");
                rc = -1;
                break;
            }
    #ifdef MQTT_ID2_CRYPTO
            EXAMPLE_TRACE("packet-id=%u, publish topic msg='0x%02x%02x%02x%02x'...",
                          (uint32_t)rc,
                          msg_pub[0], msg_pub[1], msg_pub[2], msg_pub[3]
                         );
    #else
            EXAMPLE_TRACE("packet-id=%u, publish topic msg=%s\n", (uint32_t)rc, msg_pub);
    #endif

            /* handle the MQTT packet received from TCP or SSL connection */
            IOT_MQTT_Yield(pclient, 200);

            HAL_SleepMs(3000);
        }else{

            break;
        }
        /* Generate topic message */
    }

    IOT_MQTT_Unsubscribe(pclient, topic_data);
    IOT_MQTT_Unsubscribe(pclient, topic_relay);

    IOT_MQTT_Destroy(&pclient);

do_exit:

    if (NULL != msg_buf) {
        HAL_Free(msg_buf);
    }

    if (NULL != msg_readbuf) {
        HAL_Free(msg_readbuf);
    }
	if (NULL != topic_data) {
        os_free(topic_data);
    }

    if (NULL != topic_relay) {
        os_free(topic_relay);
    }

    return rc;
}

int ota_client(void)
{
    int rc = 0, ota_over = 0;
    void *pclient = NULL, *h_ota = NULL;
    iotx_conn_info_pt pconn_info;
    iotx_mqtt_param_t mqtt_params;
    char *msg_buf = NULL, *msg_readbuf = NULL;
    char buf_ota[OTA_BUF_LEN];

    if (NULL == (msg_buf = (char *)HAL_Malloc(MSG_LEN_MAX))) {
        EXAMPLE_TRACE("not enough memory");
        rc = -1;
        goto do_exit;
    }

    if (NULL == (msg_readbuf = (char *)HAL_Malloc(MSG_LEN_MAX))) {
        EXAMPLE_TRACE("not enough memory");
        rc = -1;
        goto do_exit;
    }

    /* Device AUTH */
    if (0 != IOT_SetupConnInfo(PRODUCT_KEY, DEVICE_NAME, DEVICE_SECRET, (void **)&pconn_info)) {
        EXAMPLE_TRACE("AUTH request failed!");
        rc = -1;
        goto do_exit;
    }

    /* Initialize MQTT parameter */
    memset(&mqtt_params, 0x0, sizeof(mqtt_params));

    mqtt_params.port = pconn_info->port;
    mqtt_params.host = pconn_info->host_name;
    mqtt_params.client_id = pconn_info->client_id;
    mqtt_params.username = pconn_info->username;
    mqtt_params.password = pconn_info->password;
    mqtt_params.pub_key = pconn_info->pub_key;

    mqtt_params.request_timeout_ms = 2000;
    mqtt_params.clean_session = 0;
    mqtt_params.keepalive_interval_ms = 60000;
    mqtt_params.pread_buf = msg_readbuf;
    mqtt_params.read_buf_size = MSG_LEN_MAX;
    mqtt_params.pwrite_buf = msg_buf;
    mqtt_params.write_buf_size = MSG_LEN_MAX;

    mqtt_params.handle_event.h_fp = event_handle;
    mqtt_params.handle_event.pcontext = NULL;


    /* Construct a MQTT client with specify parameter */
    pclient = IOT_MQTT_Construct(&mqtt_params);
    if (NULL == pclient) {
        EXAMPLE_TRACE("MQTT construct failed");
        rc = -1;
        goto do_exit;
    }
    h_ota = IOT_OTA_Init(PRODUCT_KEY, DEVICE_NAME, pclient);
    if (NULL == h_ota) {
        rc = -1;
        EXAMPLE_TRACE("initialize OTA failed");
        goto do_exit;
    }

    if (0 != IOT_OTA_ReportVersion(h_ota, "iotx_esp_1.0.0")) {
        rc = -1;
        EXAMPLE_TRACE("report OTA version failed");
        goto do_exit;
    }

    HAL_SleepMs(1000);

    system_upgrade_flag_set(UPGRADE_FLAG_START);
    system_upgrade_init();

    system_upgrade("erase flash", ERASE_FLASH_SIZE);

    // OTA timeout
    os_timer_disarm(&ota_timer);
    os_timer_setfn(&ota_timer, (os_timer_func_t *)upgrade_recycle, NULL);
    os_timer_arm(&ota_timer, OTA_TIMEOUT, 0);

    while (1) {
        if (!ota_over) {
            uint32_t firmware_valid;

            EXAMPLE_TRACE("wait ota upgrade command....");

            /* handle the MQTT packet received from TCP or SSL connection */
            IOT_MQTT_Yield(pclient, 200);

            if (IOT_OTA_IsFetching(h_ota)) {
                uint32_t last_percent = 0, percent = 0;
                char version[128], md5sum[33];
                uint32_t len, size_downloaded, size_file;

                do {
                    len = IOT_OTA_FetchYield(h_ota, buf_ota, OTA_BUF_LEN, 1);
                    if (len > 0) {

                        if (false == system_upgrade(buf_ota, len)){
                            system_upgrade_flag_set(UPGRADE_FLAG_IDLE);
                            upgrade_recycle();
                        }
                        binary_file_length += len;
                        printf("Have written image length %d\n", binary_file_length);

                    }

                    /* get OTA information */
                    IOT_OTA_Ioctl(h_ota, IOT_OTAG_FETCHED_SIZE, &size_downloaded, 4);
                    IOT_OTA_Ioctl(h_ota, IOT_OTAG_FILE_SIZE, &size_file, 4);
                    IOT_OTA_Ioctl(h_ota, IOT_OTAG_MD5SUM, md5sum, 33);
                    IOT_OTA_Ioctl(h_ota, IOT_OTAG_VERSION, version, 128);

                    last_percent = percent;
                    percent = (size_downloaded * 100) / size_file;
                    if (percent - last_percent > 0) {
                        IOT_OTA_ReportProgress(h_ota, percent, NULL);
                        IOT_OTA_ReportProgress(h_ota, percent, "hello");
                    }
                    IOT_MQTT_Yield(pclient, 100);
                } while (!IOT_OTA_IsFetchFinish(h_ota));

                IOT_OTA_Ioctl(h_ota, IOT_OTAG_CHECK_FIRMWARE, &firmware_valid, 4);
                if (0 == firmware_valid) {
                    EXAMPLE_TRACE("The firmware is invalid");
                } else {
                    if(upgrade_crc_check(system_get_fw_start_sec(),binary_file_length) != true) {
                        printf("upgrade crc check failed !\n");
                        system_upgrade_flag_set(UPGRADE_FLAG_IDLE);
                        upgrade_recycle();
                    }
                system_upgrade_flag_set(UPGRADE_FLAG_FINISH);
                upgrade_recycle();
                }
                ota_over = 1;
            }
            HAL_SleepMs(2000);
        }
    }

    HAL_SleepMs(200);

do_exit:

    if (NULL != h_ota) {
        IOT_OTA_Deinit(h_ota);
    }

    if (NULL != pclient) {
        IOT_MQTT_Destroy(&pclient);
    }

    if (NULL != msg_buf) {
        HAL_Free(msg_buf);
    }

    if (NULL != msg_readbuf) {
        HAL_Free(msg_readbuf);
    }

    return rc;
}

void mqtt_proc(void*para)
{
    while(1){   // reconnect to tls
        while(!got_ip_flag){
            vTaskDelay(2000 / portTICK_RATE_MS);
        }
        printf("[ALIYUN] MQTT client example begin, free heap size:%d\n", system_get_free_heap_size());
        mqtt_client();
        printf("[ALIYUN] MQTT client example end, free heap size:%d\n", system_get_free_heap_size());
    }
}

void ota_proc(void*para)
{
    while(1){   // reconnect to tls
        while(!got_ip_flag){
            vTaskDelay(2000 / portTICK_RATE_MS);
        }
        printf("[ALIYUN] OTA client example begin, free heap size:%d\n", system_get_free_heap_size());
        ota_client();
        printf("[ALIYUN] OTA client example end, free heap size:%d\n", system_get_free_heap_size());
    }
}

void sntpfn()
{

    os_printf("Initializing SNTP\n");
    sntp_setoperatingmode(SNTP_OPMODE_POLL);
    sntp_setservername(0, "120.25.115.19");
    sntp_setservername(1, "202.112.29.82");        // set sntp server after got ip address, you had better to adjust the sntp server to your area
    sntp_setservername(2, "time-a.nist.gov");
    sntp_setservername(3, "ntp.sjtu.edu.cn");
    sntp_setservername(4, "0.nettime.pool.ntp.org");
    sntp_setservername(5, "time-b.nist.gov");
    sntp_setservername(6, "time-a.timefreq.bldrdoc.gov");
    sntp_setservername(7, "time-b.timefreq.bldrdoc.gov");
    sntp_setservername(8, "time-c.timefreq.bldrdoc.gov");
    sntp_setservername(9, "ntp.sjtu.edu.cn");
    sntp_setservername(10, "us.pool.ntp.org");
    sntp_setservername(11, "time-a.timefreq.bldrdoc.gov");
    sntp_init();
    while(1){
        u32_t ts = 0;
        ts = sntp_get_current_timestamp();
        os_printf("current time : %s\n", sntp_get_real_time(ts));
        if (ts == 0) {
            os_printf("did not get a valid time from sntp server\n");
        } else {
            break;
        }
        vTaskDelay(2000 / portTICK_RATE_MS);
    }
}

// esp_err_t event_handler(void *ctx, system_event_t *event)
void event_handler(System_Event_t *event)
{
    switch (event->event_id)
    {
    case EVENT_STAMODE_GOT_IP:
        ESP_LOGI(TAG, "Connected.");
        if (system_get_free_heap_size() < 30 * 1024){
            system_restart();
        }
        sntpfn();
        got_ip_flag = 1;
        break;

    case EVENT_STAMODE_DISCONNECTED:
        ESP_LOGI(TAG, "Wifi disconnected, try to connect ...");
        got_ip_flag = 0;
		
        while(fifo_tmp[0]!='a'||fifo_tmp[1]!='A'||fifo_tmp[2]!='b'||fifo_tmp[3]!='B'||fifo_tmp[4]!='F')
        {
          printf("[yebin]----please set network by bluetooth---");
		  vTaskDelay(2000 / portTICK_RATE_MS);
		}

        printf("[yebin]fifo_tmp--%s--",fifo_tmp);		
        fifo = fifo_tmp;
        while(*fifo!='\0')
        {  
          if(*fifo=='$'&&*(fifo+1)=='F'&&*(fifo+2)=='F')
		  break;
		  fifo++;
        }
		
        fifo+=3;
		fifo_i=0;
        while(*fifo!='\0')
        {  
          if(*fifo=='$'&&*(fifo+1)=='F'&&*(fifo+2)=='F')
		  break;
		  key[fifo_i] =*fifo;
		  fifo++;
		  fifo_i++;
        }
		key[fifo_i+1]='\0';
		
        fifo+=3;
		fifo_i=0;
		while(*fifo!='\0')
        {  
          if(*fifo=='$'&&*(fifo+1)=='F'&&*(fifo+2)=='F')
		  break;
		  name[fifo_i] =*fifo;
		  fifo++;
		  fifo_i++;
        }
		name[fifo_i+1]='\0';

        fifo+=3;
		fifo_i=0;
		while(*fifo!='\0')
        {  
          if(*fifo=='$'&&*(fifo+1)=='F'&&*(fifo+2)=='F')
		  break;
		  secret[fifo_i] =*fifo;
		  fifo++;
		  fifo_i++;
        }
		secret[fifo_i+1]='\0';

        fifo+=3;
		fifo_i=0;
		while(*fifo!='\0')
        {  
          if(*fifo=='$'&&*(fifo+1)=='F'&&*(fifo+2)=='F')
		  break;
		  wifi_ssid[fifo_i] =*fifo;
		  fifo++;
		  fifo_i++;
        }
		wifi_ssid[fifo_i+1]='\0';


		fifo+=3;
		fifo_i=0;
		while(*fifo!='\0')
        {  
          if(*fifo=='$'&&*(fifo+1)=='F'&&*(fifo+2)=='F')
		  break;
		  wifi_pass[fifo_i] =*fifo;
		  fifo++;
		  fifo_i++;
        }
		wifi_pass[fifo_i+1]='\0';
		
		fifo+=3;
		fifo_i=0;
		while(*fifo!='\0')
        {  
          if(*fifo=='$'&&*(fifo+1)=='F'&&*(fifo+2)=='F')
		  break;
		  weight[fifo_i] =*fifo;
		  fifo++;
		  fifo_i++;
        }
		weight[fifo_i+1]='\0';

	    fifo+=3;
		fifo_i=0;
		while(*fifo!='\0')
        {  
          if(*fifo=='$'&&*(fifo+1)=='F'&&*(fifo+2)=='F')
		  break;
		  high[fifo_i] =*fifo;
		  fifo++;
		  fifo_i++;
        }
		high[fifo_i+1]='\0';
	    printf("[yebin]--key is --%s--\n",key);
		printf("[yebin]--name is --%s--\n",name);
	    printf("[yebin]--secret is --%s--\n",secret);
		printf("[yebin]--wifi ssid is --%s--\n",wifi_ssid);
		printf("[yebin]--wifi_pass is --%s--\n",wifi_pass);
	    printf("[yebin]--weight is --%s--\n",weight);
		printf("[yebin]--high is --%s--\n",high);
		struct station_config config;
        bzero(&config, sizeof(struct station_config));
        sprintf(config.ssid, wifi_ssid);
        sprintf(config.password, wifi_pass);
        wifi_station_set_config(&config);
		wifi_station_connect();
        break;

    default:
        break;
    }
}

void initialize_wifi(void)
{
    os_printf("SDK version:%s %d\n", system_get_sdk_version(), system_get_free_heap_size());
     wifi_set_opmode(STATION_MODE);

     // set AP parameter
     struct station_config config;
     bzero(&config, sizeof(struct station_config));
     sprintf(config.ssid, WIFI_SSID);
     sprintf(config.password, WIFI_PASSWORD);
     wifi_station_set_config(&config);

     wifi_station_set_auto_connect(true);
     wifi_station_set_reconnect_policy(true);
     wifi_set_event_handler_cb(event_handler);
}

void heap_check_task(void*para){
    while(1){
        vTaskDelay(2000 / portTICK_RATE_MS);
        printf("[heap check task] free heap size:%d\n", system_get_free_heap_size());
    }
}





void user_init(void)
{
    extern unsigned int max_content_len;
    max_content_len = 4 * 1024;
    uart_init_new();
    printf("SDK version:%s \n", system_get_sdk_version());
    printf("\n******************************************  \n  SDK compile time:%s %s\n******************************************\n\n", __DATE__, __TIME__);
    IOT_OpenLog("mqtt");
    IOT_SetLogLevel(IOT_LOG_DEBUG);


/*--------------*/

   //if(fifo_tmp[0]!='a'||fifo_tmp[1]!='A'||fifo_tmp[2]!='b'||fifo_tmp[3]!='B')
  
  // uint8 *fifo = fifo_tmp;
  // printf("[yebin]----%s----",fifo);
  // while(*fifo!='\0')
  // {
   //  fifo++;
   //}

/*
if(fifo_tmp[0]=='a'&&fifo_tmp[1]=='A'&&fifo_tmp[2]=='b'&&fifo_tmp[3]=='B')
{
	printf("[yebin]----%s----",fifo_tmp);
	uint8 *fifo = fifo_tmp;
	uint8 *fifo_pot=NULL;
	uint8 i=0;
    while(*fifo!='\0')
    {
      if(*fifo == '$')
      {fifo_pot = fifo;i=0;} 	
      if(*(fifo_pot+1)=='0')
      key[i] = *fifo;
	  else if (*(fifo_pot+1)=='1')
	  name[i] = *fifo;	
	  else if (*(fifo_pot+1)=='2')
	  secret[i] = *fifo;
	  else if (*(fifo_pot+1)=='3')
	  wifi_ssid[i] = *fifo;
	  else if (*(fifo_pot+1)=='4')
	  wifi_pass[i] = *fifo;
	  else if (*(fifo_pot+1)=='5')
	  weight[i] = *fifo;
	  else if (*(fifo_pot+1)=='6')
	  high[i] = *fifo;
	  fifo++;
	  i++;
	}
	printf("[yebin]--key--%s----",key);
	printf("[yebin]--name--%s----",name);
	printf("[yebin]--secret--%s----",secret);
	printf("[yebin]--wifi_ssid--%s----",wifi_ssid);
	printf("[yebin]--wifi_pass--%s----",wifi_pass);
	printf("[yebin]--weight--%s----",weight);
    printf("[yebin]--high--%s----",high);

}










/*--------------*/


	
    initialize_wifi();
    got_ip_flag = 0;

#if HEAP_CHECK_TASK
    xTaskCreate(heap_check_task, "heap_check_task", 128, NULL, 5, NULL);
#endif

#if MQTT_TASK
    xTaskCreate(mqtt_proc, "mqtt_proc", 2048, NULL, 5, NULL);
    printf("\nMQTT Task Started...\n");
#elif LOCAL_OTA
    printf("\nLocal OTA Task Started...\n");
    xTaskCreate(ota_main, "ota_main", 2048, NULL, 5, NULL);
#elif OTA_TASK
    xTaskCreate(ota_proc, "ota_proc", 2048, NULL, 5, NULL);
    printf("\nOTA Task Started...\n");
#else

#endif

}


