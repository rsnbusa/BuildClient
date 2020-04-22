/*
 * webserver.cpp
 *
 *  Created on: Dec 29, 2019
 *      Author: rsn
 */
#define GLOBAL

#include "includes.h"
#include "defines.h"
#include "projStruct.h"
#include "globals.h"

extern void pprintf(const char * format, ...);
extern int sendMsg(int cualSock,uint8_t *lmessage, uint16_t son,uint8_t *donde,uint16_t maxx);

static esp_err_t challenge_get_handler(httpd_req_t *req);
static esp_err_t conn_login(httpd_req_t *req);
//static esp_err_t conn_firmware(httpd_req_t *req);
//static esp_err_t conn_tariff(httpd_req_t *req);
static esp_err_t conn_base(httpd_req_t *req);
static esp_err_t sendok(httpd_req_t *req);
static esp_err_t sendnak(httpd_req_t *req);
static esp_err_t sendlogo(httpd_req_t *req);
static esp_err_t conn_config_handler(httpd_req_t *req);
void shaMake(char * payload,uint8_t payloadLen, unsigned char *lshaResult);

extern const unsigned char answer_start[] 	asm("_binary_challenge_html_start");
extern const unsigned char setup_start[] 	asm("_binary_connSetup_html_start");
extern const unsigned char login_start[] 	asm("_binary_login_html_start");
extern const unsigned char final_start[] 	asm("_binary_ok_html_start");

extern const unsigned char ok_start[] 		asm("_binary_ok_png_start");
extern const unsigned char ok_end[] 		asm("_binary_ok_png_end");
extern const unsigned char nak_start[] 		asm("_binary_nak_png_start");
extern const unsigned char nak_end[] 		asm("_binary_nak_png_end");
extern const unsigned char logo_start[] 	asm("_binary_meter_png_start");
extern const unsigned char logo_end[] 		asm("_binary_meter_png_end");

int ok_bytes=ok_end-ok_start;
int nak_bytes=nak_end-nak_start;
int logo_bytes=logo_end-logo_start;

extern void delay(uint32_t a);
extern void write_to_flash();
//extern void firmUpdate(void* pArg);
extern uint32_t millis();
void shaMake(const char * key,uint8_t klen,uint8_t* shaResult);


static const httpd_uri_t connsetup = {
    .uri       = "/cmconfig",
    .method    = HTTP_GET,
    .handler   = conn_config_handler,
	.user_ctx	= NULL
};

static const httpd_uri_t okpng = {
    .uri       = "/ok.png",
    .method    = HTTP_GET,
    .handler   = sendok,
	.user_ctx	= NULL
};

static const httpd_uri_t nakpng = {
    .uri       = "/nak.png",
    .method    = HTTP_GET,
    .handler   = sendnak,
	.user_ctx	= NULL
};

static const httpd_uri_t logopng = {
    .uri       = "/meter.png",
    .method    = HTTP_GET,
    .handler   = sendlogo,
	.user_ctx	= NULL
};

static const httpd_uri_t challenge = {
    .uri       = "/cmchallenge",
    .method    = HTTP_GET,
    .handler   = challenge_get_handler,
	.user_ctx	= NULL
};
/*
static const httpd_uri_t cmdFW = {
    .uri       = "/cfirmware",
    .method    = HTTP_GET,
    .handler   = conn_firmware,
	.user_ctx	= NULL
};

static const httpd_uri_t cmdTariff = {
    .uri       = "/ctariff",
    .method    = HTTP_GET,
    .handler   = conn_tariff,
	.user_ctx	= NULL
};
*/
static const httpd_uri_t login = {
    .uri       = "/cmlogin",
    .method    = HTTP_GET,
    .handler   = conn_login,
	.user_ctx	= NULL
};

static const httpd_uri_t baseHtml = {
    .uri       = "/",
    .method    = HTTP_GET,
    .handler   = conn_base,
	.user_ctx	= NULL
};

