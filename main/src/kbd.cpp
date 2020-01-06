#define GLOBAL
#include "defines.h"
#include "includes.h"
#include "globals.h"
#include "projStruct.h"
#include <bits/stdc++.h>

#define MAXCMDS	25
extern void delay(uint32_t a);
extern void write_to_flash();
extern void sendStatusMeterAll();
extern void load_from_fram(u8 meter);
extern void write_to_fram(u8 meter,bool addit);
extern void timeKeeperSim(void *pArg);
extern void timeKeeper(void *pArg);

using namespace std;

typedef void (*functrsn)();

typedef struct cmdRecord{
    char comando[20];
    functrsn code;
}cmdRecord;

cmdRecord cmds[MAXCMDS];

uint16_t date2days(uint16_t y, uint8_t m, uint8_t d) {
	uint8_t daysInMonth [12] ={ 31,28,31,30,31,30,31,31,30,31,30,31 };//offsets 0,31,59,90,120,151,181,212,243,273,304,334, +1 if leap year
	uint16_t days = d;
	for (uint8_t i = 0; i < m; i++)
		days += daysInMonth[ i];
	if (m > 1 && y % 4 == 0)
		++days;
	return days ;

}
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

int kbdCmd(string key)
{
	string s1,s2,skey;
	s1=s2=skey="";

//	printf("Key %s\n",key.c_str());
	skey=string(key);

	for(int a=0;a<key.length();a++)
		skey[a]=toupper(skey[a]);

//	printf("Kbdin =%s=\n",skey.c_str());

	for (int i=0; i <MAXCMDS; i++)
	{
		s1=string(cmds[i].comando);

		for(int a=0;a<s1.length();a++)
			s2[a]=toupper(s1[a]);
	//	printf("Cmd =%s=\n",s2.c_str());
		if(strstr(s2.c_str(),skey.c_str())!=NULL)
			return i;
		s1=s2="";
	}
	return -1;
}


int askMeter()
{
	int len;
	char s1[20];

	printf("Meter:");
	fflush(stdout);
	len=get_string(UART_NUM_0,10,s1);
	if(len<=0)
	{
		printf("\n");
		return -1;
	}
	len= atoi(s1);
	if(len<MAXDEVS)
		return len;
	else
	{
		printf("%sMeter out of range 0-%d%s\n",MAGENTA,MAXDEVS-1,RESETC);
		return -1;
	}
}

int askMonth()
{
	int len;
	char s1[20];

	printf("Month(0-11):");
	fflush(stdout);
	len=get_string(UART_NUM_0,10,s1);
	if(len<=0)
	{
		printf("\n");
		return -1;
	}
	len= atoi(s1);
	if(len<12)
		return len;
	else
	{
		printf("%sMonth out of range 0-11%s\n",MAGENTA,RESETC);
		return -1;
	}
}

int askHour()
{
	int len;
	char s1[20];

	printf("Hour(0-23):");
	fflush(stdout);
	len=get_string(UART_NUM_0,10,s1);
	if(len<=0)
	{
		printf("\n");
		return -1;
	}
	len= atoi(s1);
	if(len<24)
		return len;
	else
	{
		printf("%sHour out of range 0-23%s\n",MAGENTA,RESETC);
		return -1;
	}
}

int askDay(int month)
{
	int len;
	char s1[20];

	printf("Day(0-%d):",daysInMonth[month]-1);
	fflush(stdout);
	len=get_string(UART_NUM_0,10,s1);
	if(len<=0)
	{
		printf("\n");
		return -1;
	}
	len= atoi(s1);
	if(len<daysInMonth[month])
		return len;
	else
	{
		printf("%sDay out of range 0-%d%s\n",MAGENTA,daysInMonth[month]-1,RESETC);
		return -1;
	}
}

int askValue(char *title)
{
	int len;
	char s1[20];

	printf("%s:",title);
	fflush(stdout);
	len=get_string(UART_NUM_0,10,s1);
	if(len<=0)
	{
		printf("\n");
		return -1;
	}

	return atoi(s1);
}

