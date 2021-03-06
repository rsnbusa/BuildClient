#include "includes.h"
#include "defines.h"
#include "projStruct.h"
#include "globals.h"
using namespace std;

void kbd(void *arg);
void start_webserver(void *arg);
void sendStatusMeterAll();
static int rsa_encrypt(char *nkey,size_t son,char *donde,size_t lenb);

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
			  FREEANDNULL(mbuffer)
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
	return;
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
		if(conn)
		{
			delay(theConf.sendDelay);		//configuration delay
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
		else
			delay(1000);
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
			timeDiff=millis()-theMeters[evt.unit].ampTime;
			theMeters[evt.unit].ampTime=millis();

			pcnt_get_counter_value((pcnt_unit_t)evt.unit,(short int *) &count);
			if(theConf.traceflag & (1<<INTD))
				pprintf("%sEvent PCNT unit[%d]; cnt: %d status %x\n",INTDT, evt.unit, count,evt.status);

			if (evt.status & PCNT_EVT_THRES_1)
			{
				pcnt_counter_clear((pcnt_unit_t)evt.unit);
				totalPulses+=count;						//counter of all pulses received
				theMeters[evt.unit].saveit=false;
				theMeters[evt.unit].currentBeat+=count;	//beat
				theMeters[evt.unit].beatSave+=count;	//beats per kwh
				theMeters[evt.unit].beatSaveRaw+=count;	//raw without tariff discount
				if((theMeters[evt.unit].currentBeat % THELOSS)!=0)
				{
					//force it to THELOSS mod. No idea how this happens but this is required else it will never have a Residuo of 0 and hence never inc kwh
					//shouldnt happen often I guess
					float theFloat=theMeters[evt.unit].currentBeat/THELOSS;
					int rounded=(int)round(theFloat)*THELOSS;
					theMeters[evt.unit].currentBeat=rounded;
					theMeters[evt.unit].beatSave=0;
				}

				// AMPs calculations
				double amps = theMeters[evt.unit].ampBeats*AMPCONST220VC/(timeDiff/THELOSS);
				uint16_t iAmps=(uint32_t)amps;	//to integer
				if(iAmps>theMeters[evt.unit].maxamps)
				{
					time(&theMeters[evt.unit].minampsT);
					theMeters[evt.unit].maxamps=iAmps;
				}
				if(iAmps<theMeters[evt.unit].minamps)
				{
					time(&theMeters[evt.unit].maxampsT);
					theMeters[evt.unit].minamps=iAmps;
				}

				if(diaHoraTarifa==0)
					diaHoraTarifa=100;					// should not happen but in case it does no crash

				residuo=1;
				if(theMeters[evt.unit].beatsPerkW>0) // division 0 guard
					residuo=theMeters[evt.unit].beatSave % (theMeters[evt.unit].beatsPerkW*diaHoraTarifa/100);	//0 is 1 kwh happend with tariffs

				if(theMeters[evt.unit].beatSave>theMeters[evt.unit].beatsPerkW)
				{
					theMeters[evt.unit].beatSave=0;
					residuo=0;
				}
				if(theConf.traceflag & (1<<INTD))
					pprintf("%sBeatSave %d MeterPos %d Time %d BPK %d\n",INTDT,theMeters[evt.unit].beatSave,
							theMeters[evt.unit].pos,timeDiff,theMeters[evt.unit].beatsPerkW);

				if(residuo==0 && theMeters[evt.unit].currentBeat>0)
				{
					theMeters[evt.unit].saveit=true;	//add one kwh
					theMeters[evt.unit].beatSave=0;
				}

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

	gpio_config_t io_conf;
	io_conf.intr_type =			GPIO_INTR_DISABLE;
	io_conf.mode =				GPIO_MODE_OUTPUT;
	io_conf.pull_down_en =		GPIO_PULLDOWN_ENABLE;	//set to ground
	io_conf.pull_up_en =		GPIO_PULLUP_DISABLE;

	for (int a=0;a<MAXDEVS;a++)
	{
		io_conf.pin_bit_mask = 		(1ULL<<theMeters[a].pin);
		gpio_config(&io_conf);
		gpio_set_level((gpio_num_t)theMeters[a].pin, (theConf.breakOff & (1<<a)));
	}

	theMeters[0].pinB=BREAK0;
	theMeters[1].pinB=BREAK1;
	theMeters[2].pinB=BREAK2;
	theMeters[3].pinB=BREAK3;
	theMeters[4].pinB=BREAK4;

    pcnt_isr_register(pcnt_intr_handler, NULL, 0,&user_isr_handle);
    workingDevs=0;

    for(int a=0;a<MAXDEVS;a++)
    	{
//		if(theConf.beatsPerKw[a]>0 && theConf.configured[a]==3)  //Only Authenticated PINs and BPK setting > 0
    		if(theConf.beatsPerKw[a]>0 && theConf.configured[a])  //Only Authenticated PINs and BPK setting > 0
    		{

    			theMeters[a].pos			=a;
    			pcnt_basic_init(a,THELOSS);	//grouped by 10 beats to save. Max loss is 10 beats in power failure
    			theMeters[a].ampBeats		=3600000/theConf.beatsPerKw[a];	//1 hour ms /beatsperKWH
    			theMeters[a].ampTime		=millis();	//now
    			theMeters[a].maxamps		=0;				//set it low
    			theMeters[a].minamps		= 0xffff;	//set it high
    			workingDevs++;				//keep count of PCNT used
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
	q = nvs_open((const char*)"config", NVS_READONLY,(unsigned int*) &nvshandle);
	if(q!=ESP_OK)
	{
		pprintf("Error opening NVS Read File %x\n",q);
		return;
	}

	largo=sizeof(theConf);
		q=nvs_get_blob((unsigned int)nvshandle,(const char*)"config",(void*)&theConf,(unsigned int*)&largo);

	if (q !=ESP_OK)
		pprintf("Error read %x largo %d aqui %d\n",q,largo,sizeof(theConf));

	nvs_close((unsigned int)nvshandle);
}

void write_to_flash() //save our configuration
{
	esp_err_t q ;
	q = nvs_open("config", NVS_READWRITE,(unsigned int*) &nvshandle);
	if(q!=ESP_OK)
	{
		pprintf("Error opening NVS File RW %x\n",q);
		return;
	}
	q=nvs_set_blob((unsigned int)nvshandle,"config",(void*)&theConf,sizeof(theConf));
	if (q ==ESP_OK)
		q = nvs_commit((unsigned int)nvshandle);
	nvs_close((unsigned int)nvshandle);
}

void connect_to_host(void *pArg)
{

	ip_addr_t 						remote;
	u8 								aca[4];
	tcpip_adapter_ip_info_t 		ip_info;
	int 							addr_family;
	int								ip_protocol,err,retry=0;

	conn=false;
	gsock=-1;

	while(true)
	{
		again:
		if(esp_wifi_connect()==ESP_OK)
		{
			if(theConf.traceflag & (1<<WIFID))
				pprintf("%sRequesting connection\n",WIFIDT);
			//wait for the Got IP handelr to set this bit
			EventBits_t uxBits=xEventGroupWaitBits(wifi_event_group, WIFI_BIT, false, true, 3000/  portTICK_RATE_MS); //wait for IP to be assigned
			if ((uxBits & WIFI_BIT)!=WIFI_BIT)
			{
	    		if(theConf.traceflag & (1<<WIFID))
	    			pprintf("%sConnection timeout retry %d\n",WIFIDT,retry++);
				goto again;
			}

			if(theConf.traceflag & (1<<WIFID))
				pprintf("\n%sEstablished\n",WIFIDT);

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
				connHandle=NULL;
				vTaskDelete(NULL);

			}
			if(theConf.traceflag & (1<<WIFID))
				pprintf("%sSocket %d created, connecting to %s:%d\n",WIFIDT,gsock, ip4addr_ntoa(&ip_info.gw), BDGHOSTPORT);

			err = connect(gsock, (struct sockaddr *)&dest_addr, sizeof(dest_addr));
			if (err != 0)
			{
				if(theConf.traceflag & (1<<WIFID))
					pprintf( "%sSocket %d unable to connect: errno %d\n",WIFIDT,gsock, errno);
				connHandle=NULL;
				vTaskDelete(NULL);
			}
			conn=true;
			gpio_set_level((gpio_num_t)WIFILED, 1);			//WIFILED on. We have a connection and a socket to host
			xEventGroupSetBits(wifi_event_group, LOGIN_BIT);
		}
		else
		{
			if(theConf.traceflag & (1<<WIFID))
				pprintf("%sCould not esp_connect\n",WIFIDT);
			connHandle=NULL;
		}
		if(theConf.traceflag & (1<<WIFID))
			pprintf("%sDone Connect to Host\n",WIFIDT);
		connHandle=NULL;
		vTaskDelete(NULL);
	}
}

void wifi_event_handler(void* arg, esp_event_base_t event_base,
                                int32_t event_id, void* event_data)
{
    switch (event_id) {
    	case WIFI_EVENT_STA_START:
    		if(theConf.traceflag & (1<<WIFID))
    			pprintf("%sWifi start\n",WIFIDT);
    		if(!connHandle)
    			xTaskCreate(&connect_to_host,"conn",4096,NULL, 4, &connHandle);
        break;
    	case WIFI_EVENT_STA_STOP:
    		if(theConf.traceflag & (1<<WIFID))
    			pprintf("%sWifi Stopped\n",WIFIDT);
    		conn=false;
    		esp_wifi_start();
    		break;
        case WIFI_EVENT_STA_CONNECTED:
    		if(theConf.traceflag & (1<<WIFID))
    			printf("%sMtM Connected\n",WIFIDT);
            xEventGroupClearBits(wifi_event_group, WIFI_BIT);
            break; //wait for ip
        case WIFI_EVENT_STA_DISCONNECTED:
    		if(theConf.traceflag & (1<<WIFID))
    			printf("%sMtM Disconnected\n",WIFIDT);
            gpio_set_level((gpio_num_t)WIFILED, 0);
            xEventGroupClearBits(wifi_event_group, WIFI_BIT);
            if(gsock>0)
            {
            	shutdown(gsock,0);
            	close(gsock);
            	gsock=-1;
            }
			conn=false;
			if(!connHandle)	//just once CRITICAL
				xTaskCreate(&connect_to_host,"conn",8192,NULL, 4, &connHandle);
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
        	if(theConf.traceflag & (1<<WIFID))
        		pprintf("%sIP Assigned to MtM:" IPSTR,"\n",WIFIDT,IP2STR(&ev->ip_info.ip));	//print it for testing purposes
            xEventGroupSetBits(wifi_event_group, WIFI_BIT);
            conn=true;
            break;
        case IP_EVENT_STA_LOST_IP:
        	if(theConf.traceflag & (1<<WIFID))
        		pprintf("%sMtM Lost Ip/n",WIFIDT);
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
	if(theConf.traceflag & (1<<FRAMD))
			pprintf("%sFram Save %d\n",FRAMDT,addit);
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
		//	gpio_set_level((gpio_num_t)TRIGGER, 0);	//used for Logic Analyzer
			fram.write_guard(theGuard);				// theguard is dynamic and will change every 60000ms
	//		gpio_set_level((gpio_num_t)TRIGGER, 1);

			startGuard=millis();

		}
		if(theConf.traceflag & (1<<FRAMD))
			pprintf("%sBeat[%d]=%d\n",FRAMDT,meter,theMeters[meter].currentBeat);

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

    if(framSem)
    {
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
		//	load beatSave with the remainder of currentbeat mod beatsperkwh. This is for a crash/power out
			theMeters[meter].beatSave=theMeters[meter].currentBeat % theConf.beatsPerKw[meter];
			theMeters[meter].beatSaveRaw=theMeters[meter].beatSave;
			xSemaphoreGive(framSem);

			oldCurLife[meter]=0;//for display
			oldCurLife[meter]=0;

			if(theConf.traceflag & (1<<FRAMD))
				printf("[FRAMD]Loaded Meter %d curLife %d beat %d\n",meter,theMeters[meter].curLife,theMeters[meter].currentBeat);
		}
    }
}

static void wifi_init(void)
{
    esp_netif_dns_info_t 			dnsInfo;
    esp_netif_ip_info_t 			iipInfo;

	if(theConf.traceflag & (1<<WIFID))
		printf("%sWiFi Mode %s\n",WIFIDT,theConf.active?"RunConf":"SetupConf");

    esp_netif_init();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    ESP_ERROR_CHECK(esp_wifi_set_storage(WIFI_STORAGE_RAM));

	esp_efuse_mac_get_default((unsigned char*)them);

    ESP_ERROR_CHECK(esp_event_loop_create_default());
    //STA and AP defaults MUST be created even if not used
    esp_netif_t* wifiSTA=esp_netif_create_default_wifi_sta();
    esp_netif_t* wifiAP=esp_netif_create_default_wifi_ap();

	IP4_ADDR(&iipInfo.ip, 192,168,19,1);
	IP4_ADDR(&iipInfo.gw, 192,168,19,1);
	IP4_ADDR(&iipInfo.netmask, 255,255,255,0);
	esp_netif_dhcps_stop(wifiAP);
	esp_netif_set_ip_info(wifiAP, &iipInfo);
	esp_netif_dhcps_start(wifiAP);
	inet_pton(AF_INET, "192.168.19.1", &dnsInfo.ip);
	esp_netif_set_dns_info(wifiAP,	ESP_NETIF_DNS_MAIN,&dnsInfo);

	wifi_event_group = xEventGroupCreate();
	xEventGroupClearBits(wifi_event_group, WIFI_BIT);

	ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler, NULL));
	ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &ip_event_handler, NULL));



	wifi_config_t wifi_config;
	memset(&wifi_config,0,sizeof(wifi_config));//very important
	if(theConf.active)
	{
		if(strlen(theConf.mgrName)>0)
		{
			strcpy((char*)wifi_config.sta.ssid,theConf.mgrName);
			strcpy((char*)wifi_config.sta.password,theConf.mgrPass);
			ESP_ERROR_CHECK(esp_wifi_set_mode(theConf.lock?WIFI_MODE_STA:WIFI_MODE_APSTA));
			ESP_ERROR_CHECK(esp_wifi_set_ps(WIFI_PS_NONE));
			ESP_ERROR_CHECK(esp_wifi_set_config(ESP_IF_WIFI_STA, &wifi_config));
			if(!theConf.lock)		//if we are locked, NO SETUP available. Must be unlocked by host
			{
				memset(&wifi_config,0,sizeof(wifi_config));//very important
				sprintf(tempb,"Meter%02x%02x",them[4],them[5]);
				strcpy((char*)wifi_config.ap.ssid,tempb);
				strcpy((char*)wifi_config.ap.password,tempb);
				wifi_config.ap.authmode=WIFI_AUTH_WPA_PSK;
				wifi_config.ap.ssid_hidden=false;
				wifi_config.ap.max_connection=1;
				wifi_config.ap.ssid_len=0;
				ESP_ERROR_CHECK(esp_wifi_set_config(ESP_IF_WIFI_AP, &wifi_config));
			}
			esp_wifi_start();
		}
		else
		{
			printf("No Mgr defined\n");
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
		wifi_config.ap.max_connection=1;
		wifi_config.ap.ssid_len=0;
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

static cJSON *makeCmdcJSON(int cual, meterType *meter,bool ans)
{
	short int count;

	pcnt_get_counter_value((pcnt_unit_t)cual,(short int *) &count);

	cJSON *cmdJ=cJSON_CreateObject();
	if(cmdJ)
	{
		cJSON_AddStringToObject(cmdJ,"cmd",				"cmd_status");
		cJSON_AddStringToObject(cmdJ,"mid",				theConf.medidor_id[meter->pos]);
		cJSON_AddNumberToObject(cmdJ,"T#",				++theMeters[meter->pos].vanMqtt);// when affecting theMeter this pointer is to a COPY so get its pos and update directly
		cJSON_AddNumberToObject(cmdJ,"KwH",				meter->curLife);
		cJSON_AddNumberToObject(cmdJ,"Beats",			meter->currentBeat+count);
		cJSON_AddNumberToObject(cmdJ,"Pos",				meter->pos);
		cJSON_AddNumberToObject(cmdJ,"minA",			meter->minamps);
		cJSON_AddNumberToObject(cmdJ,"maxA",			meter->maxamps);
		cJSON_AddBoolToObject(cmdJ,"reply",				ans);
	}
	return cmdJ;
}

static cJSON * makeGroupCmdAll()
{
	bool ans;
	int van,blen,ret;
	char *nkey=NULL,*b64=NULL;

	/////// Create Message for Grouped Cmds ///////////////////
	van=0;
	cJSON *root=cJSON_CreateObject();
	if(root==NULL)
	{
		pprintf("cannot create root\n");
		return NULL;
	}

	cJSON *ar = cJSON_CreateArray();
	if(!ar)
	{
		cJSON_Delete(root);
		return NULL;
	}

	for (int a=0;a<MAXDEVS;a++)
	{
	//	if(theConf.configured[a]==3)
			if(theConf.configured[a])
		{
			van++;
			if(van==workingDevs)		//when sending group just answer to the last one
				ans=true;
			else
				ans=false;
			cJSON *cmdInt=makeCmdcJSON(a,&theMeters[a],ans);
			if(cmdInt)
				cJSON_AddItemToArray(ar, cmdInt);
		}
	}
	double dmac=(double)theMacNum;

	cJSON_AddItemToObject			(root,"Batch",ar);
	cJSON_AddStringToObject			(root,"Controller",theConf.meterName);
	cJSON_AddNumberToObject			(root,"macn",dmac);
	cJSON_AddBoolToObject			(root,"fram",framGuard);
	if(rsyncf)						//send a rsync message to Host with new key
	{
		nkey=(char*)malloc(AESL); 		//get space for a new key
		if(nkey)
		{
			// generate a random key 32(AESL) bytes in size
			for (int a=0;a<AESL;a++)
				nkey[a]=esp_random() % 256;
			//	//get buffer for B64 of new key.
			b64=(char*)malloc(BSIZE);
			if(!b64)
			{
				pprintf("No RAM for b64\n");
				FREEANDNULL(nkey)
				cJSON_Delete(root);
				return NULL;
			}
			else
			{
				ret=mbedtls_base64_encode((unsigned char*)b64,BSIZE,(size_t*)&blen,(const unsigned char*)nkey,AESL);	//we have the new key in base64 for Login transmission
				if(ret)
					pprintf("Failed to b64 ret %d\n",ret);
				else
				{
					theConf.lostSync=0;
					cJSON_AddStringToObject(root,"rsync",b64);
					memcpy(syncKey,nkey,AESL);	//copy new key to saved key for SendMeterAll
				}
			}
		}
		else
			pprintf("No memory for key...incredibly low\n");

		FREEANDNULL(nkey)
		FREEANDNULL(b64)
	}
	return root;
}

int aes_decrypt(const char* src, size_t son, char *dst)
{
	bzero(dst,son);
	if(!theConf.crypt)
	{
		memcpy(dst,src,son);
		return ESP_OK;
	}
	bzero(iv,sizeof(iv));
	esp_aes_setkey( &ctx,(const unsigned char*) theConf.lkey, 256 );			//key may be chanign on the Fly
	esp_aes_crypt_cbc( &ctx, ESP_AES_DECRYPT, son, (unsigned char*)iv, (const unsigned char*)src,(unsigned char*) dst );
	return ESP_OK;
}

int aes_encrypt(const char* src, size_t son, char *dst)
{
	bzero(dst,son);
	if(!theConf.crypt)
	{
		memcpy(dst,src,son);
		return ESP_OK;
	}
	int theSize= son+ 16- (son % 16);

	char *donde=(char*)malloc(theSize);
	if (!donde)
	{
		pprintf("No memory copy message\n");
		return ESP_FAIL;
	}
	bzero(donde,theSize);
	memcpy(donde,src,son);

	bzero(iv,sizeof(iv));
	esp_aes_setkey( &ctx,(const unsigned char*) theConf.lkey, 256 );			//key may be chanign on the Fly
	esp_aes_crypt_cbc( &ctx, ESP_AES_ENCRYPT, theSize, (unsigned char*)iv,(const unsigned char*) donde,( unsigned char*) dst );
	FREEANDNULL(donde)
	return ESP_OK;
}

int sendMsg(int cualSock,uint8_t *lmessage, uint16_t son,uint8_t *rdonde,uint16_t maxx)
{
    struct timeval to;
	int err,llen;

	if (!conn)
		return ESP_FAIL;

	int theSize=son;
	int rem= theSize % 16;
	theSize+=16-rem;			//round to next 16 for AES

	char *output=(char*)malloc(theSize);
	if (!output)
	{
		pprintf("No memory ouput message\n");
		return ESP_FAIL;
	}

	if(theConf.traceflag & (1<<MSGD))
		pprintf("%sSending queue size %d %s %d\n",MSGDT,son,lmessage,esp_get_free_heap_size());

	if(aes_encrypt((const char*)lmessage,son,output)!=0)
	{
		pprintf("Failed to encrypt SendMsg\n");
		FREEANDNULL(output)
		return ESP_FAIL;
	}
	err = send(cualSock, output, theSize, 0);
	if (err < 0)
	{
		if(theConf.traceflag & (1<<MSGD))
			pprintf( "%sSock %d Error occurred during sending: errno %d\n",MSGDT,gsock, errno);
		FREEANDNULL(output)
		return ESP_FAIL;
	}

	FREEANDNULL(output)

	 if(maxx>0)		//wait for reply
	 {
			if(theConf.traceflag & (1<<MSGD))
				pprintf("%sWaiting response\n",MSGDT);

		to.tv_sec = 2;
		to.tv_usec = 0;

		if (setsockopt(cualSock,SOL_SOCKET,SO_RCVTIMEO,&to,sizeof(to)) < 0)
		{
			if(theConf.traceflag & (1<<MSGD))
				pprintf("Unable to set read timeout on socket!\n");
			return ESP_FAIL;
		}

		char *response=(char*)malloc(maxx);
		if(!response)
		{
			pprintf("Failed to get RAM for response\n");
			return ESP_FAIL;
		}

		llen = recv(cualSock, response, maxx, 0);

		if(llen<0)
		{
			//timeout. Start counting lost Syncs.
			pprintf("SendMsg Timeout %d\n",llen);
			theConf.lostSync++;
			if(theConf.lostSync>MAXLOSTSYNC)
			{
				pprintf("Reseting key to default\n");
				memset(theConf.lkey,65,AESL);
				esp_aes_setkey( &ctx,(const unsigned char*) theConf.lkey, 256 );
				theConf.lostSync=0;
				rsyncf=true;
			}
		}
		else
		{
			theConf.lostSync=0;
			//	get the decrypted message. Must be a cJSON as first guard
			aes_decrypt((const char*)response,llen,(char*)rdonde);
			if(*rdonde!='{')		//start of any cJSON
			{
				//pprintf("Not cJSON Client\n");
				theConf.lostSync++;
				if(theConf.lostSync>MAXLOSTSYNC-1)	//rsync,set default aes key
				{
					memset(theConf.lkey,65,AESL);
					esp_aes_setkey( &ctx, (const unsigned char*)theConf.lkey, 256 );
					theConf.lostSync=0;
					rsyncf=true;
				}
			}
		}

		FREEANDNULL(response)

		if(theConf.traceflag & (1<<MSGD))
			pprintf("%sSendmsg response size %d heap %d\n",MSGDT,llen,esp_get_free_heap_size());

		return llen;
	 }
	return ESP_OK;
}

static int rsa_encrypt(char *nkey,size_t son,char *donde,size_t lenb)
{
	int ret;
	int olen=0;

	char *buf=(char*)malloc(1024);	//max size for RSA key encryption
	if(!buf)
	{
		printf("No RAM for buf\n");
		return ESP_FAIL;
	}

	// now encrypt the Key into the Buf buffer, olen has the final number of bytes
	if((ret = mbedtls_pk_encrypt( &pk, (const unsigned char*)nkey, son,(unsigned char*)buf, (size_t*)&olen, 1024,mbedtls_ctr_drbg_random, &ctr_drbg)) != 0 )
	{
		printf( " failed\n  ! mbedtls_pk_encrypt returned -0x%04x\n", -ret );
		FREEANDNULL(buf)
		return  ESP_FAIL;
	}
	size_t blen;

	//to base64 to be able to inserty in a cJSON message
	ret=mbedtls_base64_encode((unsigned char*)donde,lenb,&blen,(const unsigned char*)buf,olen);	//we have the new key in base64 for Login transmission
	if(ret)
	{
		printf("Failed to b64 ret %d\n",ret);
		FREEANDNULL(buf)
		return ret;
	}
	FREEANDNULL(buf)
	return ESP_OK;
}

static int logIn()
{

	int rsa_status=0,err=0;
	double dmac=0.0;

    loginT loginData;
	char *nkey=NULL,*b64=NULL,*logMsg=NULL,*tmp=NULL;
	cJSON *root=NULL,*cmdJ=NULL,*ar=NULL;

	nkey=(char*)malloc(AESL); 		//get space for a new key
	if(nkey)
	{
		// generate a random key 32(AESL) bytes in size
		for (int a=0;a<AESL;a++)
			nkey[a]=esp_random() % 256;
	}
	else
	{
		pprintf("No memory for key...incredibly low\n");
		err= ESP_FAIL;
		goto exit;
	}

//	//get buffer for B64 of new key.
	b64=(char*)malloc(BSIZE);
	if(!b64)
	{
		pprintf("No RAM for b64\n");
		err=-2;
		goto exit;
	}
	bzero(b64,BSIZE);

	rsa_status=rsa_encrypt(nkey,AESL,b64,BSIZE);	//should check for error creating rsa/b64 key. Not going to work after this point
	if(rsa_status!=ESP_OK)
	{
		pprintf("Failed RSA-B64 Process\n");
		err=-3;
		goto exit;
	}

	setenv("TZ", LOCALTIME, 1);
	tzset();

	root=		cJSON_CreateObject();
	cmdJ=		cJSON_CreateObject();
	ar= 		cJSON_CreateArray();

	if(root==NULL)
	{
		pprintf("cannot create root\n");
		goto exit;
	}

	if(ar==NULL)
		goto exit;

	if(cmdJ==NULL)
	{
		cJSON_Delete(ar);		//it hasnt been added to root so need to delete it directly
		goto exit;
	}

	//create message password,cmd,aes key, mac# and array
	dmac=(double)theMacNum;

	cJSON_AddStringToObject	(cmdJ,"password","zipo");
	cJSON_AddStringToObject	(cmdJ,"cmd","cmd_login");
	cJSON_AddStringToObject	(cmdJ,"key",b64);
	cJSON_AddItemToArray	(ar, cmdJ);
	cJSON_AddItemToObject	(root, "Batch",ar);
	cJSON_AddNumberToObject	(root,"macn",dmac);

	logMsg=cJSON_Print(root);		//message to send in the clear. Sendmsg will encrypt AES
	if(!logMsg)
	{
		pprintf("No memory for logMsg\n");
		goto exit;
	}

	bzero(&loginData,sizeof(loginData));

	tmp=(char*)malloc(1024);
	if(tmp)
	{
		int fueron=sendMsg(gsock,(uint8_t*)logMsg,strlen(logMsg),(uint8_t*)tmp, 1024);		//send it encrypted
		if(fueron>0)
		{
			//its a JSON struct parse it and get err and ts
			tmp[fueron]=0;
			cJSON *theAns= cJSON_Parse(tmp);
			if(theAns)
			{
				//got a good json
				if(theConf.traceflag & (1<<MSGD))
					pprintf("%sAnswer Login %s\n",MSGDT,tmp);
				cJSON *timeStamp= 	cJSON_GetObjectItem(theAns,"Ts");
				cJSON *tar= 		cJSON_GetObjectItem(theAns,"Tar");
				if(tar)
					loginData.theTariff=tar->valueint;
				if(timeStamp)
					loginData.thedate=(time_t)timeStamp->valueint;

				cJSON_Delete(theAns);
			}

			if(rsa_status==ESP_OK) //must be done here so that SendMsg uses the Known key
			{
			//	printf("Login RSA\n");
				memcpy(theConf.lkey,nkey,AESL);
				write_to_flash();
				esp_aes_setkey( &ctx, (const unsigned char*)theConf.lkey, 256 );	//now definite else keep old key
			}
		}
	}
	else
	{
		pprintf("No memory for Answer\n");
		goto exit;
	}

	updateDateTime(loginData);

	oldMesg=mesg;
	oldDiag=diag;
	oldHorag=horag;
	oldYearDay=yearDay;

	if(theConf.traceflag & (1<<CMDD))
		pprintf("%sLogin year %d month %d day %d hour %d Tariff %d YDAY %d\n",CMDDT,yearg,mesg,diag,horag,loginData.theTariff,oldYearDay);

	//free our people...
	exit:
	FREEANDNULL(tmp)
	FREEANDNULL(logMsg)
	FREEANDNULL(b64)
	FREEANDNULL(nkey)
	if(root)
		cJSON_Delete(root);

	return err;
}

int findMeter(char*cualm)
{
	for (int a=0;a<MAXDEVS;a++)
		if(strcmp(theConf.medidor_id[a],cualm)==0)
			return a;
	return  ESP_FAIL;
}


bool isnumber(char* data)
{
	for (int a=0;a<strlen(data);a++)
	{
		if (!isdigit(data[a]))
			return false;
	}
	return true;
}
int sendReplyToHost(int cualm,cJSON * cj,int son,char* cmdI, ...)
{
	va_list args;
	time_t 	now;

	time(&now);

	va_start(args,cmdI);

	cJSON *req=cJSON_GetObjectItem(cj,"REQ");

	cJSON *root=cJSON_CreateObject();
	if(root)
	{
		cJSON *ar = cJSON_CreateArray();
		if(ar)
		{
			cJSON *cmdJ=cJSON_CreateObject();
			if(cmdJ)
			{
				if(req)
					cJSON_AddNumberToObject		(cmdJ,"RSP",req->valueint);
				cJSON_AddNumberToObject			(cmdJ,"TS",now);
				cJSON_AddStringToObject			(cmdJ,"cmd","cmd_sendHost");
				cJSON_AddStringToObject			(cmdJ,"MtM",theConf.meterName);
				cJSON_AddStringToObject			(cmdJ,"MID",theConf.medidor_id[cualm]);
				cJSON_AddStringToObject			(cmdJ,"connmgr",globalConn);
				char *key=cmdI;
				for(int a=0;a<son;a++)
				{
					char *xx=(char*)va_arg(args,int);
					if(!isnumber(xx))
						cJSON_AddStringToObject			(cmdJ,key,xx);
					else
						cJSON_AddNumberToObject			(cmdJ,key,atoi(xx));

					if(a<son-1)
						key=(char*)va_arg(args,int);
				}
				va_end(args);
				cJSON_AddItemToArray			(ar, cmdJ);
			}

			double dmac						=(double)theMacNum;
			cJSON_AddItemToObject			(root,"Batch",ar);
			cJSON_AddNumberToObject			(root,"macn",dmac);
			char *lmessage=cJSON_Print(root);
			if(lmessage)
			{
				sendMsg(gsock,(uint8_t*)lmessage,strlen(lmessage),NULL,0);
				FREEANDNULL(lmessage)
			}
			cJSON_Delete(root);
			return ESP_OK;
		}
		else
		{
			cJSON_Delete(root);
			return ESP_FAIL;
		}
	}
	else
		return ESP_FAIL;
}

int setBPK(cJSON *theJSON,uint8_t cualm)
{
	char numa[10],numb[10];

	cJSON *bpk=cJSON_GetObjectItem(theJSON,"bpk");
	cJSON *born=cJSON_GetObjectItem(theJSON,"born");
	if(!bpk || !born)
		return ESP_FAIL;

	theMeters[cualm].beatsPerkW=theConf.beatsPerKw[cualm]=bpk->valueint;	// beat per KW. MAGIC number.
	theMeters[cualm].curLife=theConf.bornKwh[cualm]=born->valueint;		// Initial KW in meter
	time(&theConf.bornDate[cualm]);				// save current date a birthday
	write_to_flash();

	//update FRAM values
	fram.formatMeter(cualm);
	fram.write_lifekwh(cualm,born->valueint);

	theMeters[cualm].curMonth=theMeters[cualm].curMonthRaw=theMeters[cualm].curDay=theMeters[cualm].curDayRaw=0;
	theMeters[cualm].curCycle=theMeters[cualm].currentBeat=theMeters[cualm].beatSave=theMeters[cualm].beatSaveRaw=0;
	theMeters[cualm].curHour=theMeters[cualm].curHourRaw=0;

	// send confirmation of cmd received

	itoa(bpk->valueint,numa,10);
	itoa(born->valueint,numb,10);
	sendReplyToHost(cualm,theJSON,2,(char*)"BPK",numa,"BORN",numb);
	return ESP_OK;
}


int setLockCmd(cJSON *theJSON,uint8_t cualm)
{
	char numa[20];

	cJSON *lck=cJSON_GetObjectItem(theJSON,"lock");
	if(!lck)
		return ESP_FAIL;

	theConf.lock=lck->valueint;
	write_to_flash();
	itoa(lck->valueint,numa,10);
	sendReplyToHost(cualm,theJSON,1,(char*)"LOCK",numa);
	return ESP_OK;
}


int zeroCmd(cJSON *theJSON,uint8_t cualm)
{
	memset(theConf.lkey,65,AESL);
	write_to_flash();
	esp_aes_setkey( &ctx, (unsigned char*)theConf.lkey, 256 );

	return ESP_OK;
}

int displayCmd(cJSON *theJSON,uint8_t cualm)
{

	cJSON *dispmode=cJSON_GetObjectItem(theJSON,"MODE");
	if(dispmode)
	{
		int dis=dispmode->valueint;
		if(dis>2)
			dis=2;
		displayMode=dis;
	}
	else
		return  ESP_FAIL;
	return ESP_OK;
}

int setDelayCmd(cJSON *theJSON,uint8_t cualm)
{
	char numa[10];


	cJSON *delaym=cJSON_GetObjectItem(theJSON,"DELAY");
	if(delaym)
	{
		theConf.sendDelay=delaym->valueint;
		write_to_flash();
	}
	else
		return  ESP_FAIL;

	itoa(delaym->valueint,numa,10);
	sendReplyToHost(cualm,theJSON,1,(char*)"DELAYSET",numa);
	return ESP_OK;
}

int setOnOffCmd(cJSON *theJSON,uint8_t cualm)
{
	cJSON *delaym=cJSON_GetObjectItem(theJSON,"STATE");
	if(delaym)
	{
		theConf.sendDelay=delaym->valueint;
		if(cualm)
			theConf.breakOff |= (1<<cualm);
		else
			theConf.breakOff &= ~(1<<cualm);

		gpio_set_level((gpio_num_t)theMeters[cualm].pin, (theConf.breakOff & (1<<cualm)));
		write_to_flash();
	}
	else
		return  ESP_FAIL;

	sendReplyToHost(cualm,theJSON,1,(char*)"ONOFF",delaym->valueint?"DISCO":"CONN");
	return ESP_OK;
}

int sendMonthCmd(cJSON *theJSON,uint8_t cualm)
{
	uint16_t theMonth;
	char numa[10],numb[10];

	cJSON *param=cJSON_GetObjectItem(theJSON,"MONTH");
	if(param)
		fram.read_month(cualm,param->valueint,(uint8_t*)&theMonth);
	else
		return ESP_FAIL;

	itoa(param->valueint,numa,10);
	itoa(theMonth,numb,10);
	printf("Sending reply %s %s\n",numa,numb);
	sendReplyToHost(cualm,theJSON,2,(char*)"Month",(char*)numa,(char*)"KwH",(char*)numb);
	return ESP_OK;
}

int sendMonthsInYearCmd(cJSON *theJSON,uint8_t cualm)
{
	uint16_t theMonth[12];
	char temp[10];
	string todos="";

	for(int a=0;a<12;a++)
	{
		fram.read_month(cualm,a,(uint8_t*)&theMonth[a]);
	//	printf("MY[%d]=%d ",a,theMonth[a]);
		sprintf(temp,"%d|",theMonth[a]);
		todos+=string(temp);
	}

	sendReplyToHost(cualm,theJSON,1,(char*)"MonthsKWH",(char*)todos.c_str());
	return ESP_OK;
}

int sendDayCmd(cJSON *theJSON,uint8_t cualm)
{
	uint16_t theDay;
	char numa[10],numb[10];


	cJSON *param=cJSON_GetObjectItem(theJSON,"DAY");
	if(param)
	{
		fram.read_day(cualm,(uint16_t)param->valueint,(uint8_t*)&theDay);
	//	printf("Day[%d]=%d\n",param->valueint,theDay);
	}
	else
		return ESP_FAIL;

	itoa(param->valueint,numa,10);
	itoa(theDay,numb,10);
	sendReplyToHost(cualm,theJSON,2,(char*)"Day",numa,"KwH",numb);

	return ESP_OK;
}

int sendKwHCmd(cJSON *theJSON,uint8_t cualm)
{
	uint32_t kwh;
	char numa[10];

	fram.read_lifekwh(cualm,(uint8_t*)&kwh);

	itoa(kwh,numa,10);
	sendReplyToHost(cualm,theJSON,1,(char*)"KwH",numa);
	return ESP_OK;
}


int sendBeatsCmd(cJSON *theJSON,uint8_t cualm)
{
	uint32_t beats;
	char numa[10];

	fram.read_beat(cualm,(uint8_t*)&beats);
//	printf("Beats[%d]=%d\n",cualm,beats);

	itoa(beats,numa,10);
	sendReplyToHost(cualm,theJSON,1,(char*)"Beats",numa);
	return ESP_OK;
}

int sendDaysInYearCmd(cJSON *theJSON,uint8_t cualm)
{
	uint16_t theDay[366];
	string todos="";
	char temp[10];

	for(int a=0;a<366;a++)
	{
		fram.read_day(cualm,a,(uint8_t*)&theDay[a]);
	//	printf("DY[%d]=%d ",a,theDay[a]);
		sprintf(temp,"%d|",theDay[a]);
		todos+=string(temp);
	}

	sendReplyToHost(cualm,theJSON,1,(char*)"DY",todos.c_str());
	return ESP_OK;
}

int sendDaysInMonthCmd(cJSON *theJSON,uint8_t cualm)
{
	int desde=0,hasta;
	string todos="";
	char	temp[10];


	cJSON *param=cJSON_GetObjectItem(theJSON,"MONTH");
	if(param)
	{//need to calculate first and last day
		uint16_t theMonth;

			for (int a=0;a<param->valueint;a++)
				desde+=daysInMonth[a];
			hasta=desde+daysInMonth[param->valueint];
			for(int a=desde;a<hasta;a++)
			{
				fram.read_day(cualm,a,(uint8_t*)&theMonth);
			//	printf("DM[%d]=%d ",a,theMonth);
				sprintf(temp,"%d|",theMonth);
				todos+=string(temp);
			}
	//		printf("\n");

		sendReplyToHost(cualm,theJSON,1,(char*)"DM",todos.c_str());
		return ESP_OK;
	}
	else
		return ESP_FAIL;
}
static int findCommand(char * cual)
{
	for (int a=0;a<MAXCMDS;a++)
	{
			if(strcmp(cmds[a].comando,cual)==0)
			return a;
	}
	return ESP_FAIL;
}


void cmdManager(void *parg)
{
	// logic to implement
	int forMeter=0,cualf;
	char *cmd=(char*)parg;				//must free it when done
//	printf("Cmd received [%s]\n",cmd);
	cJSON *elcmd= cJSON_Parse(cmd);
	if(elcmd)
	{
		cJSON *outer= cJSON_GetObjectItem(elcmd,"hostCmd");	//array of cmds for each MID
		if(outer)
		{
			int son=cJSON_GetArraySize(outer);
			for (int a=0;a<son;a++)
			{
				cJSON *cmdIteml = cJSON_GetArrayItem(outer, a);//next item
				cJSON *para= cJSON_GetObjectItem(cmdIteml,"MID");
				cJSON *cmdd= cJSON_GetObjectItem(cmdIteml,"cmd");
				if(!para)
				{
					printf("[Syntax]No MID sent\n");
				}
				else
				{
					forMeter=para->valueint;
					if(cmdd)
					{
						//printf("Cmd[%d]=%s\n",a,cmdd->valuestring);
						cJSON *inner=cJSON_Parse(cmdd->valuestring);
						if(inner)
						{
							cJSON *forwhom= cJSON_GetObjectItem(inner,"to");
							if(forwhom)
							{
								if(strcmp(forwhom->valuestring,theConf.medidor_id[forMeter])==0)
								{
									//printf("For %s \n",forwhom->valuestring);
									cJSON *imonton= cJSON_GetObjectItem(inner,"Batch");
									if(imonton)
									{
										int ison=cJSON_GetArraySize(imonton);
										for(int b=0;b<ison;b++)
										{
											cJSON *icmdIteml = cJSON_GetArrayItem(imonton, b);//next item
											if(icmdIteml)
											{
												cJSON *icmd= cJSON_GetObjectItem(icmdIteml,"cmd");
												if(icmd)
												{
												//	printf("Inner %d-%d cmd %s\n",b,ison,icmd->valuestring);
													//execute the Cmd. Pass the original cJSON string icmditeml so he can process the Arguments
													cualf=findCommand(icmd->valuestring);
													if(cualf<0){
														printf("Invalid cmd rx %s\n",icmd->valuestring);
													}
													else
													{
														//process this command
														(*cmds[cualf].code)(icmdIteml,forMeter);
													//	printf("Cmd result=%d\n",mres);
													}
												}
												else
												{
													printf("No cmd innerIteml %d\n",b);
												}
											}
											else
											{
												printf("failed inner Item %d\n",b);
											}
										}
									}
									else
									{
										printf("[Syntax]HostCmd sent without Batch\n");
									}
								}
								else
								{
									printf("Invalid MID %d for name %s\n",forMeter,theConf.medidor_id[forMeter]); //will never get this message cause. It will not be saved by BulkidMgr without a destination
								}
							}
							else
							{
								printf("[Syntax]HostCmd without 'To'\n");
							}
							cJSON_Delete(inner);
						}
					}
				}
			}
		}
		cJSON_Delete(elcmd);
	}
	FREEANDNULL(cmd)
	vTaskDelete(NULL);
}

void sendStatusMeterAll()
{
    loginT 	loginData;
    int 	sendStatus;
    char 	*lmessage=NULL,*ans=NULL;

	if(theConf.traceflag & (1<<MSGD))
	{
		pprintf("%sSendM %d Heap %d wait %d\n",CMDDT,++sentTotal,esp_get_free_heap_size(),uxQueueMessagesWaiting(mqttR));
		fflush(stdout);
	}

	cJSON *root=makeGroupCmdAll();
	if(root)
		lmessage=cJSON_Print(root);
	else
	{
		pprintf("Cannot create cjson root\n");
		return;
	}

	cJSON_Delete(root);

	if(!lmessage)
	{
		pprintf("No message\n");
		return;
	}

	bzero(&loginData,sizeof(loginData));

	//receive buffer greater than expected since we are sending 5 status messages and will receive 5 answers.
	// if we do not do this the sendmsg socket will SAVE the other answers for next call and never gets the logindate correct
	ans=(char*)malloc(1024);
	if(!ans)
	{
		pprintf("No ram for Ans\n");
		free(lmessage);
		return;
	}

	bzero(ans,1024);

	sendStatus=sendMsg(gsock,(uint8_t*)lmessage,strlen(lmessage),(uint8_t*)ans,1024);
	if(sendStatus>=0)
	{
		FREEANDNULL(lmessage)
		//its a JSON struct parse it and get err and ts
		cJSON *theAns= cJSON_Parse(ans);
		if(theAns)
		{
			//got a good json
			if(theConf.traceflag & (1<<MSGD))
				printf("%sAnswer %s\n",MSGDT,ans);
			cJSON *timeStamp= cJSON_GetObjectItem(theAns,"Ts");
			cJSON *connmgr= cJSON_GetObjectItem(theAns,"connmgr");
			cJSON *cmdHost= cJSON_GetObjectItem(theAns,"cmdHost");		//check if we got an order from Host
			if(cmdHost)
			{
				memcpy(globalConn,connmgr->valuestring,strlen(connmgr->valuestring));
				//launch Task to manage Cmds from host. It will kill itself
				char *fromH=(char*)malloc(strlen(cmdHost->valuestring)+1);
				bzero(fromH,strlen(cmdHost->valuestring)+1);
				memcpy(fromH,cmdHost->valuestring,strlen(cmdHost->valuestring));
				xTaskCreate(&cmdManager,"cmdmgr",9182,(void*)fromH, 10, NULL);		//task will free heap
			}
			cJSON *tar= cJSON_GetObjectItem(theAns,"Tar");
			if(tar)
				loginData.theTariff=tar->valueint;
			if(timeStamp)
				loginData.thedate=(time_t)timeStamp->valueint;
			if(rsyncf)
			{
				memcpy(theConf.lkey,syncKey,AESL);
				theConf.lostSync+=0;
				write_to_flash();
				rsyncf=false;
			}
			cJSON_Delete(theAns);
		}
		FREEANDNULL(ans)
		sendErrors=0;		//restart counter on sucess
		updateDateTime(loginData);
	}
	else
	{
		FREEANDNULL(lmessage)
		FREEANDNULL(ans)
		//keep count of missed messages. If too many, restart
		sendErrors++;
		if(sendErrors>MAXSENDERR)
		{
			if(conn)
			{
				pprintf("Errors exceeded. Restarting\n");
				delay(1000);
//				if(gsock)
//				{
//					shutdown(gsock, 0);
//					close(gsock);
//					delay(1000);
//				}
				printf("Restart conn\n");
				esp_restart(); // hopefully it will connect
			}
		}
	}

	if(theConf.traceflag & (1<<CMDD))
	{
		pprintf("%sSendMeterFram dates year %d month %d day %d hour %d Tariff %d\n",CMDDT,yearg,mesg,diag,horag,loginData.theTariff);
		pprintf("%sTar %d sent Heap %d\n",CMDDT,loginData.theTariff,esp_get_free_heap_size());
	}
	if(theConf.traceflag & (1<<MSGD))
		pprintf("%sSendAll %d\n",MSGDT,esp_get_free_heap_size());

}

static void erase_config() //do the dirty work
{
	memset(&theConf,0,sizeof(theConf));
	theConf.centinel=CENTINEL;
	theConf.beatsPerKw[0]=800;
	theConf.crypt=1;
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
		printf("Failed to InitScreen\n");
}
#endif


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
					printf("%sLoading %d\n",FRAMDT,a);
				load_from_fram(a);
			}
	}
	else
		framSem=NULL;
}