esp_err_t sendnak(httpd_req_t *req)
{
	httpd_resp_set_type(req,"image/png");
	httpd_resp_send(req,(char*)nak_start,nak_bytes);
	return ESP_OK;
}

esp_err_t sendok(httpd_req_t *req)
{
	httpd_resp_set_type(req,"image/png");
	httpd_resp_send(req,(char*)ok_start,ok_bytes);
	return ESP_OK;
}

esp_err_t sendlogo(httpd_req_t *req)
{
	httpd_resp_set_type(req,"image/png");
	httpd_resp_send(req,(char*)logo_start,logo_bytes);
	return ESP_OK;
}
/*
static esp_err_t conn_firmware(httpd_req_t *req)
{
    char*  buf=NULL;
    size_t buf_len;
    time_t now;
    struct tm timeinfo;
    char strftime_buf[64];

	time(&now);
	localtime_r(&now, &timeinfo);
	strftime(strftime_buf, sizeof(strftime_buf), "%c", &timeinfo);

    buf_len = httpd_req_get_url_query_len(req) + 1;
    if (buf_len > 1)
    {
        buf = (char*)malloc(buf_len);
        if (httpd_req_get_url_query_str(req, buf, buf_len) == ESP_OK)
        {
            char param[32];
            if (httpd_query_key_value(buf, "password", param, sizeof(param)) == ESP_OK)
            {
            	sprintf(tempb,"[%s]Invalid password",strftime_buf);
            	 if(strcmp(param,"ZiPo")!=0)
            		 	goto exit;
            }
        }
    	sprintf(tempb,"[%s]Firmware update in progress...",strftime_buf);
    	xTaskCreate(&firmUpdate,"U571",10240,NULL, 5, NULL);
    }
    else
    	sprintf(tempb,"Invalid parameters");
	exit:
	if(buf)
		free(buf);
	httpd_resp_send(req, tempb, strlen(tempb));

    return ESP_OK;
}

static esp_err_t conn_tariff(httpd_req_t *req)
{
    char*  buf=NULL;
    size_t buf_len;
    int tariff=0;
    time_t now;
    struct tm timeinfo;
    char strftime_buf[64];
    cmdType cmd;
    cJSON * root=NULL;
    char *lmessage=NULL;

	time(&now);
	localtime_r(&now, &timeinfo);
	strftime(strftime_buf, sizeof(strftime_buf), "%c", &timeinfo);

    buf_len = httpd_req_get_url_query_len(req) + 1;
    if (buf_len > 1)
    {
        buf = (char*)malloc(buf_len);
        if (httpd_req_get_url_query_str(req, buf, buf_len) == ESP_OK)
        {
            char param[32];
            if (httpd_query_key_value(buf, "password", param, sizeof(param)) == ESP_OK)
            {
            	sprintf(tempb,"[%s]Invalid password",strftime_buf);
            	 if(strcmp(param,"ZiPo")!=0)
            		 	goto exit;
            }
        	root=cJSON_CreateObject();
			cJSON *ar = cJSON_CreateArray();

			if(root==NULL || ar==NULL)
			{
				pprintf("cannot create root tariff\n");
				return -1;
			}
       //     if (httpd_query_key_value(buf, "tariff", param, sizeof(param)) == ESP_OK)
         //   	theConf.slot_Server.tariff_id=atoi(param);

			cJSON *cmdJ=cJSON_CreateObject();
			cJSON_AddStringToObject(cmdJ,"cmd","/ga_tariff");
	//		cJSON_AddNumberToObject(cmdJ,"tariff",theConf.slot_Server.tariff_id);
			cJSON_AddNumberToObject(cmdJ,"tariff",1);
			cJSON_AddItemToArray(ar, cmdJ);
			cJSON_AddItemToObject(root,"Batch", ar);
			lmessage=cJSON_Print(root);
			if(lmessage==NULL)
			{
				sprintf(tempb,"Error creating JSON Tariff");
				cJSON_Delete(root);
				goto exit;
			}
            write_to_flash();
        }
    	sprintf(tempb,"[%s]Tariff %d loading process started...",strftime_buf,tariff);

   // 	xTaskCreate(&loadit,"loadT",10240,(void*)tariff, 5, NULL);
    	cmd.mensaje=lmessage;
    	cmd.fd=3;//send from internal
    	cmd.pos=0;
    	xQueueSend( mqttR,&cmd,0 );

    }
    else
    	sprintf(tempb,"Invalid parameters");
	exit:
	if(buf)
		free(buf);
	if(root)
		cJSON_Delete(root);
	//======================
	//lmessage will be freed by the calling routine
	//======================

	httpd_resp_send(req, tempb, strlen(tempb));

    return ESP_OK;
}
*/

