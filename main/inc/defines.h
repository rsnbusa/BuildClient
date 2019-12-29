#ifndef defines_h
#define defines_h

//#define HOST							//For Host Controller else MeterController
//#define RECOVER						// If using Recover strategy. Not really worth it
#define BDGHOSTPORT						30000
#define u32								uint32_t
#define u16								uint16_t
#define u8								uint8_t

#define NKEYS							28

#define QDELAY							2
#define WIFILED							2

//FRAM pins SPI
#define FMOSI							23
#define FMISO							19
#define FCLK							18
#define FCS								5

#define MAXCHARS						40
#define TEST

#define CENTINEL            			0x12112299  //our chip id

#define LOCALTIME						"GMT+5"

#define MAXDEVS							5
#define MAXBUFF							10000

#define METER0							4
#define METER1							22
#define METER2							16
#define METER3							17
#define METER4							21

#define BREAK0							13
#define BREAK1							14
#define BREAK2							25
#define BREAK3							26
#define BREAK4							27

#define TIMEWAITPCNT 					60000 // 1 minute

//30	Black
//31	Red
//32	Green
//33	Yellow
//34	Blue
//35	Magenta
//36	Cyan
//37	White

#define BOOTDT							"\e[31m[BOOTD]\e[0m"
#define WIFIDT							"\e[31m[WIFID]\e[0m"
#define MQTTDT							"\e[32m[MQQTD]\e[0m"
#define MQTTTT							"\e[32m[MQTTT]\e[0m"
#define PUBSUBDT						"\e[33m[PUNSUBD]\e[0m"
#define CMDDT							"\e[35m[CMDD]\e[0m"
#define OTADT							"\e[34m[OTAD]\e[0m"
#define MSGDT							"\e[32m[MSGD]\e[0m"
#define INTDT							"\e[36m[INTD]\e[0m"
#define WEBDT							"\e[37m[WEBD]\e[0m"
#define GENDT							"\e[37m[GEND]\e[0m"
#define HEAPDT							"\e[37m[HEAPD]\e[0m"
#define FRAMDT							"\e[37m[FRAMD]\e[0m"
#define FRMCMDT							"\e[35m[FRMCMDT]\e[0m"
#define TIEMPOT							"\e[31m[TIMED]\e[0m"
#define ERASET							"\e[2J"

#define BLACK							"\e[30m"
#define RED								"\e[31m"
#define GREEN							"\e[32m"
#define YELLOW							"\e[33m"
#define BLUE							"\e[34m"
#define MAGENTA							"\e[35m"
#define CYAN							"\e[36m"
#define WHITE							"\e[37m"
#define RESETC							"\e[0m"


#endif
