#ifndef MAIN_INC_GLOBALS_H_
#define MAIN_INC_GLOBALS_H_

#ifdef GLOBAL
#define EXTERN extern
#else
#define EXTERN
#endif
//// do not put anything the initializers creates the space for u like char *TAG since it will NOT BE DONE without a value and cnanot have
// a value because it will have syntax error in other program files as Duplicate reference
#include "projStruct.h"
#include "includes.h"
using namespace std;

EXTERN bool								conn,framFlag;
EXTERN char								lookuptable[NKEYS][10],deb,tempb[1200],theMac[20],them[6],TAG[20];
EXTERN config_flash						theConf;
EXTERN EventGroupHandle_t 				wifi_event_group;
EXTERN FramSPI							fram;
EXTERN gpio_config_t 					io_conf;
EXTERN host_t							setupHost[MAXDEVS];
EXTERN int								gsock,starthora,startday,startmonth,startyear,WIFI_BIT;
EXTERN meterType						theMeters[MAXDEVS];
EXTERN nvs_handle 						nvshandle;
EXTERN QueueHandle_t 					mqttQ,mqttR,framQ,pcnt_evt_queue;
EXTERN SemaphoreHandle_t 				wifiSem,framSem,I2CSem;
EXTERN TaskHandle_t						webHandle,timeHandle,simHandle;
EXTERN u16                  			diaHoraTarifa,yearDay,oldYearDay,llevoMsg,waitQueue,mesg,oldMesg,diag,oldDiag,horag,oldHorag,yearg,wDelay,qdelay,addressBytes;
EXTERN u32								sentTotal,sendTcp,totalMsg[MAXDEVS],theMacNum,totalPulses,oldCurBeat[MAXDEVS],oldCurLife[MAXDEVS];
EXTERN u8								qwait,theBreakers[MAXDEVS],daysInMonth[12],lastalign,lastFont;

#ifdef DISPLAY
EXTERN I2C								miI2C;
EXTERN i2ctype 							i2cp;
#ifdef GLOBAL
EXTERN	SSD1306             			display;
#else
EXTERN SSD1306 							display(0x3c, &miI2C);
#endif
#endif

#endif /* MAIN_INC_GLOBALS_H_ */