int getSock()
{
	ip_addr_t 						remote;
	u8 								aca[4];
	tcpip_adapter_ip_info_t 		ip_info;
	int 							addr_family,lsock;
	int								ip_protocol,err;

	ESP_ERROR_CHECK(tcpip_adapter_get_ip_info(TCPIP_ADAPTER_IF_STA, &ip_info));


	ESP_ERROR_CHECK(tcpip_adapter_get_ip_info(TCPIP_ADAPTER_IF_STA, &ip_info));
	memcpy(&aca,&ip_info.gw,4);
	IP_ADDR4( &remote, aca[0],aca[1],aca[2],aca[3]);

	struct sockaddr_in dest_addr;
	memcpy(&dest_addr.sin_addr.s_addr,&ip_info.gw,4);	//send to our gateway, which is our server
	dest_addr.sin_family = AF_INET;
	dest_addr.sin_port = htons(BDGHOSTPORT);
	addr_family = AF_INET;
	ip_protocol = IPPROTO_IP;

	lsock =  socket(addr_family, SOCK_STREAM, ip_protocol);
	if (lsock < 0) {
		pprintf( "Unable to create socket: errno %d\n", errno);
		return ESP_FAIL;

	}

	err = connect(lsock, (struct sockaddr *)&dest_addr, sizeof(dest_addr));
	if (err != 0)
	{
			pprintf( "%sSocket %d unable to connect: errno %d\n",WIFIDT,gsock, errno);
			return ESP_FAIL;
	}
	return lsock;
}

cJSON *makeJson()
{

	cJSON *ar = cJSON_CreateArray();
	if(!ar)
	{
		pprintf("Error creating rsvp cjson arr\n");
		return NULL;
	}
	for (int a=0;a<MAXDEVS;a++)
	{
		if(theConf.configured[a]==1)
		{
			cJSON *cmdJ=cJSON_CreateObject();
			if(cmdJ)
			{
				cJSON_AddStringToObject(cmdJ,"mid",				theConf.medidor_id[a]);
				cJSON_AddNumberToObject(cmdJ,"KwH",				theConf.bornKwh[a]);
				cJSON_AddNumberToObject(cmdJ,"BPK",				theConf.beatsPerKw[a]);
				cJSON_AddItemToArray(ar, cmdJ);
			}
		}
	}
	return ar;
}

