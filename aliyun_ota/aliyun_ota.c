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

#include "iot_export_ota.h"

#include "esp_common.h"
#include "lwip/mem.h"
#include "aliyun_config.h"
#include "aliyun_ota.h"

LOCAL uint32 totallength = 0;
LOCAL uint32 sumlength = 0;
LOCAL bool flash_erased = false;
LOCAL os_timer_t upgrade_timer;
extern int got_ip_flag;

struct upgrade_param {
    uint32 fw_bin_addr;
    uint16 fw_bin_sec;
    uint16 fw_bin_sec_num;
    uint16 fw_bin_sec_earse;
    uint8 extra;
    uint8 save[4];
    uint8 *buffer;
};

LOCAL struct upgrade_param *upgrade;

LOCAL bool OUT_OF_RANGE(uint16 erase_sec)
{
    uint8 spi_size_map = system_get_flash_size_map();
    uint16 sec_num = 0;
    uint16 start_sec = 0;
    
    if (spi_size_map == FLASH_SIZE_8M_MAP_512_512 || 
            spi_size_map ==FLASH_SIZE_16M_MAP_512_512 ||
            spi_size_map ==FLASH_SIZE_32M_MAP_512_512){
            start_sec = (system_upgrade_userbin_check() == USER_BIN2)? 1:129;
            sec_num = 123;
    } else if(spi_size_map == FLASH_SIZE_16M_MAP_1024_1024 || 
            spi_size_map == FLASH_SIZE_32M_MAP_1024_1024){
            start_sec = (system_upgrade_userbin_check() == USER_BIN2)? 1:257;
            sec_num = 251;
    } else {
            start_sec = (system_upgrade_userbin_check() == USER_BIN2)? 1:65;
            sec_num = 59;
    }
    if((erase_sec >= start_sec) &&(erase_sec <= (start_sec + sec_num)))
    {
        return false;
    } else {
        return true;
    }
}

/******************************************************************************
 * FunctionName : user_upgrade_internal
 * Description  : a
 * Parameters   :
 * Returns      :
*******************************************************************************/
LOCAL bool system_upgrade_internal(struct upgrade_param *upgrade, uint8 *data, u32 len)
{
    bool ret = false;
    uint16 secnm=0;
    if(data == NULL || len == 0)
    {
        return true;
    }

    /*got the sumlngth,erase all upgrade sector*/
    if(len > SPI_FLASH_SEC_SIZE ) {
        upgrade->fw_bin_sec_earse=upgrade->fw_bin_sec;

        secnm=((upgrade->fw_bin_addr + len)>>12) + (len&0xfff?1:0);
        while(upgrade->fw_bin_sec_earse != secnm) {
            taskENTER_CRITICAL();
            if( OUT_OF_RANGE( upgrade->fw_bin_sec_earse) )
            {
                os_printf("fw_bin_sec_earse:%d, Out of range\n",upgrade->fw_bin_sec_earse);
                break;
            
            }
            else
            {
                 spi_flash_erase_sector(upgrade->fw_bin_sec_earse);
                 upgrade->fw_bin_sec_earse++;
            }
            taskEXIT_CRITICAL();
            vTaskDelay(10 / portTICK_RATE_MS);
        }
        os_printf("flash erase over\n");
        return true;
    }
    
    upgrade->buffer = (uint8 *)zalloc(len + upgrade->extra);

    memcpy(upgrade->buffer, upgrade->save, upgrade->extra);
    memcpy(upgrade->buffer + upgrade->extra, data, len);

    len += upgrade->extra;
    upgrade->extra = len & 0x03;
    len -= upgrade->extra;

    if(upgrade->extra<=4)
        memcpy(upgrade->save, upgrade->buffer + len, upgrade->extra);
    else
        os_printf("ERR3:arr_overflow,%u,%d\n",__LINE__,upgrade->extra);

    do {
        if (upgrade->fw_bin_addr + len >= (upgrade->fw_bin_sec + upgrade->fw_bin_sec_num) * SPI_FLASH_SEC_SIZE) {
            printf("spi_flash_write exceed\n");
            break;
        }

        if (spi_flash_write(upgrade->fw_bin_addr, (uint32 *)upgrade->buffer, len) != SPI_FLASH_RESULT_OK) {
            printf("spi_flash_write failed\n");
            break;
        }
        
        ret = true;
        upgrade->fw_bin_addr += len;
    } while (0);

    free(upgrade->buffer);
    upgrade->buffer = NULL;
    return ret;
}