static void init_vars()
{
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

	rsyncf=false;
	tempb=(char*)malloc(5000);
	if(!tempb)
		printf("Cannt Alloc Working Buffer\n");

	if(theConf.sendDelay==0)
		theConf.sendDelay=60000;

	theConf.lostSync=0;

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
	strcpy(lookuptable[14],"SIMD");

	string debugs;

	// add - sign to Keys
	for (int a=0;a<NKEYS/2;a++)
	{
		debugs="-"+string(lookuptable[a]);
		strcpy(lookuptable[a+NKEYS/2],debugs.c_str());
	}

// Host Commands
	strcpy((char*)&cmds[ 0].comando,"cmd_bpk");			cmds[0].code=setBPK;
	strcpy((char*)&cmds[ 1].comando,"cmd_zerok");		cmds[1].code=zeroCmd;
	strcpy((char*)&cmds[ 2].comando,"cmd_display");		cmds[2].code=displayCmd;
	strcpy((char*)&cmds[ 3].comando,"cmd_setdelay");	cmds[3].code=setDelayCmd;
	strcpy((char*)&cmds[ 4].comando,"cmd_sendmonth");	cmds[4].code=sendMonthCmd;
	strcpy((char*)&cmds[ 5].comando,"cmd_sendmy");		cmds[5].code=sendMonthsInYearCmd;
	strcpy((char*)&cmds[ 6].comando,"cmd_sendday");		cmds[6].code=sendDayCmd;
	strcpy((char*)&cmds[ 7].comando,"cmd_senddy");		cmds[7].code=sendDaysInYearCmd;
	strcpy((char*)&cmds[ 8].comando,"cmd_senddm");		cmds[8].code=sendDaysInMonthCmd;
	strcpy((char*)&cmds[ 9].comando,"cmd_sendkwh");		cmds[9].code=sendKwHCmd;
	strcpy((char*)&cmds[10].comando,"cmd_sendbeats");	cmds[10].code=sendBeatsCmd;
	strcpy((char*)&cmds[11].comando,"cmd_onOff");		cmds[11].code=setOnOffCmd;
	strcpy((char*)&cmds[12].comando,"cmd_lock");		cmds[12].code=setLockCmd;

	memset( iv, 0, sizeof( iv ) );
	void *zb=malloc(AESL);
	bzero(zb,AESL);

	esp_aes_init( &ctx );
	if(memcmp(zb,theConf.lkey,AESL)==0)
	{
		memset(&theConf.lkey,65,AESL);
		write_to_flash();
	}

	//set AES key to saved key
	esp_aes_setkey( &ctx, (const unsigned char*)theConf.lkey, 256 );


	//setup the RSA system for Login and RSYNC cmds
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
		return ;
	}

	mbedtls_pk_init( &pk );

	if( ( ret = mbedtls_pk_parse_public_key( &pk,cacert_pem_start,len) ) != 0 )
	    printf( " failed\n  ! mbedtls_pk_parse_public_keyfile returned -0x%04x\n", -ret );

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
		printf("%s=============== FRAM ===============%s\n",RED,YELLOW);
		printf("FRAMDATE(%s%d%s)=%s%d%s\n",GREEN,FRAMDATE,YELLOW,CYAN,METERVER-FRAMDATE,RESETC);
		printf("METERVER(%s%d%s)=%s%d%s\n",GREEN,METERVER,YELLOW,CYAN,GUARDM-METERVER,RESETC);
		printf("GUARD(%s%d%s)=%s%d%s\n",GREEN,GUARDM,YELLOW,CYAN,SCRATCH-GUARDM,RESETC);
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
		printf("TOTALFRAM(%s%d%s)\n",GREEN,TOTALFRAM,RESETC);
		printf("%s=============== FRAM ===============%s\n",RED,RESETC);
	}
}