int reserveSlot()		//send ConnMgr a request for Whitelisting this MAC
{
#define MSGSIZE	500
	int q=0,misock;

	if(strlen(connserver)==0)
	{
		printf("No connmgr\n");
		return ESP_FAIL;
	}

	if(theConf.active)
		misock=gsock;
	else
		misock=getSock();
	if(misock<0)
	{
		printf("No socket %d\n",errno);
		return ESP_FAIL;
	}

	cJSON *root=cJSON_CreateObject();
	if(root==NULL)
	{
		pprintf("cannot create root rsvp\n");
		return ESP_FAIL;
	}

	cJSON *arr=makeJson();
	if(!arr)
	{
		cJSON_Delete(root);
		return ESP_FAIL;
	}

	cJSON_AddStringToObject			(root,"Controller",theConf.meterName);
	cJSON_AddStringToObject			(root,"cmd","cmd_rsvp");
	cJSON_AddNumberToObject			(root,"macn",(double)theMacNum);
	cJSON_AddStringToObject			(root,"user",username);
	cJSON_AddStringToObject			(root,"passw",password);
	cJSON_AddItemToObject			(root,"Batch",arr);

	char *lmessage=cJSON_Print(root);
	if(!lmessage)
	{
		cJSON_Delete(root);
		return ESP_FAIL;
	}

	cJSON_Delete(root);

	uint8_t *ansmem=(uint8_t*)malloc(MSGSIZE);
	if(!ansmem)
	{
		pprintf("No heap2\n");
		FREEANDNULL(lmessage);
		return ESP_FAIL;
	}
	bzero(ansmem,MSGSIZE);

	if(theConf.active)
		theConf.crypt=1;	// encryption now
	else
		theConf.crypt=0;	//fresh setup no additions

	q=sendMsg(misock,(uint8_t*)lmessage,strlen(lmessage),(uint8_t*)ansmem,MSGSIZE);
	if(q<0)
	{
		if(theConf.traceflag & (1<<WEBD))
			printf("%sFail to send %d\n",WEBDT,q);
		FREEANDNULL(ansmem)
		FREEANDNULL(lmessage)
		return ESP_FAIL;
	}
	if(theConf.traceflag & (1<<WEBD))
		printf("%sRsvp successfull [%s]\n",WEBDT,ansmem);
	FREEANDNULL(ansmem)
	FREEANDNULL(lmessage)
	shutdown(misock,0);
	close(misock);

	return ESP_OK;
}

static int scan(char * who)
{
	 	uint16_t number = 10;
	    wifi_ap_record_t ap_info[10];
	    uint16_t ap_count = 0;
	    int lowest=-1000;

	    bzero(ap_info, sizeof(ap_info));

	    wifi_scan_config_t scan_config;
		scan_config.ssid = 0;
		scan_config.bssid = 0;
		scan_config.channel = 0;
		scan_config.show_hidden = true;
		scan_config.scan_type= WIFI_SCAN_TYPE_ACTIVE;
		scan_config.scan_time.active.max=0;	//120ms per channel. The fastest
		scan_config.scan_time.active.min=0;

	   	ESP_ERROR_CHECK(esp_wifi_scan_start(&scan_config, true));
	    ESP_ERROR_CHECK(esp_wifi_scan_get_ap_records(&number, ap_info));
	    ESP_ERROR_CHECK(esp_wifi_scan_get_ap_num(&ap_count));

	    if(theConf.traceflag & (1<<WEBD))
	    	printf("%sTotal APs scanned = %u%s\n",RED, ap_count,RESETC);

	    if(ap_count>0)
	    {
	    for (int i = 0; (i < 10) && (i < ap_count); i++)
	    {
	    	if(ap_info[i].rssi>lowest)
	    	{
				if(strstr((char *)ap_info[i].ssid,"CmgrIoT")!=NULL)
				{
					memcpy(who,&ap_info[i].ssid,strlen((char*)ap_info[i].ssid));
					if(theConf.traceflag & (1<<WEBD))
					{
						pprintf("%sSSID%s \t\t%s\n",GREEN,RESETC, ap_info[i].ssid);
						pprintf("%sRSSI%s\t\t%d\n", CYAN,RESETC,ap_info[i].rssi);
					}
				}
	    	}
	    }
	    	return ESP_OK;
	    }
	    else
	    	return ESP_FAIL;
}


void resetlater(void *parg)
{
	delay(3000);
	esp_restart();
}

