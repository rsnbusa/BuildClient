#include "includes.h"
#include "defines.h"
#include "projStruct.h"
#include "globals.h"
using namespace std;

void kbd(void *arg);
void start_webserver(void *arg);
void sendStatusMeterAll();

#ifdef DISPLAY
void displayManager(void * pArg);
void drawString(int x, int y, string que, int fsize, int align,displayType showit,overType erase);
#endif

void pprintf(const char * format, ...)
{
	  char *mbuffer;
	  va_list args;
	  if(xSemaphoreTake(printSem, portMAX_DELAY))
	  	{
		  mbuffer=(char*)malloc(4096);
		  if(mbuffer)
		  {
			  va_start (args, format);
			  vsprintf (mbuffer,format, args);
			  printf(mbuffer);
			  va_end (args);
			  free(mbuffer);
		  }
		  xSemaphoreGive(printSem);
	}

}

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

static void system_failure(char * title)
{
	display.clear();
	drawString(64,8,title,24,TEXT_ALIGN_CENTER,DISPLAYIT,NOREP);
	while(true)
	{
		gpio_set_level((gpio_num_t)WIFILED, 1);
		delay(100);
		gpio_set_level((gpio_num_t)WIFILED, 0);
		delay(100);
	}
}

void shaMake(char * payload,uint8_t payloadLen, unsigned char *lshaResult)
{
	mbedtls_md_type_t md_type = MBEDTLS_MD_SHA256;

	char key[]="me mima mucho";
	mbedtls_md_init(&mbedtls_ctx);
	ESP_ERROR_CHECK(mbedtls_md_setup(&mbedtls_ctx, mbedtls_md_info_from_type(md_type), 1));
	ESP_ERROR_CHECK(mbedtls_md_hmac_starts(&mbedtls_ctx, (const unsigned char *) key, strlen(key)));
	ESP_ERROR_CHECK(mbedtls_md_hmac_update(&mbedtls_ctx, (const unsigned char *) payload,payloadLen));
	ESP_ERROR_CHECK(mbedtls_md_hmac_finish(&mbedtls_ctx, lshaResult));
	mbedtls_md_free(&mbedtls_ctx);
	if(theConf.traceflag & (1<<WEBD))
	{
		uint8_t *lcopy=lshaResult;
		for(int a=0;a<32;a++)
		{
			pprintf("%02x",*lcopy);
			lcopy++;
		}
		pprintf("\n");
	}
}

/*
static void hourChange()
{
	if(theConf.traceflag & (1<<TIMED))
		pprintf("%sHour change Old %d New %d YDAY %d\n",TIEMPOT,oldHorag,horag,oldYearDay);


	for (int a=0;a<MAXDEVS;a++)
	{
		if(theConf.traceflag & (1<<TIMED))
			pprintf("%sHour change meter %d val %d day %d\n",TIEMPOT,a,theMeters[a].curHour,oldYearDay);
		if(xSemaphoreTake(framSem, portMAX_DELAY/ portTICK_PERIOD_MS))
		{
		//	fram.write_hour(a, oldYearDay,oldHorag, theMeters[a].curHour);//write old one before init new
		//	fram.write_hourraw(a,oldYearDay,oldHorag, theMeters[a].curHourRaw);//write old one before init new
			xSemaphoreGive(framSem);
		}
		theMeters[a].oldcurHour=theMeters[a].curHour;
		theMeters[a].curHour=0; //init it
		theMeters[a].curHourRaw=0;
	}

//	if(!(theConf.traceflag & (1<<SIMD)))

	oldHorag=horag;
}
*/
static void dayChange()
{
	if(theConf.traceflag & (1<<TIMED))
		pprintf("%sDay change Old %d New %d\n",TIEMPOT,oldDiag,diag);


	for (int a=0;a<MAXDEVS;a++)
	{
		if(theConf.traceflag & (1<<TIMED))
			pprintf("%sDay change mes %d day %d oldday %d\n",TIEMPOT,oldMesg,diag,oldDiag);
		if(framSem) //in case of damage, do not crash
			if(xSemaphoreTake(framSem, portMAX_DELAY/ portTICK_PERIOD_MS))
			{
				fram. write_day(a,oldYearDay, theMeters[a].curDay);
				fram. write_dayraw(a,oldYearDay,theMeters[a].curDayRaw);
				xSemaphoreGive(framSem);
				theMeters[a].curDay=0;
				theMeters[a].curDayRaw=0;
			}
	}
	oldDiag=diag;
	oldYearDay=yearDay;
}

