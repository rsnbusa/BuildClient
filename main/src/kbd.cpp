#define GLOBAL
#include "defines.h"
#include "includes.h"
#include "globals.h"
#include "projStruct.h"
#include <bits/stdc++.h>

#define KBDT		"\e[36m[KBD]\e[0m"
#define MAXCMDSK	20

extern void delay(uint32_t a);
extern void write_to_flash();
extern void write_to_fram(u8 meter,bool addit);

using namespace std;

typedef void (*functkbd)(string ss);

typedef struct cmd_kbd_t{
    char comando[20];
    functkbd code;
    string help;
}cmdRecord_t;

cmdRecord_t cmdds[MAXCMDSK];

uint16_t date2days(uint16_t y, uint8_t m, uint8_t d) {
	uint8_t daysInMonth [12] ={ 31,28,31,30,31,30,31,31,30,31,30,31 };
	uint16_t days = d;
	for (uint8_t i = 0; i < m; i++)
		days += daysInMonth[ i];
	if (m == 1 && y % 4 == 0)
		++days;
	return days ;

}

cJSON *makeJson(string ss)
{
	cJSON *root=cJSON_CreateObject();

	size_t start=0,found,fkey;
	int van=0;
	string cmdstr,theK;
	std::locale loc;

	if(ss=="")
		return NULL;

	do{
		found=ss.find(" ",start);
		if (found!=std::string::npos)
		{
			cmdstr=ss.substr(start,found-start);
			fkey=cmdstr.find_first_of("=:");
			if(fkey!=std::string::npos)
			{
				theK=cmdstr.substr(0,fkey);
				for (size_t i=0; i<theK.length(); ++i)
				    theK[i]= std::toupper(theK[i],loc);

				string valor=cmdstr.substr(fkey+1,cmdstr.length());

				bool isdig=true;
				for (size_t i=0; i<valor.length(); ++i)
				{
					if(!isdigit(valor[i]))
					{
							 isdig=false;
							 break;
					}
				}
				if(!isdig)
					cJSON_AddStringToObject(root,theK.c_str(),valor.c_str());
				else
					cJSON_AddNumberToObject(root,theK.c_str(),atoi(valor.c_str()));

			}
			else
				return NULL; //error is drastic
			start=found+1;
			van++;
		}

	} while(found!=std::string::npos);

//	char *lmessage=cJSON_Print(root);
//	if(lmessage)
//	{
//		printf("Root %s\n",lmessage);
//		free(lmessage);
//	}
	return root;
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

	for (int i=0; i <MAXCMDSK; i++)
	{
		s1=string(cmdds[i].comando);

		for(int a=0;a<s1.length();a++)
			s2[a]=toupper(s1[a]);
	//	printf("Cmd =%s=\n",s2.c_str());


		if(strstr(s2.c_str(),skey.c_str())!=NULL)
			return i;
		s1=s2="";
	}
	return -1;
}


int askMeter(cJSON *params) // json should be METER
{
	int len=-1;
	char s1[20];

	if(params)
	{
		cJSON *jentry= cJSON_GetObjectItem(params,"METER");
		if(jentry)
			len=jentry->valueint;
	}
	else
	{
		printf("Meter:");
		fflush(stdout);
		len=get_string(UART_NUM_0,10,s1);
		if(len<=0)
		{
			printf("\n");
			return -1;
		}
		len= atoi(s1);
	}
		if(len<MAXDEVS)
			return len;
		else
		{
			printf("%sMeter out of range 0-%d%s\n",MAGENTA,MAXDEVS-1,RESETC);
			return -1;
		}
}

int askMonth(cJSON *params)	//JSON should be MONTH
{
	int len=-1;
	char s1[20];

	if(params)
	{
		cJSON *jentry= cJSON_GetObjectItem(params,"MONTH");
		if(jentry)
			len=jentry->valueint;
	}
	else
	{
		printf("Month(0-11):");
		fflush(stdout);
		len=get_string(UART_NUM_0,10,s1);
		if(len<=0)
		{
			printf("\n");
			return -1;
		}
		len= atoi(s1);
	}
	if(len<12)
		return len;
	else
	{
		printf("%sMonth out of range 0-11%s\n",MAGENTA,RESETC);
		return -1;
	}
}