static esp_err_t challenge_get_handler(httpd_req_t *req)
{
#define SHAC	16
#define SHAL	(SHAC/2)-1
	bool		rebf=false;		//reboot flag
    char*  		buf;
    size_t 		buf_len;
	char 		param[100];

#if SHAC>8
    long long shaint=1,gotsha=0;
#else
    uint32_t shaint=0,gotsha=1;
#endif

    buf_len = httpd_req_get_url_query_len(req) + 1;
    if (buf_len > 1)
    {
		sprintf(tempb,(char*)final_start,"nak","Invalid parameters");
        buf = (char*)malloc(buf_len);
        if(buf)
        {
			if (httpd_req_get_url_query_str(req, buf, buf_len) == ESP_OK)
			{
				if (httpd_query_key_value(buf, "challenge", param, sizeof(param)) == ESP_OK)
				{
					int ll=strlen(param);

					if(ll<SHAC)
					{
						sprintf(tempb,(char*)final_start,"nak","Invalid Challenge count");
						httpd_resp_send(req, tempb, strlen(tempb));
						return ESP_OK;
					}
					if(ll>SHAC)
						param[SHAC]=0; //cut it at 8 chars

#if SHAC>8
					gotsha=strtoull(param, NULL, 16);
#else
					gotsha=strtoul(param, NULL, 16);
#endif

					uint8_t *p=(uint8_t*)&shaint;				//little endian
					uint8_t * sh=(uint8_t*)&sharesult[SHAL];	//big endian

					for (int a=SHAL;a>=0;a--)	//big endian to little endian
						*(p++) = *(sh--);

				//	pprintf("Shatint %llu gotsha %llu\n",shaint,gotsha);

					if (shaint==gotsha)		//correct challenge
					{
						int van=0;

						for (int a=0;a<MAXDEVS;a++)
						{
							if(!theConf.configured[a])
							{	//not active erase configuration
								theConf.lock=false;		//allow further setup and launch task when rebooting
								theConf.beatsPerKw[a]=theConf.bornKwh[a]=0;
								bzero(theConf.medidor_id[a],sizeof(theConf.medidor_id[0]));
							}
							else
							{
								van++;
								strcpy(theConf.medidor_id[a],setupHost[a].meterid);
								time((time_t*)&theConf.bornDate[a]);
								theConf.beatsPerKw[a]			=setupHost[a].bpkwh;
								theConf.bornKwh[a]				=setupHost[a].startKwh;
							}
							if(van==MAXDEVS)
								theConf.lock=true;
							else
								theConf.lock=false;

							if(framFlag)
							{
								if(xSemaphoreTake(framSem, portMAX_DELAY/  portTICK_RATE_MS))
								{
									fram.formatMeter(a);
									fram.write_lifekwh(a,setupHost[a].startKwh);	// write to Fram. Its beginning of life KWH
									xSemaphoreGive(framSem);
								}
							}
				//			pprintf("Writing life Meter %d= %d\n",a,setupHost[a].startKwh);
						}
						bzero(tempb,1000);
						strcpy(theConf.mgrName,connserver);
						strcpy(theConf.mgrPass,connserver);
						// request Slot
						if(reserveSlot()==ESP_OK)
						{
							theConf.active=1;
							theConf.crypt=1;
							rebf=true;
							write_to_flash();
							sprintf(tempb,(char*)final_start,"ok","Success...rebooting in 3 secs");
						}
						else
						{
							theConf.active=0;
							sprintf(tempb,(char*)final_start,"nak","ConnMgr Authorization Denied");
						}
					}
					else
						sprintf(tempb,(char*)final_start,"nak","Invalid challenge");
				}
				else
					sprintf(tempb,(char*)final_start,"nak","Missing challenge");
			}
			free(buf);
        }
     }
	httpd_resp_send(req, tempb, strlen(tempb));
	if(rebf)
	{
		xTaskCreate(&resetlater,"displ",2048,(void*)1, 4, NULL);	//wait and restart. Need to answer httpd before
	}
    return ESP_OK;
}

