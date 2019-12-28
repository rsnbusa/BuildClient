#include "defines.h"
#ifndef framDef_h
#define framDef_h

#define TXL 				SOC_SPI_MAXIMUM_BUFFER_SIZE-4
#define MAXDEVSS            (5)
#define MWORD				(2)
#define LLONG               (4)

#define FRAMDATE			0
#define METERVER			(FRAMDATE+LLONG)
#define FREEFRAM			(METERVER+LLONG)
#define SCRATCH          	(FREEFRAM+MWORD)
#define SCRATCHEND          (SCRATCH+100)

#ifdef HOST
#define TARIFADIA           (SCRATCHEND)
#define FINTARIFA           (TARIFADIA+366*24*MWORD)
#else
#define TARIFADIA			SCRATCHEND
#define FINTARIFA			SCRATCHEND
#endif

#define BEATSTART           (FINTARIFA)
#define LIFEKWH             (BEATSTART+LLONG)
#define LIFEDATE            (LIFEKWH+LLONG)
#define MONTHSTART          (LIFEDATE+LLONG)
#define MONTHRAW			(MONTHSTART+MWORD*12)
#define DAYSTART            (MONTHRAW+MWORD*12)
#define DAYRAW				(DAYSTART+MWORD*366)
#define HOURSTART           (DAYRAW+MWORD*366)
#define HOURRAW				(HOURSTART+366*24)
#define DATAEND             (HOURRAW+366*24)
#define TOTALFRAM			(DATAEND*MAXDEVSS)


#endif /* framDef_h */

//FRAMDATE(0)=4
//METERVER(4)=4
//FREEFRAM(8)=2
//SCRATCH(10)=100
//SCRATCHEND(110)=0
//TARIFADIA(110)=0
//FINTARIFA(110)=0

//BEATSTART(110)=4
//LIFEKWH(114)=4
//LIFEDATE(118)=4
//MONTHSTART(122)=24
//MONTHRAW(146)=24
//DAYSTART(170)=732
//DAYRAW(902)=732
//HOURSTART(1634)=8784
//HOURRAW(10418)=8784
//DATAEND(19202)=76808

//TOTALFRAM(96010) Devices 5

