#include "includes.h"

config_flash					theConf;
QueueHandle_t 					mqttQ,mqttR;
SemaphoreHandle_t 				wifiSem,framSem,connSem;
char							deb;
u32								sentTotal=0,sendTcp=0,framWords=0;
u8								qwait=0;
u16 							qdelay,addressBytes;
u16								mesg,diag,horag,yearg;
meterType						theMeters,algo;
gpio_config_t 					io_conf;
cmdType							theCmd;
u16                  			diaTarifa[24],yearDay;      // % of Meter tariff. Ex: 800 *120=(20% cheaper).
nvs_handle 						nvshandle;
FramSPI							fram;
uint8_t							tempb[10000],nada;

static const char *TAG = "BDGCLIENT";

static EventGroupHandle_t wifi_event_group;
const static int WIFI_BIT = BIT0;
//change new again
//#define SENDER
#ifdef SENDER
void submode(void * pArg);
#endif

uint32_t IRAM_ATTR millisISR()
{
	return xTaskGetTickCountFromISR() * portTICK_PERIOD_MS;

}

uint32_t IRAM_ATTR millis()
{
	return xTaskGetTickCount() * portTICK_PERIOD_MS;

}

void delay(uint16_t a)
{
	vTaskDelay(a /  portTICK_RATE_MS);
}


void gpio_isr_handler(void * arg)
{
	BaseType_t tasker;
	u32 fueron;
	meterType *meter=(meterType*)arg;


	uint8_t como=gpio_get_level(meter->pin);
	if(deb)
		ets_printf("%d pin %d pos %d\n",como,meter->pin,meter->pos);

		if(como)
		{
			if (meter->startTimePulse>0) //first time no frame of reference, skip
			{
				meter->timestamp=millisISR();
				meter->livingPulse+=meter->timestamp-meter->startTimePulse; //array pulseTime has time elapsed in ms between low and high
				meter->livingCount++;
				meter->pulse=meter->livingPulse/meter->livingCount;
				if (meter->livingCount>100)
				{
			//		ets_printf("Pulse %d %d %d\n",meter->livingPulse/meter->livingCount,meter->livingPulse,meter->livingCount);
					meter->livingPulse=0;
					meter->livingCount=0;
				}
			}

			fueron=meter->startTimePulse-meter->timestamp;
			 if(fueron>=80)
			 {
				 meter->timestamp=millisISR(); //last valid isr
				 meter->beatSave++;
				 meter->beatSaveRaw++;
				 meter->currentBeat++;
				if((meter->currentBeat % 80)==0) //every GMAXLOSSPER interval
				{
					if(meter->beatSaveRaw>=800)
					{
						meter->beatSaveRaw=0;
						meter->curLife++;
						meter->beatSave=0;
					}
			//		meter->startConn=millisISR();

					if(mqttR)
					{
						xQueueSendFromISR( mqttR,&theMeters,&tasker );
							if (tasker)
									portYIELD_FROM_ISR();
					}
				}
			 }
		}
		else //rising edge start pulse timer
				meter->startTimePulse=millisISR();
	}

void install_meter_interrupts()
{
	char 	temp[30];
	memset(&theMeters,0,sizeof(theMeters));
	u8 mac[6];
	theMeters.pin=4;

	esp_wifi_get_mac(ESP_IF_WIFI_STA, (u8*)&mac);
	sprintf(temp,"MeterIoT%02x%02x",mac[4],mac[5]);
	printf("Mac %s\n",temp);
	gpio_install_isr_service(ESP_INTR_FLAG_IRAM);

	io_conf.intr_type = GPIO_INTR_ANYEDGE;
	io_conf.mode = GPIO_MODE_INPUT;
	io_conf.pull_down_en =GPIO_PULLDOWN_DISABLE;
	io_conf.pull_up_en =GPIO_PULLUP_ENABLE;

	strcpy(theMeters.serialNumber,temp);
	theMeters.pos=0;
	io_conf.pin_bit_mask = (1ULL<<theMeters.pin);
	gpio_config(&io_conf);
	gpio_isr_handler_add((gpio_num_t)theMeters.pin, gpio_isr_handler, (void*)&theMeters);

	io_conf.mode = GPIO_MODE_OUTPUT;
	io_conf.pull_down_en =GPIO_PULLDOWN_ENABLE;
	io_conf.pin_bit_mask = (1ULL<<WIFILED);
	gpio_config(&io_conf);
	gpio_set_level((gpio_num_t)WIFILED, 0);

	}

