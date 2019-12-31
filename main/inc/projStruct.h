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
	 bool saveit,lowThresf;
	 char serialNumber[20];
	 u16 beatsPerkW,curMonth,curMonthRaw,curDay,curDayRaw;
	 u32 curLife,curCycle,lastKwHDate,msNow, minamps,maxamps,currentBeat,vanMqtt,ampTime,beatSave,beatSaveRaw;
	 u8 curHour,cycleMonth,curHourRaw,pos,pin,pinB;
} meterType;

typedef struct config {
    bool 	corteSent[MAXDEVS];
    char 	medidor_id[MAXDEVS][MAXCHARS],meterName[MAXCHARS];
    time_t 	lastUpload,lastTime,preLastTime,bornDate[MAXDEVS],lastBootDate;
    u16 	beatsPerKw[MAXDEVS],bootcount,bounce[MAXDEVS],diaDeCorte[MAXDEVS],lastResetCode;
    u16 	ssl,traceflag; // to make it mod 16 for AES encryption
    u32 	bornKwh[MAXDEVS],centinel;
    u8 		configured[MAXDEVS],active;
} config_flash;

typedef struct framq{
	int whichMeter;
	bool addit;
}framMeterType;

typedef struct pcntev{
    int unit;  // the PCNT unit that originated an interrupt
    uint32_t status; // information on the event type that caused the interrupt
} pcnt_evt_t;

typedef enum displayModeType {DISPLAYPULSES,DISPLAYKWH,DISPLAYUSER,DISPLAYALL,DISPLAYALLK,DISPLAYAMPS,DISPLAYRSSI,DISPLAYNADA} displayModeType;
typedef enum displayType {NODISPLAY,DISPLAYIT} displayType;
typedef enum overType {NOREP,REPLACE} overType;
typedef enum resetType {ONCE,TIMER,REPEAT,TIMEREPEAT} resetType;
typedef enum sendType {NOTSENT,SENT} sendType;
enum debugflags{BOOTD,WIFID,MQTTD,PUBSUBD,OTAD,CMDD,WEBD,GEND,MQTTT,FRMCMD,INTD,FRAMD,MSGD,TIMED,SIMD};


typedef struct internalHost{
	char		meterid[20];
	uint32_t	startKwh;
	uint16_t	bpkwh;
	uint16_t	diaCorte;
	uint16_t	tariff;
	bool		valid;
}host_t;

#endif
