#include "includes.h"
#include "defines.h"
#define GLOBAL
#include "globals.h"
// file spi_caps.h in soc/esp32/include/soc has parameter SOC_SPI_MAXIMUM_BUFFER_SIZE set to 64. Max bytes in Halfduplex.
#include "soc/spi_caps.h"

#define DEBUGMQQT
/*========================================================================*/
/*                            CONSTRUCTORS                                */
/*========================================================================*/

/**************************************************************************/
/*!
 Constructor
 */
/**************************************************************************/
FramSPI::FramSPI(void)
{
	_framInitialised = false;
	spi =NULL;
	intframWords=0;
	addressBytes=0;
	prodId=0;
	manufID=0;
}

bool FramSPI::begin(int MOSI, int MISO, int CLK, int CS,SemaphoreHandle_t *framSem)
{
	int ret;
	spi_bus_config_t 				buscfg;
	spi_device_interface_config_t 	devcfg;

	memset(&buscfg,0,sizeof(buscfg));
	memset(&devcfg,0,sizeof(devcfg));

	buscfg.mosi_io_num=MOSI;
	buscfg .miso_io_num=MISO;
	buscfg.sclk_io_num=CLK;
	buscfg.quadwp_io_num=-1;
	buscfg .quadhd_io_num=-1;
	buscfg.max_transfer_sz=10000;// useless in Half Duplex, max is set by ESPIDF in SOC_SPI_MAXIMUM_BUFFER_SIZE 64 bytes

	//Initialize the SPI bus
	ret=spi_bus_initialize(VSPI_HOST, &buscfg, 0);
	assert(ret == ESP_OK);


//	devcfg .clock_speed_hz=SPI_MASTER_FREQ_40M;              	//Clock out at 26 MHz
	devcfg .clock_speed_hz=SPI_MASTER_FREQ_26M;              	//Clock out at 26 MHz
//	devcfg .clock_speed_hz=SPI_MASTER_FREQ_8M;              	//Clock out for test in Saleae clone limited speed
	devcfg.mode=0;                                	//SPI mode 0
	devcfg.spics_io_num=CS;               			//CS pin
	devcfg.queue_size=7;                         	//We want to be able to queue 7 transactions at a time
	devcfg.flags=SPI_DEVICE_HALFDUPLEX;


	ret=spi_bus_add_device(VSPI_HOST, &devcfg, &spi);
	if (ret==ESP_OK)
	{
		getDeviceID(&manufID, &prodId);

		if(theConf.traceflag & (1<<FRAMD))
			printf("%sManufacturerId %04x ProductId %04x\n",FRAMDT,manufID,prodId);

		//Set write enable after chip is identified
		switch(prodId)
		{
		case 0x409:
			addressBytes=2;
			intframWords=16384;
			break;
		case 0x509:
			addressBytes=2;
			intframWords=32768;
			break;
		case 0x2603:
			addressBytes=2;
			intframWords=65536;
			break;
		case 0x2703:
			addressBytes=3;
			intframWords=131072;
			break;
		case 0x4803:
			addressBytes=3;
			intframWords=262144;
			break;
		default:
			addressBytes=2;
			intframWords=0;
			return false;
		}

		_framInitialised = true;

		*framSem= xSemaphoreCreateBinary();
		if(*framSem)
			xSemaphoreGive(*framSem);  //SUPER important else its born locked
		else
			printf("Cant allocate Fram Sem\n");

		if(TOTALFRAM>intframWords)
		{
			printf("Not enough space for Meter Definition %d vs %d required\n",intframWords,FINTARIFA);
			return false;
		}

		devcfg.address_bits=addressBytes*8;
		devcfg.command_bits=8;

		ret=spi_bus_add_device(VSPI_HOST, &devcfg, &spi);

		setWrite();// ONLY once required per datasheet
		return true;
	}
	return false;
}

/*========================================================================*/
/*                           PUBLIC FUNCTIONS                             */
/*========================================================================*/

/**************************************************************************/
/*!
 Send SPI Cmd. JUST the cmd.
 */