//int askHour(cJSON *params)	//json should be HOUR
//{
//	int len=-1;
//	char s1[20];
//
//	if(params)
//		{
//			cJSON *jentry= cJSON_GetObjectItem(params,"HOUR");
//			if(jentry)
//				len=jentry->valueint;
//		}
//	else
//	{
//		printf("Hour(0-23):");
//		fflush(stdout);
//		len=get_string(UART_NUM_0,10,s1);
//		if(len<=0)
//		{
//			printf("\n");
//			return -1;
//		}
//		len= atoi(s1);
//	}
//	if(len<24)
//		return len;
//	else
//	{
//		printf("%sHour out of range 0-23%s\n",MAGENTA,RESETC);
//		return -1;
//	}
//}

int askDay(cJSON *params,int month)	// JSON should be DAY
{
	int len=-1;
	char s1[20];

	if(params)
		{
			cJSON *jentry= cJSON_GetObjectItem(params,"DAY");
			if(jentry)
				len=jentry->valueint;
		}
	else
	{
		printf("Day(0-%d):",daysInMonth[month]-1);
		fflush(stdout);
		len=get_string(UART_NUM_0,10,s1);
		if(len<=0)
		{
			printf("\n");
			return -1;
		}
		len= atoi(s1);
	}
		if(len<daysInMonth[month])
			return len;
		else
		{
			printf("%sDay out of range 0-%d%s\n",MAGENTA,daysInMonth[month]-1,RESETC);
			return -1;
		}
}

