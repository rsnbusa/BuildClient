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

static esp_err_t challenge_get_handler(httpd_req_t *req);
static esp_err_t setup_get_handler(httpd_req_t *req);
static esp_err_t setupend_get_handler(httpd_req_t *req);
//static void scan_handler(httpd_req_t *req);

extern void delay(uint32_t a);
extern void write_to_flash();
extern int sendMsg(uint8_t *lmessage, uint16_t son,uint8_t *donde,uint8_t maxx);
extern void shaMake(char * payload,uint8_t payloadLen,uint8_t* shaResult);
extern uint32_t millis();

httpd_handle_t hserver = NULL;

static int reserveSlot(char *server, char* password)
{
	wifi_config_t wifi_config;
	int8_t	ans=0;
	int q=0;

	char *lmessage=(char*)malloc(100);
	memset(lmessage,0,100);
	sprintf(lmessage,"rsvp%u",theMacNum);

	//receive buffer greater than expected since we are sending 5 status messages and will receive 5 answers.
	// if we do not do this the sendmsg socket will SAVE the other answers for next call and never gets the logindate correct
	uint8_t *ansmem=(uint8_t*)malloc(100);

	//AP section
	memset(&wifi_config,0,sizeof(wifi_config));//very important

	if(strlen(server)>0)
	{
		esp_wifi_stop();
		delay(1000);
		strcpy((char*)wifi_config.sta.ssid,server);
		strcpy((char*)wifi_config.sta.password,password);
		ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
		ESP_ERROR_CHECK(esp_wifi_set_config(ESP_IF_WIFI_STA, &wifi_config));
		esp_wifi_start();
		EventBits_t uxBits=xEventGroupWaitBits(wifi_event_group, LOGIN_BIT, false, true, 100000/  portTICK_RATE_MS); //wait for IP to be assigned
		if ((uxBits & LOGIN_BIT)==LOGIN_BIT)
		q=sendMsg((uint8_t*)lmessage,strlen(lmessage),(uint8_t*)ansmem,100);
	//	printf("Reserve ans %d\n",q);
		if(q<0)
		{
			if(ansmem)
				free(ansmem);
			if(lmessage)
				free(lmessage);
			return -1;
		}
		ans= *ansmem;
	//	printf("Answer reserve %d\n",ans);
		if(lmessage)
			free(lmessage);
		if(ansmem)
			free(ansmem);
		return ans;
	}
	return -1;
}

static int scan(char * who)
{
	 	uint16_t number = 10;
	    wifi_ap_record_t ap_info[10];
	    uint16_t ap_count = 0;

	    memset(ap_info, 0, sizeof(ap_info));

	    wifi_scan_config_t scan_config = {
	    		.ssid = 0,
	    		.bssid = 0,
	    		.channel = 0,
	    	    .show_hidden = true,
				.scan_type= WIFI_SCAN_TYPE_ACTIVE
	    	};

	   	ESP_ERROR_CHECK(esp_wifi_scan_start(&scan_config, true));
	    ESP_ERROR_CHECK(esp_wifi_scan_get_ap_records(&number, ap_info));
	    ESP_ERROR_CHECK(esp_wifi_scan_get_ap_num(&ap_count));

	    if(theConf.traceflag & (1<<WEBD))
	    	printf("%sTotal APs scanned = %u%s\n",RED, ap_count,RESETC);

	    for (int i = 0; (i < 10) && (i < ap_count); i++)
	    {
	        if(strstr((char *)ap_info[i].ssid,"CmgrIoT")!=NULL)
	        {
		    	memcpy(who,&ap_info[i].ssid,strlen((char*)ap_info[i].ssid));
		        if(theConf.traceflag & (1<<WEBD))
		        {
		        	printf("%sSSID%s \t\t%s\n",GREEN,RESETC, ap_info[i].ssid);
		        	printf("%sRSSI%s\t\t%d\n", CYAN,RESETC,ap_info[i].rssi);
		        }
		        return 0;
	        }
	    }
	    return -1;
}

static const httpd_uri_t setup = {
    .uri       = "/setup",
    .method    = HTTP_GET,
    .handler   = setup_get_handler,
	.user_ctx	= NULL
};