static void read_flash()
{
	esp_err_t q ;
	size_t largo;
	q = nvs_open("config", NVS_READONLY, &nvshandle);
		if(q!=ESP_OK)
		{
			printf("Error opening NVS Read File %x\n",q);
			return;
		}

			 largo=sizeof(theConf);
				q=nvs_get_blob(nvshandle,"config",(void*)&theConf,&largo);

			if (q !=ESP_OK)
				printf("Error read %x largo %d aqui %d\n",q,largo,sizeof(theConf));
	nvs_close(nvshandle);

}

static void write_to_flash() //save our configuration
{
	esp_err_t q ;
	q = nvs_open("config", NVS_READWRITE, &nvshandle);
		if(q!=ESP_OK)
		{
			printf("Error opening NVS File RW %x\n",q);
			return;
		}

		delay(300);
	q=nvs_set_blob(nvshandle,"config",(void*)&theConf,sizeof(theConf));
	if (q ==ESP_OK)
		q = nvs_commit(nvshandle);
	nvs_close(nvshandle);
}


static void wifi_event_handler(void* arg, esp_event_base_t event_base,
                                int32_t event_id, void* event_data)
{
    switch (event_id) {
        case WIFI_EVENT_STA_START:
            break;
        case WIFI_EVENT_STA_CONNECTED:
            xEventGroupClearBits(wifi_event_group, WIFI_BIT);
            break; //wait for ip
        case WIFI_EVENT_STA_DISCONNECTED:
            xEventGroupClearBits(wifi_event_group, WIFI_BIT);
            break;
        default:
        	printf("Default Id %d\n",event_id);
        //    xEventGroupSetBits(wifi_event_group, WIFI_BIT);//make it fail
            break;
    }
    return;
}

static void ip_event_handler(void* arg, esp_event_base_t event_base,
                                int32_t event_id, void* event_data)
{
    switch (event_id) {
        case IP_EVENT_STA_GOT_IP:
            xEventGroupSetBits(wifi_event_group, WIFI_BIT);
            break;
        default:
            break;
    }
    return;
}

static void recover_fram()
{
	char textl[100];

	scratchTypespi scratch;
	if(xSemaphoreTake(framSem, portMAX_DELAY/  portTICK_RATE_MS))
	{
		fram.read_recover(&scratch);

		if (scratch.medidor.state==0)
		{
			xSemaphoreGive(framSem);
			return;
		}

		sprintf(textl,"PF Recover. Meter %d State %d Life %x\n",scratch.medidor.meter,scratch.medidor.state,scratch.medidor.life);
		fram.write_lifekwh(scratch.medidor.meter,scratch.medidor.life);
		fram.write_month(scratch.medidor.meter,scratch.medidor.mesg,scratch.medidor.month);
		fram.write_day(scratch.medidor.meter,scratch.medidor.yearg,scratch.medidor.mesg,scratch.medidor.diag,scratch.medidor.day);
		fram.write_hour(scratch.medidor.meter,scratch.medidor.yearg,scratch.medidor.mesg,scratch.medidor.diag,scratch.medidor.horag,scratch.medidor.hora);
		fram.write_cycle(scratch.medidor.meter, scratch.medidor.mesg,scratch.medidor.cycle);
//		scratch.medidor.state=0;                                //variables written state
//		fram.write_recover(scratch);
//		scratch.medidor.state=0;                                // done state. OK
//		fram.write_recover(scratch);
		fram.write8(SCRATCHOFF,0); //Fast write first byte of Scratch record to 0=done.

		xSemaphoreGive(framSem);
		printf("Recover %s",textl);
	}
	//    mlog(GENERAL, textl);
}