/******************************************************************************
 * FunctionName : system_get_fw_start_sec
 * Description  : a
 * Parameters   :
 * Returns      :
*******************************************************************************/
uint16 system_get_fw_start_sec()
{
    if(upgrade != NULL) {
        return upgrade->fw_bin_sec;
    } else {
        return 0;
    }
}

/******************************************************************************
 * FunctionName : user_upgrade
 * Description  : a
 * Parameters   :
 * Returns      :
*******************************************************************************/
bool system_upgrade(uint8 *data, uint32 len)
{
    bool ret;
    ret = system_upgrade_internal(upgrade, data, len);
    return ret;
}

void upgrade_recycle(void)
{
    totallength = 0;
    sumlength = 0;
    flash_erased = false;

    system_upgrade_deinit();


    if (system_upgrade_flag_check() == UPGRADE_FLAG_FINISH) {
        system_upgrade_reboot(); // if need
    }
    vTaskDelete(NULL);
}


/******************************************************************************
 * FunctionName : system_upgrade_init
 * Description  : a
 * Parameters   :
 * Returns      :
*******************************************************************************/
void  
system_upgrade_init(void)
{
    uint32 user_bin2_start,user_bin1_start;
    uint8 spi_size_map = system_get_flash_size_map();
    
    if (upgrade == NULL) {
        upgrade = (struct upgrade_param *)zalloc(sizeof(struct upgrade_param));
    }
    
    user_bin1_start = 1; 

    if (spi_size_map == FLASH_SIZE_8M_MAP_512_512 || 
            spi_size_map ==FLASH_SIZE_16M_MAP_512_512 ||
            spi_size_map ==FLASH_SIZE_32M_MAP_512_512){
            user_bin2_start = 129;
            upgrade->fw_bin_sec_num = 123;
    } else if(spi_size_map == FLASH_SIZE_16M_MAP_1024_1024 || 
            spi_size_map == FLASH_SIZE_32M_MAP_1024_1024){
            user_bin2_start = 257;
            upgrade->fw_bin_sec_num = 251;
    } else {
            user_bin2_start = 65;
            upgrade->fw_bin_sec_num = 59;
    }
   
    upgrade->fw_bin_sec = (system_upgrade_userbin_check() == USER_BIN1) ? user_bin2_start : user_bin1_start;

    upgrade->fw_bin_addr = upgrade->fw_bin_sec * SPI_FLASH_SEC_SIZE;
    
    upgrade->fw_bin_sec_earse = upgrade->fw_bin_sec;
}

/******************************************************************************
 * FunctionName : system_upgrade_deinit
 * Description  : a
 * Parameters   :
 * Returns      :
*******************************************************************************/
void  
system_upgrade_deinit(void)
{
    if (upgrade != NULL) {
        free(upgrade);
        upgrade = NULL;
    }else {

    }
    printf("ota end, free heap size:%d\n", system_get_free_heap_size());
    return;
}

void print_debug(const char* data, const int len, const char* note){
#define COUNT_BYTE_AND_NEW_LINE 0
#define ALL_BINARY_SHOW 0
    printf("\n********** %s [len:%d] start addr:%p **********\n", note, len, data);
    int i = 0;
    for (i = 0; i < len; ++i){
#if !(ALL_BINARY_SHOW)
    if(data[i] < 33 || data[i] > 126)
     {
        if(i > 0 && (data[i-1] >= 33 && data[i-1] <= 126) )
                printf(" ");
        printf("%02x ",data[i]);
     }else{
        printf("%c", data[i]);
     }
#else
        printf("%02x ",data[i]);
#endif

#if COUNT_BYTE_AND_NEW_LINE
   if ((i + 1) % 32 == 0){
        printf("    | %d Bytes\n",i + 1);
    }
#endif
}

printf("\n---------- %s End ----------\n", note);
}

