#include "spp_global.h"

#include "unistd.h"
#include "sys/time.h"
#include "sys/socket.h"
#include "arpa/inet.h"
#include "pthread.h"
#include "memory.h"

pthread_t nTimerThread;
pthread_t nRecvThread;
pthread_t nSendThread;
pthread_t nUserThread;

int g_service_sock;
int g_service_communicate_fd;
int g_client_sock;

void* CMALLOC(uint32_t length)
{
    return (void*)malloc(length);
}

uint8_t CFREE(void* pFreeAddress)
{
    free(pFreeAddress);
    return 1;
}

//定时器线程，每一毫秒执行一次软中断函数，这个中断函数是自己的多定时器管理函数，所以在设置定时器超时宏定义的时候，值的单位是 ms
void* MultiTimer_thread(void *parameter)
{
    printf("\nthis is timer thread!\n");
    signal(SIGALRM,SYSTimeoutHandler);

    struct itimerval new_time_value,old_time_value;
    new_time_value.it_interval.tv_sec = 0;
    new_time_value.it_interval.tv_usec = 1000;
    new_time_value.it_value.tv_sec = 0;
    new_time_value.it_value.tv_usec = 10;

    for(;;);

    return ((void*)0);
}

void* SendData_thread(void *parameter)
{
    printf("\nthis is send thread!\n");
    struct sockaddr_in service_address;

    g_client_sock = socket(AF_INET,SOCK_STREAM,0);

    memset(&service_address,0,sizeof(service_address));
    service_address.sin_addr.s_addr = inet_addr(DISTINATION_IP_ADDRESS);
    service_address.sin_family = AF_INET;
    service_address.sin_port = htons(DISTINATION_IP_PORT);

    connect(g_client_sock,(struct sockaddr*)&service_address,sizeof(service_address));
    while(1)
    {
        MACFrameWrite();
    }
    
}

void* RecvData_thread(void *parameter)
{
    printf("\nthis is recv thread!\n");
    struct sockaddr_in service_address,client_address;
    tLLCInstance *pLLCInstance;
    //int service_sock = socket(AF_INET,SOCK_STREAM,IPPROTO_TCP);
    g_service_sock = socket(AF_INET,SOCK_STREAM,IPPROTO_TCP);

    memset(&service_address,0,sizeof(service_address));
    service_address.sin_addr.s_addr = inet_addr(LOCAL_IP_ADDRESS);
    service_address.sin_family = AF_INET;
    service_address.sin_port = LOCAL_IP_PORT;

    bind(g_service_sock,(struct sockaddr*)&service_address,sizeof(service_address));
    listen(g_service_sock,128);
    socklen_t client_add_len = sizeof(client_address);
    g_service_communicate_fd = accept(g_service_sock,(struct sockaddr*)&client_address,&client_add_len);

    while(1)
    {
        pLLCInstance = MACFrameRead();
        if(pLLCInstance != NULL)
            LLCReadFrame(pLLCInstance);
    }
}

#ifdef TEST_MAIN

int return_count = 0;
uint8_t aData1[16] = {0x7e,0x09,0x90,0x48,0x44,0x15,0x23,0x46,0x73,0x20,0x7d,0x64,0x00,0x7f};
uint8_t aData2[16] = {0,0,0x7e,0x09,0x98,0x48,0xfb,0xa4,0xbe,0xdf,0x6f,0xa7,0xd9,0x4a,0x38,0x7f};
uint8_t aData3[16] = {0,0,0,0,0x7e,0x08,0xa0,0xc8,0x43,0x21,0x44,0x95,0x08,0x18,0xc3,0x7f,0};
uint8_t aData4[16] = {0,0,0,0,0x7e,0x01,0xd0,0xd1,0x7f,0x01,0xc0,0xc0,0x7f,0};//RR
//uint8_t aData3[16] = {0,0,0,0,0x7e,0x02,0xc1,0x80,0x31,0x7f,0x01,0xc0,0xc0,0x7f,0};//RR
uint8_t aData5[16] = {0,0,0,0,0,0,0x7e,0x03,0x88,0x88,0xf9,0xf6,0x00,0x7f,0x7e,0x03};//RNR REJ
uint8_t aData6[16] = {0x80,0x88,0x1b,0x10,0x7f,0x08,0x01,0x03,0x7f,0x02,0xf8,0x81,0x18,0x80,0x7f};//RSET
uint8_t aData7[16] = {0x34,0x08,0x7e,0x01,0xE6,0xE7,0x7f,0x00,0,0,0,0,0};//UA

uint8_t* aDataArr[7] = {aData1,aData2,aData3,aData4,aData5,aData6,aData7};
#endif

/**
 * @function    调用底层SPI的读函数读取字节
 * @description 
 * @parameter
 * @return
*/
uint8_t ReadBytes(uint8_t *pBuffer,uint8_t nReadLength)
{
//     if(return_count >= 7)
//         return 0;
//     uint8_t* pTargetArr;
//     pTargetArr = aDataArr[return_count];
//     return_count++;

// #ifdef TEST_MAIN
//     for(int i = 0; i < 16; i++)
//         *pBuffer++ = *pTargetArr++;
//     return 16;
// #endif
    int nReadBytes = 0;
    nReadBytes = recv(g_service_communicate_fd,(void*)pBuffer,nReadLength,0);
    if(nReadBytes <= 0)
        return 0;
    else
        return nReadBytes;
}

uint8_t SPI_SEND_BYTES(uint8_t* pData,uint8_t nLength)
{
    // printf("\n-------- send bytes as flow ----------\n\n");
    // for(int index = 0; index < nLength; index++)
    //     printf("0x%02x ",*(pData+index));
    // printf("\n\n--------------------------------------\n");
    send(g_client_sock,(void*)pData,nLength,0);
}

uint8_t OPEN_MULTITIMER_MANGMENT()
{
    int err;
    err = pthread_create(&nTimerThread,NULL,MultiTimer_thread,NULL);
    if(err != 0)
    {
        printf("\nCan't create multi timer thread!\n");
        return 0;
    }
    return 1;
}