void confStatus()
{
	struct tm timeinfo;
	char strftime_buf[64];

	printf("%s====================\nConfiguration Status\n",RED);

	localtime_r(&theConf.lastBootDate, &timeinfo);
	strftime(strftime_buf, sizeof(strftime_buf), "%c", &timeinfo);
	printf("%sConfiguration BootCount %d LReset %x Date %s RunStatus[%s] Trace %x\n",YELLOW,theConf.bootcount,theConf.lastResetCode,strftime_buf,
			theConf.active?"Run":"Setup",theConf.traceflag);

	for (int a=0;a<MAXDEVS;a++)
	{
		localtime_r(&theConf.bornDate[a], &timeinfo);
		strftime(strftime_buf, sizeof(strftime_buf), "%c", &timeinfo);
		printf("%sMeter[%d] Serial %s BPW %d Born %s BornkWh %d Corte %d Active %s\n",(a % 2)?CYAN:GREEN,a,theConf.medidor_id[a],theConf.beatsPerKw[a],
				strftime_buf,theConf.bornKwh[a],theConf.diaDeCorte[a],theConf.configured[a]==3?"Yes":"No");
	}
	printf("====================%s\n\n",RESETC);

}

void meterStatus()
{
	uint32_t valr;


	printf("%s============\nMeter status\n",RED);
	int meter=askMeter();
	if(meter<0)
		return;
	printf("============%s\n\n",YELLOW);

	if(xSemaphoreTake(framSem, portMAX_DELAY/  portTICK_RATE_MS))
	{
		printf("Meter Compare test Year %d Month %d Day %d Hour %d YDAY %d\n",yearg,mesg,diag,horag,oldYearDay);
		valr=0;
		fram.read_beat(meter,(uint8_t*)&valr);
		printf("Beat[%d]=%d %d\n",meter,valr,theMeters[meter].currentBeat);
		valr=0;
		fram.read_lifekwh(meter,(uint8_t*)&valr);
		printf("LastKwh[%d]=%d %d\n",meter,valr,theMeters[meter].curLife);
		valr=0;
		fram.read_lifedate(meter,(uint8_t*)&valr);
		printf("LifeDate[%d]=%d %d %s",meter,valr,theMeters[meter].lastKwHDate,ctime((time_t*)&theMeters[meter].lastKwHDate));
		valr=0;
		fram.read_month(meter,mesg,(uint8_t*)&valr);
		printf("Month[%d]Mes[%d]=%d %d\n",meter,mesg,valr,theMeters[meter].curMonth);
		valr=0;
		fram.read_monthraw(meter,mesg,(uint8_t*)&valr);
		printf("MonthRaw[%d]Mes[%d]=%d %d\n",meter,mesg,valr,theMeters[meter].curMonthRaw);
		valr=0;
		fram.read_day(meter,oldYearDay,(uint8_t*)&valr);
		printf("Day[%d]Mes[%d]Dia[%d]=%d %d\n",meter,mesg,diag,valr,theMeters[meter].curDay);
		valr=0;
		fram.read_dayraw(meter,oldYearDay,(uint8_t*)&valr);
		printf("DayRaw[%d]Mes[%d]Dia[%d]=%d %d\n",meter,mesg,diag,valr,theMeters[meter].curDayRaw);
		valr=0;
		fram.read_hour(meter,oldYearDay,horag,(uint8_t*)&valr);
		printf("Hour[%d]Mes[%d]Dia[%d]Hora[%d]=%d %d\n",meter,mesg,diag,horag,valr,theMeters[meter].curHour);
		valr=0;
		fram.read_hourraw(meter,oldYearDay,horag,(uint8_t*)&valr);
		printf("HourRaw[%d]Mes[%d]Dia[%d]Hora[%d]=%d %d%s\n",meter,mesg,diag,horag,valr,theMeters[meter].curHourRaw,RESETC);

		xSemaphoreGive(framSem);
	}
}

