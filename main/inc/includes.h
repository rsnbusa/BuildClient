#ifndef includes_h
#define includes_h

extern "C" {

#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include "esp_wifi.h"
#include "esp_system.h"
#include "driver/gpio.h"
#include "driver/uart.h"
#include "nvs_flash.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "freertos/queue.h"
#include "freertos/event_groups.h"

#include "lwip/sockets.h"
#include "lwip/netdb.h"
#include "lwip/sys.h"
#include "lwip/err.h"
#include "lwip/api.h"
#include "lwip/tcp.h"

#include "esp_log.h"

#include "cJSON.h"

#include "framSPI.h"
#include "framDef.h"

#include "defines.h"
#include "projStruct.h"


void app_main();
}

#endif
