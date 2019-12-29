#include "includes.h"
#include "defines.h"

bool							conn=false,framFlag,df=false;
char							lookuptable[NKEYS][10],deb,tempb[1200],theMac[20],them[6];
config_flash					theConf;
const static int 				WIFI_BIT = BIT0;
FramSPI							fram;
gpio_config_t 					io_conf;
int								gsock;
meterType						theMeters[MAXDEVS];
nvs_handle 						nvshandle;
QueueHandle_t 					mqttQ,mqttR,framQ,pcnt_evt_queue;
SemaphoreHandle_t 				wifiSem,framSem;
static const char 				*TAG = "BDGCLIENT";
static EventGroupHandle_t 		wifi_event_group;
u16                  			diaHoraTarifa,yearDay,llevoMsg=0,waitQueue=500,mesg,oldMesg,diag,oldDiag,horag,oldHorag,yearg,wDelay,qdelay,addressBytes;
u32								sentTotal=0,sendTcp=0,totalMsg[MAXDEVS],theMacNum,totalPulses;
u8								qwait=0,theBreakers[MAXDEVS],daysInMonth [12] ={ 31,28,31,30,31,30,31,31,30,31,30,31 };
host_t							setupHost[MAXDEVS];
TaskHandle_t					webHandle;

using namespace std;

#ifdef SENDER
void submode(void * pArg);
#endif

extern void kbd(void *arg);

void sendStatusMeter(meterType* meter);
void sendStatusMeterAll();
static esp_err_t challenge_get_handler(httpd_req_t *req);
static esp_err_t setup_get_handler(httpd_req_t *req);
static esp_err_t setupend_get_handler(httpd_req_t *req);


uint32_t IRAM_ATTR millisISR()
{
	return xTaskGetTickCountFromISR() * portTICK_PERIOD_MS;
}

uint32_t IRAM_ATTR millis()
{
	return xTaskGetTickCount() * portTICK_PERIOD_MS;
}

void delay(uint32_t a)
{
	vTaskDelay(a /  portTICK_PERIOD_MS);
}


void hourChange()
{
	if(theConf.traceflag & (1<<TIMED))
		printf("%sHour change Old %d New %d\n",TIEMPOT,oldHorag,horag);


	for (int a=0;a<MAXDEVS;a++)
	{
		if(theConf.traceflag & (1<<TIMED))
			printf("%sHour change meter %d val %d\n",TIEMPOT,a,theMeters[a].curHour);
		if(xSemaphoreTake(framSem, portMAX_DELAY))
		{
			fram.write_hour(a, yearg,oldMesg,oldDiag,oldHorag, theMeters[a].curHour);//write old one before init new
			fram.write_hourraw(a, yearg,oldMesg,oldDiag,oldHorag, theMeters[a].curHourRaw);//write old one before init new
			xSemaphoreGive(framSem);
		}
		theMeters[a].curHour=0; //init it
		theMeters[a].curHourRaw=0;
	//	delay(500);
	}
	sendStatusMeterAll();
	oldHorag=horag;
}

void dayChange()
{
	if(theConf.traceflag & (1<<TIMED))
		printf("%sDay change Old %d New %d\n",TIEMPOT,oldDiag,diag);


	for (int a=0;a<MAXDEVS;a++)
			{
				if(theConf.traceflag & (1<<TIMED))
					printf("%sDay change mes %d day %d oldday %d corte %d sent %d\n",TIEMPOT,oldMesg,diag,oldDiag,theConf.diaDeCorte[a],theConf.corteSent[a]);
				if(xSemaphoreTake(framSem, portMAX_DELAY))
				{
					fram. write_day(a,yearg, oldMesg,oldDiag, theMeters[a].curDay);
					fram. write_dayraw(a,yearg, oldMesg,oldDiag, theMeters[a].curDayRaw);
					theMeters[a].curDay=0;
					theMeters[a].curDayRaw=0;
					xSemaphoreGive(framSem);
				}
			}
			oldDiag=diag;
}

void monthChange()
{
	if(theConf.traceflag & (1<<TIMED))
		printf("%sMonth change Old %d New %d\n",TIEMPOT,oldMesg,mesg);

	for (int a=0;a<MAXDEVS;a++)
			{
				if(xSemaphoreTake(framSem, portMAX_DELAY))
				{
					fram.write_month(a, oldMesg, theMeters[a].curMonth);
					fram.write_monthraw(a, oldMesg, theMeters[a].curMonthRaw);
					xSemaphoreGive(framSem);
					theMeters[a].curMonth=0;
					theMeters[a].curMonthRaw=0;
				}
			}
			oldMesg=mesg;
}

void check_date_change()
{
	time_t now;
	struct tm timep;
	time(&now);
	localtime_r(&now,&timep);
	mesg=timep.tm_mon;   // Global Month
	diag=timep.tm_mday-1;    // Global Day
	yearg=timep.tm_year+1900;     // Global Year
	horag=timep.tm_hour;     // Global Hour
	yearDay=timep.tm_yday;

	if(theConf.traceflag & (1<<TIMED))
		printf("%sHour change mes %d- %d day %d- %d hora %d- %d Min %d Sec %d dYear %d\n",TIEMPOT,mesg,oldMesg,diag,oldDiag,horag,oldHorag,
				timep.tm_min,timep.tm_sec,yearDay);

	if(horag==oldHorag && diag==oldDiag && mesg==oldMesg)
		return;

	if(horag!=oldHorag) // hour change up or down
		hourChange();

	if(diag!=oldDiag) // day change up or down. Also hour MUST HAVE CHANGED before
	{
		//	setCycleChange(diag,oldDiag);
		dayChange();
	}

	if(mesg!=oldMesg) // month change up or down. What to do with prev Year???? MONTH MUST HAVE CHANGED
		monthChange();
}

static void timeKeeper(void *pArg)
{
#define QUE 1000 //used to test timer
	time_t now;


	time(&now);
	int faltan=3600- (now % 3600)+2; //second to next hour +2 secs
	if(theConf.traceflag & (1<<TIMED))
		printf("%sSecs to Hour %d now %d\n",TIEMPOT,faltan,(u32)now);

	delay(faltan*QUE);
	while(true)
	{
		check_date_change();
		delay(3600000);//every hour
	}
}