static void write_to_fram(u8 meter,bool adding)
{
	// FRAM Semaphore is taken by the Interrupt Manager. Safe to work.
	scratchTypespi scratch;

//	if(aqui.traceflag & (1<<BEATD)) //Should not print. semaphore is taking longer
//			printf("[BEATD]Save KWH Meter %d Month %d Day %d Hour %d Year %d lifekWh %d\n",meter,mesg,diag,horag,yearg,theMeters.curLife+1);
	theMeters.curLife++;
	theMeters.curMonth++;
	theMeters.curDay++;
	theMeters.curHour++;
	theMeters.curCycle++;
	time(&theMeters.lastKwHDate); //last time we saved data

	scratch.medidor.state=1;                    //scratch written state. Must be 0 to be ok. Every 800-1000 beats so its worth it
	scratch.medidor.meter=meter;
	scratch.medidor.month=theMeters.curMonth;
	scratch.medidor.life=theMeters.curLife;
	scratch.medidor.day=theMeters.curDay;
	scratch.medidor.hora=theMeters.curHour;
	scratch.medidor.cycle=theMeters.curCycle;
	scratch.medidor.mesg=mesg;
	scratch.medidor.diag=diag;
	scratch.medidor.horag=horag;
	scratch.medidor.yearg=yearg;
	fram.write_recover(scratch);            //Power Failure recover register

	fram.write_beat(meter,theMeters.currentBeat);
	fram.write_lifekwh(meter,theMeters.curLife);
	fram.write_month(meter,mesg,theMeters.curMonth);
	fram.write_monthraw(meter,mesg,theMeters.curMonthRaw);
	fram.write_day(meter,yearg,mesg,diag,theMeters.curDay);
	fram.write_dayraw(meter,yearg,mesg,diag,theMeters.curDayRaw);
	fram.write_hour(meter,yearg,mesg,diag,horag,theMeters.curHour);
	fram.write_hourraw(meter,yearg,mesg,diag,horag,theMeters.curHourRaw);
	fram.write_cycle(meter, mesg,theMeters.curCycle);
//	fram.write_cycle(meter, theMeters.cycleMonth,theMeters.curCycle);
	fram.write_minamps(meter,theMeters.minamps);
	fram.write_maxamps(meter,theMeters.maxamps);
	fram.write_lifedate(meter,theMeters.lastKwHDate);  //should be down after scratch record???
//	scratch.medidor.state=2;            //variables written state
//	FramSPI_write_recover(scratch);

	fram.write8(SCRATCHOFF,0); //Fast write first byte of Scratch record to 0=done.

//	scratch.medidor.state=0;            // done state. OK
//	FramSPI_write_recover(scratch);
}