static esp_err_t setup_get_handler(httpd_req_t *req)
{
    char*  buf;
    size_t buf_len;
    int cualm=0;
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
            if (httpd_query_key_value(buf, "meter", param, sizeof(param)) == ESP_OK)
            {
            	 cualm=atoi(param);
            	 sprintf(tempb,"[%s]Invalid Meter range %d",strftime_buf,cualm);
            	 if (cualm>=MAXDEVS)
            		 goto exit;
            }
            if (httpd_query_key_value(buf, "mid", param, sizeof(param)) == ESP_OK)
            	 strcpy((char*)&setupHost[cualm].meterid,param);

            if (httpd_query_key_value(buf, "kwh", param, sizeof(param)) == ESP_OK)
            	 setupHost[cualm].startKwh=atoi(param);

            if (httpd_query_key_value(buf, "bpk", param, sizeof(param)) == ESP_OK)
            	 setupHost[cualm].bpkwh=atoi(param);

            if (httpd_query_key_value(buf, "tar", param, sizeof(param)) == ESP_OK)
            	 setupHost[cualm].tariff=atoi(param);

        }
        free(buf);
    }

    if(setupHost[cualm].bpkwh>0 && setupHost[cualm].startKwh>0 && strlen(setupHost[cualm].meterid)>0)
    {
		time(&now);
		localtime_r(&now, &timeinfo);
		strftime(strftime_buf, sizeof(strftime_buf), "%c", &timeinfo);
    	setupHost[cualm].valid=true;
    	theConf.configured[cualm]=1; //in transit mode
    	sprintf(tempb,"[%s]Meter:%d Id=%s kWh=%d BPK=%d",strftime_buf,cualm,setupHost[cualm].meterid,setupHost[cualm].
    					startKwh,setupHost[cualm].bpkwh);
    }
    else
    {
    	exit:
    	setupHost[cualm].valid=false;
        sprintf(tempb,"[%s]Invalid parameters",strftime_buf);
    }

	httpd_resp_send(req, tempb, strlen(tempb));

    return ESP_OK;
}

static const httpd_uri_t challenge = {
    .uri       = "/challenge",
    .method    = HTTP_GET,
    .handler   = challenge_get_handler,
	.user_ctx	= NULL
};