static bool getParam(const char *buf,const char *cualp,char *donde)
{
	if( httpd_query_key_value(buf, cualp, donde, 30)==ESP_OK)
		return true;
	else
		return false;
}


static esp_err_t conn_base(httpd_req_t *req)		//default page
{
	httpd_resp_send(req,(const char*)login_start, (size_t)strlen((char*)login_start));
	return ESP_OK;
}

static esp_err_t conn_login(httpd_req_t *req) //receives user/password and sends Conf Data
{
	int buf_len;
	char *buf=NULL;
	char param[30];

	buf_len = httpd_req_get_url_query_len(req) + 1;
	if (buf_len > 1)
	{
		buf = (char*)malloc(buf_len);
		if (httpd_req_get_url_query_str(req, buf, buf_len) == ESP_OK)
		{
			bzero(param,sizeof(param));
			if(getParam(buf,(char*)"user",param))
			{
				if(strlen(param)>=8)
				{
					strcpy(username,param);
					bzero(param,sizeof(param));
					if(getParam(buf,(char*)"password",param))
					{
						if(strlen(param)>=8)
						{
							strcpy(password,param);
							bzero(tempb,2000);
							// prepare html wiht known parameters
		sprintf(tempb,(const char*)setup_start,
		theConf.meterName,theConf.sendDelay,
		theConf.medidor_id[0],theConf.beatsPerKw[0]==800?"Selected":"",theConf.beatsPerKw[0]==1000?"Selected":"",theConf.beatsPerKw[0]==1200?"Selected":"",theConf.beatsPerKw[0]==2000?"Selected":"",theConf.bornKwh[0],theConf.configured[0]?"checked":"",
		theConf.medidor_id[1],theConf.beatsPerKw[1]==800?"Selected":"",theConf.beatsPerKw[1]==1000?"Selected":"",theConf.beatsPerKw[1]==1200?"Selected":"",theConf.beatsPerKw[1]==2000?"Selected":"",theConf.bornKwh[1],theConf.configured[1]?"checked":"",
		theConf.medidor_id[2],theConf.beatsPerKw[2]==800?"Selected":"",theConf.beatsPerKw[2]==1000?"Selected":"",theConf.beatsPerKw[2]==1200?"Selected":"",theConf.beatsPerKw[2]==2000?"Selected":"",theConf.bornKwh[2],theConf.configured[2]?"checked":"",
		theConf.medidor_id[3],theConf.beatsPerKw[3]==800?"Selected":"",theConf.beatsPerKw[3]==1000?"Selected":"",theConf.beatsPerKw[3]==1200?"Selected":"",theConf.beatsPerKw[3]==2000?"Selected":"",theConf.bornKwh[3],theConf.configured[3]?"checked":"",
		theConf.medidor_id[4],theConf.beatsPerKw[4]==800?"Selected":"",theConf.beatsPerKw[4]==1000?"Selected":"",theConf.beatsPerKw[4]==1200?"Selected":"",theConf.beatsPerKw[4]==2000?"Selected":"",theConf.bornKwh[4],theConf.configured[4]?"checked":"");

		httpd_resp_send(req, tempb, strlen(tempb));
						}
					}
				}
			}
		}
	}
	return ESP_OK;
}

