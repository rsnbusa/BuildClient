#ifndef structs_h
#define structs_h

#include "defines.h"

//
//typedef struct
//{
//    uint8_t       state;
//    uint8_t		  free;
//    uint16_t      meter;
//    uint32_t      life;
//    uint16_t      month;
//    uint16_t      day;
//    uint16_t      cycle;
//    uint16_t      hora;
//    uint16_t      mesg,diag,horag;
//    uint16_t      yearg;
//} meterspi;
//
//typedef union
//{
//	meterspi medidor;
//	char dataint[40];
//}scratchTypespi ;
//
//typedef struct scratchType{
//    u16      state;
//    u16      meter;
//    u32      life;
//    u16      month;
//    u16      day;
//    u16      cycle;
//    u16      hora;
//    u16      mesg,diag,horag;
//    u16      yearg;
//} scratchType;

//typedef struct intPin{
//	char		serialNumber[20];
//	gpio_num_t	pin;
//	u8			pos;
//	u32 		startTimePulse,beatSave,beatSaveRaw,vanMqtt,currentKwH;
//	u32			currentBeat,msNow,livingPulse,livingCount,timestamp,pulse,startConn;
//}meterType;

typedef struct mqttMsg{
	u16		cmd;
	char	mensaje[500];
}cmdType;


typedef struct meterType{
	 char serialNumber[20];
	 uint8_t meterid,state,pos,pin;
	 uint32_t timestamp,startConn,pulse;
	 uint32_t currentBeat,oldbeat,vanMqtt;
	 u16 elpin;
	 bool saveit;
	 u32 msNow, minamps,maxamps;
	 u16 curMonth,curMonthRaw,curDay,curDayRaw,beatSave,beatSaveRaw;
	 u8 curHour,cycleMonth,curHourRaw;
	 u32 curLife,curCycle,lastKwHDate;
	 u16 beatsPerkW,maxLoss;
	 u32 livingPulse,livingCount,startTimePulse;
} meterType;

typedef struct config {
    u32 	centinel;
    char 	ssid[5][MAXCHARS],pass[5][10],meterName[MAXCHARS];
    u8 		working;
    time_t 	lastUpload,lastTime,preLastTime,bornDate[MAXDEVS];
    char 	mqtt[MAXCHARS];
    char 	domain[MAXCHARS];
    u16 	ucount;
    u16 	bootcount;
    char 	actualVersion[20];
    char 	groupName[MAXCHARS];
    u16 	DISPTIME;
    u16 	beatsPerKw[MAXDEVS];
    char 	medidor_id[MAXDEVS][MAXCHARS];
    u16 	bounce[MAXDEVS];
    u16 	lastResetCode;
    u16 	MODDISPLAY[MAXDEVS];
    u8 		activeMeters;
    u32 	bornKwh[MAXDEVS];
    u16 	diaDeCorte[MAXDEVS];
    bool 	corteSent[MAXDEVS];
    u8 		breakers[MAXDEVS];
    u16 	mqttport;
    char 	mqttUser[MAXCHARS];
    char 	mqttPass[MAXCHARS];
    u16 	displayFlag;
    u16 	ssl;
    u8 		lastSSID;
    u16 	traceflag; // to make it mod 16 for AES encryption
    u32 	responses,cmdsIn;
    u8 		lastMeter;
    char 	serial[MAXDEVS][20];
    u8		statusSend;
} config_flash;

#endif
