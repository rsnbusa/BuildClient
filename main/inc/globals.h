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
EXTERN char								lookuptable[NKEYS][10],deb,tempb[1200],theMac[20],them[6];
EXTERN config_flash						theConf;
EXTERN int 								WIFI_BIT;
EXTERN FramSPI							fram;
EXTERN gpio_config_t 					io_conf;
EXTERN int								gsock,starthora,startday,startmonth,startyear;
EXTERN meterType						theMeters[MAXDEVS];
EXTERN nvs_handle 						nvshandle;
EXTERN QueueHandle_t 					mqttQ,mqttR,framQ,pcnt_evt_queue;
EXTERN SemaphoreHandle_t 				wifiSem,framSem;
EXTERN char 							TAG[20];
EXTERN EventGroupHandle_t 				wifi_event_group;
EXTERN u16                  			diaHoraTarifa,yearDay,oldYearDay,llevoMsg,waitQueue,mesg,oldMesg,diag,oldDiag,horag,oldHorag,yearg,wDelay,qdelay,addressBytes;
EXTERN u32								sentTotal,sendTcp,totalMsg[MAXDEVS],theMacNum,totalPulses;
EXTERN u8								qwait,theBreakers[MAXDEVS],daysInMonth[12];
EXTERN host_t							setupHost[MAXDEVS];
EXTERN TaskHandle_t						webHandle;


#endif /* MAIN_INC_GLOBALS_H_ */