static esp_err_t conn_config_handler(httpd_req_t *req)
{
	    int buf_len;
		char param[32],tt[10];
		bool success=false;
	    char challengeSHA[50],aca[20];
	    char *buf=NULL;

		buf_len = httpd_req_get_url_query_len(req) + 1;
		if (buf_len > 1)
		{
			buf = (char*)malloc(buf_len);
			if (httpd_req_get_url_query_str(req, buf, buf_len) == ESP_OK)
			{
				bzero(param,sizeof(param));
				if(getParam(buf,(char*)"conn",param))
				{
					sprintf(tempb,(char*)final_start,"nak","Invalid Paramters");
				   	strcpy(theConf.meterName,param);
					for (int a=0;a<MAXDEVS;a++)
					{
						sprintf(tt,"m%d",a+1);
						bzero(param,sizeof(param));
						if(getParam(buf,tt,param))
							if(strlen(param)>0)
								strcpy(setupHost[a].meterid,param);
							else
								continue;
						else
							continue;

						sprintf(tt,"b%d",a+1);
						bzero(param,sizeof(param));
						if(getParam(buf,tt,param))
							if(strlen(param)>0)
								setupHost[a].bpkwh=atoi(param);
							else
								continue;
						else
							continue;

						sprintf(tt,"k%d",a+1);
						bzero(param,sizeof(param));
						if(getParam(buf,tt,param))
							if(strlen(param)>0)
								setupHost[a].startKwh=atoi(param);
							else
								continue;
						else
							continue;

						sprintf(tt,"c%d",a+1);
						bzero(param,sizeof(param));
						if(getParam(buf,tt,param))
								theConf.configured[a]=1;
							else
								theConf.configured[a]=0;

						success=true;
					}
				}
			}
		}
		if(success)
		{
		 	 sprintf(challengeSHA,"%s%u%06x",theConf.meterName,millis(),theMacNum);
			 shaMake(challengeSHA,strlen(challengeSHA),(uint8_t*)sharesult);
			 memcpy(aca,sharesult,16);
			 aca[16]=0;
			 shaMake(aca,16,(uint8_t*)sharesult);		//sharesult has the SHA to compare against when challenge received
			 size_t writ=0;
			 char newdest[100];
			 bzero(newdest,sizeof(newdest));

			 mbedtls_base64_encode((unsigned char*)newdest,sizeof(newdest),&writ,(unsigned char*)aca,16);
			 sprintf(tempb,(char*)answer_start,newdest);
		}
		httpd_resp_send(req, tempb, strlen(tempb));

	    return ESP_OK;
}

void doScanBackground(void *pArg)
{
	wifi_config_t wifi_config;

	if(theConf.active)
		vTaskDelete(NULL);			//already connected
	bzero(connserver,sizeof(connserver));				//global name of ConnMgr
    if(scan(connserver)==ESP_OK)
    {
    	memset(&wifi_config,0,sizeof(wifi_config));//very important
    	strcpy((char*)wifi_config.sta.ssid,connserver);
		strcpy((char*)wifi_config.sta.password,connserver);
		ESP_ERROR_CHECK(esp_wifi_set_config(ESP_IF_WIFI_STA, &wifi_config));
		ESP_ERROR_CHECK(esp_wifi_connect());
    }
	vTaskDelete(NULL);
}

void start_webserver(void *pArg)
{
    httpd_handle_t server = NULL;
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();

#ifdef DEBUGX
    if(theConf.traceflag & (1<<WEBD))
    	pprintf("%sStarting server on port:%d\n",WEBDT, config.server_port);
#endif
    if (httpd_start(&server, &config) == ESP_OK) {

    	xTaskCreate(&doScanBackground,"web",8192,(void*)1, 4, NULL);

    	bzero(&setupHost,sizeof(setupHost));
        // Set URI handlers
        httpd_register_uri_handler(server, &challenge);
        httpd_register_uri_handler(server, &connsetup);
        httpd_register_uri_handler(server, &login);
        httpd_register_uri_handler(server, &baseHtml);
        httpd_register_uri_handler(server, &okpng);
        httpd_register_uri_handler(server, &nakpng);
        httpd_register_uri_handler(server, &logopng);
        //    httpd_register_uri_handler(server, &cmdFW);
        //    httpd_register_uri_handler(server, &cmdTariff);

    }
#ifdef DEBUGX
    if(theConf.traceflag & (1<<WEBD))
    	pprintf("WebServer Started\n");
#endif
    vTaskDelete(NULL);		//we are done
}