static void monthChange()
{
	if(theConf.traceflag & (1<<TIMED))
		pprintf("%sMonth change Old %d New %d\n",TIEMPOT,oldMesg,mesg);

	for (int a=0;a<MAXDEVS;a++)
	{
		if(framSem)
			if(xSemaphoreTake(framSem, portMAX_DELAY/ portTICK_PERIOD_MS))
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

void timeKeeper(void *pArg)
{
	time_t now;
	struct tm timep;
#define QUE 1000 //used to test timer
//	time_t now;
//
//	time(&now);
//	int faltan=3600- (now % 3600)+2; //second to next hour +2 secs
//	if(theConf.traceflag & (1<<TIMED))
//		pprintf("%sSecs to Hour %d now %d\n",TIEMPOT,faltan,(u32)now);

//	delay(faltan*QUE);
	while(true)
	{
		delay(theConf.sendDelay);		//configuration delay
	//	delay(4000);
		time(&now);
		localtime_r(&now,&timep);
		mesg=timep.tm_mon;   			// Global Month
		diag=timep.tm_mday-1;    		// Global Day
		yearg=timep.tm_year+1900;   	// Global Year
		horag=timep.tm_hour;     		// Global Hour
		yearDay=timep.tm_yday;

		if(theConf.traceflag & (1<<TIMED))
			pprintf("%sHour change mes %d- %d day %d- %d hora %d- %d Min %d Sec %d dYear %d %s",TIEMPOT,mesg,oldMesg,diag,oldDiag,horag,oldHorag,
					timep.tm_min,timep.tm_sec,yearDay,ctime(&now));

		//if(horag==oldHorag && diag==oldDiag && mesg==oldMesg)
		//	return;
	//hours is a FACT that should change due to timer being fired every 1 hour

	//	if(horag!=oldHorag) // hour change up or down
	//		hourChange();

		if(diag!=oldDiag) // day change up or down. Also hour MUST HAVE CHANGED before
			dayChange();

		if(mesg!=oldMesg) // month change up or down. What to do with prev Year???? MONTH MUST HAVE CHANGED
			monthChange();

		sendStatusMeterAll();
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
	{
		pprintf("Failed create queue PCNT\n");
		system_failure((char*)"PCNT Q");
	}

	while(1)
	{
		res = xQueueReceive(pcnt_evt_queue, (void*)&evt,portMAX_DELAY / portTICK_PERIOD_MS);
		if (res == pdTRUE)
		{
			pcnt_get_counter_value((pcnt_unit_t)evt.unit,(short int *) &count);
			if(theConf.traceflag & (1<<INTD))
				pprintf("%sEvent PCNT unit[%d]; cnt: %d status %x\n",INTDT, evt.unit, count,evt.status);

			if (evt.status & PCNT_EVT_THRES_1)
			{
				pcnt_counter_clear((pcnt_unit_t)evt.unit);
				totalPulses+=count;						//counter of all pulses received
				theMeters[evt.unit].saveit=false;		//adding a new kwh flag
				theMeters[evt.unit].currentBeat+=count;	//beat
				theMeters[evt.unit].beatSave+=count;	//beats per kwh
				theMeters[evt.unit].beatSaveRaw+=count;	//raw without tariff discount
				if((theMeters[evt.unit].currentBeat % 10)!=0)
				{
					//force it to 10s. No idea how this happens but this is required else it will never have a Residuo of 0 and hence never inc kwh
					//shouldnt happen often I guess
					float theFloat=theMeters[evt.unit].currentBeat/10;
					int rounded=(int)round(theFloat)*10;
					theMeters[evt.unit].currentBeat=rounded;
					theFloat=theMeters[evt.unit].beatSave/10;
					rounded=(int)round(theFloat)*10;
					theMeters[evt.unit].beatSave=rounded;
				}
				if(diaHoraTarifa==0)
					diaHoraTarifa=100;					// should not happen but in case it does no crash

				if(theMeters[evt.unit].beatsPerkW>0)	// div by 0 protection
					residuo=theMeters[evt.unit].beatSave % (theMeters[evt.unit].beatsPerkW*diaHoraTarifa/100);	//0 is 1 kwh happend with tariffs
				else
					residuo=1;

				if(theConf.traceflag & (1<<INTD))
					pprintf("%sResiduo %d Beat %d MeterPos %d Time %d BPK %d\n",INTDT,residuo,theMeters[evt.unit].currentBeat,
							theMeters[evt.unit].pos,timeDiff,theMeters[evt.unit].beatsPerkW);

				if(residuo==0 && theMeters[evt.unit].currentBeat>0)
				{
					theMeters[evt.unit].saveit=true;	//add onw kwh
					theMeters[evt.unit].beatSave-=theMeters[evt.unit].beatsPerkW*diaHoraTarifa/100;
				}
				else
					theMeters[evt.unit].saveit=false;

				if(framQ)
				{
					theMeter.addit=theMeters[evt.unit].saveit;
					theMeter.whichMeter=evt.unit;
					xQueueSend( framQ,&theMeter,0 );	//dispatch it to the fram manager
				}
			}
        } else
            pprintf("PCNT Failed Queue\n");
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

static void pcnt_basic_init(uint16_t who, uint16_t th1)
{
	pcnt_config_t pcnt_config;

	memset((void*)&pcnt_config,0,sizeof(pcnt_config));

	pcnt_config.ctrl_gpio_num 	= -1;				//not using any control pin
	pcnt_config.channel 		= PCNT_CHANNEL_0;
	pcnt_config.pos_mode 		= PCNT_COUNT_INC;   // Count up on the positive edge
	pcnt_config.neg_mode 		= PCNT_COUNT_DIS;   // not used
	pcnt_config.lctrl_mode 		= PCNT_MODE_KEEP; 	// not used
	pcnt_config.hctrl_mode 		= PCNT_MODE_KEEP;	// not used

	pcnt_config.pulse_gpio_num	 = theMeters[who].pin;
	pcnt_config.unit 			= (pcnt_unit_t)who;
	pcnt_unit_config(&pcnt_config);

	pcnt_event_disable((pcnt_unit_t)who, PCNT_EVT_H_LIM);
	pcnt_event_disable((pcnt_unit_t)who, PCNT_EVT_L_LIM);
	pcnt_event_disable((pcnt_unit_t)who, PCNT_EVT_ZERO);

	pcnt_set_filter_value((pcnt_unit_t)who, 1000);
	pcnt_filter_enable((pcnt_unit_t)who);

	pcnt_set_event_value((pcnt_unit_t)who, PCNT_EVT_THRES_1, th1);
	pcnt_event_enable((pcnt_unit_t)who, PCNT_EVT_THRES_1);

	pcnt_counter_pause((pcnt_unit_t)who);
	pcnt_counter_clear((pcnt_unit_t)who);
	pcnt_intr_enable((pcnt_unit_t)who);
	pcnt_counter_resume((pcnt_unit_t)who);
}

static void pcnt_init(void)
{
    pcnt_isr_handle_t 	user_isr_handle = NULL; //user's ISR service handle

    //defines has the pins we are going to use for Inputs and Outputs (breakers)
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

    pcnt_isr_register(pcnt_intr_handler, NULL, 0,&user_isr_handle);
    workingDevs=0;

    for(int a=0;a<MAXDEVS;a++)
    	{
    		if(theConf.beatsPerKw[a]>0 && theConf.configured[a]==3)  //Only Authenticated PINs and BPK setting > 0
    		{
    			theMeters[a].pos=a;
    			pcnt_basic_init(a,10);	//grouped by 10 beats to save. Max loss is 10 beats in power failure
    			workingDevs++;			//keep count of PCNT used
    		}
    	}

#ifdef DISPLAY
    if (!workingDevs)
    	{
    		if(I2CSem)	//if we have Display
    			if(xSemaphoreTake(I2CSem, portMAX_DELAY))
				{
					drawString(64,32,"NO METERS",16,TEXT_ALIGN_CENTER,DISPLAYIT,NOREP);
					xSemaphoreGive(I2CSem);
				}
    	}
#endif

	if(theConf.traceflag & (1<<INTD))
		pprintf("%s%d Meters were activated\n",INTDT,workingDevs);
}

static void read_flash()
{
	esp_err_t q ;
	size_t largo;
	q = nvs_open("config", NVS_READONLY, &nvshandle);
	if(q!=ESP_OK)
	{
		pprintf("Error opening NVS Read File %x\n",q);
		return;
	}

	largo=sizeof(theConf);
		q=nvs_get_blob(nvshandle,"config",(void*)&theConf,&largo);

	if (q !=ESP_OK)
		pprintf("Error read %x largo %d aqui %d\n",q,largo,sizeof(theConf));

	nvs_close(nvshandle);
}

void write_to_flash() //save our configuration
{
	esp_err_t q ;
	q = nvs_open("config", NVS_READWRITE, &nvshandle);
	if(q!=ESP_OK)
	{
		pprintf("Error opening NVS File RW %x\n",q);
		return;
	}
	q=nvs_set_blob(nvshandle,"config",(void*)&theConf,sizeof(theConf));
	if (q ==ESP_OK)
		q = nvs_commit(nvshandle);
	nvs_close(nvshandle);
}

void connect_to_host(void *pArg)
{

	ip_addr_t 						remote;
	u8 								aca[4];
	tcpip_adapter_ip_info_t 		ip_info;
    struct timeval to;
	int addr_family;
	int ip_protocol,err;

	conn=false;

	gsock=-1;
	if(esp_wifi_connect()==ESP_OK)
	{
		if(theConf.traceflag & (1<<MSGD))
			pprintf("%sEstablish connect\n",MSGDT);
		EventBits_t uxBits=xEventGroupWaitBits(wifi_event_group, WIFI_BIT, false, true, 10000/  portTICK_RATE_MS); //wait for IP to be assigned
		if ((uxBits & WIFI_BIT)!=WIFI_BIT)
			vTaskDelete(NULL);


		ESP_ERROR_CHECK(tcpip_adapter_get_ip_info(TCPIP_ADAPTER_IF_STA, &ip_info));
		//    		pprintf("IP Address:  %s\n", ip4addr_ntoa(&ip_info.ip));
		//    		pprintf("Subnet mask: %s\n", ip4addr_ntoa(&ip_info.netmask));
		//    		pprintf("Gateway:     %s\n", ip4addr_ntoa(&ip_info.gw));
		memcpy(&aca,&ip_info.gw,4);
		IP_ADDR4( &remote, aca[0],aca[1],aca[2],aca[3]);

		struct sockaddr_in dest_addr;
		memcpy(&dest_addr.sin_addr.s_addr,&ip_info.gw,4);	//send to our gateway, which is our server
		dest_addr.sin_family = AF_INET;
		dest_addr.sin_port = htons(BDGHOSTPORT);
		addr_family = AF_INET;
		ip_protocol = IPPROTO_IP;
	 //   inet_ntoa_r(dest_addr.sin_addr, addr_str, sizeof(addr_str) - 1);

		gsock =  socket(addr_family, SOCK_STREAM, ip_protocol);
		if (gsock < 0) {
			pprintf( "Unable to create socket: errno %d\n", errno);
			vTaskDelete(NULL);

		}
		if(theConf.traceflag & (1<<MSGD))
			pprintf("%sSocket %d created, connecting to %s:%d\n",MSGDT,gsock, ip4addr_ntoa(&ip_info.gw), BDGHOSTPORT);

		err = connect(gsock, (struct sockaddr *)&dest_addr, sizeof(dest_addr));
		if (err != 0)
		{
			if(theConf.traceflag & (1<<MSGD))
				pprintf( "%sSocket unable to connect: errno %d\n",MSGDT, errno);
			vTaskDelete(NULL);
		}
		conn=true;
        gpio_set_level((gpio_num_t)WIFILED, 1);			//WIFILED on. We have a connection and a socket to host
        xEventGroupSetBits(wifi_event_group, LOGIN_BIT);

		vTaskDelete(NULL);
	}
    else
    {
		if(theConf.traceflag & (1<<MSGD))
			pprintf("%sCould not esp_connect\n",MSGDT);
		vTaskDelete(NULL);
    }
	vTaskDelete(NULL);
}

void wifi_event_handler(void* arg, esp_event_base_t event_base,
                                int32_t event_id, void* event_data)
{
    switch (event_id) {
    	case WIFI_EVENT_STA_START:
    	//	printf("start sta wifi\n");
    		xTaskCreate(&connect_to_host,"conn",4096,NULL, 4, NULL);
        break;
        case WIFI_EVENT_STA_CONNECTED:
    //		printf("start wifi conn\n");
            xEventGroupClearBits(wifi_event_group, WIFI_BIT);
            break; //wait for ip
        case WIFI_EVENT_STA_DISCONNECTED:
    		printf("start wifi disco\n");

            gpio_set_level((gpio_num_t)WIFILED, 0);
            xEventGroupClearBits(wifi_event_group, WIFI_BIT);
            if(gsock>0)
            {
            	close(gsock);
            	gsock=-1;
            }
			conn=false;
            xTaskCreate(&connect_to_host,"conn",4096,NULL, 4, NULL);
            break;
        default:
			if(theConf.traceflag & (1<<WIFID))
				pprintf("%sDefault Id %d\n",WIFIDT,event_id);
            break;
    }
    return;
}

static void ip_event_handler(void* arg, esp_event_base_t event_base,
                                int32_t event_id, void* event_data)
{
	 ip_event_got_ip_t *ev=(ip_event_got_ip_t*)event_data;

   switch (event_id) {
        case IP_EVENT_STA_GOT_IP:
        	pprintf("\nIP Assigned:" IPSTR, IP2STR(&ev->ip_info.ip));	//print it for testing purposes
            xEventGroupSetBits(wifi_event_group, WIFI_BIT);
            break;
        default:
            break;
    }
    return;
}

void write_to_fram(u8 meter,bool addit)
{
	time_t timeH;
	uint16_t mas;
	uint16_t theG;

    if(!framSem)
    {
		if(theConf.traceflag & (1<<FRAMD))
			pprintf("%sFram Is Not Valid\n",FRAMDT);
		return;
    }

    time(&timeH);

	if(addit)			// 1 kWh has occured for this Meter. Save it
	{
		theMeters[meter].curLife++;
		theMeters[meter].curMonth++;
		theMeters[meter].curDay++;
		theMeters[meter].curHour++;
		if(theConf.traceflag & (1<<FRAMD))
					pprintf("W_T_FRAM Meter %d Year %d Month %d Day %d Hour %d val=%d\n",meter,yearg,mesg,diag,horag,theMeters[meter].curHour);
		if(theMeters[meter].beatSaveRaw>=theMeters[meter].beatsPerkW)
		{
			mas=theMeters[meter].beatSaveRaw / theMeters[meter].beatsPerkW;
			theMeters[meter].curMonthRaw+=mas;
			theMeters[meter].curDayRaw+=mas;
			theMeters[meter].curHourRaw+=mas;
			theMeters[meter].beatSaveRaw=theMeters[meter].beatSaveRaw % theMeters[meter].beatsPerkW; //not 0 else we lose beats that happend that we not recorded on time
		}
		time((time_t*)&theMeters[meter].lastKwHDate); //last time we saved data

		fram.write_beat(meter,theMeters[meter].currentBeat);
		fram.write_lifekwh(meter,theMeters[meter].curLife);
		fram.write_lifedate(meter,theMeters[meter].lastKwHDate);
		fram.write_month(meter,mesg,theMeters[meter].curMonth);
		fram.write_monthraw(meter,mesg,theMeters[meter].curMonthRaw);
		fram.write_day(meter,oldYearDay,theMeters[meter].curDay);
		fram.write_dayraw(meter,oldYearDay,theMeters[meter].curDayRaw);
	//	fram.write_hour(meter,oldYearDay,horag,theMeters[meter].curHour);
	//	fram.write_hourraw(meter,oldYearDay,horag,theMeters[meter].curHourRaw);

	}
	else	//save the beats and last time for recovery TIME in case we have no ConnMgr. Also test FRAM HW Guard
	{
		fram.read_guard((uint8_t*)&theG);
		if(theG!=theGuard)
		{
			nofram++;
			if(nofram>10)
			{
				pprintf("Fram is lost need %x got %x\n",theGuard,theG);
				nofram=0;
			}
			theGuard=esp_random();
			fram.write_guard(theGuard);				// theguard is dynamic and will change every 10 minutes
			fram.read_guard((uint8_t*)&theG);
			if(theG !=theGuard)
				framGuard=true;
		}
		else
		{
			framGuard=false;
			nofram=0;
		}
		if(millis()-startGuard>60000)// every minute change guard
		{
			theGuard=esp_random();
		//	gpio_set_level((gpio_num_t)TRIGGER, 0);
			fram.write_guard(theGuard);				// theguard is dynamic and will change every 60000ms
	//		gpio_set_level((gpio_num_t)TRIGGER, 1);

			startGuard=millis();

		}
		if(theConf.traceflag & (1<<FRAMD))
			pprintf("Beat[%d]=%d\n",meter,theMeters[meter].currentBeat);

		fram.write_beat(meter,theMeters[meter].currentBeat);
		fram.writeMany(FRAMDATE,(uint8_t*)&timeH,sizeof(timeH));//last known date
	}
}

void load_from_fram(u8 meter)
{
    if(!framSem)
    {
		if(theConf.traceflag & (1<<FRAMD))
			pprintf("%sFram Is Not Valid\n",FRAMDT);
		return;
    }

	if(xSemaphoreTake(framSem, portMAX_DELAY/  portTICK_RATE_MS))
	{
		fram.read_beat(meter,(u8*)&theMeters[meter].currentBeat);
		fram.read_lifekwh(meter,(u8*)&theMeters[meter].curLife);
		fram.read_lifedate(meter,(u8*)&theMeters[meter].lastKwHDate);
		fram.read_month(meter, mesg, (u8*)&theMeters[meter].curMonth);
		fram.read_monthraw(meter, mesg, (u8*)&theMeters[meter].curMonthRaw);
		fram.read_day(meter, oldYearDay, (u8*)&theMeters[meter].curDay);
		fram.read_dayraw(meter, oldYearDay, (u8*)&theMeters[meter].curDayRaw);
	//	fram.read_hour(meter, oldYearDay, horag, (u8*)&theMeters[meter].curHour);
	//	fram.read_hourraw(meter, oldYearDay, horag, (u8*)&theMeters[meter].curHourRaw);
		totalPulses+=theMeters[meter].currentBeat;
		if(theConf.beatsPerKw[meter]==0)
			theConf.beatsPerKw[meter]=800;// just in case div by 0 crash
		theMeters[meter].beatsPerkW=theConf.beatsPerKw[meter];// just in case div by 0 crash
		u16 nada=theMeters[meter].currentBeat/theConf.beatsPerKw[meter];
		theMeters[meter].beatSave=theMeters[meter].currentBeat-(nada*theConf.beatsPerKw[meter]);
		theMeters[meter].beatSaveRaw=theMeters[meter].beatSave;
		xSemaphoreGive(framSem);

		oldCurLife[meter]=0;//for display
		oldCurLife[meter]=0;

		if(theConf.traceflag & (1<<FRAMD))
			pprintf("[FRAMD]Loaded Meter %d curLife %d beat %d\n",meter,theMeters[meter].curLife,theMeters[meter].currentBeat);
	}
}

static void init_fram( bool load)
{
	// FRAM Setup. Will initialize the Semaphore. If NO FRAM, NO METER so it should really stop but for sakes of testing lets go on...
	theGuard = esp_random();
	framGuard=true;		//assume no FRAM, guard activated=true

	spi_flash_init();

	framFlag=fram.begin(FMOSI,FMISO,FCLK,FCS,&framSem); //will create SPI channel and Semaphore
	if(framFlag)
	{
		framGuard=false;						//fram is not dead
		//load all devices counters from FRAM
		startGuard=millis();
		fram.write_guard(theGuard);				// theguard is dynamic and will change every boot.
		if(load)
			for (int a=0;a<MAXDEVS;a++)
			{
				if(theConf.traceflag & (1<<FRAMD))
					pprintf("%sLoading %d\n",FRAMDT,a);
				load_from_fram(a);
			}
	}
	else
		framSem=NULL;
}

static void wifi_init(void)
{
	tcpip_adapter_ip_info_t 		ipInfo;

	if(theConf.traceflag & (1<<WIFID))
		pprintf("%sWiFi Mode %s\n",WIFIDT,theConf.active?"RunConf":"SetupConf");

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
	    {
	    	esp_netif_create_default_wifi_sta();
	    	esp_netif_create_default_wifi_ap();
	    }

	    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler, NULL));
	    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &ip_event_handler, NULL));

	    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
	    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
	    ESP_ERROR_CHECK(esp_wifi_set_storage(WIFI_STORAGE_RAM));

	    wifi_config_t wifi_config;
	    memset(&wifi_config,0,sizeof(wifi_config));//very important
		if(theConf.active)
	    {
			if(strlen(theConf.mgrName)>0)
			{
				strcpy((char*)wifi_config.sta.ssid,theConf.mgrName);
				strcpy((char*)wifi_config.sta.password,theConf.mgrPass);
				ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
				ESP_ERROR_CHECK(esp_wifi_set_config(ESP_IF_WIFI_STA, &wifi_config));
				esp_wifi_start();
			}
			else
			{
				pprintf("No Mgr defined\n");
				theConf.active=false;
				write_to_flash();
				system_failure((char*)"NOWIFI");	//stop. Tell user need to configure MetrController by resetting and settin up
			}
	    }
		else
	    //AP section, we are in SetupMode
	    {
			sprintf(tempb,"Meter%02x%02x",them[4],them[5]);
			strcpy((char*)wifi_config.ap.ssid,tempb);
			strcpy((char*)wifi_config.ap.password,tempb);
			wifi_config.ap.authmode=WIFI_AUTH_WPA_PSK;
			wifi_config.ap.ssid_hidden=false;
			wifi_config.ap.beacon_interval=400;
			wifi_config.ap.max_connection=1;
			wifi_config.ap.ssid_len=0;
			wifi_config.ap.channel=1;
			ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_APSTA));
			ESP_ERROR_CHECK(esp_wifi_set_config(ESP_IF_WIFI_AP, &wifi_config));
			esp_wifi_start();
	    }
}