static void load_from_fram(u8 meter)
{
	if(xSemaphoreTake(framSem, portMAX_DELAY/  portTICK_RATE_MS))
	{
		fram.read_lifekwh(meter,(u8*)&theMeters.curLife);
		fram.read_lifedate(meter,(u8*)&theMeters.lastKwHDate);
		fram.read_month(meter, mesg, (u8*)&theMeters.curMonth);
		fram.read_monthraw(meter, mesg, (u8*)&theMeters.curMonthRaw);
		fram.read_day(meter, yearg,mesg, diag, (u8*)&theMeters.curDay);
		fram.read_dayraw(meter, yearg,mesg, diag, (u8*)&theMeters.curDayRaw);
		fram.read_hour(meter, yearg,mesg, diag, horag, (u8*)&theMeters.curHour);
		fram.read_hourraw(meter, yearg,mesg, diag, horag, (u8*)&theMeters.curHourRaw);
		fram.read_cycle(meter, mesg, (u8*)&theMeters.curCycle); //should we change this here too and use cycleMonth[meter]?????
		fram.read_beat(meter,(u8*)&theMeters.currentBeat);
		theMeters.oldbeat=theMeters.currentBeat;
		if(theConf.beatsPerKw[meter]==0)
			theConf.beatsPerKw[meter]=800;// just in case div by 0 crash
		u16 nada=theMeters.currentBeat/theConf.beatsPerKw[meter];
		theMeters.beatSave=theMeters.currentBeat-(nada*theConf.beatsPerKw[meter]);
		theMeters.beatSaveRaw=theMeters.beatSave;
		fram.read_minamps(meter,(u8*)&theMeters.minamps);
		fram.read_maxamps(meter,(u8*)&theMeters.maxamps);
		xSemaphoreGive(framSem);

//		if(aqui.traceflag & (1<<BEATD))
//			printf("[BEATD]Loaded Meter %d curLife %d\n",meter,theMeters.curLife);
	}
}

static void loadDayBPK(u16 hoy)
{
	if(xSemaphoreTake(framSem, portMAX_DELAY/  portTICK_RATE_MS)){
		fram.read_tarif_day(hoy, (u8*)&diaTarifa); // all 24 hours of todays Tariff Types [0..255] of TarifaBPW
		xSemaphoreGive(framSem);
	}
	else
		return;
//	if(aqui.traceflag & (1<<BOOTD))
//	{
//		for (int a=0;a<24;a++)
//			printf("[BOOTD]H[%d]=%d ",a,diaTarifa[a]);
//		printf("\n");
//
//	}
}

static void init_fram()
{
	scratchTypespi scratch;
	// FRAM Setup
	fram.begin(FMOSI,FMISO,FCLK,FCS,&framSem); //will create SPI channel and Semaphore
	//framWords=fram.intframWords;
	spi_flash_init();



		if(xSemaphoreTake(framSem, portMAX_DELAY/  portTICK_RATE_MS))
		{
			fram.read_recover(&scratch);
			xSemaphoreGive(framSem);
		}

		if (scratch.medidor.state!=0)
		{
			//  check_log_file(); //Our log file. Need to open it before normal sequence
			printf("Recover Fram\n");
			recover_fram();
			//recf=true;
		}
		//all okey in our Fram after this point

		//load all devices counters from FRAM
		for (int a=0;a<1;a++)
			load_from_fram(a);

//		if(xSemaphoreTake(framSem, portMAX_DELAY/  portTICK_RATE_MS))
//		{
//			fram.read_tarif_bytes(0, (u8*)&tarifaBPK, sizeof(tarifaBPK)); // read all 100 types of BPK
//			xSemaphoreGive(framSem);
//		}

		if(fram.intframWords>32768)
		{
			//	printf("Call load day \n");
			loadDayBPK(yearDay);
		}

}

static void wifi_init(void)
{
  //  tcpip_adapter_init();
    esp_netif_init();

    wifi_event_group = xEventGroupCreate();
    xEventGroupClearBits(wifi_event_group, WIFI_BIT);

    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_sta();

    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler, NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &ip_event_handler, NULL));

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    ESP_ERROR_CHECK(esp_wifi_set_storage(WIFI_STORAGE_RAM));
    wifi_config_t wifi_config;// = {
    memset(&wifi_config,0,sizeof(wifi_config));//very important
	strcpy((char*)wifi_config.sta.ssid,"Meteriot");
	strcpy((char*)wifi_config.sta.password,"Meteriot");
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(ESP_IF_WIFI_STA, &wifi_config));
    esp_wifi_start();
}