static void pcntManager(void * pArg)
{
	framMeterType theMeter;
	pcnt_evt_t evt;
	portBASE_TYPE res;
	u16 residuo,count;
	uint32_t timeDiff=0;

	pcnt_evt_queue = xQueueCreate( 20, sizeof( pcnt_evt_t ) );
	if(!pcnt_evt_queue)
		printf("Failed create queue PCNT\n");

	while(1)
	{
		res = xQueueReceive(pcnt_evt_queue, (void*)&evt,portMAX_DELAY / portTICK_PERIOD_MS);
		if (res == pdTRUE)
		{
			pcnt_get_counter_value((pcnt_unit_t)evt.unit,(short int *) &count);
			if(theConf.traceflag & (1<<INTD))
				printf("%sEvent PCNT unit[%d]; cnt: %d status %x\n",INTDT, evt.unit, count,evt.status);

			if (evt.status & PCNT_EVT_THRES_1)
			{
				pcnt_counter_clear((pcnt_unit_t)evt.unit);

				if(theMeters[evt.unit].ampTime>0)
					timeDiff=millis()-theMeters[evt.unit].ampTime;
				theMeters[evt.unit].ampTime=millis();

				totalPulses+=count;						//counter of all pulses received

				theMeters[evt.unit].saveit=false;		//adding a new kwh flag
				theMeters[evt.unit].currentBeat+=count;	//beat
				theMeters[evt.unit].beatSave+=count;	//beats per kwh
				theMeters[evt.unit].beatSaveRaw+=count;	//raw without tariff discount

				if(diaHoraTarifa==0)
					diaHoraTarifa=100;					// should not happen but in case it does no crash

				residuo=theMeters[evt.unit].beatSave % (theMeters[evt.unit].beatsPerkW*diaHoraTarifa/100);	//0 is 1 kwh happend with tariffs

				if(theConf.traceflag & (1<<INTD))
					printf("%sResiduo %d Beat %d MeterPos %d Time %d BPK %d\n",INTDT,residuo,theMeters[evt.unit].currentBeat,
							theMeters[evt.unit].pos,timeDiff,theMeters[evt.unit].beatsPerkW);

				if(residuo==0 && theMeters[evt.unit].currentBeat>0)
				{
					theMeters[evt.unit].saveit=true;	//add onw kwh
					//theMeters[evt.unit].beatSave-=theMeters[evt.unit].beatsPerkW*diaHoraTarifa/100; //should be 0? why this
					theMeters[evt.unit].beatSave=0; //should be 0? why this
				}
				else
					theMeters[evt.unit].saveit=false;

				if(mqttQ)
				{
					theMeter.addit=residuo==0?1:0;
					theMeter.whichMeter=evt.unit;
					xQueueSend( framQ,&theMeter,0 );	//dispathc it to the fram manager
				}
			}
        } else
            printf("PCNT Failed Queue\n");
	}
}

static void pcnt_intr_handler(void *arg)
{
    uint32_t intr_status = PCNT.int_st.val;
    int i;
    pcnt_evt_t evt;
    portBASE_TYPE HPTaskAwoken = pdFALSE;

    for (i = 0; i < PCNT_UNIT_MAX+1; i++) {
        if (intr_status & (BIT(i))) {
            evt.unit = i;
            evt.status = PCNT.status_unit[i].val;
            PCNT.int_clr.val = BIT(i);

            xQueueSendFromISR(pcnt_evt_queue, &evt, &HPTaskAwoken);
            if (HPTaskAwoken == pdTRUE) {
                portYIELD_FROM_ISR();
            }
        }
    }
}