static void updateDateTime(loginT loginData)
{
    struct tm timeinfo;

	if(theConf.traceflag & (1<<FRMCMD))
		pprintf("%s Login Date %d Tariff %d\n",FRMCMDT,(int)loginData.thedate,loginData.theTariff);

	localtime_r(&loginData.thedate, &timeinfo);
	diaHoraTarifa=loginData.theTariff;			// Host will give us Hourly Tariff. No need to store

	if(theConf.traceflag & (1<<FRMCMD))
		pprintf("%sYear %d Month %d Day %d Hour %d\n",FRMCMDT,timeinfo.tm_year+1900,timeinfo.tm_mon,timeinfo.tm_mday-1,timeinfo.tm_hour);

	mesg=timeinfo.tm_mon;
	diag=timeinfo.tm_mday-1;
	yearg=timeinfo.tm_year+1900;
	horag=timeinfo.tm_hour;
	yearDay=timeinfo.tm_yday;

	struct timeval now = { .tv_sec = loginData.thedate, .tv_usec=0};
	settimeofday(&now, NULL);

	// save our last received time in our fram for time Emergency case
	if(framSem)
		if(xSemaphoreTake(framSem, portMAX_DELAY/  portTICK_RATE_MS))
		{
			fram.writeMany(FRAMDATE,(uint8_t*)&loginData.thedate,sizeof(loginData.thedate));//last known date
			xSemaphoreGive(framSem);
		}
}

