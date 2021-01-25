/******************************************************************
* Copyright(c) 2020-2028 ZHENGZHOU Tiamaes LTD.
* All right reserved. See COPYRIGHT for detailed Information.
*
* @fileName: parsedata.cpp
* @brief: 域通信客户端数据接收处理实现
* @author: dinglf
* @date: 2020-05-12
* @history:
*******************************************************************/

#include "parsedata.h"

ParseData::ParseData()
{
	pTRingBuffer = NULL;
}
ParseData::~ParseData()
{
	if(NULL == pTRingBuffer)
	{
		delete pTRingBuffer;
		pTRingBuffer = NULL;
	}
}
void ParseData::dataprocess(char *buffer,int buflen,pFun pCallback)
{
	char szPacket[2048];
	int iPacketlen = 0;


	if(NULL == pTRingBuffer)
	{
		pTRingBuffer = new TRingBuffer;
		pTRingBuffer->Create(4096);
	}
	if(buflen>0)
	{
		if(!pTRingBuffer->WriteBinary((uint8 *)buffer, buflen))
		{
			//printf("can not write ringbuffer,buflen = %d.\n",buflen);
		}
	}
	while(pTRingBuffer->GetMaxReadSize()>0)
	{
		iPacketlen = ReadPacket(szPacket, sizeof(szPacket));
		if(iPacketlen> 0)
		{
			////printf("Read packet success,iPacketlen = %d\n",iPacketlen);
			//具体协议处理
			pCallback(szPacket,iPacketlen);
			iPacketlen = 0;
		}
		else
		{
			//printf("Read packet failed\n");
			//一直读取包，直到读取不成功，退出；也可能一次都不成功
			break;
			//m_readBuffer.Clear();
		}
	}
}
int ParseData::ReadPacket(char* szPacket, int iPackLen)
{
	int iRet = 0;

	int iStartPos = 0;
	int iStopPos = 0;
	int packetlen = 0;
	unsigned char ch1;
	unsigned char ch2;
	unsigned char ch3;
	unsigned char packetlenlow;
	unsigned char packetlenhigh;
	
	if(NULL == pTRingBuffer)
		return iRet;

	if (!pTRingBuffer->FindChar(0x23, iStartPos)) //find #
	{
        //0x23都查找不到 肯定是无效数据 清空
		//printf("can not find #,clear ringbuffer.\n");
        pTRingBuffer->Clear();	
		return iRet;		
	}

	if(pTRingBuffer->GetMaxReadSize() <= iStartPos+6)
	{
		//printf("can not find total protocol.\n");
		//丢弃#前面的数据
		pTRingBuffer->ThrowSomeData(iStartPos);
		return iRet;
	}
	
	if((!pTRingBuffer->PeekChar(iStartPos+1,ch1))\
		||(!pTRingBuffer->PeekChar(iStartPos+2,ch2))\
		||(!pTRingBuffer->PeekChar(iStartPos+3,ch3)))
	{
		//printf("can not find char.\n");
		return iRet;
	}
	
	if((ch1==0x23)&&(ch2==0x23)&&(ch3==0x23))
	{
		//丢弃#前面的数据
		pTRingBuffer->ThrowSomeData(iStartPos);
		iStartPos = 0;
		//数据包长度
		pTRingBuffer->PeekChar(iStartPos+4,packetlenlow);
		pTRingBuffer->PeekChar(iStartPos+5,packetlenhigh);
		packetlen = MAKESHORT(packetlenlow,packetlenhigh);
		if(packetlen > 2048)
		{
			//处理异常数据
			pTRingBuffer->Clear();	
			return iRet;
		}		
		iStopPos = iStartPos+packetlen;
		if (iStopPos <= pTRingBuffer->GetMaxReadSize())
		{
			if (iStopPos > iPackLen) pTRingBuffer->ThrowSomeData(iStopPos); //数据超长，丢弃
			else if (pTRingBuffer->ReadBinary((uint8*)szPacket, iStopPos))
			{
				iRet = packetlen;
				//m_readBuffer.Clear();
			}
		}
	}
	else
	{
		//长度够，但是不完全符合格式，清空
		//printf("imcomplete with data format,clear all.\n");
		pTRingBuffer->Clear();	
		return iRet;	
	}
	return iRet;		
}