void meterTest()
{
	printf("%s================\nTest Write Meter\n",RED);

	int meter=askMeter();
	int val=askValue((char*)"Value");
	printf("================%s\n\n",RESETC);
	if(meter<0 || val<0)
		return;

	uint32_t valr;

	if(xSemaphoreTake(framSem, portMAX_DELAY/  portTICK_RATE_MS))
	{
		printf("Fram Write Meter %d Year %d Month %d Day %d Hour %d Value %d YDAY %d\n",meter,yearg,mesg,diag,horag,val,oldYearDay);

		fram.write_beat(meter,val);
		fram.write_lifekwh(meter,val);
		fram.write_lifedate(meter,val);
		fram.write_month(meter,mesg,val);
		fram.write_monthraw(meter,mesg,val);
		fram.write_day(meter,oldYearDay,val);
		fram.write_dayraw(meter,oldYearDay,val);
		fram.write_hour(meter,oldYearDay,horag,val);
		fram.write_hourraw(meter,oldYearDay,horag,val);

		valr=0;
		printf("Reading now\n");
		fram.read_beat(meter,(uint8_t*)&valr);
		printf("Beat[%d]=%d\n",meter,valr);
		fram.read_lifekwh(meter,(uint8_t*)&valr);
		printf("LifeKwh[%d]=%d\n",meter,valr);
		fram.read_lifedate(meter,(uint8_t*)&valr);
		printf("LifeDate[%d]=%d\n",meter,valr);
		fram.read_month(meter,mesg,(uint8_t*)&valr);
		printf("Month[%d]=%d\n",meter,valr);
		fram.read_monthraw(meter,mesg,(uint8_t*)&valr);
		printf("MonthRaw[%d]=%d\n",meter,valr);
		fram.read_day(meter,oldYearDay,(uint8_t*)&valr);
		printf("Day[%d]=%d\n",meter,valr);
		fram.read_dayraw(meter,oldYearDay,(uint8_t*)&valr);
		printf("DayRaw[%d]=%d\n",meter,valr);
		fram.read_hour(meter,oldYearDay,horag,(uint8_t*)&valr);
		printf("Hour[%d]=%d\n",meter,valr);
		fram.read_hourraw(meter,oldYearDay,horag,(uint8_t*)&valr);
		printf("HourRaw[%d]=%d\n",meter,valr);

		xSemaphoreGive(framSem);
	}
}

void webReset()
{
	theConf.active=!theConf.active;
	printf("%sWeb %s\n",RED,theConf.active?"RunConf":"SetupConf");
	printf("===========%s\n",RESETC);
	if(theConf.active)
		memset(theConf.configured,3,sizeof(theConf.configured));
	else
		memset(theConf.configured,0,sizeof(theConf.configured));
	write_to_flash();
}

void meterCount()
{
	time_t timeH;

	int tots=0;
	printf("%s==============\nMeter Counters%s\n",RED,RESETC);

	for (int a=0;a<MAXDEVS;a++)
	{
		tots+=theMeters[a].currentBeat;
		printf("%sMeter[%d]=%s Beats %d kWh %d BPK %d\n",(a % 2)?GREEN:CYAN,a,RESETC,theMeters[a].currentBeat,theMeters[a].curLife,
				theMeters[a].beatsPerkW);
	}
	printf("%sTotal Pulses rx=%s %d (%s)\n",RED,RESETC,totalPulses,tots==totalPulses?"Ok":"No");
	fram.readMany(FRAMDATE,(uint8_t*)&timeH,sizeof(timeH));//last known date
	printf("Last Date %s",ctime(&timeH));
	printf("==============%s\n\n",RESETC);

}

void dumpCore()
{
	u8 *p;
	printf("%sDumping core ins 3 secs%s\n",RED,RESETC);
	delay(3000);
	p=0;
	*p=0;
}

void formatFram()
{
	printf("%s===========\nFormat FRAM\n",RED);

	int val=askValue((char*)"Init value");
	printf("===========%s\n\n",RESETC);

	if(val<0)
		return;

	fram.format(val,NULL,1000,true);
	printf("Format done\n");

	for(int a=0;a<MAXDEVS;a++)
		load_from_fram(a);
}