/******************************************************************************
 * FunctionName : upgrade_download
 * Description  : parse http response ,and download remote data and write in flash
 * Parameters   : int sta_socket : ota client socket fd
 *                char *pusrdata : remote data
 *                length         : data length
 * Returns      : none
*******************************************************************************/
void upgrade_download(int sta_socket,char *pusrdata, unsigned short length)
{
    char *ptr = NULL;
    char *ptmp2 = NULL;
    char lengthbuffer[32];
    if (totallength == 0 && (ptr = (char *)strstr(pusrdata, "\r\n\r\n")) != NULL &&
            (ptr = (char *)strstr(pusrdata, "Content-Length")) != NULL) {
        ptr = (char *)strstr(pusrdata, "\r\n\r\n");
        length -= ptr - pusrdata;
        length -= 4;
        printf("upgrade file download start.\n");

        ptr = (char *)strstr(pusrdata, "Content-Length: ");
        if (ptr != NULL) {
            ptr += 16;
            ptmp2 = (char *)strstr(ptr, "\r\n");

            if (ptmp2 != NULL) {
                memset(lengthbuffer, 0, sizeof(lengthbuffer));
                memcpy(lengthbuffer, ptr, ptmp2 - ptr);
                sumlength = atoi(lengthbuffer);
                if(sumlength > 0) {
                    if (false == system_upgrade(pusrdata, sumlength)) {
                        system_upgrade_flag_set(UPGRADE_FLAG_IDLE);
                        goto ota_recycle;
                    }
                    flash_erased = true;
                    ptr = (char *)strstr(pusrdata, "\r\n\r\n");
                    if (false == system_upgrade(ptr + 4, length)){
                        system_upgrade_flag_set(UPGRADE_FLAG_IDLE);
                        goto ota_recycle;
                    }
                    totallength += length;
                    printf("sumlength = %d\n",sumlength);
                    return;
                }
            } else {
                printf("sumlength failed\n");
                system_upgrade_flag_set(UPGRADE_FLAG_IDLE);
                goto ota_recycle;
            }
        } else {
            printf("Content-Length: failed\n");
            system_upgrade_flag_set(UPGRADE_FLAG_IDLE);
            goto ota_recycle;
        }
    } else {
        if(totallength == 0 && sumlength == 0){
            printf("%s\n", pusrdata);
            return;
        }
        totallength += length;
        printf("totallen = %d\n",totallength);
        if (false == system_upgrade(pusrdata, length)){
            system_upgrade_flag_set(UPGRADE_FLAG_IDLE);
            goto ota_recycle;
        }
        if (totallength == sumlength) {
            printf("upgrade file download finished.\n");

            if(upgrade_crc_check(system_get_fw_start_sec(),sumlength) != true) {
                printf("upgrade crc check failed !\n");
                system_upgrade_flag_set(UPGRADE_FLAG_IDLE);
                goto ota_recycle;
            }

        system_upgrade_flag_set(UPGRADE_FLAG_FINISH);
        goto ota_recycle;
        } else {
            return;
        }
    }


ota_recycle :
        printf("go to ota recycle\n");
        close(sta_socket);
        upgrade_recycle();

}

