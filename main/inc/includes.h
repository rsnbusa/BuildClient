#ifndef includes_h
#define includes_h

#include <inttypes.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <string>

extern "C"{
#include "cJSON.h"
#include "driver/gpio.h"
#include "driver/i2c.h"
#include "driver/pcnt.h"
#include "driver/spi_master.h"
#include "driver/uart.h"
#include "esp_event.h"
#include "esp_http_server.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_ota_ops.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "esp32/aes.h"
#include "framDef.h"
#include "framSPI.h"
#include "freertos/event_groups.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include "I2C.h"
#include "lwip/err.h"
#include "lwip/netdb.h"
#include "lwip/sockets.h"
#include "lwip/sys.h"
#include "math.h"
#include "mbedtls/base64.h"
#include "mbedtls/ctr_drbg.h"
#include "mbedtls/entropy.h"
#include "mbedtls/pk.h"
#include "nvs_flash.h"
#include "nvs.h"
#include "rom/rtc.h"
#include "SSD1306.h"
#include "tcpip_adapter.h"

void app_main();
}

#endif
