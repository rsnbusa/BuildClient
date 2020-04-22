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

EXTERN bool								conn,framFlag,framGuard,rsyncf;
EXTERN char								lookuptable[NKEYS][10],deb,*tempb,theMac[20],them[6],TAG[20],challengeSHA[50],sharesult[32],rsnsha[10],syncKey[AESL],globalConn[20],connserver[34],username[20],password[20];
;
EXTERN config_flash						theConf;
EXTERN EventGroupHandle_t 				wifi_event_group;
EXTERN FramSPI							fram;
EXTERN gpio_config_t 					io_conf;
EXTERN host_t							setupHost[MAXDEVS];
EXTERN int								gsock,starthora,startday,startmonth,startyear,WIFI_BIT,LOGIN_BIT,displayMode;
EXTERN mbedtls_md_context_t 			mbedtls_ctx;
EXTERN meterType						theMeters[MAXDEVS];
EXTERN nvs_handle_t 					nvshandle;
EXTERN QueueHandle_t 					mqttQ,mqttR,framQ,pcnt_evt_queue;
EXTERN SemaphoreHandle_t 				wifiSem,framSem,I2CSem,printSem;
EXTERN TaskHandle_t						webHandle,timeHandle,simHandle,connHandle;
EXTERN u16                  			theGuard,diaHoraTarifa,yearDay,oldYearDay,llevoMsg,waitQueue,mesg,oldMesg,diag,oldDiag,horag,oldHorag,yearg,wDelay,qdelay,addressBytes;
EXTERN u32								sentTotal,sendTcp,totalMsg[MAXDEVS],theMacNum,totalPulses,oldCurBeat[MAXDEVS],oldCurLife[MAXDEVS],startGuard;
EXTERN u8								qwait,theBreakers[MAXDEVS],daysInMonth[12],lastalign,lastFont,workingDevs,nofram,sendErrors,iv[16],key[32];
EXTERN esp_aes_context					ctx ;
EXTERN mbedtls_pk_context 				pk;
EXTERN mbedtls_ctr_drbg_context			ctr_drbg;
#ifdef DISPLAY
EXTERN I2C								miI2C;
EXTERN i2ctype 							i2cp;
EXTERN cmdRecord						cmds[MAXCMDS];
#ifdef GLOBAL
EXTERN	SSD1306             			display;
#else
EXTERN SSD1306 							display(0x3c, &miI2C);
#endif
#endif

#endif /* MAIN_INC_GLOBALS_H_ */