static cJSON *makeCmdcJSON(meterType *meter,bool ans)
{
	cJSON *cmdJ=cJSON_CreateObject();

	cJSON_AddStringToObject(cmdJ,"cmd",				"/ga_status");
	cJSON_AddStringToObject(cmdJ,"mid",				theConf.medidor_id[meter->pos]);
	cJSON_AddNumberToObject(cmdJ,"Ts",				++theMeters[meter->pos].vanMqtt);// when affecting theMeter this pointer is to a COPY so get its pos and update directly
	cJSON_AddNumberToObject(cmdJ,"KwH",				meter->curLife);
	cJSON_AddNumberToObject(cmdJ,"Beats",			meter->currentBeat);
	cJSON_AddNumberToObject(cmdJ,"Pos",				meter->pos);
	cJSON_AddBoolToObject(cmdJ,"reply",				ans);

	return cmdJ;
}

static cJSON * makeGroupCmdAll()
{
	bool ans;
	int van;

	/////// Create Message for Grouped Cmds ///////////////////
	van=0;
	cJSON *root=cJSON_CreateObject();
	if(root==NULL)
	{
		pprintf("cannot create root\n");
		return NULL;
	}

	cJSON *ar = cJSON_CreateArray();
	for (int a=0;a<MAXDEVS;a++)
	{
		if(theConf.configured[a]==3)
		{
			van++;
			if(van==workingDevs)		//when sending group just answer to the last one
				ans=true;
			else
				ans=false;
			cJSON *cmdInt=makeCmdcJSON(&theMeters[a],ans);
			cJSON_AddItemToArray(ar, cmdInt);
		}
	}
	double dmac=(double)theMacNum;

	cJSON_AddItemToObject		(root,"Batch",ar);
	cJSON_AddStringToObject		(root,"Controller",theConf.meterName);
	cJSON_AddNumberToObject		(root,"macn",dmac);
	cJSON_AddBoolToObject		(root,"fram",framGuard);

	return root;
}