void sendMsg(char * message)
{
    struct netconn *conn = netconn_new(NETCONN_TCP);
    ip_addr_t remote;
    int waitTime;

    IP_ADDR4( &remote, 192,168,4,1);
    do
    	{
    	waitTime=rand() % 600;
    	} while (waitTime<200);

    if(esp_wifi_connect()==ESP_OK)
    {

    	EventBits_t uxBits=xEventGroupWaitBits(wifi_event_group, WIFI_BIT, false, true, 10000/  portTICK_RATE_MS);

    		if ((uxBits & WIFI_BIT)!=WIFI_BIT)
    		{
    		//	printf("Failed wait\n");// wait for connection
    			return;
      		}

		err_t err = netconn_connect(conn, &remote, BDGHOSTPORT);
		if (err != ERR_OK) {
		  printf("Connect failed %d\n",err);
		  return;
		}

		tcp_nagle_disable(conn->pcb.tcp); /* HACK */
		netconn_write(conn, message, strlen(message), NETCONN_NOCOPY);
	    delay(waitTime);
		netconn_close(conn);
		netconn_delete(conn);
		esp_wifi_disconnect();
      }
    else
    	printf("Could not esp_connect\n");

}
/*
static void host_publish(char * message)
{
    char addr_str[128];
    int addr_family;
    int ip_protocol,err,retries;

    if(esp_wifi_connect()==ESP_OK)
    {
        retries=0;
    	xEventGroupWaitBits(wifi_event_group, WIFI_BIT, false, true, portMAX_DELAY/  portTICK_RATE_MS);// wait for connection

        struct sockaddr_in dest_addr;
        dest_addr.sin_addr.s_addr = inet_addr(BDGHOSTADDR);
        dest_addr.sin_family = AF_INET;
        dest_addr.sin_port = htons(BDGHOSTPORT);
        addr_family = AF_INET;
        ip_protocol = IPPROTO_IP;
        inet_ntoa_r(dest_addr.sin_addr, addr_str, sizeof(addr_str) - 1);

        int sock =  socket(addr_family, SOCK_STREAM, ip_protocol);
		if (sock < 0) {
			 ESP_LOGE(TAG, "Unable to create socket: errno %d", errno);
		//	 esp_wifi_stop();
			 esp_wifi_disconnect();
			 return;
		 }
	//	ESP_LOGI(TAG, "Socket created, connecting to host");

		while(1)
		{
			err = connect(sock, (struct sockaddr *)&dest_addr, sizeof(dest_addr));
			if (err != 0) {
				if( ++retries<5){
					shutdown(sock, 2);
					delay(1000);
					printf("retry connect sock\n");
				}
				else
				{
				ESP_LOGE(TAG, "Socket unable to connect: errno %d", errno);
				shutdown(sock, 2);
				close(sock);
				esp_wifi_disconnect();
				return;
				}
		 }
			else
				break;
		}
	//	ESP_LOGI(TAG, "Successfully connected");

        err = send(sock, message, strlen(message), 0);
          if (err < 0) {
              ESP_LOGE(TAG, "Error occurred during sending: errno %d", errno);
              shutdown(sock, 2);
              close(sock);
 			 esp_wifi_disconnect();

              return;
          }
  //		ESP_LOGI(TAG, "Successfully sent");
       //   if(err>0)
        //	  printf("Send %d\n")
  		delay(sendTcp);
        shutdown(sock, 2);
        close(sock);
		 esp_wifi_disconnect();

    }
    else
        ESP_LOGE(TAG, "Error occurred starting interface");
}
*/
void sendStatusMeter(meterType* meter)
{

	cJSON *root=cJSON_CreateObject();

	if(root==NULL)
	{
		printf("cannot create root\n");
		return;
	}

	cJSON_AddStringToObject(root,"MeterPin33",		meter->serialNumber);
	cJSON_AddNumberToObject(root,"Transactions",	++meter->vanMqtt);
	cJSON_AddNumberToObject(root,"KwH",				meter->curLife);
	cJSON_AddNumberToObject(root,"Beats",			meter->currentBeat);
	cJSON_AddNumberToObject(root,"Pulse",			meter->pulse);

	char *rendered=cJSON_Print(root);
	if (deb)
	{
		printf("[MQTTD]Json %s\n",rendered);
	}
	printf("Sending message %d... ",++sentTotal);
	fflush(stdout);

	//if(xSemaphoreTake(wifiSem, portMAX_DELAY/  portTICK_RATE_MS))
	//{
	//	host_publish(rendered);
		sendMsg(rendered);
		printf("sent\n");
	//	xSemaphoreGive(wifiSem);
	//}

	free(rendered);
	cJSON_Delete(root);

	}