/**************************************************************************/
int  FramSPI::sendCmd (uint8_t cmd)
{
	esp_err_t ret;
	spi_transaction_ext_t t;

	memset(&t, 0, sizeof(t));       //Zero out the transaction no need to set to 0 or null unused params

	t.base.flags= 		( SPI_TRANS_VARIABLE_CMD );
	t.base.cmd=			cmd;
	t.command_bits = 	8;

	ret=spi_device_polling_transmit(spi, (spi_transaction_t*)&t);  //Transmit!
	return ret;

}

/**************************************************************************/
/*!
 Status SPI Cmd.
 */
/**************************************************************************/
int  FramSPI::readStatus ( uint8_t* donde)
{
	esp_err_t ret;
	spi_transaction_ext_t t;

	memset(&t, 0, sizeof(t));       //Zero out the transaction no need to set to 0 or null unused params

	t.base.flags= 		( SPI_TRANS_VARIABLE_CMD | SPI_TRANS_USE_RXDATA );
	t.base.cmd=			MBRSPI_RDSR;
	t.base.rxlength=	8;
	t.command_bits = 	8;                                                    //zero command bits
	ret=spi_device_polling_transmit(spi, (spi_transaction_t*)&t);  //Transmit!
    *donde=t.base.rx_data[0];
	return ret;
}

int  FramSPI::writeStatus ( uint8_t streg)
{
	esp_err_t ret;
	spi_transaction_t t;
	memset(&t, 0, sizeof(t));       //Zero out the transaction
	t.tx_data[0]=MBRSPI_WRSR;
	t.tx_data[1]=streg;
	t.length=16;                     //Command is 8 bits
	t.flags=SPI_TRANS_USE_TXDATA;
    ret=spi_device_polling_transmit(spi, &t);  //Transmit!
	return ret;
}

void FramSPI::setWrite()
{
	int countst=10;
	uint8_t st=0;

	while(countst>0 && st!=2)
	{
		readStatus(&st);
		st=st&2;
		if(st!=2)
		{
			sendCmd(MBRSPI_WREN);
			countst--;
		}
	}
}


int FramSPI::writeMany (uint32_t framAddr, uint8_t *valores,uint32_t son)
{
	spi_transaction_ext_t t;
	esp_err_t ret=0;
	int count,fueron;

	memset(&t,0,sizeof(t));	//Zero out the transaction no need to set to 0 or null unused params
	count=son;

	while(count>0)
	{
		fueron=				count>TXL?TXL:count;
		t.base.flags= 		( SPI_TRANS_VARIABLE_ADDR | SPI_TRANS_VARIABLE_CMD );
		t.base.addr = 		framAddr;
		t.base.length = 	fueron*8;
		t.base.tx_buffer = 	valores;
		t.base.cmd=			MBRSPI_WRITE;
		t.command_bits = 	8;
		t.address_bits = 	addressBytes*8;
		ret=spi_device_polling_transmit(spi, (spi_transaction_t*)&t);  //Transmit and wait!

		count				-=fueron; 		// reduce bytes processed
		framAddr			+=fueron;  	// advance Address by fueron bytes processed
		valores				+=fueron;  	// advance Buffer to write by processed bytes
	}
	return ret;
}

int FramSPI::readMany (uint32_t framAddr, uint8_t *valores, uint32_t son)
{
	esp_err_t ret=0;
	spi_transaction_ext_t t;
	int cuantos,fueron;

	memset(&t, 0, sizeof(t));	//Zero out the transaction no need to set to 0 or null unused params

	cuantos=son;
	while(cuantos>0)
	{
		fueron=				cuantos>TXL?TXL:cuantos;

		t.base.flags= 		( SPI_TRANS_VARIABLE_ADDR | SPI_TRANS_VARIABLE_CMD );
		t.base.addr = 		framAddr;
		t.base.cmd=			MBRSPI_READ;
		t.command_bits = 	8;
		t.address_bits = 	addressBytes*8;
		t.base.rx_buffer=	valores;
		t.base.rxlength=	fueron*8;
		ret=spi_device_polling_transmit(spi, (spi_transaction_t*)&t);	//Transmit and wait

		cuantos				-=fueron;
		framAddr			+=fueron;
		valores				+=fueron;
	}
	return ret;
}