int sendMsg(uint8_t *lmessage, uint16_t son,uint8_t *donde,uint8_t maxx)
{
	ip_addr_t 						remote;
	u8 								aca[4];
	tcpip_adapter_ip_info_t 		ip_info;
    struct timeval to;
	int addr_family;
	int ip_protocol,err;


	if(theConf.traceflag & (1<<MSGD))
		pprintf("%sSending queue size %d\n",MSGDT,son);

	// err = send(gsock, (char*)lmessage, strlen((char*)lmessage), 0);
	 err = send(gsock, (char*)lmessage, son, 0);
	 if (err < 0)
	 {
		if(theConf.traceflag & (1<<MSGD))
			pprintf( "%sSock %d Error occurred during sending: errno %d\n",MSGDT,gsock, errno);
	//	close(gsock);
	//	conn=false;
		return -1;
	}

	 if(maxx>0)		//wait for reply
	 {
		to.tv_sec = 2;
		to.tv_usec = 0;

		if (setsockopt(gsock,SOL_SOCKET,SO_RCVTIMEO,&to,sizeof(to)) < 0)

		{
			if(theConf.traceflag & (1<<MSGD))
				pprintf("Unable to set read timeout on socket!\n");
			return -1;
		}

		int len = recv(gsock, donde, maxx, 0);
		int len1=len;

		if(theConf.traceflag & (1<<MSGD))
			pprintf("%sSendmsg response size %d\n",MSGDT,len1);
		return len1;
	 }
	return 0;
}


static void logIn()
{
    loginT loginData;
    size_t ret;
	size_t olen = 0;
	char * buf=malloc(1024);
	char * b64=malloc(1000);

	memset( iv, 0, sizeof( iv ) );
	memset( key, 65, sizeof( key ) );

	esp_aes_init( &ctx );
	esp_aes_setkey( &ctx, key, 256 );		//send first Login with known secret AES


	mbedtls_entropy_context entropy;
	extern const unsigned char cacert_pem_start[] asm("_binary_public_pem_start");
	extern const unsigned char cacert_pem_end[]   asm("_binary_public_pem_end");
	int len = cacert_pem_end - cacert_pem_start;

	mbedtls_ctr_drbg_init(&ctr_drbg);
	mbedtls_entropy_init(&entropy);
	if((ret=mbedtls_ctr_drbg_seed(&ctr_drbg,mbedtls_entropy_func,&entropy,NULL,0))!=0)
	{
	printf("failed seed %x\n",ret);
	return false;
	}

	char *nkey=malloc(32);
	for (int a=0;a<32;a++)
		nkey[a]=esp_random() %256;
	memset(nkey,66,32);//test should change to BBBBBBBB

	mbedtls_pk_init( &pk );

	if( ( ret = mbedtls_pk_parse_public_key( &pk,cacert_pem_start,len) ) != 0 )
	printf( " failed\n  ! mbedtls_pk_parse_public_keyfile returned -0x%04x\n", -ret );
	else
	{
	//	printf("Public Ok\n");

	/*
	 * Calculate the RSA encryption of the data.
	 */
	if( ( ret = mbedtls_pk_encrypt( &pk, nkey, sizeof(key),
									buf, &olen, 1024,
									mbedtls_ctr_drbg_random, &ctr_drbg ) ) != 0 )
	{
		printf( " failed\n  ! mbedtls_pk_encrypt returned -0x%04x\n", -ret );
	}
	//		else
	//			printf("Encrypted len %d\n",olen);
	}
	size_t blen;
	ret=mbedtls_base64_encode(b64,1000,&blen,buf,olen);
	//	if(ret==0)
	//		printf("Base64[%d] %s\n",blen,b64);



	setenv("TZ", LOCALTIME, 1);
	tzset();

	cJSON *root=cJSON_CreateObject();
	cJSON *cmdJ=cJSON_CreateObject();
	cJSON *ar = cJSON_CreateArray();

	if(root==NULL)
	{
		pprintf("cannot create root\n");
		return;
	}

	cJSON_AddStringToObject(cmdJ,"password","zipo");
	cJSON_AddStringToObject(cmdJ,"cmd","/ga_login");
	cJSON_AddStringToObject(cmdJ,"key",b64);

	cJSON_AddItemToArray(ar, cmdJ);
	cJSON_AddItemToObject(root, "Batch",ar);
	double dmac=(double)theMacNum;
	cJSON_AddNumberToObject(root,"macn",dmac);
	char *logMsg=cJSON_Print(root);
	int theSize=strlen(logMsg);

	int rem= theSize % 16;
	theSize+=16-rem;			//round to next 16 for AES

	char * donde=(char*)malloc(theSize);
	if (!donde)
		{
		printf("No memory copy message\n");
		return;
		}
	bzero(donde,theSize);

	memcpy(donde,logMsg,strlen(logMsg));

	char * output=(char*)malloc(theSize);
	if (!output)
		{
		printf("No memory ouput message\n");
		return;
		}
	bzero(output,theSize);

	bzero(iv,sizeof(iv));
	bzero(output,theSize);
//	printf("Login bkey %s\n",key);
	esp_aes_crypt_cbc( &ctx, ESP_AES_ENCRYPT, theSize, iv, donde, output );


	memset((void*)&loginData,0,sizeof(loginData));
	char *tmp=(char*)malloc(100);
//	int fueron=sendMsg((uint8_t*)logMsg,strlen(logMsg),(uint8_t*)tmp, 100);
	int fueron=sendMsg((uint8_t*)output,theSize,(uint8_t*)tmp, 100);
	if(fueron>0)
		memcpy((void*)&loginData,tmp,sizeof(loginData));
	free(tmp);

	memcpy(key,nkey,32);
	esp_aes_setkey( &ctx, key, 256 );	//now definite
////	printf("After Key %s\n",key);
	free(nkey);

	updateDateTime(loginData);
	oldMesg=mesg;
	oldDiag=diag;
	oldHorag=horag;
	oldYearDay=yearDay;

	if(theConf.traceflag & (1<<CMDD))
		pprintf("%sLogin year %d month %d day %d hour %d Tariff %d YDAY %d\n",CMDDT,yearg,mesg,diag,horag,loginData.theTariff,oldYearDay);

	free(logMsg);
	cJSON_Delete(root);
	if(donde)
	{
		free(donde);
		donde=NULL;
	}
	if(output)
	{
		free(output);
		output=NULL;
	}
}