#ifdef SENDER
void sendStatusNow(meterType* meter)
{

	cJSON *root=cJSON_CreateObject();
	cJSON *cmdJ=cJSON_CreateObject();
	cJSON *cmdJ2=cJSON_CreateObject();
	cJSON *ar = cJSON_CreateArray();

	if(root==NULL)
	{
		printf("cannot create root\n");
		return;
	}
	cJSON_AddStringToObject(cmdJ,"password",	"zipo");
	cJSON_AddStringToObject(cmdJ,"uid",			"2C7BB292");
	cJSON_AddStringToObject(cmdJ,"cmd",			"/ga_session");
	cJSON_AddStringToObject(cmdJ,"time",		"07:14:08");
	cJSON_AddStringToObject(cmdJ,"date",		"2019-11-02");

	cJSON_AddStringToObject(cmdJ2,"password",	"zipo");
	cJSON_AddStringToObject(cmdJ2,"uid",		"2C7BB292");
	cJSON_AddStringToObject(cmdJ2,"cmd",		"/ga_firmware");
	cJSON_AddStringToObject(cmdJ2,"time",		"07:14:08");
	cJSON_AddStringToObject(cmdJ2,"date",		"2019-11-02");

	cJSON_AddItemToArray(ar, cmdJ);
	cJSON_AddItemToArray(ar, cmdJ2);
	cJSON_AddItemToObject(root, "Batch",ar);
	cJSON_AddNumberToObject(root,"Tran",++sentTotal);

	char *rendered=cJSON_Print(root);
	if (deb)
	{
		printf("[MQTTD]Json %s\n",rendered);
	}
	printf("Sending message Sent %d\n",sentTotal);

	if(xSemaphoreTake(wifiSem, portMAX_DELAY/  portTICK_RATE_MS))
	{
		host_publish(rendered);
		xSemaphoreGive(wifiSem);
	}
	free(rendered);
	cJSON_Delete(root);

	}

 void submode(void * pArg)
{
	 meterType meter;

	 while(1)
	{

		if( xQueueReceive( mqttQ, &meter, portMAX_DELAY/  portTICK_RATE_MS ))
		{
			if (deb)
			{
				printf("Heap after submode rx %d\n",esp_get_free_heap_size());
				printf("SubClientMode Queue %d\n",uxQueueMessagesWaiting( mqttQ ));
			}
			sendStatusNow(&meter);
			if(deb)
				printf("Heap after submode %d\n",esp_get_free_heap_size());

		}

		else
			vTaskDelay(100 /  portTICK_RATE_MS);
	}
}
#endif


static void mqttManager(void* arg)
{
	meterType meter;

	while(1)
	{
		if( xQueueReceive( mqttR, &meter, portMAX_DELAY/  portTICK_RATE_MS ))
			{
				printf("Meter %d Beat %d Pos %d Queue %d\n",meter.pin,meter.currentBeat,meter.pos,uxQueueMessagesWaiting(mqttR));
				sendStatusMeter(&theMeters);
			}
		else
			delay(100);
	}

}

