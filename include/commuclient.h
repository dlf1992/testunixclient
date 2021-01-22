/*
 * 	commuclient.h
 *
 */
 
/*条件编译*/
#ifndef COMMU_CLIENT_H_
#define COMMU_CLIENT_H_
 
#ifdef __cplusplus
extern "C"  //C++
{
#endif
	typedef  int (*pFun)(const char *,int);
	int StartUnixClient(const char* sockfile,pFun Callback,int type);
	int SenddatatoSvr(const char* senddata,int datalen);
#ifdef __cplusplus
}
#endif
 
#endif /* COMMU_CLIENT_H_ */
 