void dump(char * title,char *desde, int son)
{
	printf("%s  %p\n",title,desde);
	for (int a=0;a<son;a++)
		printf("%02x",desde[a]);
	printf("\n");
}

void sendStatusMeterAll()
{
    loginT 	loginData;
    int sendStatus;
    char *lmessage=NULL;

	if(theConf.traceflag & (1<<CMDD))
	{
		pprintf("%sSendM %d Heap %d wait %d... ",CMDDT,++sentTotal,esp_get_free_heap_size(),uxQueueMessagesWaiting(mqttR));
		fflush(stdout);
	}
	cJSON *root=makeGroupCmdAll();
	if(root)
		lmessage=cJSON_Print(root);

	if(!lmessage)
	{
		printf("No message\n");
		return -1;
	}

	bzero(&loginData,sizeof(loginData));
	bzero(iv,sizeof(iv));

	int theSize=strlen(lmessage);
	int rem= theSize % 16;
	theSize+=16-rem;			//round to next 16 for AES

	char * donde=(char*)malloc(theSize);
	if (!donde)
		{
		printf("No memory copy message\n");
		return;
		}
	bzero(donde,theSize);

	memcpy(donde,lmessage,strlen(lmessage));
	char * output=(char*)malloc(theSize);
	if (!output)
		{
		printf("No memory ouput message\n");
		return;
		}
	bzero(output,theSize);

	esp_aes_crypt_cbc( &ctx, ESP_AES_ENCRYPT, theSize, iv, donde, output );

	//receive buffer greater than expected since we are sending 5 status messages and will receive 5 answers.
	// if we do not do this the sendmsg socket will SAVE the other answers for next call and never gets the logindate correct
	void *ans=malloc(100);
	if(!ans)
	{
		printf("No ram for Ans\n");
		return;
	}
//	sendStatus=sendMsg((uint8_t*)lmessage,strlen(lmessage),(uint8_t*)ans,100);
	sendStatus=sendMsg((uint8_t*)output,theSize,(uint8_t*)ans,100);
	if(sendStatus>=0)
	{
		sendErrors=0;		//restart counter on sucess
		memcpy((void*)&loginData,ans,sizeof(loginData));
		updateDateTime(loginData);
	}
	else
	{
		//keep count of missed messages. If too many, restart
		sendErrors++;
		if(sendErrors>MAXSENDERR)
		{
			if(conn)
			{
				pprintf("Errors exceeded. Restarting\n");
				delay(1000);
				esp_restart(); // hopefully it will connect
			}
		}
	}
//	printf("After aes %d\n",esp_get_free_heap_size());

	if(lmessage)
	{
		free(lmessage);
		lmessage=NULL;
	}
	if(ans)
	{
		free(ans);
		ans=NULL;
	}
	if(donde)
	{
		free(donde);
		donde=NULL;
	}
	if(output)
	{
		free(output);
		output=NULL;
	}
	if(root)
		cJSON_Delete(root);

	if(theConf.traceflag & (1<<CMDD))
	{
		pprintf("%sSendMeterFram dates year %d month %d day %d hour %d Tariff %d\n",CMDDT,yearg,mesg,diag,horag,loginData.theTariff);
		pprintf("%sTar %d sent Heap %d\n",CMDDT,loginData.theTariff,esp_get_free_heap_size());
	}
}

static void erase_config() //do the dirty work
{
	memset(&theConf,0,sizeof(theConf));
	theConf.centinel=CENTINEL;
	theConf.beatsPerKw[0]=800;
	write_to_flash();
	pprintf("Centinel %x\n",theConf.centinel);
}

static void framManager(void * pArg)
{
	framMeterType theMeter;

	while(true)
	{
		if( xQueueReceive( framQ, &theMeter, portMAX_DELAY/  portTICK_RATE_MS ))
		{
			if(framSem)
			{
				if(xSemaphoreTake(framSem, portMAX_DELAY/  portTICK_RATE_MS))
				{
					write_to_fram(theMeter.whichMeter,theMeter.addit);
					if(theConf.traceflag & (1<<FRMCMD))
						pprintf("%sSaving Meter %d add %d Beats %d\n",FRMCMDT,theMeter.whichMeter,theMeter.addit,theMeters[theMeter.whichMeter].currentBeat);
					xSemaphoreGive(framSem);
				}
			}
		}
		else
		{
			if(theConf.traceflag & (1<<FRAMD))
				pprintf("%sFailed framQ Manager\n",FRAMDT);
			delay(1000);
		}
	}
}

#ifdef DISPLAY
static void initI2C()
{
	i2cp.sdaport=(gpio_num_t)SDAW;
	i2cp.sclport=(gpio_num_t)SCLW;
	i2cp.i2cport=I2C_NUM_0;
	miI2C.init(i2cp.i2cport,i2cp.sdaport,i2cp.sclport,400000,&I2CSem);//Will reserve a Semaphore for Control
}