#ifdef SENDER
void sender(void *pArg)
{
	meterType algo;

	while(true)
	{
		if(deb)
			printf("Heap before send %d\n",esp_get_free_heap_size());
		if(!xQueueSend( mqttQ,&algo,0))
			printf("Error sending queue %d\n",uxQueueMessagesWaiting( mqttQ ));
		else
			if(deb)
			{
				//printf("Sending %d\n",algo++);
				printf("Heap after send %d\n",esp_get_free_heap_size());
			}


		if(uxQueueMessagesWaiting(mqttQ)>uxQueueSpacesAvailable(mqttQ))
		{
			while(uxQueueMessagesWaiting(mqttQ)>2)
				delay(1000);
		}
		delay(qdelay);
	}
}
#endif

#ifdef TEST
	int get_string(uart_port_t uart_num,uint8_t cual,char *donde)
	{
		uint8_t ch;
		int son=0,len;
		son=0;
		while(1)
		{
			len = uart_read_bytes(UART_NUM_0, (uint8_t*)&ch, 1,4/ portTICK_RATE_MS);
			if(len>0)
			{
				if(ch==cual)
				{
					*donde=0;
					return son;
				}

				else
				{
					*donde=(char)ch;
					donde++;
					son++;
				}

			}

			vTaskDelay(30/portTICK_PERIOD_MS);
		}
	}


	void kbd(void *arg) {
		int len,reg,ic;
		uart_port_t uart_num = UART_NUM_0 ;
		char s1[20];
		char data[20];
		u8 readReg,val;
		u8 *p;


		uart_config_t uart_config = {
				.baud_rate = 115200,
				.data_bits = UART_DATA_8_BITS,
				.parity = UART_PARITY_DISABLE,
				.stop_bits = UART_STOP_BITS_1,
				.flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
				.rx_flow_ctrl_thresh = 122,
		};
		uart_param_config(uart_num, &uart_config);
		uart_set_pin(uart_num, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);
		esp_err_t err= uart_driver_install(uart_num, 1024 , 1024, 10, NULL, 0);
		if(err!=ESP_OK)
			printf("Error UART Install %d\n",err);


		while(1)
		{
			len = uart_read_bytes((uart_port_t)uart_num, (uint8_t*)data, sizeof(data),20);
			if(len>0)
			{
				switch(data[0])
				{
				case 'x':
				case 'X':
					printf("To dump core\n");
					p=0;
					delay(2000);
					*p=0;
					break;
				case 'f':
				case 'F':
					printf("Format FRAM initValue:");
					fflush(stdout);
					len=get_string((uart_port_t)uart_num,10,s1);
					if(len<=0)
					{
						printf("\n");
						break;
					}
					memset(&tempb,atoi(s1),sizeof(tempb));
			//		FramSPI_format(atoi(s1),tempb,sizeof(tempb),true);
					printf("Format started\n");
					break;
				case 'q':
				case 'Q':
						printf("Queue Waiting %d available %d\n",uxQueueMessagesWaiting( mqttQ ),uxQueueSpacesAvailable(mqttQ));
						break;
				case 'd':
				case 'D':
						deb=!deb;
						printf("Debug %s\n",deb?"On":"Off");
						break;
				case 't':
						printf("Wait Delay(ms) is %d:",sendTcp);
						fflush(stdout);
						len=get_string((uart_port_t)uart_num,10,s1);
						if(len<=0)
						{
							printf("\n");
							break;
						}
						sendTcp=atoi(s1);
						printf("%d\n",sendTcp);
						break;
				case 'T':
						printf("Msg Delay(ms) is %d:", qdelay);
						fflush(stdout);
						len=get_string((uart_port_t)uart_num,10,s1);
						if(len<=0)
						{
							printf("\n");
							break;
						}
						qdelay=atoi(s1);
						printf("%d\n",qdelay);
						break;
				case 'l':
				case 'L':
						printf("LOG level:(N)one (I)nfo (E)rror (V)erbose (W)arning:");
						fflush(stdout);
						len=get_string((uart_port_t)uart_num,10,s1);
						if(len<=0)
						{
							printf("\n");
							break;
						}
							switch (s1[0])
							{
								case 'n':
								case 'N':
										esp_log_level_set("*", ESP_LOG_NONE);
										break;
								case 'i':
								case 'I':
										esp_log_level_set("*", ESP_LOG_INFO);
										break;
								case 'e':
								case 'E':
										esp_log_level_set("*", ESP_LOG_ERROR);
										break;
								case 'v':
								case 'V':
										esp_log_level_set("*", ESP_LOG_VERBOSE);
										break;
								case 'w':
								case 'W':
										esp_log_level_set("*", ESP_LOG_WARN);
										break;
							}
						break;

				default:
					break;
				}

			}
			vTaskDelay(100 / portTICK_PERIOD_MS);
		}
	}
