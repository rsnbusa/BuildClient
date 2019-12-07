#include "includes.h"

config_flash					theConf;
QueueHandle_t 					mqttQ,mqttR;
SemaphoreHandle_t 				wifiSem,framSem;
char							deb;
u32								sentTotal=0,sendTcp=0;
u8								qwait=0;
u16 							qdelay,addressBytes;
u16								mesg,diag,horag,yearg;
meterType						theMeters[MAXDEVS];
gpio_config_t 					io_conf;
cmdType							theCmd;
u16                  			diaHoraTarifa,yearDay;      // % of Meter tariff. Ex: 800 *120=(20% cheaper).
nvs_handle 						nvshandle;
FramSPI							fram;
uint8_t							tempb[MAXBUFF];
uint8_t 						daysInMonth [12] ={ 31,28,31,30,31,30,31,31,30,31,30,31 };
char							theMac[20],them[6];

static const char *TAG = "BDGCLIENT";

//* change

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

	if(meter->beatsPerkW==0)
		meter->beatsPerkW=800;

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
				if((meter->currentBeat % (meter->beatsPerkW/10))==0) //every GMAXLOSSPER interval
				{
					meter->saveit=false;

					if(meter->beatSaveRaw >= meter->beatsPerkW*diaHoraTarifa/100)
					{
						meter->beatSaveRaw=0;
						//meter->curLife++;
						meter->beatSave=0;
						meter->saveit=true;
					}

					if(mqttR)
					{
						xQueueSendFromISR( mqttR,&theMeters[meter->pos],&tasker );
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
	u8 		mac[6];

	theMeters[0].pin=METER1;
	theMeters[1].pin=METER2;
	theMeters[2].pin=METER3;
	theMeters[3].pin=METER4;

	esp_wifi_get_mac(ESP_IF_WIFI_STA, (u8*)&mac);
	sprintf(temp,"MeterIoT%02x%02x",mac[4],mac[5]);
	printf("Mac %s\n",temp);
	gpio_install_isr_service(ESP_INTR_FLAG_IRAM);

	io_conf.intr_type = GPIO_INTR_ANYEDGE;
	io_conf.mode = GPIO_MODE_INPUT;
	io_conf.pull_down_en =GPIO_PULLDOWN_DISABLE;
	io_conf.pull_up_en =GPIO_PULLUP_ENABLE;

	for (int a=0;a<MAXDEVS;a++)
	{
		sprintf(theMeters[a].serialNumber,"MeterIoT%02x%02x/%d",mac[4],mac[5],a);
		theMeters[a].pos=a;
		io_conf.pin_bit_mask = (1ULL<<theMeters[a].pin);
		gpio_config(&io_conf);
		gpio_isr_handler_add((gpio_num_t)theMeters[a].pin, gpio_isr_handler, (void*)&theMeters[a]);
	}

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


static void write_to_fram(u8 meter,bool addit)
{
	time_t timeH;
    struct tm timeinfo;

	// FRAM Semaphore is taken by the Interrupt Manager. Safe to work.
	scratchTypespi scratch;
    time(&timeH);
	localtime_r(&timeH, &timeinfo);
	mesg=timeinfo.tm_mon;
	diag=timeinfo.tm_mday-1;
	yearg=timeinfo.tm_year+1900;
	horag=timeinfo.tm_hour-1;
//	if(aqui.traceflag & (1<<BEATD)) //Should not print. semaphore is taking longer
	//		printf("[BEATD]Save KWH Meter %d Month %d Day %d Hour %d Year %d lifekWh %d beats %d addkw %d\n",meter,mesg,diag,horag,yearg,
	//					theMeters.curLife,theMeters.currentBeat,addit);
			if(addit)
			{
				theMeters[meter].curLife++;
				theMeters[meter].curMonth++;
				theMeters[meter].curDay++;
				theMeters[meter].curHour++;
				theMeters[meter].curCycle++;
				time(&theMeters[meter].lastKwHDate); //last time we saved data


	scratch.medidor.state=1;                    //scratch written state. Must be 0 to be ok. Every 800-1000 beats so its worth it
	scratch.medidor.meter=meter;
	scratch.medidor.month=theMeters[meter].curMonth;
	scratch.medidor.life=theMeters[meter].curLife;
	scratch.medidor.day=theMeters[meter].curDay;
	scratch.medidor.hora=theMeters[meter].curHour;
	scratch.medidor.cycle=theMeters[meter].curCycle;
	scratch.medidor.mesg=mesg;
	scratch.medidor.diag=diag;
	scratch.medidor.horag=horag;
	scratch.medidor.yearg=yearg;
	fram.write_recover(scratch);            //Power Failure recover register

	fram.write_beat(meter,theMeters[meter].currentBeat);
	fram.write_lifekwh(meter,theMeters[meter].curLife);
	fram.write_month(meter,mesg,theMeters[meter].curMonth);
	fram.write_monthraw(meter,mesg,theMeters[meter].curMonthRaw);
	fram.write_day(meter,yearg,mesg,diag,theMeters[meter].curDay);
	fram.write_dayraw(meter,yearg,mesg,diag,theMeters[meter].curDayRaw);
	fram.write_hour(meter,yearg,mesg,diag,horag,theMeters[meter].curHour);
	fram.write_hourraw(meter,yearg,mesg,diag,horag,theMeters[meter].curHourRaw);
	fram.write_cycle(meter, mesg,theMeters[meter].curCycle);
	fram.write_minamps(meter,theMeters[meter].minamps);
	fram.write_maxamps(meter,theMeters[meter].maxamps);
	fram.write_lifedate(meter,theMeters[meter].lastKwHDate);  //should be down after scratch record???
	fram.write8(SCRATCHOFF,0); //Fast write first byte of Scratch record to 0=done.
			}
			else
			fram.write_beat(meter,theMeters[meter].currentBeat);
//	scratch.medidor.state=0;            // done state. OK
//	FramSPI_write_recover(scratch);
}

static void load_from_fram(u8 meter)
{
	if(xSemaphoreTake(framSem, portMAX_DELAY/  portTICK_RATE_MS))
	{
		fram.read_lifekwh(meter,(u8*)&theMeters[meter].curLife);
		fram.read_lifedate(meter,(u8*)&theMeters[meter].lastKwHDate);
		fram.read_month(meter, mesg, (u8*)&theMeters[meter].curMonth);
		fram.read_monthraw(meter, mesg, (u8*)&theMeters[meter].curMonthRaw);
		fram.read_day(meter, yearg,mesg, diag, (u8*)&theMeters[meter].curDay);
		fram.read_dayraw(meter, yearg,mesg, diag, (u8*)&theMeters[meter].curDayRaw);
		fram.read_hour(meter, yearg,mesg, diag, horag, (u8*)&theMeters[meter].curHour);
		fram.read_hourraw(meter, yearg,mesg, diag, horag, (u8*)&theMeters[meter].curHourRaw);
		fram.read_cycle(meter, mesg, (u8*)&theMeters[meter].curCycle); //should we change this here too and use cycleMonth[meter]?????
		fram.read_beat(meter,(u8*)&theMeters[meter].currentBeat);
		theMeters[meter].oldbeat=theMeters[meter].currentBeat;
		if(theConf.beatsPerKw[meter]==0)
			theConf.beatsPerKw[meter]=800;// just in case div by 0 crash
		u16 nada=theMeters[meter].currentBeat/theConf.beatsPerKw[meter];
		theMeters[meter].beatSave=theMeters[meter].currentBeat-(nada*theConf.beatsPerKw[meter]);
		theMeters[meter].beatSaveRaw=theMeters[meter].beatSave;
		fram.read_minamps(meter,(u8*)&theMeters[meter].minamps);
		fram.read_maxamps(meter,(u8*)&theMeters[meter].maxamps);
		xSemaphoreGive(framSem);

			printf("[BEATD]Loaded Meter %d curLife %d beat %d\n",meter,theMeters[meter].curLife,theMeters[meter].currentBeat);
	}
}

//static void loadDayBPK(u16 hoy)
//{
//	if(xSemaphoreTake(framSem, portMAX_DELAY/  portTICK_RATE_MS)){
//		fram.read_tarif_day(hoy, (u8*)&diaTarifa); // all 24 hours of todays Tariff Types [0..255] of TarifaBPW
//		xSemaphoreGive(framSem);
//	}
//	else
//		return;
////	if(aqui.traceflag & (1<<BOOTD))
////	{
////		for (int a=0;a<24;a++)
////			printf("[BOOTD]H[%d]=%d ",a,diaTarifa[a]);
////		printf("\n");
////
////	}
//}

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
		for (int a=0;a<MAXDEVS;a++)
			load_from_fram(a);

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

void updateDateTime(loginT loginData)
{
    struct tm timeinfo;

	localtime_r(&loginData.thedate, &timeinfo);
	diaHoraTarifa=loginData.theTariff;// Host will give us Hourly Tariff. No need to store

	mesg=timeinfo.tm_mon;
	diag=timeinfo.tm_mday;
	yearg=timeinfo.tm_year+1900;
	horag=timeinfo.tm_hour;
	struct timeval now = { .tv_sec = loginData.thedate, .tv_usec=0};
	settimeofday(&now, NULL);
}

cJSON* makecJSONMeter(meterType *meter)
{
	cJSON *root=cJSON_CreateObject();

	if(root==NULL)
	{
		printf("cannot create root\n");
		return NULL;
	}

	cJSON *cmdJ=cJSON_CreateObject();
	cJSON *ar = cJSON_CreateArray();

	if(cmdJ==NULL || ar==NULL)
	{
		printf("cannot create aux json\n");
		return;
	}

	cJSON_AddStringToObject(cmdJ,"MAC",				theMac);
	cJSON_AddStringToObject(cmdJ,"cmd",				"/ga_status");
	cJSON_AddStringToObject(cmdJ,"MeterId",			meter->serialNumber);
	cJSON_AddNumberToObject(cmdJ,"Transactions",	++meter->vanMqtt);
	cJSON_AddNumberToObject(cmdJ,"KwH",				meter->curLife);
	cJSON_AddNumberToObject(cmdJ,"Beats",			meter->currentBeat);
	cJSON_AddNumberToObject(cmdJ,"Pulse",			meter->pulse);
	cJSON_AddItemToArray(ar, cmdJ);

	cJSON_AddItemToObject(root, "Batch",ar);
	return root;
}

u16_t sendMsg(char * message, uint8_t *donde)
{
    struct netconn *conn = netconn_new(NETCONN_TCP);
    ip_addr_t remote;
    int waitTime;
	struct netbuf *inbuf;
	uint8_t *buf;
	u16_t buflen;
    loginT loginData;
    char strftime_buf[64];
    struct tm timeinfo;
	u8 aca[4];
	tcpip_adapter_ip_info_t ip_info;
	meterType meter;

        do
        {
        	waitTime=rand() % 600;
        	} while (waitTime<200);

        delay(waitTime);

        if(esp_wifi_connect()==ESP_OK)
        {

        	EventBits_t uxBits=xEventGroupWaitBits(wifi_event_group, WIFI_BIT, false, true, 10000/  portTICK_RATE_MS);

        		if ((uxBits & WIFI_BIT)!=WIFI_BIT)
        		{
        		//	printf("Failed wait\n");// wait for connection
        			return;
          		}

    		ESP_ERROR_CHECK(tcpip_adapter_get_ip_info(TCPIP_ADAPTER_IF_STA, &ip_info));
    //    		printf("IP Address:  %s\n", ip4addr_ntoa(&ip_info.ip));
    //    		printf("Subnet mask: %s\n", ip4addr_ntoa(&ip_info.netmask));
    //    		printf("Gateway:     %s\n", ip4addr_ntoa(&ip_info.gw));
    	memcpy(&aca,&ip_info.gw,4);
    	IP_ADDR4( &remote, aca[0],aca[1],aca[2],aca[3]);

		err_t err = netconn_connect(conn, &remote, BDGHOSTPORT);
		if (err != ERR_OK) {
		  printf("Connect failed %d\n",err);
		  return;
		}

		tcp_nagle_disable(conn->pcb.tcp);
		printf("Sending mainq %s %d\n",message, strlen(message));
		netconn_write(conn, message, strlen(message), NETCONN_NOCOPY);

		buflen=0;
		netconn_set_recvtimeout(conn, 200);
		err = netconn_recv(conn, &inbuf);
		if (err == ERR_OK) {
				printf("Client In \n");
				netbuf_data(inbuf, (void**)&buf, &buflen);
				memcpy(donde,buf,buflen);
				netbuf_delete(inbuf);
		}
		else
			printf("Error netconn recv %d\n",err);

		while(uxQueueMessagesWaiting(mqttR)>0)
		{
			printf("Execute waiting in Queue %d\n",uxQueueMessagesWaiting(mqttR));
			if( xQueueReceive( mqttR, &meter, 500/  portTICK_RATE_MS ))
			{
				//delay(200);
				cJSON *nroot=makecJSONMeter(&meter);
				char *lmessage=cJSON_Print(nroot);
				printf("Sending queue %s\n",lmessage);
				err=netconn_write(conn, lmessage, strlen(lmessage), NETCONN_NOCOPY);
				if(err!=ERR_OK)
					printf("Queue send err %d\n",err);
		//		delay(200);
				free(lmessage);
				free(nroot);

			}
			else
			{
				printf("Nothing in queue\n");
				break;
			}
		}
		printf("Closing client\n");
		netconn_close(conn);
		netconn_delete(conn);
		esp_wifi_disconnect();
      }
    else
    	printf("Could not esp_connect\n");
        return buflen;

}


void logIn()
{
    loginT loginData;

	setenv("TZ", LOCALTIME, 1);
	tzset();

	cJSON *root=cJSON_CreateObject();
	cJSON *cmdJ=cJSON_CreateObject();
	cJSON *ar = cJSON_CreateArray();

	if(root==NULL)
	{
		printf("cannot create root\n");
		return;
	}


	cJSON_AddStringToObject(cmdJ,"MAC",				theMac);
	cJSON_AddStringToObject(cmdJ,"password",		"zipo");
	cJSON_AddStringToObject(cmdJ,"cmd",				"/ga_login");

	cJSON_AddItemToArray(ar, cmdJ);
	cJSON_AddItemToObject(root, "Batch",ar);

	char *logMsg=cJSON_Print(root);

	sendMsg(logMsg,(uint8_t*)&loginData);

	updateDateTime(loginData);

	if(deb)
		printf("Login year %d month %d day %d hour %d Tariff %d\n",yearg,mesg,diag,horag,loginData.theTariff);

	free(logMsg);
	free(root);
}


void sendStatusMeter(meterType* meter)
{
    loginT 	loginData;

    cJSON *root=makecJSONMeter(meter);
    char *rendered=cJSON_Print(root);

	if (deb)
		printf("[MQTTD]Json %s\n",rendered);

	printf("Sending message %d wait %d... ",++sentTotal,uxQueueMessagesWaiting(mqttR));
	fflush(stdout);

	sendMsg(rendered,(uint8_t*)&loginData);
	updateDateTime(loginData);

	if(deb)
		printf("SendMeterFram dates year %d month %d day %d hour %d Tariff %d\n",yearg,mesg,diag,horag,loginData.theTariff);

	free(rendered);
	cJSON_Delete(root);
	printf("Tar %d sent Heap %d\n",loginData.theTariff,esp_get_free_heap_size());

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
			if(xSemaphoreTake(framSem, portMAX_DELAY/  portTICK_RATE_MS))
			{
				write_to_fram(meter.pos,meter.saveit);
				xSemaphoreGive(framSem);
			}
			sendStatusMeter(&meter);
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
		int len,ret,cualf,total;
		uart_port_t uart_num = UART_NUM_0 ;
		char s1[20];
		char data[20];
		u32 framAddress;
		u8 fueron,valor;
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
					printf("Dumping core...\n");
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
					fram.format(atoi(s1),tempb,sizeof(tempb),true);
					printf("Format done\n");
					break;
				case 'r':
				case 'R':
					printf("Read FRAM address:");
					fflush(stdout);
					len=get_string((uart_port_t)uart_num,10,s1);
					if(len<=0)
					{
						printf("\n");
						break;
					}
					framAddress=atoi(s1);
					printf("Count:");
					fflush(stdout);
					len=get_string((uart_port_t)uart_num,10,s1);
					if(len<=0)
					{
						printf("\n");
						break;
					}
					fueron=atoi(s1);
					fram.readMany(framAddress,tempb,fueron);
					for (int a=0;a<fueron;a++)
						printf("%02x-",tempb[a]);
					printf("\n");
					break;
				case 'w':
				case 'W':
					printf("Write FRAM address:");
					fflush(stdout);
					len=get_string((uart_port_t)uart_num,10,s1);
					if(len<=0)
					{
						printf("\n");
						break;
					}
					framAddress=atoi(s1);
					printf("Value:");
					fflush(stdout);
					len=get_string((uart_port_t)uart_num,10,s1);
					if(len<=0)
					{
						printf("\n");
						break;
					}
					fueron=atoi(s1);
					fram.write8(framAddress,fueron);
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

				case '5'://All days in Month
					printf("Days in Month search\nMonth(0-11):");
					fflush(stdout);
					get_string((uart_port_t)uart_num,10,s1);
					len=atoi(s1);
					if(len<0 || len>11){
						printf("Invalid month\n");
						break;
					}
					if(xSemaphoreTake(framSem, portMAX_DELAY))		{
						for (int a=0;a<daysInMonth[len];a++)
						{
							fram.read_day(0,yearg,len, a, (u8*)&valor);
							if(valor>0)
								printf("M[%d]D[%d]=%d ",len,a,valor);
						}
						xSemaphoreGive(framSem);

					}
						printf("\n");
							break;
				case '4': //All Hours in Day
					printf("Hours in Day search\nMonth(0-11):");
					fflush(stdout);
					get_string((uart_port_t)uart_num,10,s1);
					len=atoi(s1);
					if(len<0 || len>11){
						printf("Invalid month\n");
						break;
					}
					printf("Day(0-%d):",daysInMonth[len]);
					fflush(stdout);
					get_string((uart_port_t)uart_num,10,s1);
					ret=atoi(s1);
					if(ret<0 || ret>daysInMonth[len]){
						printf("Invalid Day range\n");
						break;
					}
					if(xSemaphoreTake(framSem, portMAX_DELAY))		{
						for (int a=0;a<24;a++)
						{
							fram.read_hour(0, yearg,len, ret, a, (u8*)&valor);
							if(valor>0)
								printf("M[%d]D[%d]H[%d]=%d ",len,ret,a,valor);
						}
						xSemaphoreGive(framSem);

					}
						printf("\n");
							break;
				case '3': //Hour search
					printf("Month-Day-Hour search\nMonth(0-11):");
					fflush(stdout);
					get_string((uart_port_t)uart_num,10,s1);
					len=atoi(s1);
					if(len<0 || len>11){
						printf("Invalid month\n");
						break;
					}
					printf("Day(0-%d):",daysInMonth[len]);
					fflush(stdout);
					get_string((uart_port_t)uart_num,10,s1);
					ret=atoi(s1);
					if(ret<0 || ret>daysInMonth[len]){
						printf("Invalid Day range\n");
						break;
					}
					printf("Hour(0-23):");
					fflush(stdout);
					get_string((uart_port_t)uart_num,10,s1);
					cualf=atoi(s1);
					if(cualf<0 || cualf>23){
						printf("Invalid Hour range\n");
						break;
					}
					if(xSemaphoreTake(framSem, portMAX_DELAY))
					{
						fram.read_day(0, yearg,len, ret, (u8*)&valor);
						xSemaphoreGive(framSem);
						if(valor>0)
							printf("Date %d/%d/%d=%d\n",yearg,len,ret,valor);
					}
							break;
				case '2':
					printf("Month-Day search\nMonth:");
					fflush(stdout);
					get_string((uart_port_t)uart_num,10,s1);
					len=atoi(s1);
					if(len<0 || len>11){
						printf("Invalid month\n");
						break;
					}
					printf("Day:");
					fflush(stdout);
					get_string((uart_port_t)uart_num,10,s1);
					ret=atoi(s1);
					if(ret<0 || ret>daysInMonth[len]){
						printf("Invalid day range\n");
						break;
					}

					if(xSemaphoreTake(framSem, portMAX_DELAY))
					{
						fram.read_day(0, yearg,len, ret, (u8*)&valor);
						xSemaphoreGive(framSem);
						printf("Date %d/%d/%d=%d\n",yearg,len,ret,valor);
					}
							break;
				case '1':
					total=0;
					printf("Months Readings\n");
					if(xSemaphoreTake(framSem, portMAX_DELAY))		{
						for (int a=0;a<12;a++)
						{
							fram.read_month(0, a, (u8*)&valor);
							if(valor>0)
								printf("M[%d]=%d ",a,valor);
							total+=valor;
						}
						xSemaphoreGive(framSem);
						printf("\nTotal %d\n",total);
					}
					break;
				case '0':
					printf("Flushing Fram...");
					fflush(stdout);
					for(int a=0;a<MAXDEVS;a++)
						write_to_fram(a,false);
					printf("%d meters flushed\n",MAXDEVS);
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

	mqttR = xQueueCreate( 20, sizeof( meterType ) );
	if(!mqttR)
		printf("Failed queue Rx\n");

	memset(&theMeters,0,sizeof(theMeters));

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

//get time from host and set local time for any time related work
	char them[6];

	esp_efuse_mac_get_default(them);
	sprintf(theMac,"%02x:%02x:%02x:%02x:%02x:%02x",them[0],them[1],them[2],them[3],them[4],them[5]);
	logIn();
}