static void initScreen()
{
	if(xSemaphoreTake(I2CSem, portMAX_DELAY/ portTICK_PERIOD_MS))
	{
		display.init();
		display.flipScreenVertically();
		display.clear();
		drawString(64,8,"MeterIoT",24,TEXT_ALIGN_CENTER,DISPLAYIT,NOREP);
		xSemaphoreGive(I2CSem);
	}
	else
		pprintf("Failed to InitScreen\n");
}
#endif

bool decryptLogin(char* b64, uint16_t blen, char *decryp, size_t * fueron)
{
	mbedtls_entropy_context entropy;
	int ret;
	size_t ilen=0;

	char *encryp=malloc(1024);
	ret=mbedtls_base64_decode(encryp,1024,&ilen,b64,blen);
	if(ret!=0)
	{
		printf("Failed b64 %d\n",ret);
		return false;
	}

	extern const unsigned char prvtkey_pem_start[] asm("_binary_prvtkey_pem_start");
	extern const unsigned char prvtkey_pem_end[] asm("_binary_prvtkey_pem_end");
	int len = prvtkey_pem_end - prvtkey_pem_start;

	mbedtls_ctr_drbg_init(&ctr_drbg);
	mbedtls_entropy_init(&entropy);
	if((ret=mbedtls_ctr_drbg_seed(&ctr_drbg,mbedtls_entropy_func,&entropy,NULL,0))!=0)
	{
		printf("failed seed %x\n",ret);
		return false;
	}

	mbedtls_pk_init( &pk );

	if( ( ret = mbedtls_pk_parse_key( &pk, prvtkey_pem_start,len,NULL,0)) != 0 )
	{
		    printf( " failed\n  ! mbedtls_pk_parse_private_keyfile returned -0x%04x\n", -ret );
		    return false;
	}
	else
	{
	//	printf("Private Ok\n");
		size_t aca;
		if( ( ret = mbedtls_pk_decrypt( &pk, encryp, ilen,decryp, &aca,1024, mbedtls_ctr_drbg_random, &ctr_drbg ) ) != 0 )
		{
		    printf( " failed  mbedtls_pk_decrypt returned -0x%04x\n", -ret );
		    return false;

		}
		else
		{
		//	printf("Decrypted[%d] >%s<\n",aca,decryp);
			*fueron=aca;
		}
		return true;
	}
}
static void init_vars()
{
	char * buf=malloc(1024);
	char *result=malloc(1024);
//	char to_encrypt[]="0123456789ABCDEFABCDEFGHIJKLMNOP";


	size_t olen = 0;

	qwait=QDELAY;
	qdelay=qwait*1000;
	sendTcp=waitQueue=500;
	wDelay=TIMEWAITPCNT;
	wifiSem= xSemaphoreCreateBinary();
	xSemaphoreGive(wifiSem);
	diaHoraTarifa=100;// div by zero if not and not loaded
	strcpy(TAG , "BDGCLIENT");
	WIFI_BIT = BIT0;
	LOGIN_BIT= BIT1;

	waitQueue=500;
	sendErrors=0;

	if(theConf.sendDelay==0)
		theConf.sendDelay=60000;

	gpio_config_t io_conf;

	io_conf.intr_type =			GPIO_INTR_DISABLE;
	io_conf.mode =				GPIO_MODE_OUTPUT;
	io_conf.pull_down_en =		GPIO_PULLDOWN_DISABLE;
	io_conf.pull_up_en =		GPIO_PULLUP_DISABLE;
	io_conf.pin_bit_mask = 		(1ULL<<WIFILED);
	gpio_config(&io_conf);
	io_conf.pull_up_en =		GPIO_PULLUP_ENABLE;
	io_conf.pin_bit_mask = 		(1ULL<<TRIGGER);
	gpio_config(&io_conf);

	memset((void*)&theMeters,0,sizeof(theMeters));

	daysInMonth [0] =31;
	daysInMonth [1] =28;
	daysInMonth [2] =31;
	daysInMonth [3] =30;
	daysInMonth [4] =31;
	daysInMonth [5] =30;
	daysInMonth [6] =31;
	daysInMonth [7] =31;
	daysInMonth [8] =30;
	daysInMonth [9] =31;
	daysInMonth [10] =30;
	daysInMonth [11] =31;

	mqttQ = xQueueCreate( 20, sizeof( meterType ) );
	if(!mqttQ)
	pprintf("Failed queue Tx\n");

	mqttR = xQueueCreate( 20, sizeof( meterType ) );
	if(!mqttR)
	pprintf("Failed queue Rx\n");

	framQ = xQueueCreate( 20, sizeof( framMeterType ) );
	if(!framQ)
	pprintf("Failed queue Fram\n");

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
	strcpy(lookuptable[14],"SIMD");

	string debugs;

	// add - sign to Keys
	for (int a=0;a<NKEYS/2;a++)
	{
		debugs="-"+string(lookuptable[a]);
		strcpy(lookuptable[a+NKEYS/2],debugs.c_str());
	}

	memset( iv, 0, sizeof( iv ) );
	memset( key, 65, sizeof( key ) );

	esp_aes_init( &ctx );
	esp_aes_setkey( &ctx, key, 256 );


	mbedtls_entropy_context entropy;
	int ret;
	extern const unsigned char cacert_pem_start[] asm("_binary_public_pem_start");
	extern const unsigned char cacert_pem_end[]   asm("_binary_public_pem_end");
	int len = cacert_pem_end - cacert_pem_start;

	mbedtls_ctr_drbg_init(&ctr_drbg);
	mbedtls_entropy_init(&entropy);
	if((ret=mbedtls_ctr_drbg_seed(&ctr_drbg,mbedtls_entropy_func,&entropy,NULL,0))!=0)
	{
		printf("failed seed %x\n",ret);
		return false;
	}

	mbedtls_pk_init( &pk );

	if( ( ret = mbedtls_pk_parse_public_key( &pk,cacert_pem_start,len) ) != 0 )
	    printf( " failed\n  ! mbedtls_pk_parse_public_keyfile returned -0x%04x\n", -ret );
	else
	{
		printf("Public Ok\n");

		/*
		 * Calculate the RSA encryption of the data.
		 */
//		if( ( ret = mbedtls_pk_encrypt( &pk, to_encrypt, sizeof(to_encrypt),
//		                                buf, &olen, 1024,
//		                                mbedtls_ctr_drbg_random, &ctr_drbg ) ) != 0 )
//		{
//		    printf( " failed\n  ! mbedtls_pk_encrypt returned -0x%04x\n", -ret );
//		}
//		else
//			printf("Encrypted len %d\n",olen);
	}
//	size_t fueron;
//	size_t blen;
//	char *b64=malloc(1000);
//	ret=mbedtls_base64_encode(b64,1000,&blen,buf,olen);
////	if(ret==0)
////		printf("Base64[%d] %s\n",blen,b64);
//
////	if(decryptLogin(buf,olen,result,&fueron))
//		if(decryptLogin(b64,blen,result,&fueron))
//		printf("Decrypted[%d] [%s]\n",fueron,result);
//	else
//		printf("Failed to decrypt\n");
	free(buf);
	free(result);
}