static void pcnt_init(void)
{
    pcnt_config_t pcnt_config;
    pcnt_isr_handle_t user_isr_handle = NULL; //user's ISR service handle
    uint8_t fueron=0;

	theMeters[0].pin=METER0;
	theMeters[1].pin=METER1;
	theMeters[2].pin=METER2;
	theMeters[3].pin=METER3;
	theMeters[4].pin=METER4;

	theMeters[0].pinB=BREAK0;
	theMeters[1].pinB=BREAK1;
	theMeters[2].pinB=BREAK2;
	theMeters[3].pinB=BREAK3;
	theMeters[4].pinB=BREAK4;

    memset((void*)&pcnt_config,0,sizeof(pcnt_config));

	pcnt_config .ctrl_gpio_num 	= 0; 				// Dont need it but -1 DOES NOT work
	pcnt_config .channel 		= PCNT_CHANNEL_0;	// signle channel
	pcnt_config .pos_mode 		= PCNT_COUNT_INC;   // Count up on the positive edge
	pcnt_config .neg_mode 		= PCNT_COUNT_DIS;   // Keep the counter value on the negative edge
	pcnt_config .lctrl_mode 	= PCNT_MODE_KEEP; 	// Reverse counting direction if low
	pcnt_config .hctrl_mode 	= PCNT_MODE_KEEP;    // Keep the primary counter mode if high

    pcnt_isr_register(pcnt_intr_handler, NULL, 0,&user_isr_handle);

	for(int a=0;a<MAXDEVS;a++)
	{
		if(theConf.traceflag & (1<<INTD))
			printf("%sMeter %d conf %d\n",INTDT,a,theConf.configured[a]);
		if(theConf.configured[a]==3)  //Only Authenticated PINs
		{
			fueron++;
			pcnt_config.pulse_gpio_num = theMeters[a].pin;
			pcnt_config.unit = (pcnt_unit_t)a;
			pcnt_unit_config(&pcnt_config);

			theMeters[a].pos=a;
			if(theMeters[a].beatsPerkW==0)
				theMeters[a].beatsPerkW=800;

			pcnt_event_disable((pcnt_unit_t)a, PCNT_EVT_H_LIM);
			pcnt_event_disable((pcnt_unit_t)a, PCNT_EVT_L_LIM);
			pcnt_event_disable((pcnt_unit_t)a, PCNT_EVT_THRES_0);
			pcnt_event_disable((pcnt_unit_t)a, PCNT_EVT_ZERO);

			pcnt_set_filter_value((pcnt_unit_t)a, 1000);
			pcnt_filter_enable((pcnt_unit_t)a);

			pcnt_set_event_value((pcnt_unit_t)a, PCNT_EVT_THRES_1, 10);// instead of a lot of code, just lose a most 10 beats
			pcnt_event_enable((pcnt_unit_t)a, PCNT_EVT_THRES_1);

			pcnt_counter_pause((pcnt_unit_t)a);
			pcnt_counter_clear((pcnt_unit_t)a);
			pcnt_intr_enable((pcnt_unit_t)a);
			pcnt_counter_resume((pcnt_unit_t)a);
		}
	}
	if(theConf.traceflag & (1<<INTD))
		printf("%s%d Meters were activated\n",INTDT,fueron);
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

void write_to_flash() //save our configuration
{
	esp_err_t q ;
	q = nvs_open("config", NVS_READWRITE, &nvshandle);
	if(q!=ESP_OK)
	{
		printf("Error opening NVS File RW %x\n",q);
		return;
	}

//	delay(300);
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
			if(theConf.traceflag & (1<<WIFID))
				printf("%sDefault Id %d\n",WIFIDT,event_id);
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
#ifdef RECOVER
static void recover_fram()
{
	char textl[100];

	scratchTypespi scratch;

    if(!framFlag)
    {
		if(theConf.traceflag & (1<<FRAMD))
			printf("%sFram Is Not Valid\n",FRAMDT);
		return;
    }

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
	//	fram.write_cycle(scratch.medidor.meter, scratch.medidor.mesg,scratch.medidor.cycle);
		fram.write8(SCRATCH,0); //Fast write first byte of Scratch record to 0=done.

		xSemaphoreGive(framSem);
		if(theConf.traceflag & (1<<FRAMD))
			printf("%sRecover %s",FRAMDT,textl);
	}
}
#endif

void write_to_fram(u8 meter,bool addit)
{
	time_t timeH;
    struct tm timeinfo;
#ifdef RECOVER
	scratchTypespi scratch;
#endif
	uint16_t mas;

    if(!framFlag)
    {
		if(theConf.traceflag & (1<<FRAMD))
			printf("%sFram Is Not Valid\n",FRAMDT);
		return;
    }

    time(&timeH);
	localtime_r(&timeH, &timeinfo);
	mesg=timeinfo.tm_mon;
	diag=timeinfo.tm_mday-1;
	yearg=timeinfo.tm_year+1900;
	horag=timeinfo.tm_hour;

	if(addit)
	{
		if(theConf.traceflag & (1<<FRAMD))
			printf("W_T_FRAM Year %d Month %d Day %d Hour %d\n",yearg,mesg,diag,horag);
		theMeters[meter].curLife++;
		theMeters[meter].curMonth++;
		theMeters[meter].curDay++;
		theMeters[meter].curHour++;
		if(theMeters[meter].beatSaveRaw>=theMeters[meter].beatsPerkW)
		{
			mas=theMeters[meter].beatSaveRaw / theMeters[meter].beatsPerkW;
			theMeters[meter].curMonthRaw+=mas;
			theMeters[meter].curDayRaw+=mas;
			theMeters[meter].curHourRaw+=mas;
			theMeters[meter].beatSaveRaw=theMeters[meter].beatSaveRaw % theMeters[meter].beatsPerkW; //not 0 else we lose beats that happend that we not recorded on time
		}
		time((time_t*)&theMeters[meter].lastKwHDate); //last time we saved data

#ifdef RECOVER
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
#endif

		fram.write_beat(meter,theMeters[meter].currentBeat);
		fram.write_lifekwh(meter,theMeters[meter].curLife);
		fram.write_month(meter,mesg,theMeters[meter].curMonth);
		fram.write_monthraw(meter,mesg,theMeters[meter].curMonthRaw);
		fram.write_day(meter,yearg,mesg,diag,theMeters[meter].curDay);
		fram.write_dayraw(meter,yearg,mesg,diag,theMeters[meter].curDayRaw);
		fram.write_hour(meter,yearg,mesg,diag,horag,theMeters[meter].curHour);
		fram.write_hourraw(meter,yearg,mesg,diag,horag,theMeters[meter].curHourRaw);
		fram.write_lifedate(meter,theMeters[meter].lastKwHDate);  //should be down after scratch record???
#ifdef RECOVER
		fram.write8(SCRATCH,0); //Fast write first byte of Scratch record to 0=done.
#endif

	}
		else
		{
			fram.write_beat(meter,theMeters[meter].currentBeat);
			fram.writeMany(FRAMDATE,(uint8_t*)&timeH,sizeof(timeH));//last known date
		}
}

void load_from_fram(u8 meter)
{
    if(!framFlag)
    {
		if(theConf.traceflag & (1<<FRAMD))
			printf("%sFram Is Not Valid\n",FRAMDT);
		return;
    }

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
		fram.read_beat(meter,(u8*)&theMeters[meter].currentBeat);
		totalPulses+=theMeters[meter].currentBeat;
		if(theConf.beatsPerKw[meter]==0)
			theConf.beatsPerKw[meter]=800;// just in case div by 0 crash
		theMeters[meter].beatsPerkW=theConf.beatsPerKw[meter];// just in case div by 0 crash
		u16 nada=theMeters[meter].currentBeat/theConf.beatsPerKw[meter];
		theMeters[meter].beatSave=theMeters[meter].currentBeat-(nada*theConf.beatsPerKw[meter]);
		theMeters[meter].beatSaveRaw=theMeters[meter].beatSave;
	xSemaphoreGive(framSem);

		if(theConf.traceflag & (1<<FRAMD))
			printf("[FRAMD]Loaded Meter %d curLife %d beat %d\n",meter,theMeters[meter].curLife,theMeters[meter].currentBeat);
	}
}

static void init_fram()
{
#ifdef RECOVER
	scratchTypespi scratch;
#endif
	// FRAM Setup

	spi_flash_init();

	framFlag=fram.begin(FMOSI,FMISO,FCLK,FCS,&framSem); //will create SPI channel and Semaphore

	if(framFlag)
	{
#ifdef RECOVER
		if(xSemaphoreTake(framSem, portMAX_DELAY/  portTICK_RATE_MS))
		{
			fram.read_recover(&scratch);
			xSemaphoreGive(framSem);
		}

		if (scratch.medidor.state!=0)
		{
			printf("Recover Fram\n");
			recover_fram();
		}
#endif
		//all okey in our Fram after this point

		//load all devices counters from FRAM
		for (int a=0;a<MAXDEVS;a++)
			load_from_fram(a);
	}
}