void app_main()
{
	printf("MtM %u\n",esp_get_free_heap_size());

	printSem= xSemaphoreCreateBinary();
	xSemaphoreGive(printSem);

    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES)
    {
		printf("No free pages erased!!!!\n");
		ESP_ERROR_CHECK(nvs_flash_erase());
		err = nvs_flash_init();
    }

	read_flash();				//our Configuration structure

    esp_log_level_set("*",(esp_log_level_t) theConf.logLevel);

	if(theConf.traceflag & (1<<BOOTD))
	{
		printf("MeterIoT MtMManager\n");
		printf("%sBuildMgr starting up\n",BOOTDT);
		printf("%sFree memory: %d bytes\n", BOOTDT,esp_get_free_heap_size());
		printf("%sIDF version: %s\n", BOOTDT,esp_get_idf_version());
	}

	delay(3000);//for erasing

    if (theConf.centinel!=CENTINEL || !gpio_get_level((gpio_num_t)0))
	{
		printf("Read centinel %x not valid. Erasing Config\n",theConf.centinel);
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

		xTaskCreate(&pcntManager,"pcntMgr",4096,NULL, 4, NULL);		// start the Pulse Manager task
		pcnt_init();												// start receiving pulses
		xTaskCreate(&framManager,"fmg",4096,NULL, 10, NULL);		// in charge of saving meter activity to Fram
		xTaskCreate(&timeKeeper,"tmK",4096,NULL, 10, &timeHandle);	// Due to Tariffs, we need to check hour,day and month changes
		EventBits_t uxBits=xEventGroupWaitBits(wifi_event_group, LOGIN_BIT, false, true, 100000/  portTICK_RATE_MS); //wait for IP to be assigned
		if ((uxBits & LOGIN_BIT)==LOGIN_BIT)
			logIn();
	}

	xTaskCreate(&start_webserver,"web",10240,(void*)1, 10, &webHandle);// Messages from the Meters. Controller Section socket manager

#ifdef DISPLAY
		xTaskCreate(&displayManager,"displ",4096,(void*)1, 4, &webHandle);// Messages from the Meters. Controller Section socket manager
#endif

		theConf.lastResetCode=rtc_get_reset_reason(0);
		time((time_t*)&theConf.lastBootDate);
		theConf.bootcount++;
		write_to_flash();

												//we are MeterControllers need to login to our Host Controller. For order purposes
}