/******************************************************************************
 * FunctionName : ota_begin
 * Description  : ota_task function
 * Parameters   : task param
 * Returns      : none
*******************************************************************************/
void ota_begin()
{
    int recbytes;
    int sin_size;
    int sta_socket;
    char recv_buf[1460];
    uint8 user_bin[21] = {0};
    struct sockaddr_in remote_ip;
    printf("Hello, welcome to client!\r\n");
    printf("ota serer addr %s port %d\n", LOCAL_OTA_SERVER_IP, LOCAL_OTA_SERVER_PORT);

#if DNS_TEST
    #define OTA_SERVER_NAME "www.espressif.com"
    ip_addr_t target_ip;
    int ret;
    do {
        ret = netconn_gethostbyname(OTA_SERVER_NAME, &target_ip);
    } while(ret);
    os_printf("get target IP is %d.%d.%d.%d\n", (unsigned char)((target_ip.addr & 0x000000ff) >> 0),
                                                (unsigned char)((target_ip.addr & 0x0000ff00) >> 8),
                                                (unsigned char)((target_ip.addr & 0x00ff0000) >> 16),
                                                (unsigned char)((target_ip.addr & 0xff000000) >> 24));
    remote_ip.sin_addr.s_addr = target_ip.addr;
    while(1){
        vTaskDelay(2000 / portTICK_RATE_MS);
    }
#endif


    while (1) {
        sta_socket = socket(PF_INET,SOCK_STREAM,0);
        if (-1 == sta_socket)
        {

            close(sta_socket);
            printf("socket fail !\r\n");
            continue;
        }
        printf("socket ok!\r\n");
        bzero(&remote_ip,sizeof(struct sockaddr_in));
        remote_ip.sin_family=AF_INET;

        remote_ip.sin_addr.s_addr= inet_addr(LOCAL_OTA_SERVER_IP);
        remote_ip.sin_port=htons(LOCAL_OTA_SERVER_PORT);

        if(0 != connect(sta_socket,(struct sockaddr *)(&remote_ip),sizeof(struct sockaddr)))
        {
            close(sta_socket);
            printf("connect fail!\r\n");
            system_upgrade_flag_set(UPGRADE_FLAG_IDLE);
            upgrade_recycle();
        }
        printf("connect ok!\r\n");
        char *pbuf = (char *)zalloc(512);
        if (system_upgrade_userbin_check() == UPGRADE_FW_BIN1) {
            memcpy(user_bin, "user2.2048.new.5.bin", 21);

        } else if (system_upgrade_userbin_check() == UPGRADE_FW_BIN2) {
            memcpy(user_bin, "user1.2048.new.5.bin", 21);
        }

        sprintf(pbuf, "GET /%s HTTP/1.0\r\nHost: \"%s\":%d\r\n"pheadbuffer"",
                       user_bin, LOCAL_OTA_SERVER_IP, LOCAL_OTA_SERVER_PORT);

        printf(pbuf);
        if(write(sta_socket,pbuf,strlen(pbuf)+1) < 0) {
                close(sta_socket);
                printf("send fail\n");
                free(pbuf);
                upgrade_recycle();
        }
        printf("send success\n");
        free(pbuf);

        while((recbytes = read(sta_socket ,recv_buf,1460)) >= 0){
            if(recbytes != 0 ) {
                upgrade_download(sta_socket,recv_buf,recbytes);
            }
        }
        printf("recbytes = %d\n",recbytes);
        if(recbytes < 0){
            printf("read data fail!\r\n");
            close(sta_socket);
            system_upgrade_flag_set(UPGRADE_FLAG_IDLE);
            upgrade_recycle();
        }
    }
}

// ota main task
void ota_main(void *pvParameter){
#if LOCAL_OTA
    while(!got_ip_flag){
        vTaskDelay(2000 / portTICK_RATE_MS);
        printf("wait for fetching IP...\n");
    }
#endif
    printf("ota begin, free heap size:%d\n", system_get_free_heap_size());
    system_upgrade_flag_set(UPGRADE_FLAG_START);
    system_upgrade_init();

    ota_begin();

    // OTA timeout
    os_timer_disarm(&upgrade_timer);
    os_timer_setfn(&upgrade_timer, (os_timer_func_t *)upgrade_recycle, NULL);
    os_timer_arm(&upgrade_timer, OTA_TIMEOUT, 0);

}