esp_err_t challenge_get_handler(httpd_req_t *req)
{
#define SHAC	16				//best force it to 16 chars, 8 hex values from the SHA challenge
#define SHAL	(SHAC/2)-1

    char*  buf;
    size_t buf_len;
    int cualm;
    time_t now;
    struct tm timeinfo;
    char strftime_buf[64],server[34];
    char param[64];
#if SHAC>8
    long long shaint=1,gotsha=0;
#else
    uint32_t shaint=0,gotsha=1;
#endif

	time(&now);
	localtime_r(&now, &timeinfo);
	strftime(strftime_buf, sizeof(strftime_buf), "%c", &timeinfo);

    buf_len = httpd_req_get_url_query_len(req) + 1;
    if (buf_len > 1)
    {
		sprintf(tempb,"[%s]Invalid parameters",strftime_buf);
        buf = (char*)malloc(buf_len);
        if(buf)
        if (httpd_req_get_url_query_str(req, buf, buf_len) == ESP_OK)
        {
			if (httpd_query_key_value(buf, "challenge", param, sizeof(param)) == ESP_OK)
			{
				int ll=strlen(param);

				if(ll<SHAC)
				{
					sprintf(tempb,"[%s]Invalid Challenge count\n",strftime_buf);
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

				uint8_t *p=(uint8_t*)&shaint;
				uint8_t * sh=(uint8_t*)&sharesult[SHAL];
				for (int a=SHAL;a>=0;a--)	//bif endian to little endian
					*(p++) = *(sh--);

				if (shaint==gotsha)
				{
					memset(server,0,sizeof(server));
					int fue=scan(server);
					if(fue<0)
					{
						sprintf(tempb,"[%s]No server found. No valid configuration active\n",strftime_buf);
						httpd_resp_send(req, tempb, strlen(tempb));
						return ESP_OK;
					}

					sprintf(tempb,"[%s]Meters were saved permanently. Server %s System restarting in a few seconds",strftime_buf,server);
					httpd_resp_send(req, tempb, strlen(tempb));

					for (int a=0;a<MAXDEVS;a++)
					{
						if(theConf.configured[a]==2)
						{
							theMeters[a].beatsPerkW=setupHost[a].bpkwh;
							memcpy((void*)&theMeters[a].serialNumber,(void*)&setupHost[a].meterid,sizeof(theMeters[a].serialNumber));
							theMeters[a].curLife=setupHost[a].startKwh;
							time((time_t*)&theMeters[a].ampTime);

							theConf.beatsPerKw[a]=setupHost[a].bpkwh;
							memcpy(theConf.medidor_id[a],(void*)&setupHost[a].meterid,sizeof(theConf.medidor_id[cualm]));
							time((time_t*)&theConf.bornDate[a]);
							theConf.beatsPerKw[a]=setupHost[a].bpkwh;
							theConf.bornKwh[a]=setupHost[a].startKwh;
							theConf.configured[a]=3;					//final status configured
							theConf.tariff[a]=setupHost[a].tariff;
							fram.write_lifekwh(a,setupHost[a].startKwh);	// write to Fram. Its beginning of life KWH
						}
						else
							theConf.configured[a]=0; //reset it
					}
					theConf.active=1;				// theConf is now ACTIVE and Certified

					memset(&setupHost,0,sizeof(setupHost));
					strcpy(theConf.mgrName,server);
					strcpy(theConf.mgrPass,server);
					write_to_flash();
					int q=reserveSlot(server,server);
				    if(theConf.traceflag & (1<<WEBD))
				    	printf("%sWeb Reserve Answer %d\n",WEBDT,q);
				//	sprintf(tempb,"[%s]Meters were saved permanently. Server %s Slot %d...system restarting",strftime_buf,server,q);
				//	httpd_resp_send(req, tempb, strlen(tempb));
				//	delay(2000);
				//	esp_restart();
				}
				else
					sprintf(tempb,"[%s]Invalid challenge",strftime_buf);
			}
			else
				sprintf(tempb,"[%s]Missing challenge",strftime_buf);
        }
        free(buf);
       }

	httpd_resp_send(req, tempb, strlen(tempb));

    return ESP_OK;
}

static const httpd_uri_t setupend = {
    .uri       = "/setupend",
    .method    = HTTP_GET,
    .handler   = setupend_get_handler,
	.user_ctx	= NULL
};

static esp_err_t setupend_get_handler(httpd_req_t *req)
{
    char*  	buf;
    size_t	buf_len;
    int 	cuantos;
    time_t 	now;
    struct 	tm timeinfo;
    char 	strftime_buf[64];
    char	aca[20];
	time(&now);
	localtime_r(&now, &timeinfo);
	strftime(strftime_buf, sizeof(strftime_buf), "%c", &timeinfo);

    buf_len = httpd_req_get_url_query_len(req) + 1;
    if (buf_len > 1)
    {
		sprintf(tempb,"[%s]Invalid parameters",strftime_buf);
        buf = (char*)malloc(buf_len);
        if (httpd_req_get_url_query_str(req, buf, buf_len) == ESP_OK)
        {
            char param[32];
            if (httpd_query_key_value(buf, "meter", param, sizeof(param)) == ESP_OK) //number of meters to make permanent
            {
           		cuantos=atoi(param);

                if (httpd_query_key_value(buf, "name", param, sizeof(param)) == ESP_OK)
                	 strcpy(theConf.meterName,param);

                if(strcmp(theConf.meterName,"")==0)
                {
                	sprintf(tempb,"MeterName missing\n");
                	goto exit;
                }
            	 if (cuantos<=MAXDEVS)
            	 {
            		 for (int a=0;a<cuantos;a++)
            		 {
            			 if(theConf.configured[a]!=1)
            			 {
              				sprintf(tempb,"[%s]Meter %d is not configured",strftime_buf,a);
              				goto exit;
            			 }
            			 else
            				 theConf.configured[a]=2;								//waiting for challenge
            		 }
            			time(&now);
            		 // setup challenge. MeterName+millis+MAC then take 16 first shabytes and redo a sha256, base64 to send it
            		 	 sprintf(challengeSHA,"%s%u%06x",theConf.meterName,millis(),theMacNum);
            		 	 shaMake(challengeSHA,strlen(challengeSHA),(uint8_t*)sharesult);
            		 	 memcpy(aca,sharesult,16);
            		 	 aca[16]=0;
            		 	 shaMake(aca,16,(uint8_t*)sharesult);
            		 	 size_t writ=0;
            		 	 char newdest[100];
            		 	 mbedtls_base64_encode((unsigned char*)newdest,sizeof(newdest),&writ,(unsigned char*)aca,16);
            			 sprintf(tempb,"[%s]Configuration and meters 1-%d will be saved if challenge met. Challenge=[%s]",strftime_buf,cuantos,newdest);
            	 }
            	 else
     				sprintf(tempb,"[%s]Invalid number of meters %d",strftime_buf,cuantos);
            }
        }
        free(buf);
       }
exit:
	httpd_resp_send(req, tempb, strlen(tempb));

    return ESP_OK;
}

void start_webserver(void *pArg)
{

    httpd_config_t config = HTTPD_DEFAULT_CONFIG();

    if(theConf.traceflag & (1<<WEBD))
    	printf("%sStarting server on port:%d\n",WEBDT, config.server_port);
    if (httpd_start(&hserver, &config) == ESP_OK) {
        // Set URI handlers
        httpd_register_uri_handler(hserver, &setup); 		//setup upto 5 meters
        httpd_register_uri_handler(hserver, &setupend);		//end setup and send challenge
        httpd_register_uri_handler(hserver, &challenge);		//confirm challenge and store in flash
    //    httpd_register_uri_handler(server, &sysScan);		//scan
    }
    if(theConf.traceflag & (1<<WEBD))
    	printf("WebServer Started\n");
    vTaskDelete(NULL);
}


