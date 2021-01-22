#include "commuclient.h"
#include "unixclient.h"

UnixClient *punixclient = NULL;
static void *work(void *arg);
pFun callback_func;
char socketfile[128];
char protocol[] = {0x23,0x23,0x23,0x23,0x0b,0x00,0x00,0x00,0x01,0x02,0x03};
int StartUnixClient(const char* sockfile,pFun Callback,int type)
{
	memset(socketfile,0,sizeof(socketfile));
	memcpy(socketfile,sockfile,strlen(sockfile));
	callback_func = Callback;
	protocol[7] = type;
	////printf("callback_func:%p\n",callback_func);
	punixclient = new UnixClient;
	if(NULL == punixclient)
	{
		//printf("punixclient=NULL\n");
		return -1;
	}
	pthread_t tid;
	if(pthread_create(&tid, NULL, work, NULL) != 0)
	{
		//printf("thread creat error.\n");
		return -1;
	}
	if(pthread_detach(tid) != 0)
	{
		//printf("thread detach error.\n");
		return -1;
	}	
	return 0;
}
int SenddatatoSvr(const char* senddata,int datalen)
{
	int ret = 0;
	if(NULL == punixclient)
	{
		//printf("punixclient=NULL\n");
		return -1;
	}	
	ret = punixclient->senddata(senddata,datalen);
	return ret;
}

void *work(void *arg)
{
	prctl(PR_SET_NAME,"unixclient");
	static int connecttimes = 0;
	if(NULL == punixclient)
	{
		return NULL;
	}
	while(1)
	{
		if(punixclient->connectserver(socketfile,callback_func))
		{
			//printf("client connect success,connecttimes = %d\n",connecttimes);
			punixclient->senddata(protocol,sizeof(protocol));
			punixclient->rcvdata();
		}
		else
		{
			//printf("client connect failed,connecttimes = %d\n",connecttimes);
			sleep(10);
		}
		connecttimes++;		
	}
	
	return punixclient;
}