static void wifi_init(void)
{
	tcpip_adapter_ip_info_t 		ipInfo;
	printf("%sWiFi Mode %s\n",BOOTDT,theConf.active?"RunConf":"SetupConf");
	if(!theConf.active)
	    tcpip_adapter_init();
	else
	    esp_netif_init();

	esp_efuse_mac_get_default(them);

    //if imperative to change default 192.168.4.1 to anything else use below. Careful with esp_net_if deprecation warnings
	if(!theConf.active)	{
		memset(&ipInfo,0,sizeof(ipInfo));
		//set IP Address of AP for Stations DHCP
		ESP_ERROR_CHECK(tcpip_adapter_dhcps_stop(TCPIP_ADAPTER_IF_AP));
		IP4_ADDR(&ipInfo.ip, 192,168,19,1);
		IP4_ADDR(&ipInfo.gw, 192,168,19,1);
		IP4_ADDR(&ipInfo.netmask, 255,255,255,0);
		ESP_ERROR_CHECK(tcpip_adapter_set_ip_info(TCPIP_ADAPTER_IF_AP, &ipInfo));
		ESP_ERROR_CHECK(tcpip_adapter_dhcps_start(TCPIP_ADAPTER_IF_AP));
	}

	    wifi_event_group = xEventGroupCreate();
	    xEventGroupClearBits(wifi_event_group, WIFI_BIT);

	    ESP_ERROR_CHECK(esp_event_loop_create_default());
	    if(theConf.active)
	    	esp_netif_create_default_wifi_sta();

	    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler, NULL));
	    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &ip_event_handler, NULL));

	    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
	    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
	    ESP_ERROR_CHECK(esp_wifi_set_storage(WIFI_STORAGE_RAM));

	    wifi_config_t wifi_config;
	    memset(&wifi_config,0,sizeof(wifi_config));//very important
		if(theConf.active)
	    {
			strcpy((char*)wifi_config.sta.ssid,"Meteriot");
			strcpy((char*)wifi_config.sta.password,"Meteriot");
			ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
			ESP_ERROR_CHECK(esp_wifi_set_config(ESP_IF_WIFI_STA, &wifi_config));
			esp_wifi_start();
	    }

	    //AP section
		if(!theConf.active)
	    {
			memset(&wifi_config,0,sizeof(wifi_config));//very important
			sprintf(tempb,"Meter%02x%02x",them[4],them[5]);
			strcpy((char*)wifi_config.ap.ssid,tempb);
			strcpy((char*)wifi_config.ap.password,"csttpstt");
			wifi_config.ap.authmode=WIFI_AUTH_WPA_PSK;
			wifi_config.ap.ssid_hidden=false;
			wifi_config.ap.beacon_interval=400;
			wifi_config.ap.max_connection=50;
			wifi_config.ap.ssid_len=0;
			wifi_config.ap.channel=1;
			ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_AP));
			ESP_ERROR_CHECK(esp_wifi_set_config(ESP_IF_WIFI_AP, &wifi_config));
			esp_wifi_start();
	    }
}

void updateDateTime(loginT loginData)
{
    struct tm timeinfo;

	if(theConf.traceflag & (1<<FRMCMD))
		printf("%sLogin Date %d Tariff %d\n",FRMCMDT,(int)loginData.thedate,loginData.theTariff);

	localtime_r(&loginData.thedate, &timeinfo);
	diaHoraTarifa=loginData.theTariff;// Host will give us Hourly Tariff. No need to store

	if(theConf.traceflag & (1<<FRMCMD))
	{
		printf("%sYear %d Month %d Day %d Hour %d\n",FRMCMDT,timeinfo.tm_year+1900,timeinfo.tm_mon,timeinfo.tm_mday-1,timeinfo.tm_hour);
	}
	mesg=timeinfo.tm_mon;
	diag=timeinfo.tm_mday-1;
	yearg=timeinfo.tm_year+1900;
	horag=timeinfo.tm_hour;
	struct timeval now = { .tv_sec = loginData.thedate, .tv_usec=0};
	settimeofday(&now, NULL);
}

cJSON *makeCmdcJSON(meterType *meter)
{
	cJSON *cmdJ=cJSON_CreateObject();
	cJSON_AddStringToObject(cmdJ,"MAC",				theMac);
	cJSON_AddStringToObject(cmdJ,"cmd",				"/ga_status");
	cJSON_AddStringToObject(cmdJ,"MeterId",			meter->serialNumber);
	cJSON_AddNumberToObject(cmdJ,"Transactions",	++theMeters[meter->pos].vanMqtt);// when affecting theMeter this pointer is to a COPY so get its pos and update directly
	cJSON_AddNumberToObject(cmdJ,"KwH",				meter->curLife);
	cJSON_AddNumberToObject(cmdJ,"Beats",			meter->currentBeat);
	cJSON_AddNumberToObject(cmdJ,"Pos",				meter->pos);
	cJSON_AddNumberToObject(cmdJ,"macn",			theMacNum);
	return cmdJ;
}

cJSON* makecJSONMeter(meterType *meter)
{
	cJSON *root=cJSON_CreateObject();

	if(root==NULL)
	{
		printf("cannot create root\n");
		return NULL;
	}

	cJSON *cmdJ=makeCmdcJSON(meter);
	cJSON *ar = cJSON_CreateArray();

	if(cmdJ==NULL || ar==NULL)
	{
		printf("cannot create aux json\n");
		return;
	}

	cJSON_AddItemToArray(ar, cmdJ);
	cJSON_AddItemToObject(root, "Batch",ar);
	return root;
}

cJSON * makeGroupCmdAll()
{

	/////// Create Message for Grouped Cmds ///////////////////
			cJSON *root=cJSON_CreateObject();
			if(root==NULL)
			{
				printf("cannot create nroot\n");
				return NULL;
			}

			//already done at the beginning
			esp_efuse_mac_get_default(them);
			double dmac=(double)theMacNum;

			cJSON *ar = cJSON_CreateArray();
			for (int a=0;a<MAXDEVS;a++)
			{
				cJSON *cmdInt=makeCmdcJSON(&theMeters[a]);
				cJSON_AddItemToArray(ar, cmdInt);
			}
			cJSON_AddItemToObject(root, "Batch",ar);
			return root;
}