int askValue(const char *title,cJSON *params)	//JSON should be VAL
{
	int len;
	char s1[20];

	if(params)
		{
			cJSON *jentry= cJSON_GetObjectItem(params,"VAL");
			if(jentry)
				return(jentry->valueint);
			else
				return -1;
		}
	else
	{
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
	return -1;
}

void confStatus(string ss)
{
	struct tm timeinfo;
	char strftime_buf[64];

	printf("%s====================\nConfiguration Status\n",RED);
	printf("%s   %s\n",LRED,theConf.active?"RunMode":"SetupMode");
	if(fram._framInitialised)
		printf("%sFram Manufacturer:%04x ProdId:%04x Speed:%d%s\n",GREEN,fram.manufID,fram.prodId,fram.maxSpeed,LRED);
	else
		printf("%sNO FRAM%s\n",RED,LRED);
	printf("MtM=%s CommMgr=%s Password=%s\n",theConf.meterName, theConf.mgrName,theConf.mgrPass);

	localtime_r(&theConf.lastBootDate, &timeinfo);
	strftime(strftime_buf, sizeof(strftime_buf), "%c", &timeinfo);
	printf("%sConfiguration BootCount %d LReset %x Date %s RunStatus[%s] SendD %d Trace %x\n",YELLOW,theConf.bootcount,theConf.lastResetCode,strftime_buf,
			theConf.active?"Run":"Setup",theConf.sendDelay,theConf.traceflag);

	for (int a=0;a<MAXDEVS;a++)
	{
		localtime_r(&theConf.bornDate[a], &timeinfo);
		strftime(strftime_buf, sizeof(strftime_buf), "%c", &timeinfo);
		printf("%sMeter[%d] Serial %s BPW %d Born %s BornkWh %d Active %s\n",(a % 2)?CYAN:GREEN,a,theConf.medidor_id[a],theConf.beatsPerKw[a],
				strftime_buf,theConf.bornKwh[a],theConf.configured[a]==3?"Yes":"No");
	}
	printf("====================%s\n\n",RESETC);

}

void meterStatus(string ss)
{
	uint32_t valr;
	cJSON *params=NULL;
	int meter=-1;

	params=makeJson(ss);

	printf("%s============\nMeter status\n",KBDT);
	meter=askMeter(params);
	if(meter<0||meter>MAXDEVS-1)
	{
		printf("Meter %d out of range\n",meter);
		return;
	}

	printf("============%s\n\n",YELLOW);

	if(xSemaphoreTake(framSem, portMAX_DELAY/  portTICK_RATE_MS))
	{
	//	printf("Meter Compare test Year %d Month %d Day %d Hour %d YDAY %d\n",yearg,mesg,diag,horag,yearDay);
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
		fram.read_day(meter,yearDay,(uint8_t*)&valr);
		printf("Day[%d]Mes[%d]Dia[%d]=%d %d\n",meter,mesg,diag,valr,theMeters[meter].curDay);
		valr=0;
		fram.read_dayraw(meter,yearDay,(uint8_t*)&valr);
		printf("DayRaw[%d]Mes[%d]Dia[%d]=%d %d\n",meter,mesg,diag,valr,theMeters[meter].curDayRaw);
		xSemaphoreGive(framSem);
		printf("%s============\n",KBDT);

	}
}

void sendDelay(string ss)
{
	cJSON *params=NULL;
	int val=0;
	char textl[40];

	params=makeJson(ss);
	sprintf(textl,"Send Time(%dms)",theConf.sendDelay);
	val=askValue(textl,params);
	if(val<0)
		return;
	theConf.sendDelay=val;
	write_to_flash();

}

void meterTest(string ss)
{
	cJSON *params=NULL;
	int val=0,meter=0;

	params=makeJson(ss);

	meter=askMeter(params);
	if(meter<0||meter>MAXDEVS-1)
	{
		printf("Meter %d out of range\n",meter);
		return;
	}
		val=askValue((char*)"Value",params);


	printf("%s================\nTest Write Meter\n",KBDT);


	printf("================%s\n\n",RESETC);

	uint32_t valr;

	if(xSemaphoreTake(framSem, portMAX_DELAY/  portTICK_RATE_MS))
	{
		printf("Fram Write Meter %d Year %d Month %d Day %d Hour %d Value %d YDAY %d\n",meter,yearg,mesg,diag,horag,val,yearDay);

		fram.write_beat(meter,val);
		fram.write_lifekwh(meter,val);
		fram.write_lifedate(meter,val);
		fram.write_month(meter,mesg,val);
		fram.write_monthraw(meter,mesg,val);
		fram.write_day(meter,yearDay,val);
		fram.write_dayraw(meter,yearDay,val);

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
		fram.read_day(meter,yearDay,(uint8_t*)&valr);
		printf("Day[%d]=%d\n",meter,valr);
		fram.read_dayraw(meter,yearDay,(uint8_t*)&valr);
		printf("DayRaw[%d]=%d\n",meter,valr);
		xSemaphoreGive(framSem);
	}
}

void webReset(string ss)
{
	theConf.active=!theConf.active;
	printf("%sWeb %s\n",KBDT,theConf.active?"RunConf":"SetupConf");
	printf("===========%s\n",KBDT);
	if(theConf.active)
		memset(theConf.configured,3,sizeof(theConf.configured));
	else
		memset(theConf.configured,0,sizeof(theConf.configured));
	write_to_flash();
	esp_restart();

}

void meterCount(string ss)
{
	time_t timeH;

	int tots=0;
	printf("%s==============\nMeter Counters%s\n",KBDT,RESETC);

	for (int a=0;a<MAXDEVS;a++)
	{
		tots+=theMeters[a].currentBeat;
		printf("%sMeter[%d]=%s Beats %d kWh %d BPK %d\n",(a % 2)?GREEN:CYAN,a,RESETC,theMeters[a].currentBeat,theMeters[a].curLife,
				theMeters[a].beatsPerkW);
	}
	printf("%sTotal Pulses rx=%s %d (%s)\n",RED,RESETC,totalPulses,tots==totalPulses?"Ok":"No");
	fram.readMany(FRAMDATE,(uint8_t*)&timeH,sizeof(timeH));//last known date
	printf("Last Date %s",ctime(&timeH));
	printf("==============%s\n\n",KBDT);

}

void dumpCore(string ss)
{

	u8 *p;
	printf("%sDumping core ins 3 secs%s\n",RED,RESETC);
	delay(3000);
	p=0;
	*p=0;
}

void formatFram(string ss)
{
	cJSON *params=NULL;
	int val=0;

	params=makeJson(ss);

	if(params)
	{
		cJSON *jval= cJSON_GetObjectItem(params,"VAL");
		if(jval)
			val=jval->valueint;
		cJSON_Delete(params);
	}
	else
	{

		printf("%s===========\nFormat FRAM\n",KBDT);

		val=askValue((char*)"Init value",params);
		printf("===========%s\n\n",KBDT);
	}
	if(val<0)
		return;

	fram.format(val,NULL,100,true);
	printf("Format done...rebooting\n");

	memset(&theConf.medidor_id,0,sizeof(theConf.medidor_id));
	memset(&theConf.beatsPerKw,0,sizeof(theConf.beatsPerKw));
	memset(&theConf.configured,0,sizeof(theConf.configured));
	memset(&theConf.bornKwh ,0,sizeof(theConf.bornKwh));
	memset(&theConf.bornDate ,0,sizeof(theConf.bornDate));

	write_to_flash();

	esp_restart();
}


void readFram(string ss)
{
	cJSON *params=NULL;
	int framAddress=0,fueron=0;
	params=makeJson(ss);

	if(params)
	{
		cJSON *jmeter= cJSON_GetObjectItem(params,"ADDR");
		if(jmeter)
			framAddress=jmeter->valueint;
		cJSON *jval= cJSON_GetObjectItem(params,"COUNT");
		if(jval)
			fueron=jval->valueint;
		cJSON_Delete(params);
	}
	else
	{
		printf("%s=========\nRead FRAM\n",KBDT);
		framAddress=askValue((char*)"Address",params);
		fueron=askValue((char*)"Count",params);
		printf("=========%s\n\n",KBDT);
	}
	if(fueron<0 || framAddress<0)
		return;

	fram.readMany(framAddress,(uint8_t*)tempb,fueron);
	for (int a=0;a<fueron;a++)
		printf("%02x-",tempb[a]);
	printf("\n");

}

void writeFram(string ss)
{
	cJSON *params=NULL;
	int fueron=0,framAddress=0;
	params=makeJson(ss);

	if(params)
	{
		cJSON *jmeter= cJSON_GetObjectItem(params,"ADDR");
		if(jmeter)
			framAddress=jmeter->valueint;
		cJSON *jval= cJSON_GetObjectItem(params,"VAL");
		if(jval)
			fueron=jval->valueint;
		cJSON_Delete(params);
	}
	else
	{
		printf("%s==========\nWrite FRAM\n",KBDT);
		framAddress=askValue((char*)"Address",params);
		fueron=askValue((char*)"Value",params);
		printf("==========%s\n\n",KBDT);
	}
	if(fueron<0 || framAddress<0)
		return;

	fram.write8(framAddress,fueron);

}

void logLevel(string ss)
{
	cJSON *params=NULL;
	char s1[10];

	params=makeJson(ss);

	if(params)
	{
		cJSON *jmeter= cJSON_GetObjectItem(params,"LEVEL");
		if(jmeter)
			s1[0]=jmeter->valuestring[0];
		cJSON_Delete(params);
	}
	else
	{

		printf("%sLOG level:(N)one (I)nfo (E)rror (V)erbose (W)arning:",KBDT);
		fflush(stdout);
		int len=get_string(UART_NUM_0,10,s1);
		if(len<=0)
		{
			printf("\n");
			return;
		}
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

void framDays(string ss)
{
	uint16_t valor;
	uint32_t tots=0;
	cJSON *params=NULL;
	int meter=0,month=0;

	params=makeJson(ss);

	if(params)
	{
		cJSON *jmeter= cJSON_GetObjectItem(params,"METER");
		if(jmeter)
			meter=jmeter->valueint;
		cJSON *jval= cJSON_GetObjectItem(params,"MONTH");
		if(jval)
			month=jval->valueint;
		cJSON_Delete(params);
	}
	else
	{

		printf("%s==================\nFram Days in Month\n",KBDT);
		meter=askMeter(params);
		if(meter<0)
			return;
		month=askMonth(params);
		if(month<0)
			return;
	}
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
		printf("==================%s\n",KBDT);
	}

}

void framDaySearch(string ss)
{
	uint16_t valor;
	cJSON *params=NULL;
	int meter=0,month=0,dia=0;

	params=makeJson(ss);

	if(params)
	{
		cJSON *jmeter= cJSON_GetObjectItem(params,"METER");
		if(jmeter)
			meter=jmeter->valueint;
		cJSON *jval= cJSON_GetObjectItem(params,"MONTH");
		if(jval)
			month=jval->valueint;
		jval= cJSON_GetObjectItem(params,"DAY");
		if(jval)
			dia=jval->valueint;
		cJSON_Delete(params);
	}
	else
	{
		printf("%s===============\nFram Day Search\n",KBDT);

		meter=askMeter(params);
		if(meter<0)
			return;
		month=askMonth(params);
		if(month<0)
			return;
		dia=askDay(params,month);
		if(dia<0)
			return;
	}

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
	printf("===============%s\n",KBDT);

}

void framMonthSearch(string ss)
{
	uint16_t valor;
	cJSON *params=NULL;
	int meter=0,month=0;

	params=makeJson(ss);

	if(params)
	{
		cJSON *jmeter= cJSON_GetObjectItem(params,"METER");
		if(jmeter)
			meter=jmeter->valueint;
		cJSON *jval= cJSON_GetObjectItem(params,"MONTH");
		if(jval)
			month=jval->valueint;
		cJSON_Delete(params);
	}
	else
	{
		printf("%s=================\nFram Month Search\n",KBDT);

		meter=askMeter(params);
		if(meter<0)
			return;
		month=askMonth(params);
		if(month<0)
			return;
	}
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
	printf("\n=================%s\n",KBDT);
}

void framMonths(string ss)
{
	uint16_t valor;
	uint32_t tots=0;
	cJSON *params=NULL;
	int meter=0;

	params=makeJson(ss);

	if(params)
	{
		cJSON *jmeter= cJSON_GetObjectItem(params,"METER");
		if(jmeter)
			meter=jmeter->valueint;
		cJSON_Delete(params);
	}
	else
	{
		printf("%s=================\nFram Month All\n",KBDT);
		meter=askMeter(params);
	}

	if(meter<0)
		return;

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
		printf("=================%s\n",KBDT);

		xSemaphoreGive(framSem);
	}
	printf("\n");
}

void flushFram(string ss)
{
	printf("%s=============\nFlushing Fram\n",KBDT);
	for(int a=0;a<MAXDEVS;a++)
		write_to_fram(a,false);
	printf("%d meters flushed\n",MAXDEVS);
	printf("=============%s\n",KBDT);
}

void msgCount(string ss)
{
	printf("%s=============\nMessage Count\n",KBDT);
	for (int a=0;a<MAXDEVS;a++)
		//printf("TotalMsg[%d] sent msgs %d\n",a,totalMsg[a]);
	printf("TotalMsg[%d] sent msgs %d\n",a,1);
	printf("Controller Total %d\n",sentTotal);
	printf("=============%s\n",KBDT);

}

void traceFlags(string sss)
{
	char s1[60],s2[20];
	string ss;
	std::locale loc;

	printf("%sTrace Flags:%s",KBDT,CYAN);

	for (int a=0;a<NKEYS/2;a++)
	if (theConf.traceflag & (1<<a))
		printf("%s ",lookuptable[a]);
	printf("\n");

	if(sss=="")
	{
		printf("%s\nEnter TRACE FLAG:%s",RED,RESETC);
		fflush(stdout);
		memset(s1,0,sizeof(s1));
		get_string(UART_NUM_0,10,s1);
		memset(s2,0,sizeof(s2));
		for(int a=0;a<strlen(s1);a++)
			s2[a]=toupper(s1[a]);

		if(strlen(s2)<=1)
			return;
	}
	else
		strcpy(s2,sss.c_str());


	  char *ch;
	  ch = strtok(s2, " ");
	  while (ch != NULL)
	  {
		  ss=string(ch);
		//  printf("[%s]\n", ch);
		  ch = strtok(NULL, " ");

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
	  printf("%s\n",KBDT);
}

void waitDelay()
{
	printf("%s==============\nWait Delay(%dms)\n",RED,sendTcp);
	printf("==============%s\n",RESETC);
//	sendTcp=askValue((char*)"Value");
}

void msgDelay()
{
	printf("%s=============\nMsg Delay(%dms)\n",RED,qdelay);
	printf("=============%s\n",RESETC);
//	qdelay=askValue((char*)"Value");
}


bool compareCmd(cmdRecord_t r1, cmdRecord_t r2)
{
	int a=strcmp(r1.comando,r2.comando);
	if (a<0)
		return true;
	else
		return false;
}

void showHelp(string ss)
{
	cJSON *params=NULL;
	bool longf=false;
	params=makeJson(ss);

	if(params)
	{
		cJSON *jmeter= cJSON_GetObjectItem(params,"LONG");
		if(jmeter)
			longf=true;
		cJSON_Delete(params);
	}
    int n = sizeof(cmdds)/sizeof(cmdds[0]);

    std::sort(cmdds, cmdds+n, compareCmd);

	printf("%s======CMDS======\n",RED);
	for (int a=0;a<MAXCMDSK;a++)
	{
		if(a % 2)
		{
			printf("%s%s ",CYAN,cmdds[a].comando);
			if (longf)
				printf("(%s)\n",cmdds[a].help.c_str());
		}
		else
		{
			printf("%s%s ",GREEN,cmdds[a].comando);
			if (longf)
				printf("(%s)\n",cmdds[a].help.c_str());
		}
	}
	printf("\n======CMDS======%s\n",RESETC);
}

static void zeroKeys(string ss)
{
	char s1[10];
	int len;

	printf("Reset Key sure?");
	fflush(stdout);
	len=get_string(UART_NUM_0,10,s1);
	if(len<=0)
		return;
	memset(theConf.lkey,65,AESL);
	write_to_flash();
	esp_aes_setkey( &ctx, (unsigned char*)theConf.lkey, 256 );
	printf("Keys erased\n");
}

static void cryptoOption(string ss)
{
	char s1[10];
	int pos;

	cJSON *params=makeJson(ss);

	if(params)
	{
		cJSON *dly= cJSON_GetObjectItem(params,"MODE");
		if(dly)
			theConf.crypt=dly->valueint;
		cJSON_Delete(params);
	}
	else
	{
		printf("%sCrypto(%d):%s",MAGENTA,theConf.crypt,RESETC);
		fflush(stdout);
		pos=get_string(UART_NUM_0,10,s1);
		if(pos<0)
			return;
		theConf.crypt=atoi(s1);
	}
	write_to_flash();
}
void init_kbd_commands()
{
	strcpy((char*)&cmdds[0].comando,"Config");			cmdds[ 0].code=confStatus;			cmdds[0].help="SHORT";
	strcpy((char*)&cmdds[1].comando,"WebReset");		cmdds[ 1].code=webReset;			cmdds[1].help="";
	strcpy((char*)&cmdds[2].comando,"MeterStat");		cmdds[ 2].code=meterStatus;			cmdds[2].help="METER";
	strcpy((char*)&cmdds[3].comando,"MeterCount");		cmdds[ 3].code=meterCount;			cmdds[3].help="";
	strcpy((char*)&cmdds[4].comando,"DumpCore");		cmdds[ 4].code=dumpCore;			cmdds[4].help="";
	strcpy((char*)&cmdds[5].comando,"FormatFram");		cmdds[ 5].code=formatFram;			cmdds[5].help="VAL";
	strcpy((char*)&cmdds[6].comando,"ReadFram");		cmdds[ 6].code=readFram;			cmdds[6].help="COUNT";
	strcpy((char*)&cmdds[7].comando,"WriteFram");		cmdds[ 7].code=writeFram;			cmdds[7].help="ADDR VAL";
	strcpy((char*)&cmdds[8].comando,"LogLevel");		cmdds[ 8].code=logLevel;			cmdds[8].help="None Info Err Verb Warn";
	strcpy((char*)&cmdds[9].comando,"FramDaysAll");		cmdds[ 9].code=framDays;			cmdds[9].help="METER MONTH";
	strcpy((char*)&cmdds[10].comando,"FramDay");		cmdds[10].code=framDaySearch;		cmdds[10].help="METER MONTH";
	strcpy((char*)&cmdds[11].comando,"FramMonth");		cmdds[11].code=framMonthSearch;		cmdds[11].help="METER MONTH";
	strcpy((char*)&cmdds[12].comando,"Flush");			cmdds[12].code=flushFram;			cmdds[12].help="";
	strcpy((char*)&cmdds[13].comando,"MessageCount");	cmdds[13].code=msgCount;			cmdds[13].help="";
	strcpy((char*)&cmdds[14].comando,"Help");			cmdds[14].code=showHelp;			cmdds[14].help="LONG";
	strcpy((char*)&cmdds[15].comando,"Trace");			cmdds[15].code=traceFlags;			cmdds[15].help="NONE ALL BOOTD WIFID MQTTD PUBSUBD OTAD CMDD WEBD GEND MQTTT FRMCMD INTD FRAMD MSGD TIMED SIMD HOSTD";
	strcpy((char*)&cmdds[16].comando,"FramMonthsAll");	cmdds[16].code=framMonths;			cmdds[16].help="METER";
	strcpy((char*)&cmdds[17].comando,"StatusDelay");	cmdds[17].code=sendDelay;			cmdds[17].help="SENDDELAY";
	strcpy((char*)&cmdds[18].comando,"ZeroK");			cmdds[18].code=zeroKeys;			cmdds[18].help="ZKeys";
	strcpy((char*)&cmdds[19].comando,"Crypto");			cmdds[19].code=cryptoOption;		cmdds[18].help="SetCrypt";

}

void kbd(void *arg)
{
	int len,cualf;
	uart_port_t uart_num = UART_NUM_0 ;
	char kbdstr[100],oldcmd[100];
	string ss;
	std::locale loc;


	//cJSON *params;

	uart_config_t uart_config = {
			.baud_rate = 460800,
			.data_bits = UART_DATA_8_BITS,
			.parity = UART_PARITY_DISABLE,
			.stop_bits = UART_STOP_BITS_1,
			.flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
			.rx_flow_ctrl_thresh = 122,
			.use_ref_tick=false
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
			ss=string(kbdstr);

			 size_t found = ss.find(" ");
			  if (found!=std::string::npos)
			  {
				  strcpy(kbdstr,ss.substr(0,found).c_str());	//Just cmd
				  ss.erase(0,found+1);
				  ss+=" ";
			  }
			  else ss="";

			  for (size_t i=0; i<ss.length(); ++i)
			  			ss[i]= std::toupper(ss[i],loc);


			cualf=kbdCmd(string(kbdstr));
			if(cualf>=0)
			{
				(*cmdds[cualf].code)(ss);
				strcpy(oldcmd,kbdstr);
			}

		vTaskDelay(100 / portTICK_PERIOD_MS);
	}
}