static void check_boot_options()
{
	char them[6];

	esp_efuse_mac_get_default((unsigned char*)them);
	memcpy(&theMacNum,&them[2],4);
	sprintf(theMac,"%02x:%02x:%02x:%02x:%02x:%02x",them[0],them[1],them[2],them[3],them[4],them[5]);

	if(theConf.traceflag & (1<<BOOTD))
	    pprintf("%s Manufacturer %04x Fram Id %04x Fram Size %d%s\n",MAGENTA,fram.manufID,fram.prodId,fram.intframWords,RESETC);

	if((theConf.traceflag & (1<<BOOTD)) )
	{
		pprintf("%s=============== FRAM ===============%s\n",RED,YELLOW);
		pprintf("FRAMDATE(%s%d%s)=%s%d%s\n",GREEN,FRAMDATE,YELLOW,CYAN,METERVER-FRAMDATE,RESETC);
		pprintf("METERVER(%s%d%s)=%s%d%s\n",GREEN,METERVER,YELLOW,CYAN,GUARDM-METERVER,RESETC);
		pprintf("GUARD(%s%d%s)=%s%d%s\n",GREEN,GUARDM,YELLOW,CYAN,SCRATCH-GUARDM,RESETC);
		pprintf("SCRATCH(%s%d%s)=%s%d%s\n",GREEN,SCRATCH,YELLOW,CYAN,SCRATCHEND-SCRATCH,RESETC);
		pprintf("SCRATCHEND(%s%d%s)=%s%d%s\n",GREEN,SCRATCHEND,YELLOW,CYAN,TARIFADIA-SCRATCHEND,RESETC);
		pprintf("TARIFADIA(%s%d%s)=%s%d%s\n",GREEN,TARIFADIA,YELLOW,CYAN,FINTARIFA-TARIFADIA,RESETC);
		pprintf("FINTARIFA(%s%d%s)=%s%d%s\n",GREEN,FINTARIFA,YELLOW,CYAN,BEATSTART-FINTARIFA,RESETC);
		pprintf("BEATSTART(%s%d%s)=%s%d%s\n",GREEN,BEATSTART,YELLOW,CYAN,LIFEKWH-BEATSTART,RESETC);
		pprintf("LIFEKWH(%s%d%s)=%s%d%s\n",GREEN,LIFEKWH,YELLOW,CYAN,LIFEDATE-LIFEKWH,RESETC);
		pprintf("LIFEDATE(%s%d%s)=%s%d%s\n",GREEN,LIFEDATE,YELLOW,CYAN,MONTHSTART-LIFEDATE,RESETC);
		pprintf("MONTHSTART(%s%d%s)=%s%d%s\n",GREEN,MONTHSTART,YELLOW,CYAN,MONTHRAW-MONTHSTART,RESETC);
		pprintf("MONTHRAW(%s%d%s)=%s%d%s\n",GREEN,MONTHRAW,YELLOW,CYAN,DAYSTART-MONTHRAW,RESETC);
		pprintf("DAYSTART(%s%d%s)=%s%d%s\n",GREEN,DAYSTART,YELLOW,CYAN,DAYRAW-DAYSTART,RESETC);
		pprintf("DAYRAW(%s%d%s)=%s%d%s\n",GREEN,DAYRAW,YELLOW,CYAN,HOURSTART-DAYRAW,RESETC);
		pprintf("HOURSTART(%s%d%s)=%s%d%s\n",GREEN,HOURSTART,YELLOW,CYAN,HOURRAW-HOURSTART,RESETC);
		pprintf("HOURRAW(%s%d%s)=%s%d%s\n",GREEN,HOURRAW,YELLOW,CYAN,DATAEND-HOURRAW,RESETC);
		pprintf("DATAEND(%s%d%s)=%s%d%s\n",GREEN,DATAEND,YELLOW,CYAN,TOTALFRAM-DATAEND,RESETC);
		pprintf("TOTALFRAM(%s%d%s)\n",GREEN,TOTALFRAM,RESETC);
		pprintf("%s=============== FRAM ===============%s\n",RED,RESETC);
	}
}

void app_main()
{

	printSem= xSemaphoreCreateBinary();
	xSemaphoreGive(printSem);

    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES)
    {
		pprintf("No free pages erased!!!!\n");
		ESP_ERROR_CHECK(nvs_flash_erase());
		err = nvs_flash_init();
    }

	read_flash();				//our Configuration structure

    esp_log_level_set("*",(esp_log_level_t) theConf.logLevel);

	if(theConf.traceflag & (1<<BOOTD))
	{
		pprintf("MeterIoT ConnManager\n");
		pprintf("%sBuildMgr starting up\n",BOOTDT);
		pprintf("%sFree memory: %d bytes\n", BOOTDT,esp_get_free_heap_size());
		pprintf("%sIDF version: %s\n", BOOTDT,esp_get_idf_version());
	}

	delay(3000);//for erasing

    if (theConf.centinel!=CENTINEL || !gpio_get_level((gpio_num_t)0))
	{
		pprintf("Read centinel %x not valid. Erasing Config\n",theConf.centinel);
		erase_config();
	}

    init_vars();				// load initial vars
    wifi_init(); 				// start the wifi
    init_fram(false);			// start the Fram Driver and load our Meters
    check_boot_options();		// see if we need to display boot stuff

#ifdef DISPLAY
    initI2C();
    initScreen();
#endif

#ifdef TEST
	xTaskCreate(&kbd,"kbd",4096,NULL, 4, NULL);	//debuging only
#endif

	if(theConf.active)
	{
		//load from FRAM AFTER we have the date
		for (int a=0;a<MAXDEVS;a++)
			load_from_fram(a);

		xTaskCreate(&pcntManager,"pcntMgr",8192,NULL, 4, NULL);		// start the Pulse Manager task
		pcnt_init();												// start receiving pulses
		xTaskCreate(&framManager,"fmg",8192,NULL, 10, NULL);		// in charge of saving meter activity to Fram
		xTaskCreate(&timeKeeper,"tmK",4096,NULL, 10, &timeHandle);	// Due to Tariffs, we need to check hour,day and month changes
	}
	else
		xTaskCreate(&start_webserver,"web",10240,(void*)1, 4, &webHandle);// Messages from the Meters. Controller Section socket manager

#ifdef DISPLAY
		xTaskCreate(&displayManager,"8192",4096,(void*)1, 4, &webHandle);// Messages from the Meters. Controller Section socket manager
#endif

		theConf.lastResetCode=rtc_get_reset_reason(0);
		time((time_t*)&theConf.lastBootDate);
		theConf.bootcount++;
		write_to_flash();

		EventBits_t uxBits=xEventGroupWaitBits(wifi_event_group, LOGIN_BIT, false, true, 100000/  portTICK_RATE_MS); //wait for IP to be assigned
		if ((uxBits & LOGIN_BIT)==LOGIN_BIT)
			logIn();													//we are MeterControllers need to login to our Host Controller. For order purposes
}