cJSON * makeGroupCmd(meterType *pmeter)
{
	meterType meter;
	framMeterType thisMeter;

		thisMeter.whichMeter=pmeter->pos;
		thisMeter.addit=pmeter->saveit;
		xQueueSend(framQ,&thisMeter,0);

	/////// Create Message for Grouped Cmds ///////////////////
			cJSON *root=cJSON_CreateObject();
			if(root==NULL)
			{
				printf("cannot create nroot\n");
				return NULL;
			}
			esp_efuse_mac_get_default(them);
			double dmac=(double)theMacNum;

			cJSON *ar = cJSON_CreateArray();
			cJSON *cmdJ=cJSON_CreateObject();
			cJSON_AddStringToObject(cmdJ,"MAC",				theMac);
			cJSON_AddStringToObject(cmdJ,"cmd",				"/ga_status");
			cJSON_AddStringToObject(cmdJ,"MeterId",			pmeter->serialNumber);
			cJSON_AddNumberToObject(cmdJ,"Transactions",	++theMeters[pmeter->pos].vanMqtt);// when affecting theMeter this pointer is to a COPY so get its pos and update directly
			cJSON_AddNumberToObject(cmdJ,"KwH",				pmeter->curLife);
			cJSON_AddNumberToObject(cmdJ,"Beats",			pmeter->currentBeat);
			cJSON_AddNumberToObject(cmdJ,"Pos",				pmeter->pos);
			cJSON_AddNumberToObject(cmdJ,"macn",			dmac);

			totalMsg[pmeter->pos]++;

			cJSON_AddItemToArray(ar, cmdJ);
			delay(waitQueue); // accumulate more

			//////////////  test if something else in Queue and send them all //////////////////
			if(uxQueueMessagesWaiting(mqttR)>0)
			{
				while(uxQueueMessagesWaiting(mqttR)>0)
				{
					if( xQueueReceive( mqttR, &meter, 500/  portTICK_RATE_MS ))
					{
						cJSON *cmdInt=makeCmdcJSON(&meter);
						cJSON_AddItemToArray(ar, cmdInt);
						thisMeter.whichMeter=meter.pos;
						thisMeter.addit=meter.saveit;
						totalMsg[meter.pos]++;
						xQueueSend(framQ,&thisMeter,0);
					}
				}
			}
			cJSON_AddItemToObject(root, "Batch",ar);
			return root;
}


int sendMsg(uint8_t *lmessage, uint8_t *donde,uint8_t maxx)
{
    int waitTime;
	ip_addr_t 						remote;
	u8 								aca[4];
	tcpip_adapter_ip_info_t 		ip_info;
    struct timeval to;
	int addr_family;
	int ip_protocol,err;

	if(!conn)
	{
		gsock=-1;
		if(esp_wifi_connect()==ESP_OK)
		{
			if(theConf.traceflag & (1<<MSGD))
				printf("%sEstablish connect\n",MSGDT);
			EventBits_t uxBits=xEventGroupWaitBits(wifi_event_group, WIFI_BIT, false, true, 10000/  portTICK_RATE_MS);
			if ((uxBits & WIFI_BIT)!=WIFI_BIT)
				return -1;

			conn=true;

			ESP_ERROR_CHECK(tcpip_adapter_get_ip_info(TCPIP_ADAPTER_IF_STA, &ip_info));
			    	//	printf("IP Address:  %s\n", ip4addr_ntoa(&ip_info.ip));
			    	//	printf("Subnet mask: %s\n", ip4addr_ntoa(&ip_info.netmask));
			    	//	printf("Gateway:     %s\n", ip4addr_ntoa(&ip_info.gw));
			memcpy(&aca,&ip_info.gw,4);
			IP_ADDR4( &remote, aca[0],aca[1],aca[2],aca[3]);

			struct sockaddr_in dest_addr;
			memcpy(&dest_addr.sin_addr.s_addr,&ip_info.gw,4);
			dest_addr.sin_family = AF_INET;
			dest_addr.sin_port = htons(BDGHOSTPORT);
			addr_family = AF_INET;
			ip_protocol = IPPROTO_IP;
		 //   inet_ntoa_r(dest_addr.sin_addr, addr_str, sizeof(addr_str) - 1);

			gsock =  socket(addr_family, SOCK_STREAM, ip_protocol);
			if (gsock < 0) {
				printf( "Unable to create socket: errno %d\n", errno);
				return -1;

			}
			if(theConf.traceflag & (1<<MSGD))
				printf("%sSocket %d created, connecting to %s:%d\n",MSGDT,gsock, ip4addr_ntoa(&ip_info.gw), BDGHOSTPORT);

			err = connect(gsock, (struct sockaddr *)&dest_addr, sizeof(dest_addr));
			if (err != 0)
			{
				if(theConf.traceflag & (1<<MSGD))
					printf( "%sSocket unable to connect: errno %d\n",MSGDT, errno);
				return -1;
			}
		}
	    else
	    {
			if(theConf.traceflag & (1<<MSGD))
				printf("%sCould not esp_connect\n",MSGDT);
			return -1;
	    }
	} //if conn
	do
	{
		waitTime=rand() % 600;
	} while (waitTime<150); //minimum 150

	delay(waitTime);// let OTHER meterclients have a chance based on randomness

	if(theConf.traceflag & (1<<MSGD))
		printf("%sSending queue %s\n",MSGDT,lmessage);

	 err = send(gsock, (char*)lmessage, strlen((char*)lmessage), 0);
	 if (err < 0)
	 {
		if(theConf.traceflag & (1<<MSGD))
			printf( "%sSock %d Error occurred during sending: errno %d\n",MSGDT,gsock, errno);
	   conn=false;
	   return -1;
	}

	to.tv_sec = 2;
	to.tv_usec = 0;

	if (setsockopt(gsock,SOL_SOCKET,SO_RCVTIMEO,&to,sizeof(to)) < 0)

	{
		if(theConf.traceflag & (1<<MSGD))
			printf("Unable to set read timeout on socket!\n");
		return -1;
	}

	int len = recv(gsock, donde, maxx, 0);

	if(theConf.traceflag & (1<<MSGD))
		printf("%sSendmsg successful %d\n",MSGDT,len);

	return len;
}


void logIn()
{
    loginT loginData;
	gpio_set_level((gpio_num_t)WIFILED, 1);

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

	sendMsg((uint8_t*)logMsg,(uint8_t*)&loginData, sizeof(loginData));

	updateDateTime(loginData);
	oldMesg=mesg;
	oldDiag=diag;
	oldHorag=horag;

	if(theConf.traceflag & (1<<CMDD))
		printf("%sLogin year %d month %d day %d hour %d Tariff %d\n",CMDDT,yearg,mesg,diag,horag,loginData.theTariff);

	free(logMsg);
	cJSON_Delete(root);
	gpio_set_level((gpio_num_t)WIFILED, 0);

}

void sendStatusMeterAll()
{
    loginT 	loginData;
    int sendStatus;

	gpio_set_level((gpio_num_t)WIFILED, 1);

	if(theConf.traceflag & (1<<CMDD))
		printf("%sSendM %d Heap %d wait %d... ",CMDDT,++sentTotal,esp_get_free_heap_size(),uxQueueMessagesWaiting(mqttR));
	fflush(stdout);
	cJSON *root=makeGroupCmdAll();
	char *lmessage=cJSON_Print(root);
	sendStatus=sendMsg((uint8_t*)lmessage,(uint8_t*)&loginData,sizeof(loginData));
	if(sendStatus>=0)
		updateDateTime(loginData);
	if(lmessage)
		free(lmessage);
	if(root)
		cJSON_Delete(root);

	if(theConf.traceflag & (1<<CMDD))
	{
		printf("%sSendMeterFram dates year %d month %d day %d hour %d Tariff %d\n",CMDDT,yearg,mesg,diag,horag,loginData.theTariff);
		printf("%sTar %d sent Heap %d\n",CMDDT,loginData.theTariff,esp_get_free_heap_size());
	}
	gpio_set_level((gpio_num_t)WIFILED, 0);
}