int FramSPI::write8 (uint32_t framAddr, uint8_t value)
{
	esp_err_t ret;
	spi_transaction_ext_t t;

	memset(&t,0,sizeof(t));	//Zero out the transaction no need to set to 0 or null unused params

	t.base.tx_data[0]=	value;
	t.base.flags= 		( SPI_TRANS_VARIABLE_ADDR | SPI_TRANS_VARIABLE_CMD| SPI_TRANS_USE_TXDATA );
	t.base.addr = 		framAddr;
	t.base.length = 	8;
	t.base.cmd=			MBRSPI_WRITE;
	t.command_bits = 	8;
	t.address_bits = 	addressBytes*8;
	ret=spi_device_polling_transmit(spi, (spi_transaction_t*)&t);  //Transmit and wait!
	return ret;
}

int FramSPI::read8 (uint32_t framAddr,uint8_t *donde)
{
	spi_transaction_ext_t t;
	int ret;

	memset(&t, 0, sizeof(t));       //Zero out the transaction no need to set to 0 or null unused params

	t.base.flags= 		( SPI_TRANS_VARIABLE_ADDR | SPI_TRANS_VARIABLE_CMD| SPI_TRANS_USE_RXDATA );
	t.base.addr = 		framAddr;                                         //set address
	t.base.cmd=			MBRSPI_READ;
	t.command_bits = 	8;
	t.address_bits = 	addressBytes*8;
	t.base.rxlength=	8;
	ret=spi_device_polling_transmit(spi, (spi_transaction_t*)&t);  //Transmit!
	*donde=t.base.rx_data[0];
	return ret;
}

void FramSPI::getDeviceID(uint16_t *manufacturerID, uint16_t *productID)
{
	spi_transaction_ext_t t;

	memset(&t, 0, sizeof(t));       //Zero out the transaction no need to set to 0 or null unused params

	t.base.flags= 		( SPI_TRANS_VARIABLE_CMD| SPI_TRANS_USE_RXDATA );
	t.base.cmd=			MBRSPI_RDID;
	t.command_bits = 	8;
	t.base.rxlength=	32;
	spi_device_polling_transmit(spi, (spi_transaction_t*)&t);  //Transmit and wait!

	// Shift values to separate manuf and prod IDs
	// See p.10 of http://www.fujitsu.com/downloads/MICRO/fsa/pdf/products/memory/fram/MB85RC256V-DS501-00017-3v0-E.pdf
	*manufacturerID=(t.base.rx_data[0]<<8)+t.base.rx_data[1];
	*productID=(t.base.rx_data[2]<<8)+t.base.rx_data[3];
}



int FramSPI::format(uint8_t valor, uint8_t *lbuffer,uint32_t len,bool all)
{
	uint32_t add=0;
	int count=intframWords,ret;

	uint8_t *buffer=(uint8_t*)malloc(len);
	if(!buffer)
	{
		printf("Failed format buf\n");
		return -1;
	}

	while (count>0)
	{
		if(lbuffer!=NULL)
				memcpy(buffer,lbuffer,len); //Copy whatever was passed
			else
				memset(buffer,valor,len);  //Should be done only once

		if (count>len)
		{
		//	printf("Format add %d len %d count %d val %d\n",add,len,count,valor);
			ret=writeMany(add,buffer,len);
			if (ret!=0)
				return ret;
		}
		else
		{
		//	printf("FinalFormat add %d len %d val %d\n",add,count,valor);
			ret=writeMany(add,buffer,count);
			if (ret!=0)
				return ret;
		}
		count-=len;
		add+=len;
//		delay(5);
	}
	free(buffer);
	return ESP_OK;
}

int FramSPI::formatSlow(uint8_t valor)
{
	uint32_t add=0;
	int count=intframWords;
	while (count>0)
	{
		write8(add,valor);
		count--;
		add++;
	}
	return ESP_OK;
}


int FramSPI::formatMeter(uint8_t cual, uint8_t *buffer,uint16_t len)
{
	uint32_t add;
	int count=DATAEND-BEATSTART,ret;
	add=count*cual+BEATSTART;
	while (count>0)
	{
		if (count>len)
		{
			memset(buffer,0,len);
			ret=writeMany(add,buffer,len);
			if (ret!=0)
				return ret;
		}
		else
		{
			memset(buffer,0,count);
			ret=writeMany(add,buffer,count);
			if (ret!=0)
				return ret;
		}
		count-=len;
		add+=len;
	//	delay(2);
	}
	return ESP_OK;

}
int FramSPI::read_tarif_bytes(uint32_t add,uint8_t*  donde,uint32_t cuantos)
{
	int ret;
	ret=readMany(add,donde,cuantos);
	return ret;
}

