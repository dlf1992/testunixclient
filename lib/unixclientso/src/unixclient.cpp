/******************************************************************
* Copyright(c) 2020-2028 ZHENGZHOU Tiamaes LTD.
* All right reserved. See COPYRIGHT for detailed Information.
*
* @fileName: unixclient.cpp
* @brief: 域通信客户端实现
* @author: dinglf
* @date: 2020-05-12
* @history:
*******************************************************************/
#include "unixclient.h"

UnixClient::UnixClient()
{
	FD_ZERO(&rest);
	FD_ZERO(&west);
	error = 0;
	optlen = sizeof(error);
	sock = 0;
	connectstatus = false;
	memset(rcvbuffer,0,MAX_BUFFER);
	rcvbuflen = 0;
	pParseData = NULL;
}
UnixClient::~UnixClient()
{
	if(NULL == pParseData)
	{
		delete pParseData;
		pParseData = NULL;
	}
}
bool UnixClient::init()
{
	sock = socket(AF_UNIX, SOCK_STREAM, 0);
    //printf("sock = %d\n",sock);
    if (-1 == sock)
	{
		//printf("socket error.\n");		
		return false;
	}    
    if(-1 == fcntl(sock, F_SETFL, fcntl(sock, F_GETFL) | O_NONBLOCK))
	{
		//printf("set socket nonblock error.\n");
		close(sock);
		return false;
	}
    return true;
}
bool UnixClient::connectserver(const char *socketfile,pFun pCallback)
{
	if(!init())
	{
        //printf("init failed!\n");
	    return false;
	}
	m_pCallback = pCallback;
	struct sockaddr_un serveraddr;
	serveraddr.sun_family=AF_UNIX;
    strcpy(serveraddr.sun_path,socketfile);
	int ret = connect(sock, (struct sockaddr *)&serveraddr, sizeof(serveraddr));
	if (-1 == ret && EINPROGRESS != errno)
	{
		//printf("sock = %d,errno = %d,domain connect failed.\n",sock,errno);
		close(sock);
		return false;
	}
	/*添加select超时响应*/
	FD_ZERO(&rest);
	FD_ZERO(&west);
	FD_SET(sock, &rest);
	FD_SET(sock, &west);
	struct timeval tempval;
	tempval.tv_sec = 3;
	tempval.tv_usec = 0;
	int flag = select(sock+1, &rest, &west, NULL, &tempval);//监听套接的可读和可写条件
	if(flag < 0)
	{
		//printf("select error\n");
		close(sock);
		return false;
	}
	else
	{
		/*说明*/
		/*在何时确认连接成功，第一种：可写不可读，第二种：可读可写，通过getsockopt获取
		sock信息，返回值为0且返回错误值也为0的情况连接成功。
		*/
        if(!FD_ISSET(sock, &rest) && !FD_ISSET(sock, &west))
        {
            //printf("domain connect no response\n");
			close(sock);
			return false;
        }
		else if(FD_ISSET(sock, &rest) && FD_ISSET(sock, &west))
		{
			flag = getsockopt(sock, SOL_SOCKET, SO_ERROR, &error, &optlen);
			//printf("flag = %d,error = %d\n",flag,error);
			if(flag == 0 && error == 0)
			{
				//printf("domain connect success....FD_ISSET all ok\n");
				//获取互斥锁 
				//更新接收时间戳
				variable_locker.mutex_lock();
				connectstatus = true;
				Rcvtimestamp = GetSysTime();
				variable_locker.mutex_unlock();
				return true;
			}
			else
			{
				//printf("domain connect error....FD_ISSET all ok but error\n");
				close(sock);
				return false;
			}
		}
		else if(!FD_ISSET(sock, &rest) && FD_ISSET(sock, &west))
		{
			//printf("domain connect success....FD_ISSET write ok\n");
			//获取互斥锁 
			//更新接收时间戳
			variable_locker.mutex_lock();
			connectstatus = true;
			Rcvtimestamp = GetSysTime();
			variable_locker.mutex_unlock();
			return true;	
		}
		else
		{
			//printf("domain connect error....\n");
			close(sock);
			return false;
		}		
	}	
}
void UnixClient::rcvdata()
{
	bool connectflag = false;
	//获取互斥锁
	variable_locker.mutex_lock();
	connectflag = connectstatus;
	variable_locker.mutex_unlock();

	if(!connectflag)
	{
		//printf("domain tcp server is not connected.\n");
		return;
	}
	else
	{
		while(connectflag)
		{
			FD_ZERO(&rest);//empty
			//FD_ZERO(&west);
			FD_SET(sock,&rest);//set
			//FD_SET(sock,&west);//
			//switch(select(sock+1,&rest,&west,NULL,NULL))
            //select超时3s
            struct timeval timeout={3,0};
			switch(select(sock+1,&rest,NULL,NULL,&timeout))
			{
				 case 0:  //timeout	
					////printf("timeout...\n");
					break;
				 case -1: //error
					//printf("there are bug\n");
					//printf("errno = %d\n",errno);
					break;
				 default: //data ready
					{
						if(FD_ISSET(sock,&rest))//judge
						{
							memset(rcvbuffer,0,MAX_BUFFER);
							ssize_t _size=read(sock,rcvbuffer,MAX_BUFFER);
							uint32 datalen = _size;
							if(_size>0)
							{
								//获取互斥锁 
								//更新接收时间戳
								variable_locker.mutex_lock();
								Rcvtimestamp = GetSysTime();
								variable_locker.mutex_unlock();
								
								// //printf("echo:%s\n",rcvbuffer);
								// for(int i=0;i<_size;i++)
								// {
									// //printf("%02x",rcvbuffer[i]);
								// }
								// //printf("\n");
								////printf("domain reveived data %d bytes.\n",datalen);
								if(NULL == pParseData)
								{
									pParseData = new ParseData;
								}
								pParseData->dataprocess(rcvbuffer,datalen,m_pCallback);
									
							}
							else
							{
								//TCP SERVER断开连接
								//printf("domain server disconnected\n");
								//获取互斥锁
								variable_locker.mutex_lock();
								connectstatus = false;
								connectflag = connectstatus;
								variable_locker.mutex_unlock();
								//shutdown(sock,SHUT_RDWR);
								close(sock);
							}
						}
					}				
					break;
			}
            variable_locker.mutex_lock();
            connectflag = connectstatus;
            variable_locker.mutex_unlock();
		}
		//printf("exit while loop.\n");
	}
}
int UnixClient::senddata(const char* buf,int buflen)
{
	bool connectflag = false;
	//获取互斥锁
	variable_locker.mutex_lock();
	connectflag = connectstatus;
	variable_locker.mutex_unlock();
	if(!connectflag)
	{
		//printf("domain tcp server is not connected.\n");
		return 0;
	}
	else
	{
		int sendlen = 0;
		sendlen = send(sock,buf,buflen,0);
		////printf("domain senddata = %d\n",sendlen);
		if(sendlen > 0)
        {
            ////printf("domain send data succed,sendlen = %d.\n",sendlen);
        }
        else if(sendlen<0)
		{
            if(errno == EINTR || errno == EAGAIN)
            {
                //printf("send error,errno = %d.\n",errno);
            }
            else
            {
                disconnect();
                //printf("send error,errno = %d.\n",errno);
            }
		}
		else
        {
            disconnect();
        }
		return sendlen;	
	}
}
void UnixClient::disconnect()
{
	shutdown(sock, SHUT_RDWR);
	close(sock);
    //printf("sock=%d,domain disconnect.\n",sock);
	//获取互斥锁
	variable_locker.mutex_lock();
	connectstatus = false;
	variable_locker.mutex_unlock();
}