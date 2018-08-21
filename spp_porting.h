#ifndef __SPP_PORTING_H__
#define __SPP_PORTING_H__

#include "pthread.h"

#define CDebugAssert(value) if(!(value)){printf("\n%s is not true!\nreturn...\n",#value);return;}

#if SERVICE_CODE
#define LOCAL_IP_ADDRESS "192.168.149.128"
#define LOCAL_IP_PORT 5555

#define DISTINATION_IP_ADDRESS "192.168.149.129"
#define DISTINATION_IP_PORT 4444

#else 
#define LOCAL_IP_ADDRESS "192.168.149.129"
#define LOCAL_IP_PORT 4444

#define DISTINATION_IP_ADDRESS "192.168.149.128"
#define DISTINATION_IP_PORT 5555
#endif

extern pthread_t nTimerThread;
extern pthread_t nRecvThread;
extern pthread_t nSendThread;
extern pthread_t nUserThread;
extern pthread_mutex_t nWtirelist;

typedef struct tMessageStruct
{
    uint32_t nMessagelen;
    uint8_t nMessagePriority;
    uint8_t* pMessageData;
}tMessageStruct;
/**
 * @function    调用底层SPI的读函数读取字节
 * @description 
 * @parameter
 * @return      实际读到的字节数，如果读出错则返回 -1
*/
uint8_t ReadBytes(uint8_t *pBuffer,uint8_t nReadLength);
void* CMALLOC(uint32_t length);
uint8_t CFREE(void* pFreeAddress);
uint8_t OPEN_MULTITIMER_MANGMENT();

void UNLOCK_WRITE();
void LOCK_WRITE();
uint8_t INIT_LOCK();

void* MultiTimer_thread(void *parameter);
void* SendData_thread(void *parameter);
void* RecvData_thread(void *parameter);


#endif