int FramSPI::read_tarif_day(uint16_t dia,uint8_t*  donde) //Read 24 Hours of current Day(0-365)
{
	int ret;
	uint32_t add=TARIFADIA+dia*24*MWORD;
//	if(theConf.traceflag & (1<<FRMCMD))
//		printf("%sR TarDay %d Add %d\n",FRMCMDT,dia,add);
	ret=read_tarif_bytes(add,donde,24*MWORD);
	return ret;
}

int FramSPI::read_tarif_hour(uint16_t dia,uint8_t hora,uint8_t*  donde) //Read specific Hour in a Day. Day 0-365 and Hour 0-23
{
	int ret;
	uint32_t add=TARIFADIA+dia*24*MWORD+hora;
//	if(theConf.traceflag & (1<<FRMCMD))
//	printf("%sR TarHour Day %d Hour %d Add %d\n",FRMCMDT,dia,hora,add);
	ret=read_tarif_bytes(add,donde,MWORD);
	return ret;
}

// Meter Data Management

int FramSPI::write_bytes(uint8_t meter,uint32_t add,uint8_t*  desde,uint32_t cuantos)
{
	int ret;
	add+=DATAEND*meter;
//	if(theConf.traceflag & (1<<FRMCMD))
//		printf("%sWBytesMeter %d Add %d son %d\n",FRMCMDT,meter,add,cuantos);
	ret=writeMany(add,desde,cuantos);
	return ret;
}

int FramSPI::read_bytes(uint8_t meter,uint32_t add,uint8_t*  donde,uint32_t cuantos)
{
	add+=DATAEND*meter;
	//	if(theConf.traceflag & (1<<FRMCMD))
	//		printf("%sRBytesMeter %d Add %d son %d\n",FRMCMDT,meter,add,cuantos);
	int ret;
	ret=readMany(add,donde,cuantos);
	return ret;
}

int FramSPI::write_recover(scratchTypespi value)
{
	uint32_t add=SCRATCH;
	uint8_t*  desde=(uint8_t* )&value;
	uint8_t cuantos=sizeof(scratchTypespi);
	int ret=writeMany(add,desde,cuantos);
	return ret;
}

int FramSPI::write_beat(uint8_t medidor, uint32_t value)
{
	int ret;
	uint32_t badd=BEATSTART;
//	if(theConf.traceflag & (1<<FRMCMD))
//		printf("%sWBeat Meter %d Add %d\n",FRMCMDT,medidor,badd);
	ret=write_bytes(medidor,badd,(uint8_t* )&value,LLONG);
	return ret;
}

int FramSPI::write_lifedate(uint8_t medidor, uint32_t value)
{
	int ret;
	uint32_t badd=LIFEDATE;
	//	if(theConf.traceflag & (1<<FRMCMD))
	//	printf("%sWLifeDate Meter %d Add %d\n",FRMCMDT,medidor,badd);
	ret=write_bytes(medidor,badd,(uint8_t* )&value,LLONG);
	return ret;
}

int FramSPI::write_lifekwh(uint8_t medidor, uint32_t value)
{
	int ret;
	uint32_t badd=LIFEKWH;
	//if(theConf.traceflag & (1<<FRMCMD))
	//	printf("%sWLifeKWH Meter %d Add %d\n",FRMCMDT,medidor,badd);
	ret=write_bytes(medidor,badd,(uint8_t* )&value,LLONG);
	return ret;
}

int FramSPI::write_month(uint8_t medidor,uint8_t month,uint16_t value)
{
	int ret;
	uint32_t badd=MONTHSTART+month*MWORD;
	//if(theConf.traceflag & (1<<FRMCMD))
	//	printf("%sWMonth Meter %d Month %d Add %d\n",FRMCMDT,medidor,month,badd);
	ret=write_bytes(medidor,badd,(uint8_t* )&value,MWORD);
	return ret;
}

int FramSPI::write_monthraw(uint8_t medidor,uint8_t month,uint16_t value)
{
	int ret;
	uint32_t badd=MONTHRAW+month*MWORD;
	//	if(theConf.traceflag & (1<<FRMCMD))
	//	printf("%sWMonthRaw Meter %d Month %d Add %d\n",FRMCMDT,medidor,month,badd);
	ret=write_bytes(medidor,badd,(uint8_t* )&value,MWORD);
	return ret;
}

