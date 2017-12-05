#ifndef ALIYUN_CONFIG_H_
#define ALIYUN_CONFIG_H_


#define HEAP_CHECK_TASK 0

#define MQTT_TASK 1
#define LOCAL_OTA 0
#define OTA_TASK 0

#define MQTT_DIRECT

#define PRODUCT_KEY             "ymXuzyfmuQb"
#define DEVICE_NAME             "esp8266_test001"
#define DEVICE_SECRET           "32fZgRbygua80rdVNLb5femE7wHBh7G9"

// These are pre-defined topics
#define TOPIC_UPDATE            "/"PRODUCT_KEY"/"DEVICE_NAME"/update"
#define TOPIC_ERROR             "/"PRODUCT_KEY"/"DEVICE_NAME"/update/error"
#define TOPIC_GET               "/"PRODUCT_KEY"/"DEVICE_NAME"/get"
#define TOPIC_DATA              "/"PRODUCT_KEY"/"DEVICE_NAME"/data"
#define TOPIC_RELAY             "/"PRODUCT_KEY"/"DEVICE_NAME"/relay"

#define WIFI_SSID       "Tencent ZC2"
#define WIFI_PASSWORD   "zckj1234"
#define MSG_LEN_MAX             (2048)

int got_ip_flag;

#define OTA_BUF_LEN 1460
#define ERASE_FLASH_SIZE 400 * 1024
#if 1
#define EXAMPLE_TRACE(fmt, args...)  \
    do { \
        os_printf("%s|%03d :: ", __func__, __LINE__); \
        os_printf(fmt, ##args); \
        os_printf("%s", "\r\n"); \
    } while(0)
#else
#define EXAMPLE_TRACE(fmt, args...)  \
    os_printf(fmt, ##args)
#endif



#endif