#endif

	void erase_config() //do the dirty work
	{
		memset(&theConf,0,sizeof(theConf));
		theConf.centinel=CENTINEL;
		theConf.ssl=0;
		memcpy(theConf.mqtt," ",1);//fixed mosquito server
		theConf.mqttport=30000;
		printf("Mqtt Erase %s\n",theConf.mqtt);
		memcpy(theConf.domain,"feediot.co.nf",13);// mosquito server feediot.co.nf
		theConf.domain[13]=0;
		theConf.beatsPerKw[0]=800;//old way
		theConf.bounce[0]=100;
		//    fram.write_tarif_bpw(0, 800); // since everything is going to be 0, BPW[0]=800 HUMMMMMM????? SHould load Tariffs after this
		write_to_flash();
		//	if(  xSemaphoreTake(logSem, portMAX_DELAY/  portTICK_RATE_MS))
		//	{
		//		fclose(bitacora);
		//	    bitacora = fopen("/spiflash/log.txt", "w"); //truncate
		//	    if(bitacora)
		//	    {
		//	    	fclose(bitacora); //Close and reopen r+
		//		    bitacora = fopen("/spiflash/log.txt", "r+");
		//		    if(!bitacora)
		//		    	printf("Could not reopen logfile\n");
		//		    else
		//			    postLog(0,"Log cleared");
		//	    }
		//	    xSemaphoreGive(logSem);
		//	}
		printf("Centinel %x\n",theConf.centinel);
	}

void app_main()
{
	ESP_LOGI(TAG, "[APP] Startup..");
    ESP_LOGI(TAG, "[APP] Free memory: %d bytes", esp_get_free_heap_size());
    ESP_LOGI(TAG, "[APP] IDF version: %s", esp_get_idf_version());

    esp_log_level_set("*", ESP_LOG_WARN);

    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES) {
        // NVS partition was truncated and needs to be erased
        // Retry nvs_flash_init
    	printf("No free pages erased!!!!\n");
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }
	read_flash();
    if (theConf.centinel!=CENTINEL || !gpio_get_level((gpio_num_t)0))
    	{
    		printf("Read centinel %x",theConf.centinel);
    		erase_config();
    	}


   deb=false;
   qwait=QDELAY;
   qdelay=qwait*1000;
   sendTcp=500;

   wifiSem= xSemaphoreCreateBinary();
   xSemaphoreGive(wifiSem);

   mqttQ = xQueueCreate( 20, sizeof( meterType ) );
   if(!mqttQ)
	   printf("Failed queue Tx\n");
   printf("Submode Queue %x\n",(uint32_t)mqttQ);

	mqttR = xQueueCreate( 20, sizeof( meterType ) );
	if(!mqttR)
		printf("Failed queue Rx\n");
	printf("Meter Queue %x\n",(uint32_t)mqttR);

   wifi_init();
   init_fram();
   install_meter_interrupts();
   xTaskCreate(&mqttManager,"meters",4096,NULL, 5, NULL);
#ifdef SENDER
   xTaskCreate(&submode,"U571",10240,NULL, 5, NULL);
   delay(1000);
   xTaskCreate(&sender,"sender",4096,NULL, 5, NULL);
#endif
#ifdef TEST
	xTaskCreate(&kbd,"kbd",4096,NULL, 4, NULL);
#endif

}
