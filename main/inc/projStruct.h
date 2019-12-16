#ifndef structs_h
#define structs_h

#include "defines.h"

typedef struct mqttMsg{
	u16		cmd;
	char	mensaje[500];
}cmdType;

typedef struct loginTarif{
	time_t thedate;
	uint16_t theTariff;
} loginT;

typedef struct meterType{
	 char serialNumber[20];
	 uint8_t meterid,state,pos,pin,pinB;
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
    time_t 	lastUpload,lastTime,preLastTime,bornDate[MAXDEVS],lastBootDate;
    u16 	bootcount;
    u16 	beatsPerKw[MAXDEVS];
    u16 	bounce[MAXDEVS];
    u16 	lastResetCode;
    u32 	bornKwh[MAXDEVS];
    u16 	diaDeCorte[MAXDEVS];
    bool 	corteSent[MAXDEVS];
    u8 		breakers[MAXDEVS];
    u16 	ssl;
    u16 	traceflag; // to make it mod 16 for AES encryption
    char 	meterName[MAXCHARS];
    char 	medidor_id[MAXDEVS][MAXCHARS];
    u8		statusSend;
} config_flash;

#endif