void readFram()
{
	printf("%s=========\nRead FRAM\n",RED);

	int framAddress=askValue((char*)"Address");
	int fueron=askValue((char*)"Count");
	printf("=========%s\n\n",RESETC);
	if(fueron<0 || framAddress<0)
		return;

	fram.readMany(framAddress,(uint8_t*)tempb,fueron);
	for (int a=0;a<fueron;a++)
		printf("%02x-",tempb[a]);
	printf("\n");

}

void writeFram()
{
	printf("%s==========\nWrite FRAM\n",RED);

	int framAddress=askValue((char*)"Address");
	int fueron=askValue((char*)"Value");
	printf("==========%s\n\n",RESETC);
	if(fueron<0 || framAddress<0)
		return;

	fram.write8(framAddress,fueron);

}

void logLevel()
{
	char s1[10];

	printf("%sLOG level:(N)one (I)nfo (E)rror (V)erbose (W)arning:%s",RED,RESETC);
	fflush(stdout);
	int len=get_string(UART_NUM_0,10,s1);
	if(len<=0)
	{
		printf("\n");
		return;
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

}

void framDays()
{
	uint16_t valor;
	uint32_t tots=0;

	printf("%s==================\nFram Days in Month\n",RED);
	printf("==================%s\n",RESETC);

	int meter=askMeter();
	if(meter<0)
		return;
	int month=askMonth();
	if(month<0)
		return;

	if(xSemaphoreTake(framSem, portMAX_DELAY))
	{
		int startday=date2days(yearg,month,0);//this year, this month, first day

		for (int a=0;a<daysInMonth[month];a++)
		{
			fram.read_day(meter,startday+a, (u8*)&valor);
			if(valor>0 || (startday==yearDay && theMeters[meter].curDay>0 ))
			{
				if(startday==yearDay && theMeters[meter].curDay>0 )
					printf("M[%d]D[%d]=%d RAM=%d ",month,a,valor,theMeters[meter].curDay);
				else
					printf("M[%d]D[%d]=%d ",month,a,valor);
			}
			tots+=valor;
		}
		xSemaphoreGive(framSem);
		printf(" Total=%d\n",tots);
	}

}

void framHours()
{
	uint8_t valor;
	uint32_t tots=0;

	printf("%s=================\nFram Hours in Day\n",RED);

	int meter=askMeter();
	if(meter<0)
		return;
	int month=askMonth();
	if(month<0)
		return;
	int dia=askDay(month);
	if(dia<0)
		return;

	printf("=================%s\n",RESETC);

	if(xSemaphoreTake(framSem, portMAX_DELAY))
	{
		int startday=date2days(yearg,month,dia);//this year, this month, this day

		for(int a=0;a<24;a++)
		{
			fram.read_hour(meter, startday, a, (u8*)&valor);
				if(valor>0 || (startday==yearDay && horag==a && theMeters[meter].curHour>0))
				{
					if(startday==yearDay && horag==a && theMeters[meter].curHour>0)
						printf("%sM[%d]D[%d]H[%d]=%d RAM=%d ",(a % 2)?CYAN:GREEN,month,dia,a,valor,theMeters[meter].curHour);
					else
						printf("%sM[%d]D[%d]H[%d]=%d ",(a % 2)?CYAN:GREEN,month,dia,a,valor);
				}
				tots+=valor;
		}
		xSemaphoreGive(framSem);
		printf(" Total=%d\n",tots);
	}
}

void framMonthsHours()
{
	uint8_t valor;
	uint32_t tots=0;
	bool was;

	printf("%s=================\nFram Hours in Month\n",RED);

	int meter=askMeter();
	if(meter<0)
		return;
	int month=askMonth();
	if(month<0)
		return;

	printf("=================%s\n",RESETC);

	if(xSemaphoreTake(framSem, portMAX_DELAY))
	{
		for(int b=0;b<daysInMonth[month];b++)
		{
			int startday=date2days(yearg,month,b);//this year, this month, this day
			was=false;
			for(int a=0;a<24;a++)
			{
				fram.read_hour(meter, startday, a, (u8*)&valor);
					if(valor>0 || (startday==yearDay && a==horag && theMeters[meter].curHour>0))
					{
						was=true;
						if(startday==yearDay && a==horag && theMeters[meter].curHour>0)
							printf("%sM[%d]D[%d]H[%d]=%d RAM=%d ",(a % 2)?CYAN:GREEN,month,b,a,valor,theMeters[meter].curHour);
						else
							printf("%sM[%d]D[%d]H[%d]=%d ",(a % 2)?CYAN:GREEN,month,b,a,valor);
					}
					tots+=valor;
			}
			if(was)
				printf(" Total=%d\n",tots);
		}
			xSemaphoreGive(framSem);
	}
}

void framHourSearch()
{
	uint8_t valor;

	printf("%s================\nFram Hour Search\n",RED);

	int meter=askMeter();
	if(meter<0)
		return;
	int month=askMonth();
	if(month<0)
		return;
	int dia=askDay(month);
	if(dia<0)
		return;
	int hora=askHour();
	if(hora<0)
		return;

	printf("================%s\n",RESETC);

	if(xSemaphoreTake(framSem, portMAX_DELAY))
	{
		int startday=date2days(yearg,month,dia);//this year, this month, first day

		fram.read_hour(meter, startday, hora,(u8*)&valor);
		xSemaphoreGive(framSem);
		if(valor>0 || (horag==horag && startday==yearDay && theMeters[meter].curHour>0))
			{
			if(horag==horag && startday==yearDay && theMeters[meter].curHour>0)
				printf("Date %d/%d/%d %d:00:00=%d RAM=%d\n",yearg,month,dia,hora,valor,theMeters[meter].curHour);
			else
				printf("Date %d/%d/%d %d:00:00=%d\n",yearg,month,dia,hora,valor);
			}
	}
}


void framDaySearch()
{
	uint16_t valor;

	printf("%s===============\nFram Day Search\n",RED);

	int meter=askMeter();
	if(meter<0)
		return;
	int month=askMonth();
	if(month<0)
		return;
	int dia=askDay(month);
	if(dia<0)
		return;
	printf("===============%s\n",RESETC);


	if(xSemaphoreTake(framSem, portMAX_DELAY))
	{
		int startday=date2days(yearg,month,dia);//this year, this month, first day

		fram.read_day(meter, startday,(u8*)&valor);
		xSemaphoreGive(framSem);
		if(valor>0 || (theMeters[meter].curDay>0 && startday==yearDay))
		{
		if(theMeters[meter].curDay>0 && startday==yearDay)
			printf("Date %d/%d/%d =%d RAM=%d\n",yearg,month,dia,valor,theMeters[meter].curDay);
		else
			printf("Date %d/%d/%d =%d\n",yearg,month,dia,valor);

		}
	}
}

void framMonthSearch()
{
	uint16_t valor;

	printf("%s=================\nFram Month Search\n",RED);

	int meter=askMeter();
	if(meter<0)
		return;
	int month=askMonth();
	if(month<0)
		return;
	printf("=================%s\n",RESETC);

	if(xSemaphoreTake(framSem, portMAX_DELAY))
	{
		fram.read_month(meter,month,(u8*)&valor);
		xSemaphoreGive(framSem);
		if(valor>0 || (theMeters[meter].curMonth>0 && mesg==month))
		{
			if(theMeters[meter].curMonth>0 && mesg==month)
				printf("Month[%d] =%d RAM=%d\n",month,valor,theMeters[meter].curMonth);
			else
				printf("Month[%d] =%d\n",month,valor);
		}
	}
	printf("\n");
}

void framMonths()
{
	uint16_t valor;
	uint32_t tots=0;

	printf("%s=================\nFram Month All\n",RED);
	int meter=askMeter();
	if(meter<0)
		return;
	printf("=================%s\n",RESETC);

	if(xSemaphoreTake(framSem, portMAX_DELAY))
	{
		printf("Meter[%d]",meter);
		for(int a=0;a<12;a++)
		{
			fram.read_month(meter,a,(u8*)&valor);
			if(valor>0)
			{
			if(theMeters[meter].curMonth>0 && a==mesg)
				printf("%s[%d]=%d RAM=%d ",(a % 2)?CYAN:GREEN,a,valor,theMeters[meter].curMonth);
			else
				printf("%s[%d]=%d ",(a % 2)?CYAN:GREEN,a,valor);
			}
			tots+=valor;
		}
		printf(" Total=%d\n",tots);
		xSemaphoreGive(framSem);
	}
	printf("\n");
}

void flushFram()
{
	printf("%s=============\nFlushing Fram\n",RED);
	for(int a=0;a<MAXDEVS;a++)
		write_to_fram(a,false);
	printf("%d meters flushed\n",MAXDEVS);
	printf("=============%s\n",RESETC);
}

void msgCount()
{
	printf("%s=============\nMessage Count\n",RED);
	printf("=============%s\n",RESETC);
	for (int a=0;a<MAXDEVS;a++)
		printf("TotalMsg[%d] sent msgs %d\n",a,totalMsg[a]);
	printf("Controller Total %d\n",sentTotal);
}

void traceFlags()
{
	char s1[60],s2[20];
	char traces[20][20];
	string ss;

	printf("%sTrace Flags:%s",RED,CYAN);

	for (int a=0;a<NKEYS/2;a++)
	if (theConf.traceflag & (1<<a))
		printf("%s ",lookuptable[a]);

	printf("%s\nEnter TRACE FLAG:%s",RED,RESETC);
	fflush(stdout);
	memset(s1,0,sizeof(s1));
	get_string(UART_NUM_0,10,s1);
	memset(s2,0,sizeof(s2));
	for(int a=0;a<strlen(s1);a++)
		s2[a]=toupper(s1[a]);

	if(strlen(s2)<=1)
		return;

	  char *ch;
	  ch = strtok(s2, ",");
	  while (ch != NULL)
	  {
		  ss=string(ch);
	//	  printf("%s\n", ch);
		  ch = strtok(NULL, " ,");

		if(strcmp(ss.c_str(),"NONE")==0)
		{
			theConf.traceflag=0;
			write_to_flash();
			return;
		}

		if(strcmp(ss.c_str(),"ALL")==0)
		{
			theConf.traceflag=0xFFFF;
			write_to_flash();
			return;
		}

		int cualf=cmdfromstring((char*)ss.c_str());
		if(cualf<0)
		{
			printf("Invalid Debug Option\n");
			return;
		}
		if(cualf<NKEYS/2 )
		{
			printf("%s%s+ ",GREEN,lookuptable[cualf]);
			theConf.traceflag |= 1<<cualf;
			write_to_flash();
		}
		else
		{
			cualf=cualf-NKEYS/2;
			printf("%s%s- ",RED,lookuptable[cualf]);
			theConf.traceflag ^= (1<<cualf);
			write_to_flash();
		}

	  }
	  printf("%s\n",RESETC);
}


void waitDelay()
{
	printf("%s==============\nWait Delay(%dms)\n",RED,sendTcp);
	printf("==============%s\n",RESETC);
	sendTcp=askValue((char*)"Value");
}

void msgDelay()
{
	printf("%s=============\nMsg Delay(%dms)\n",RED,qdelay);
	printf("=============%s\n",RESETC);
	qdelay=askValue((char*)"Value");
}

bool compareCmd(cmdRecord r1, cmdRecord r2)
{
	int a=strcmp(r1.comando,r2.comando);
	if (a<0)
		return true;
	else
		return false;
}

void showHelp()
{
    int n = sizeof(cmds)/sizeof(cmds[0]);

    std::sort(cmds, cmds+n, compareCmd);

	printf("%s======CMDS======\n",RED);
	for (int a=0;a<MAXCMDS;a++)
	{
		if(a % 2)
			printf("%s%s ",CYAN,cmds[a].comando);
		else
			printf("%s%s ",GREEN,cmds[a].comando);
	}
	printf("\n======CMDS======%s\n",RESETC);
}

void startSim()
{
	int cuanto=0;
	int len;
	char s1[10];
	if(timeHandle)
	{
		vTaskDelete(timeHandle);
		timeHandle=NULL;
	}
		if(simHandle)
		{
			vTaskDelete(simHandle);
			simHandle=NULL;
			printf("%sEnding Time Simulator and restarting TimeKeeper%s\n",CYAN,RESETC);
			xTaskCreate(&timeKeeper,"tmSim",4096,NULL, 10, &timeHandle);			// Due to Tariffs, we need to check hour,day and month changes
		}
		else
		{
			printf("%sStarting Time Simulator%s\n",GREEN,RESETC);
			printf("%s==============\nHour Delay(ms)\n",RED);
			cuanto=askValue((char*)"Value");
			printf("\nFormat?");
			fflush(stdout);
			len=get_string(UART_NUM_0,10,s1);
			if(len>0)
			{
				if(s1[0]=='y' || s1[0]=='Y')
					formatFram();

			}
			printf("==============%s\n",RESETC);

			xTaskCreate(&timeKeeperSim,"tmSim",4096,(void*)cuanto, 10, &simHandle);			// Due to Tariffs, we need to check hour,day and month changes
		}
}

void init_kbd_commands()
{
	strcpy((char*)&cmds[0].comando,"Config");			cmds[0].code=confStatus;
	strcpy((char*)&cmds[1].comando,"WebReset");			cmds[1].code=webReset;
	strcpy((char*)&cmds[2].comando,"MsgSend");			cmds[2].code=sendStatusMeterAll;
	strcpy((char*)&cmds[3].comando,"MeterStat");		cmds[3].code=meterStatus;
	strcpy((char*)&cmds[4].comando,"MeterTest");		cmds[4].code=meterTest;
	strcpy((char*)&cmds[5].comando,"MeterCount");		cmds[5].code=meterCount;
	strcpy((char*)&cmds[6].comando,"DumpCore");			cmds[6].code=dumpCore;
	strcpy((char*)&cmds[7].comando,"FormatFram");		cmds[7].code=formatFram;
	strcpy((char*)&cmds[8].comando,"ReadFram");			cmds[8].code=readFram;
	strcpy((char*)&cmds[9].comando,"WriteFram");		cmds[9].code=writeFram;
	strcpy((char*)&cmds[10].comando,"LogLevel");		cmds[10].code=logLevel;
	strcpy((char*)&cmds[11].comando,"FramDaysAll");		cmds[11].code=framDays;
	strcpy((char*)&cmds[12].comando,"FramHoursAll");	cmds[12].code=framHours;
	strcpy((char*)&cmds[13].comando,"FramHour");		cmds[13].code=framHourSearch;
	strcpy((char*)&cmds[14].comando,"FramDay");			cmds[14].code=framDaySearch;
	strcpy((char*)&cmds[15].comando,"FramMonth");		cmds[15].code=framMonthSearch;
	strcpy((char*)&cmds[16].comando,"Flush");			cmds[16].code=flushFram;
	strcpy((char*)&cmds[17].comando,"MessageCount");	cmds[17].code=msgCount;
	strcpy((char*)&cmds[18].comando,"WaitDelay");		cmds[18].code=waitDelay;
	strcpy((char*)&cmds[19].comando,"MsgDelay");		cmds[19].code=msgDelay;
	strcpy((char*)&cmds[20].comando,"Help");			cmds[20].code=showHelp;
	strcpy((char*)&cmds[21].comando,"Trace");			cmds[21].code=traceFlags;
	strcpy((char*)&cmds[22].comando,"Simulation");		cmds[22].code=startSim;
	strcpy((char*)&cmds[23].comando,"FramMonthsAll");	cmds[23].code=framMonths;
	strcpy((char*)&cmds[24].comando,"FramMonthHours");	cmds[24].code=framMonthsHours;
}

void kbd(void *arg)
{
	int len,cualf;
	uart_port_t uart_num = UART_NUM_0 ;
	char kbdstr[30],oldcmd[30];
	string ss;

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

	init_kbd_commands();


	while(1)
	{
			len=get_string(UART_NUM_0,10,kbdstr);
			if(len<=0)
				strcpy(kbdstr,oldcmd);

			cualf=kbdCmd(string(kbdstr));
			if(cualf>=0)
			{
				(*cmds[cualf].code)();
				strcpy(oldcmd,kbdstr);
			}
		vTaskDelay(100 / portTICK_PERIOD_MS);
	}
}