//int FramSPI::write_day(uint8_t medidor,uint16_t yearl,uint8_t month,uint8_t dia,uint16_t value)
int FramSPI::write_day(uint8_t medidor,uint16_t days,uint16_t value)
{
	int ret;
//	uint16_t days=date2daysSPI(yearl,month,dia);
	uint32_t badd=DAYSTART+days*MWORD;
	//if(theConf.traceflag & (1<<FRMCMD))
	//	printf("%sWDay Meter %d Yeard %d Add %d\n",FRMCMDT,medidor,days,badd);
	ret=write_bytes(medidor,badd,(uint8_t* )&value,MWORD);
	return ret;
}

//int FramSPI::write_dayraw(uint8_t medidor,uint16_t yearl,uint8_t month,uint8_t dia,uint16_t value)
int FramSPI::write_dayraw(uint8_t medidor,uint16_t days,uint16_t value)
{
	int ret;
//	uint16_t days=date2daysSPI(yearl,month,dia);
	uint32_t badd=DAYRAW+days*MWORD;
	//if(theConf.traceflag & (1<<FRMCMD))
	//	printf("%sWDayRaw Meter %d Yeard %d Add %d\n",FRMCMDT,medidor,days,badd);
	ret=write_bytes(medidor,badd,(uint8_t* )&value,MWORD);
	return ret;
}

//int FramSPI::write_hour(uint8_t medidor,uint16_t yearl,uint8_t month,uint8_t dia,uint8_t hora,uint8_t value)
int FramSPI::write_hour(uint8_t medidor,uint16_t days,uint8_t hora,uint8_t value)
{
	int ret;
//	uint16_t days=date2daysSPI(yearl,month,dia);
	uint32_t badd=HOURSTART+(days*24)+hora;
	//if(theConf.traceflag & (1<<FRMCMD))
	//	printf("%sWHour Meter %d Yeard %d Hour %d Add %d\n",FRMCMDT,medidor,days,hora,badd);
	ret=write_bytes(medidor,badd,(uint8_t* )&value,1);
	return ret;
}

//int FramSPI::write_hourraw(uint8_t medidor,uint16_t yearl,uint8_t month,uint8_t dia,uint8_t hora,uint8_t value)
int FramSPI::write_hourraw(uint8_t medidor,uint16_t days,uint8_t hora,uint8_t value)
{
	int ret;
//	uint16_t days=date2daysSPI(yearl,month,dia);
	uint32_t badd=HOURRAW+(days*24)+hora;
	//if(theConf.traceflag & (1<<FRMCMD))
	//	printf("%sWHourRaw Meter %d Yeard %d Hour %d Add %d\n",FRMCMDT,medidor,days,hora,badd);
	ret=write_bytes(medidor,badd,(uint8_t* )&value,1);
	return ret;
}

int FramSPI::read_lifedate(uint8_t medidor, uint8_t*  value)
{
	int ret;
	uint32_t badd=LIFEDATE;

	ret=read_bytes(medidor,badd,value,LLONG);
	//	if(theConf.traceflag & (1<<FRMCMD))
	//	printf("%sRLifeDate Meter %d Add %d Value %d\n",FRMCMDT,medidor,badd,(uint32_t)*value);
	return ret;
}

int FramSPI::read_recover(scratchTypespi* aqui)
{
	int ret;
	uint8_t*  donde=(uint8_t* )aqui;
	uint16_t cuantos = sizeof(scratchTypespi);
	uint32_t add=SCRATCH;
	ret=readMany(add,donde,cuantos);
	if (ret!=0)
		printf("Read REcover error %d\n",ret);
	return ret;
}


int FramSPI::read_lifekwh(uint8_t medidor, uint8_t*  value)
{
	int ret;
	uint32_t badd=LIFEKWH;
//	printf("Lifeadd %x\n",(uint32_t)value);
	ret=read_bytes(medidor,badd,value,LLONG);
//	if(theConf.traceflag & (1<<FRMCMD))
//		printf("%sRLifeKWH Meter %d Add %d Value %d\n",FRMCMDT,medidor,badd,(uint32_t)*value);
	return ret;
}