void sendStatusMeter(meterType* meter)
{
    loginT 	loginData;
    int sendStatus;

	gpio_set_level((gpio_num_t)WIFILED, 1);

	if(theConf.traceflag & (1<<CMDD))
		printf("%sSendM %d Heap %d wait %d... ",CMDDT,++sentTotal,esp_get_free_heap_size(),uxQueueMessagesWaiting(mqttR));
	fflush(stdout);
	cJSON *root=makeGroupCmd(meter);
	char *lmessage=cJSON_Print(root);
	sendStatus=sendMsg((uint8_t*)lmessage,(uint8_t*)&loginData,sizeof(loginData));
	if(sendStatus>=0)
		updateDateTime(loginData);
	free(lmessage);
	cJSON_Delete(root);

	if(theConf.traceflag & (1<<CMDD))
	{
		printf("%sSendMeterFram dates year %d month %d day %d hour %d Tariff %d\n",CMDDT,yearg,mesg,diag,horag,loginData.theTariff);
		printf("%sTar %d sent Heap %d\n",CMDDT,loginData.theTariff,esp_get_free_heap_size());
	}
	gpio_set_level((gpio_num_t)WIFILED, 0);
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

#ifdef SENDER
static void mqttManager(void* arg)
{
	meterType meter;

	while(1)
	{
		if( xQueueReceive( mqttR, &meter, portMAX_DELAY/  portTICK_RATE_MS ))
		{
			printf("Meter %d Beat %d Pos %d Queue %d Heap %d\n",meter.pin,meter.currentBeat,meter.pos,uxQueueMessagesWaiting(mqttR),esp_get_free_heap_size());
			sendStatusMeter(&meter);
			printf("mqttloop heap %d\n",esp_get_free_heap_size());
		}
		else
			delay(100);
	}
}


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

void framManager(void * pArg)
{
	framMeterType theMeter;

	while(true)
	{
		if( xQueueReceive( framQ, &theMeter, portMAX_DELAY/  portTICK_RATE_MS ))
		{
			if(xSemaphoreTake(framSem, portMAX_DELAY/  portTICK_RATE_MS))
			{
				write_to_fram(theMeter.whichMeter,theMeter.addit);
				if(theConf.traceflag & (1<<FRAMD))
					printf("%sSaving Meter %d add %d Beats %d\n",FRAMDT,theMeter.whichMeter,theMeter.addit,theMeters[theMeter.whichMeter].currentBeat);
				xSemaphoreGive(framSem);
			}
		}
		else
		{
			if(theConf.traceflag & (1<<FRAMD))
				printf("%sFailed framQ Manager\n",FRAMDT);
			delay(1000);
		}
	}
}

void init_vars()
{
	   deb=false;
	   qwait=QDELAY;
	   qdelay=qwait*1000;
	   sendTcp=waitQueue=500;
	   wDelay=TIMEWAITPCNT;
	   wifiSem= xSemaphoreCreateBinary();
	   xSemaphoreGive(wifiSem);
	   diaHoraTarifa=100;// div by zero if not and not loaded

	   mqttQ = xQueueCreate( 20, sizeof( meterType ) );
	   if(!mqttQ)
		   printf("Failed queue Tx\n");

		mqttR = xQueueCreate( 20, sizeof( meterType ) );
		if(!mqttR)
			printf("Failed queue Rx\n");

		framQ = xQueueCreate( 20, sizeof( framMeterType ) );
		if(!framQ)
			printf("Failed queue Fram\n");

		strcpy(lookuptable[0],"BOOTD");
		strcpy(lookuptable[1],"WIFID");
		strcpy(lookuptable[2],"MQTTD");
		strcpy(lookuptable[3],"PUBSUBD");
		strcpy(lookuptable[4],"OTAD");
		strcpy(lookuptable[5],"CMDD");
		strcpy(lookuptable[6],"WEBD");
		strcpy(lookuptable[7],"GEND");
		strcpy(lookuptable[8],"MQTTT");
		strcpy(lookuptable[9],"FRMCMD");
		strcpy(lookuptable[10],"INTD");
		strcpy(lookuptable[11],"FRAMD");
		strcpy(lookuptable[12],"MSGD");
		strcpy(lookuptable[13],"TIMED");

		string debugs;

		// add - sign to Keys
		for (int a=0;a<NKEYS/2;a++)
		{
			debugs="-"+string(lookuptable[a]);
			strcpy(lookuptable[a+NKEYS/2],debugs.c_str());
		}
}

void check_boot_options()
{
	char them[6];

	esp_efuse_mac_get_default(them);
	memcpy(&theMacNum,&them[2],4);
	sprintf(theMac,"%02x:%02x:%02x:%02x:%02x:%02x",them[0],them[1],them[2],them[3],them[4],them[5]);

	if(theConf.traceflag & (1<<BOOTD))
	    printf("%s Manufacturer %04x Fram Id %04x Fram Size %d%s\n",MAGENTA,fram.manufID,fram.prodId,fram.intframWords,RESETC);

	if((theConf.traceflag & (1<<BOOTD)) && fram.intframWords>0)
	{
		printf("%s=============== FRAM ===============%s\n",RED,YELLOW);
		printf("FRAMDATE(%s%d%s)=%s%d%s\n",GREEN,FRAMDATE,YELLOW,CYAN,METERVER-FRAMDATE,RESETC);
		printf("METERVER(%s%d%s)=%s%d%s\n",GREEN,METERVER,YELLOW,CYAN,FREEFRAM-METERVER,RESETC);
		printf("FREEFRAM(%s%d%s)=%s%d%s\n",GREEN,FREEFRAM,YELLOW,CYAN,SCRATCH-FREEFRAM,RESETC);
		printf("SCRATCH(%s%d%s)=%s%d%s\n",GREEN,SCRATCH,YELLOW,CYAN,SCRATCHEND-SCRATCH,RESETC);
		printf("SCRATCHEND(%s%d%s)=%s%d%s\n",GREEN,SCRATCHEND,YELLOW,CYAN,TARIFADIA-SCRATCHEND,RESETC);
		printf("TARIFADIA(%s%d%s)=%s%d%s\n",GREEN,TARIFADIA,YELLOW,CYAN,FINTARIFA-TARIFADIA,RESETC);
		printf("FINTARIFA(%s%d%s)=%s%d%s\n",GREEN,FINTARIFA,YELLOW,CYAN,BEATSTART-FINTARIFA,RESETC);
		printf("BEATSTART(%s%d%s)=%s%d%s\n",GREEN,BEATSTART,YELLOW,CYAN,LIFEKWH-BEATSTART,RESETC);
		printf("LIFEKWH(%s%d%s)=%s%d%s\n",GREEN,LIFEKWH,YELLOW,CYAN,LIFEDATE-LIFEKWH,RESETC);
		printf("LIFEDATE(%s%d%s)=%s%d%s\n",GREEN,LIFEDATE,YELLOW,CYAN,MONTHSTART-LIFEDATE,RESETC);
		printf("MONTHSTART(%s%d%s)=%s%d%s\n",GREEN,MONTHSTART,YELLOW,CYAN,MONTHRAW-MONTHSTART,RESETC);
		printf("MONTHRAW(%s%d%s)=%s%d%s\n",GREEN,MONTHRAW,YELLOW,CYAN,DAYSTART-MONTHRAW,RESETC);
		printf("DAYSTART(%s%d%s)=%s%d%s\n",GREEN,DAYSTART,YELLOW,CYAN,DAYRAW-DAYSTART,RESETC);
		printf("DAYRAW(%s%d%s)=%s%d%s\n",GREEN,DAYRAW,YELLOW,CYAN,HOURSTART-DAYRAW,RESETC);
		printf("HOURSTART(%s%d%s)=%s%d%s\n",GREEN,HOURSTART,YELLOW,CYAN,HOURRAW-HOURSTART,RESETC);
		printf("HOURRAW(%s%d%s)=%s%d%s\n",GREEN,HOURRAW,YELLOW,CYAN,DATAEND-HOURRAW,RESETC);
		printf("DATAEND(%s%d%s)=%s%d%s\n",GREEN,DATAEND,YELLOW,CYAN,TOTALFRAM-DATAEND,RESETC);
		printf("TOTALFRAM(%s%d%s) Devices %s%d%s\n",GREEN,TOTALFRAM,YELLOW,CYAN,TOTALFRAM/DATAEND,RESETC);
		printf("%s=============== FRAM ===============%s\n",RED,RESETC);
	}
}

static const httpd_uri_t setup = {
    .uri       = "/setup",
    .method    = HTTP_GET,
    .handler   = setup_get_handler,
	.user_ctx	= NULL
};

static esp_err_t setup_get_handler(httpd_req_t *req)
{
    char*  buf;
    size_t buf_len;
    int cualm=0;
    time_t now;
    struct tm timeinfo;
    char strftime_buf[64];

	time(&now);
	localtime_r(&now, &timeinfo);
	strftime(strftime_buf, sizeof(strftime_buf), "%c", &timeinfo);

    buf_len = httpd_req_get_url_query_len(req) + 1;
    if (buf_len > 1)
    {
        buf = (char*)malloc(buf_len);
        if (httpd_req_get_url_query_str(req, buf, buf_len) == ESP_OK)
        {
            char param[32];
            if (httpd_query_key_value(buf, "meter", param, sizeof(param)) == ESP_OK)
            {
            	 cualm=atoi(param);
            	 sprintf(tempb,"[%s]Invalid Meter range %d",strftime_buf,cualm);
            	 if (cualm>=MAXDEVS)
            		 goto exit;
            }
            if (httpd_query_key_value(buf, "mid", param, sizeof(param)) == ESP_OK)
            	 strcpy((char*)&setupHost[cualm].meterid,param);

            if (httpd_query_key_value(buf, "kwh", param, sizeof(param)) == ESP_OK)
            	 setupHost[cualm].startKwh=atoi(param);

            if (httpd_query_key_value(buf, "bpk", param, sizeof(param)) == ESP_OK)
            	 setupHost[cualm].bpkwh=atoi(param);

            if (httpd_query_key_value(buf, "duedate", param, sizeof(param)) == ESP_OK)
            	 setupHost[cualm].diaCorte=atoi(param);

            if (httpd_query_key_value(buf, "tariff", param, sizeof(param)) == ESP_OK)
            	 setupHost[cualm].tariff=atoi(param);
        }
        free(buf);
    }

    if(setupHost[cualm].bpkwh>0 && setupHost[cualm].diaCorte>0 && setupHost[cualm].startKwh>0 && setupHost[cualm].tariff>0 && strlen(setupHost[cualm].meterid)>0)
    {
		time(&now);
		localtime_r(&now, &timeinfo);
		strftime(strftime_buf, sizeof(strftime_buf), "%c", &timeinfo);
    	setupHost[cualm].valid=true;
    	theConf.configured[cualm]=1; //in transit mode
    	sprintf(tempb,"[%s]Meter:%d Id=%s kWh=%d BPK=%d Corte=%d Tariff=%d",strftime_buf,cualm,setupHost[cualm].meterid,setupHost[cualm].startKwh,setupHost[cualm].bpkwh,
    			setupHost[cualm].diaCorte,setupHost[cualm].tariff);
    }
    else
    {
    	exit:
    	setupHost[cualm].valid=false;
        sprintf(tempb,"[%s]Invalid parameters",strftime_buf);
    }

	httpd_resp_send(req, tempb, strlen(tempb));

    return ESP_OK;
}

static const httpd_uri_t challenge = {
    .uri       = "/challenge",
    .method    = HTTP_GET,
    .handler   = challenge_get_handler,
	.user_ctx	= NULL
};

static esp_err_t challenge_get_handler(httpd_req_t *req)
{
    char*  buf;
    size_t buf_len;
    int cualm,valor;
    time_t now;
    struct tm timeinfo;
    char strftime_buf[64];

	time(&now);
	localtime_r(&now, &timeinfo);
	strftime(strftime_buf, sizeof(strftime_buf), "%c", &timeinfo);

    buf_len = httpd_req_get_url_query_len(req) + 1;
    if (buf_len > 1)
    {
		sprintf(tempb,"[%s]Invalid parameters",strftime_buf);
        buf = (char*)malloc(buf_len);
        if (httpd_req_get_url_query_str(req, buf, buf_len) == ESP_OK)
        {
            char param[32];

			if (httpd_query_key_value(buf, "challenge", param, sizeof(param)) == ESP_OK)
			{
				valor=atoi(param);
				if (valor==654321)
				{
					for (int a=0;a<MAXDEVS;a++)
					{
						if(theConf.configured[a]==2)
						{
							theMeters[a].beatsPerkW=setupHost[a].bpkwh;
							memcpy((void*)&theMeters[a].serialNumber,(void*)&setupHost[a].meterid,sizeof(theMeters[a].serialNumber));
							theMeters[a].curLife=setupHost[a].startKwh;
							time((time_t*)&theMeters[a].ampTime);

							theConf.beatsPerKw[a]=setupHost[a].bpkwh;
							memcpy(theConf.medidor_id[a],(void*)&setupHost[a].meterid,sizeof(theConf.medidor_id[cualm]));
							time((time_t*)&theConf.bornDate[a]);
							theConf.beatsPerKw[a]=setupHost[a].bpkwh;
							theConf.bornKwh[a]=setupHost[a].startKwh;
							theConf.diaDeCorte[a]=setupHost[a].tariff;
							theConf.configured[a]=3;					//final status configured
						}
						else
							theConf.configured[a]=0; //reset it
					}
					theConf.active=1;				// theConf is now ACTIVE and Certified
					write_to_flash();
					memset(&setupHost,0,sizeof(setupHost));


					sprintf(tempb,"[%s]Meters were saved permanently",strftime_buf);
				}
				else
					sprintf(tempb,"[%s]Invalid challenge",strftime_buf);
			}
			else
				sprintf(tempb,"[%s]Missing challenge",strftime_buf);
        }
        free(buf);
       }
	httpd_resp_send(req, tempb, strlen(tempb));

    return ESP_OK;
}

static const httpd_uri_t setupend = {
    .uri       = "/setupend",
    .method    = HTTP_GET,
    .handler   = setupend_get_handler,
	.user_ctx	= NULL
};

static esp_err_t setupend_get_handler(httpd_req_t *req)
{
    char*  buf;
    size_t buf_len;
    int cuantos;
    time_t now;
    struct tm timeinfo;
    char strftime_buf[64];

	time(&now);
	localtime_r(&now, &timeinfo);
	strftime(strftime_buf, sizeof(strftime_buf), "%c", &timeinfo);

    buf_len = httpd_req_get_url_query_len(req) + 1;
    if (buf_len > 1)
    {
		sprintf(tempb,"[%s]Invalid parameters",strftime_buf);
        buf = (char*)malloc(buf_len);
        if (httpd_req_get_url_query_str(req, buf, buf_len) == ESP_OK)
        {
            char param[32];
            if (httpd_query_key_value(buf, "meter", param, sizeof(param)) == ESP_OK) //number of meters to make permanent
            {
            	 cuantos=atoi(param);
            	 if (cuantos<=MAXDEVS)
            	 {
            		 for (int a=0;a<cuantos;a++)
            		 {
            			 if(theConf.configured[a]!=1)
            			 {
              				sprintf(tempb,"[%s]Meter %d is not configured",strftime_buf,a);
              				goto exit;
            			 }
            			 else
            				 theConf.configured[a]=2;								//waiting for challenge
            		 }
            			 sprintf(tempb,"[%s]Meters 1-%d will be saved. Challenge=123456",strftime_buf,cuantos);
            	 }
            	 else
     				sprintf(tempb,"[%s]Invalid number of meters %d",strftime_buf,cuantos);
            }
        }
        free(buf);
       }
exit:
	httpd_resp_send(req, tempb, strlen(tempb));

    return ESP_OK;
}

static void start_webserver(void)
{
    httpd_handle_t server = NULL;
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();

    if(theConf.traceflag & (1<<WEBD))
    	printf("%sStarting server on port:%d\n",WEBDT, config.server_port);
    if (httpd_start(&server, &config) == ESP_OK) {
        // Set URI handlers
        httpd_register_uri_handler(server, &setup); 		//setup upto 5 meters
        httpd_register_uri_handler(server, &setupend);		//end setup and send challenge
        httpd_register_uri_handler(server, &challenge);		//confirm challenge and store in flash
    }
    if(theConf.traceflag & (1<<WEBD))
    	printf("WebServer Started\n");
    vTaskDelete(NULL);
}


void app_main()
{
    esp_log_level_set("*", ESP_LOG_WARN);

	ESP_LOGI(TAG, "[APP] BuildClient starting up");
    ESP_LOGI(TAG, "[APP] Free memory: %d bytes", esp_get_free_heap_size());
    ESP_LOGI(TAG, "[APP] IDF version: %s", esp_get_idf_version());

    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES)
    {
		printf("No free pages erased!!!!\n");
		ESP_ERROR_CHECK(nvs_flash_erase());
		err = nvs_flash_init();
    }

	read_flash();				//our Configuration structure

    if (theConf.centinel!=CENTINEL || !gpio_get_level((gpio_num_t)0))
	{
		printf("Read centinel %x not valid. Erasing Config\n",theConf.centinel);
		erase_config();
	}

    init_vars();				// load initial vars
    wifi_init(); 				// start the wifi
    init_fram();				// start the Fram Driver and load our Meters
    check_boot_options();		// see if we need to display boot stuff

#ifdef SENDER
    xTaskCreate(&mqttManager,"meters",4096,NULL, 5, NULL);
    xTaskCreate(&submode,"U571",10240,NULL, 5, NULL);
    delay(1000);
    xTaskCreate(&sender,"sender",4096,NULL, 5, NULL);
#endif
#ifdef TEST
	xTaskCreate(&kbd,"kbd",4096,NULL, 4, NULL);	//debuging only
#endif
	if(theConf.active)
	{
		logIn();													//we are MeterControllers need to login to our Host Controller. For order purposes
		xTaskCreate(&pcntManager,"pcntMgr",4096,NULL, 4, NULL);		// start the Pulse Manager task
		pcnt_init();												// start receiving pulses
		xTaskCreate(&framManager,"fmg",4096,NULL, 10, NULL);		//in charge of saving meter activity to Fram
		xTaskCreate(&timeKeeper,"tmK",4096,NULL, 10, NULL);			// Due to Tariffs, we need to check hour,day and month changes
	}
	else
		xTaskCreate(&start_webserver,"web",10240,(void*)1, 4, &webHandle);// Messages from the Meters. Controller Section socket manager

		theConf.lastResetCode=rtc_get_reset_reason(0);
		time((time_t*)&theConf.lastBootDate);
		theConf.bootcount++;
		write_to_flash();
;
}
