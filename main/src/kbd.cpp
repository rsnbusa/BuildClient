#include "defines.h"
#include "includes.h"

using namespace std;

extern char								lookuptable[NKEYS][10],deb,tempb[1200];
extern config_flash						theConf;
extern FramSPI							fram;
extern meterType						theMeters[MAXDEVS];
extern SemaphoreHandle_t 				framSem;
extern u16                  			waitQueue,mesg,diag,horag,yearg,qdelay;
extern u32								sentTotal,sendTcp,totalMsg[MAXDEVS],totalPulses;
extern u8								daysInMonth [12];

extern void delay(uint32_t a);
extern void write_to_flash();
extern void sendStatusMeterAll();
extern void load_from_fram(u8 meter);
extern void write_to_fram(u8 meter,bool addit);

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


	int cmdfromstring(string key)
	{
	    for (int i=0; i <NKEYS; i++)
	    {
	    	string s1=string(lookuptable[i]);
	    	if(strstr(s1.c_str(),key.c_str())!=NULL)
	            return i;
	    }
	    return -1;
	}

	void confStatus()
	{
	    struct tm timeinfo;
		char strftime_buf[64];

		localtime_r(&theConf.lastBootDate, &timeinfo);
		strftime(strftime_buf, sizeof(strftime_buf), "%c", &timeinfo);
		printf("Configuration BootCount %d LReset %x Date %s RunStatus[%s] Trace %x\n",theConf.bootcount,theConf.lastResetCode,strftime_buf,
				theConf.active?"Run":"Setup",theConf.traceflag);

		for (int a=0;a<MAXDEVS;a++)
		{
			localtime_r(&theConf.bornDate[a], &timeinfo);
			strftime(strftime_buf, sizeof(strftime_buf), "%c", &timeinfo);
			printf("Meter[%d] Serial %s BPW %d Born %s BornkWh %d Corte %d\n",a,theConf.medidor_id[a],theConf.beatsPerKw[a],
					strftime_buf,theConf.bornKwh[a],theConf.diaDeCorte[a]);
		}
	}

	void meterStatus(uint8_t meter)
	{
		uint32_t valr;
	    struct tm timeinfo;
	    char strftime_buf[64];

		if(xSemaphoreTake(framSem, portMAX_DELAY/  portTICK_RATE_MS))
			{
				//printf("Meter Compare test Year %d Month %d Day %d Hour %d\n",yearg,mesg,diag,horag);
				valr=0;
				fram.read_lifekwh(meter,(uint8_t*)&valr);
				printf("LifeKwh[%d]=%d %d\n",meter,valr,theMeters[meter].curLife);
				valr=0;
				fram.read_beat(meter,(uint8_t*)&valr);
				printf("Beat[%d]=%d %d\n",meter,valr,theMeters[meter].currentBeat);
				valr=0;
				fram.read_month(meter,mesg,(uint8_t*)&valr);
				printf("Month[%d]Mes[%d]=%d %d\n",meter,mesg,valr,theMeters[meter].curMonth);
				valr=0;
				fram.read_monthraw(meter,mesg,(uint8_t*)&valr);
				printf("MonthRaw[%d]Mes[%d]=%d %d\n",meter,mesg,valr,theMeters[meter].curMonthRaw);
				valr=0;
				fram.read_day(meter,yearg,mesg,diag,(uint8_t*)&valr);
				printf("Day[%d]Mes[%d]Dia[%d]=%d %d\n",meter,mesg,diag,valr,theMeters[meter].curDay);
				valr=0;
				fram.read_dayraw(meter,yearg,mesg,diag,(uint8_t*)&valr);
				printf("DayRaw[%d]Mes[%d]Dia[%d]=%d %d\n",meter,mesg,diag,valr,theMeters[meter].curDayRaw);
				valr=0;
				fram.read_hour(meter,yearg,mesg,diag,horag,(uint8_t*)&valr);
				printf("Hour[%d]Mes[%d]Dia[%d]Hora[%d]=%d %d\n",meter,mesg,diag,horag,valr,theMeters[meter].curHour);
				valr=0;
				fram.read_hourraw(meter,yearg,mesg,diag,horag,(uint8_t*)&valr);
				printf("HourRaw[%d]Mes[%d]Dia[%d]Hora[%d]=%d %d\n",meter,mesg,diag,horag,valr,theMeters[meter].curHourRaw);
				valr=0;
				fram.read_lifedate(meter,(uint8_t*)&valr);  //should be down after scratch record???
				printf("LifeDate[%d]=%d %d\n",meter,valr,theMeters[meter].lastKwHDate);
				localtime_r(&valr, &timeinfo);
				strftime(strftime_buf, sizeof(strftime_buf), "%c", &timeinfo);
				printf("LifDate %s\n",strftime_buf);
				xSemaphoreGive(framSem);
			}
	}

	void test_write(uint8_t meter, volatile const uint32_t val)
	{
		uint32_t valr;

		if(xSemaphoreTake(framSem, portMAX_DELAY/  portTICK_RATE_MS))
			{
			printf("Fram Write Meter %d Year %d Month %d Day %d Hour %d Value %d\n",meter,yearg,mesg,diag,horag,val);

				fram.write_beat(meter,val);
				delay(100);
				fram.write_lifekwh(meter,val);
				delay(100);
				fram.write_month(meter,0,val);
				delay(100);
				fram.write_monthraw(meter,0,val);
				delay(100);
				fram.write_day(meter,2019,0,0,val);
				delay(100);
				fram.write_dayraw(meter,2019,0,0,val);
				delay(100);
				fram.write_hour(meter,2019,0,0,0,val);
				delay(100);
				fram.write_hourraw(meter,2019,0,0,0,val);
				fram.write_lifedate(meter,val);
				xSemaphoreGive(framSem);

				valr=0;
				printf("Reading now\n");
				fram.read_lifekwh(meter,(uint8_t*)&valr);
				printf("LifeKwh[%d]=%d\n",meter,valr);
				fram.read_beat(meter,(uint8_t*)&valr);
				printf("Beat[%d]=%d\n",meter,valr);
				fram.read_month(meter,mesg,(uint8_t*)&valr);
				printf("Month[%d]=%d\n",meter,valr);
				fram.read_monthraw(meter,mesg,(uint8_t*)&valr);
				printf("MonthRaw[%d]=%d\n",meter,valr);
				fram.read_day(meter,yearg,mesg,diag,(uint8_t*)&valr);
				printf("Day[%d]=%d\n",meter,valr);
				fram.read_dayraw(meter,yearg,mesg,diag,(uint8_t*)&valr);
				printf("DayRaw[%d]=%d\n",meter,valr);
				fram.read_hour(meter,yearg,mesg,diag,horag,(uint8_t*)&valr);
				printf("Hour[%d]=%d\n",meter,valr);
				fram.read_hourraw(meter,yearg,mesg,diag,horag,(uint8_t*)&valr);
				printf("HourRaw[%d]=%d\n",meter,valr);
				fram.read_lifedate(meter,(uint8_t*)&valr);  //should be down after scratch record???
				printf("LifeDate[%d]=%d\n",meter,valr);
			}

	}

	void kbd(void *arg) {
		int len,ret,cualf,total,a;
		uart_port_t uart_num = UART_NUM_0 ;
		char s1[20],s2[20],lastcmd=0;
		char data[20];
		u32 framAddress;
		u8 fueron,valor,cualm;
		u8 *p;
		string ss;
		u32 tots=0;

		uart_config_t uart_config = {
				.baud_rate = 460800,
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
				if(data[0]==10)
					data[0]=lastcmd;
				lastcmd=data[0];

				switch(data[0])
				{
				case '[':
				case ']':
					confStatus();
					break;
				case '-':
					theConf.active=0;
					memset(theConf.configured,0,sizeof(theConf.configured));
					write_to_flash();
					break;
				case '=':
					sendStatusMeterAll();
					break;
				case 'B':
				case 'b':
					printf("Status Meter:");
					fflush(stdout);
					len=get_string((uart_port_t)uart_num,10,s1);
					if(len<=0)
					{
						printf("\n");
						break;
					}
					fueron=atoi(s1);
					meterStatus(fueron);
					break;
				case '+':
					printf("TestWrite Meter:");
					fflush(stdout);
					len=get_string((uart_port_t)uart_num,10,s1);
					if(len<=0)
					{
						printf("\n");
							break;
					}
					fueron=atoi(s1);
					printf("Value:");
					fflush(stdout);
					len=get_string((uart_port_t)uart_num,10,s1);
					if(len<=0)
					{
						printf("\n");
							break;
					}
					valor=atoi(s1);
					test_write(fueron,valor);
					break;
				case 'c':
					case 'C':
						tots=0;
						for (int a=0;a<MAXDEVS;a++)
						{
							//if(theMeters[a].currentBeat>0)
							//{
								tots+=theMeters[a].currentBeat;
								printf("%sMeter[%d]=%s Beats %d kWh %d BPK %d\n",YELLOW,a,RESETC,theMeters[a].currentBeat,theMeters[a].curLife,
										theMeters[a].beatsPerkW);
							//}
						}
						printf("%sTotal Pulses rx=%s %d (%s)\n",RED,RESETC,totalPulses,tots==totalPulses?"Ok":"No");
						break;
				case 'q':
				case 'Q':
					printf("Queue Wait(%d):",waitQueue);
					fflush(stdout);
					len=get_string((uart_port_t)uart_num,10,s1);
					if(len<=0)
					{
						printf("\n");
							break;
					}
					waitQueue=atoi(s1);
					printf("Wait now %d\n",waitQueue);
					break;
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
			//		lbuf=(u8*)malloc(1000);
				//	if(lbuf){
						fram.format(atoi(s1),NULL,1000,true);
						printf("Format done\n");
					//	free(lbuf);
						for(int a=0;a<MAXDEVS;a++)
							load_from_fram(a);
				//	}
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
					printf("Days in Month search\nMeter:");
					fflush(stdout);
					get_string((uart_port_t)uart_num,10,s1);
					len=atoi(s1);
					if(len<0 || len>MAXDEVS){
						printf("Invalid meter\n");
						break;
					}
					cualm=atoi(s1);
					printf("Month(0-11):");
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
							fram.read_day(cualm,yearg,len, a, (u8*)&valor);
							if(valor>0)
								printf("M[%d]D[%d]=%d ",len,a,valor);
						}
						xSemaphoreGive(framSem);

					}
						printf("\n");
							break;
				case '4': //All Hours in Day
					printf("Hours in Day search\nMeter:");
					fflush(stdout);
					get_string((uart_port_t)uart_num,10,s1);
					len=atoi(s1);
					if(len<0 || len>MAXDEVS){
						printf("Invalid meter\n");
						break;
					}
					cualm=atoi(s1);
					printf("Month(0-11):");
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
							fram.read_hour(cualm, yearg,len, ret, a, (u8*)&valor);
							if(valor>0)
								printf("M[%d]D[%d]H[%d]=%d ",len,ret,a,valor);
						}
						xSemaphoreGive(framSem);

					}
						printf("\n");
							break;
				case '3': //Hour search
					printf("Month-Day-Hour search\nMeter:");
					fflush(stdout);
					get_string((uart_port_t)uart_num,10,s1);
					len=atoi(s1);
					if(len<0 || len>MAXDEVS){
						printf("Invalid meter\n");
						break;
					}
					cualm=atoi(s1);
					printf("Month(0-11):");
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
						fram.read_day(cualm, yearg,len, ret, (u8*)&valor);
						xSemaphoreGive(framSem);
						if(valor>0)
							printf("Date %d/%d/%d=%d\n",yearg,len,ret,valor);
					}
							break;
				case '2':
					printf("Month-Day search\nMeter:");
					fflush(stdout);
					get_string((uart_port_t)uart_num,10,s1);
					len=atoi(s1);
					if(len<0 || len>MAXDEVS){
						printf("Invalid meter\n");
						break;
					}
					cualm=atoi(s1);
					printf("Month:");
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
						fram.read_day(cualm, yearg,len, ret, (u8*)&valor);
						xSemaphoreGive(framSem);
						printf("Date %d/%d/%d=%d\n",yearg,len,ret,valor);
					}
							break;
				case '1':
					printf("Month search\nMeter:");
					fflush(stdout);
					get_string((uart_port_t)uart_num,10,s1);
					len=atoi(s1);
					if(len<0 || len>MAXDEVS){
						printf("Invalid meter\n");
						break;
					}
					cualm=atoi(s1);
					total=0;
					printf("Months\n");
					if(xSemaphoreTake(framSem, portMAX_DELAY))		{
						for (int a=0;a<12;a++)
						{
							fram.read_month(cualm, a, (u8*)&valor);
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
				case '9':
					for (int a=0;a<MAXDEVS;a++)
						printf("TotalMsg[%d] sent msgs %d\n",a,totalMsg[a]);
					printf("Controller Total %d\n",sentTotal);
					break;
				case 'v':
				case 'V':{
					printf("Trace Flags ");
					for (int a=0;a<NKEYS/2;a++)
						if (theConf.traceflag & (1<<a))
						{
							if(a<(NKEYS/2)-1)
								printf("%s-",lookuptable[a]);
							else
								printf("%s",lookuptable[a]);
						}
					printf("\nEnter TRACE FLAG:");
					fflush(stdout);
					memset(s1,0,sizeof(s1));
					get_string((uart_port_t)uart_num,10,s1);
					memset(s2,0,sizeof(s2));
					for(a=0;a<strlen(s1);a++)
						s2[a]=toupper(s1[a]);
					ss=string(s2);
					if(strlen(s2)<=1)
						break;
					if(strcmp(ss.c_str(),"NONE")==0)
					{
						theConf.traceflag=0;
						write_to_flash();
						break;
					}
					if(strcmp(ss.c_str(),"ALL")==0)
					{
						theConf.traceflag=0xFFFF;
						write_to_flash();
						break;
					}
					cualf=cmdfromstring((char*)ss.c_str());
					if(cualf<0)
					{
						printf("Invalid Debug Option\n");
						break;
					}
					if(cualf<NKEYS/2 )
					{
						printf("Debug Key %s added\n",lookuptable[cualf]);
						theConf.traceflag |= 1<<cualf;
						write_to_flash();
						break;
					}
					else
					{
						cualf=cualf-NKEYS/2;
						printf("Debug Key %s removed\n",lookuptable[cualf]);
						theConf.traceflag ^= (1<<cualf);
						write_to_flash();
						break;
					}

					}
				default:
					break;
				}

			}
			vTaskDelay(100 / portTICK_PERIOD_MS);
		}
	}

