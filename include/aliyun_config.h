#ifndef ALIYUN_CONFIG_H_
#define ALIYUN_CONFIG_H_


#define HEAP_CHECK_TASK 0

#define MQTT_TASK 1
#define LOCAL_OTA 0
#define OTA_TASK 0

#define MQTT_DIRECT


#define PRODUCT_KEY             "guUmT5yuxec"
#define DEVICE_NAME             "test1"
#define DEVICE_SECRET           "KkX50XRQM2jq3l9uYrYSR1opf5kf9dzi"
// These are pre-defined topics
#define TOPIC_UPDATE            "/"PRODUCT_KEY"/"DEVICE_NAME"/update"
#define TOPIC_ERROR             "/"PRODUCT_KEY"/"DEVICE_NAME"/update/error"
#define TOPIC_GET               "/"PRODUCT_KEY"/"DEVICE_NAME"/get"
#define TOPIC_DATA              "/"PRODUCT_KEY"/"DEVICE_NAME"/data"
#define TOPIC_RELAY             "/"PRODUCT_KEY"/"DEVICE_NAME"/relay"



#define WIFI_SSID       "Tencent ZC"
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