int FramSPI::read_beat(uint8_t medidor, uint8_t*  value)
{
	int ret;
	uint32_t badd=BEATSTART;
//	printf("Beat %x\n",(uint32_t)value);

	ret=read_bytes(medidor,badd,value,LLONG);
//	if(theConf.traceflag & (1<<FRMCMD))
//		printf("%sRBeat Meter %d Add %d Value %d\n",FRMCMDT,medidor,badd,(uint32_t)*value);
	return ret;
}

int FramSPI::read_month(uint8_t medidor,uint8_t month,uint8_t*  value)
{
	int ret;
	uint32_t badd=MONTHSTART+month*MWORD;

	ret=read_bytes(medidor,badd,value,MWORD);
//	if(theConf.traceflag & (1<<FRMCMD))
//		printf("%sRMonth Meter %d Month %d Add %d Val %d\n",FRMCMDT,medidor,month,badd,(uint16_t)*value);
	return ret;
}

int FramSPI::read_monthraw(uint8_t medidor,uint8_t month,uint8_t*  value)
{
	int ret;
	uint32_t badd=MONTHRAW+month*MWORD;
	ret=read_bytes(medidor,badd,value,MWORD);
//	if(theConf.traceflag & (1<<FRMCMD))
//		printf("%sRMonthRaw Meter %d Month %d Add %d Val %d\n",FRMCMDT,medidor,month,badd,(uint16_t)*value);
	return ret;
}

//int FramSPI::read_day(uint8_t medidor,uint16_t yearl,uint8_t month,uint8_t dia,uint8_t*  value)
int FramSPI::read_day(uint8_t medidor,uint16_t days,uint8_t*  value)
{
	int ret;
//	int days=date2daysSPI(yearl,month,dia);
	uint32_t badd=DAYSTART+days*MWORD;
	ret=read_bytes(medidor,badd,value,MWORD);
//	if(theConf.traceflag & (1<<FRMCMD))
//		printf("%sRDay Meter %d Month %d Day %d Add %d Days %d Val %d\n",FRMCMDT,medidor,month,dia,badd,days,(uint16_t)*value);
	return ret;
}

//int FramSPI::read_dayraw(uint8_t medidor,uint16_t yearl,uint8_t month,uint8_t dia,uint8_t*  value)
int FramSPI::read_dayraw(uint8_t medidor,uint16_t days,uint8_t*  value)
{
	int ret;
//	int days=date2daysSPI(yearl,month,dia);
	uint32_t badd=DAYRAW+days*MWORD;
	ret=read_bytes(medidor,badd,value,MWORD);
//	if(theConf.traceflag & (1<<FRMCMD))
//		printf("%sRDayRaw Meter %d Month %d Day %d Add %d Val %d\n",FRMCMDT,medidor,month,dia,badd,(uint16_t)*value);
	return ret;
}

//int FramSPI::read_hour(uint8_t medidor,uint16_t yearl,uint8_t month,uint8_t dia,uint8_t hora,uint8_t*  value)
int FramSPI::read_hour(uint8_t medidor,uint16_t days,uint8_t hora,uint8_t*  value)
{
	int ret;
//	uint16_t days=date2daysSPI(yearl,month,dia);
	uint32_t badd=HOURSTART+(days*24)+hora;
	ret=read_bytes(medidor,badd,value,1);
//	if(theConf.traceflag & (1<<FRMCMD))
//		printf("%sRHour Meter %d Month %d Day %d Hour %d Add %d Value %d\n",FRMCMDT,medidor,month,dia,hora,badd,(uint8_t)*value);
	return ret;
}
//int FramSPI::read_hourraw(uint8_t medidor,uint16_t yearl,uint8_t month,uint8_t dia,uint8_t hora,uint8_t*  value)
int FramSPI::read_hourraw(uint8_t medidor,uint16_t days,uint8_t hora,uint8_t*  value)
{
	int ret;
//	uint16_t days=date2daysSPI(yearl,month,dia);
	uint32_t badd=HOURRAW+(days*24)+hora;
	ret=read_bytes(medidor,badd,value,1);
//	if(theConf.traceflag & (1<<FRMCMD))
//		printf("%sRHourRaw Meter %d Month %d Day %d Hour %d Add %d Value %d\n",FRMCMDT,medidor,month,dia,hora,badd,(uint8_t)*value);
	return ret;
}
