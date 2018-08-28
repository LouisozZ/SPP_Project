#include "spp_global.h"

#include "sys/time.h"
#include "pthread.h"
#include "signal.h"

#if SERVICE_CODE
//服务器端，接受数据线程
void* User_Thread(void* parameter)
{
    printf("\nthis is user thread!\n");
    int err;
    sigset_t old_sig_mask;
    if(err = pthread_sigmask(SIG_SETMASK,&g_sigset_mask,&old_sig_mask) != 0)
    {
        printf("\nset sig mask error!\n");
        return ;
    }

    void *pRecvedData;
    uint32_t nRecvedDataLen;
    tMessageStruct* pRecvedMessage;
    while(1)
    {
        //printf("\nservice loop\n");
        
        if(g_sSPPInstance->nConnectStatus != CONNECT_STATU_CONNECTED)
            continue;
        if(RecvMessage(&pRecvedData,&nRecvedDataLen))
        {
            pRecvedMessage = (tMessageStruct*)CMALLOC(sizeof(uint8_t)*nRecvedDataLen);
            for(int index = 0; index < nRecvedDataLen; index++)
            {
                *(uint8_t*)(pRecvedMessage + index) = *(uint8_t*)(pRecvedData + index);
            }
            pRecvedMessage = (tMessageStruct*)pRecvedMessage;
            printf("\npriority : 0x%02x\nlength : 0x%02x",pRecvedMessage->nMessagePriority,pRecvedMessage->nMessagelen);
            if(*(pRecvedMessage->pMessageData) == 0x00)
            {
                //SendMessage((void*)pRecvedMessage);
            }
            printf("\nCFREE(pRecvedMessage)\n");
            CFREE(pRecvedMessage);
            pRecvedMessage = NULL;
        }
    }
}
#else
//客户端发送数据线程
void* User_Thread(void* parameter)
{
    printf("\nthis is user thread!\n");
    int err;
    sigset_t old_sig_mask;
    if(err = pthread_sigmask(SIG_SETMASK,&g_sigset_mask,&old_sig_mask) != 0)
    {
        printf("\nset sig mask error!\n");
        return ;
    }

    uint8_t *pSendData;
    uint32_t nSendDataLen;
    uint8_t* pSendMessage;
    uint32_t nWaiting = 1;

    ConnectToMCU();

    while(1)
    {
        //printf("\nclient loop\n");

        if(g_sSPPInstance->nConnectStatus != CONNECT_STATU_CONNECTED)
        {
            // nWaiting++;
            // if(nWaiting == 0)
            //     ConnectToMCU();
            continue;
        }
            
        else 
            break;
    }
    {
        printf("\nsend data 1\n");
        nSendDataLen = 90;
        pSendMessage = (uint8_t*)CMALLOC(sizeof(nSendDataLen));
        pSendData = pSendMessage;
        //priority
        *(uint8_t*)pSendData = 3;
        pSendData++;
        //length
        *(uint16_t*)pSendData = nSendDataLen;
        pSendData += 2;
        printf("\npSendMessage len : %d\n",*(uint16_t*)(pSendMessage+1));
        
        for(uint8_t index = 3;index < nSendDataLen;index++)
        {
            *((uint8_t*)(pSendMessage + index)) = index;
        }
        printf("\nsend data 2\nThe message is as flow : \n");
        for(uint8_t index = 0;index < nSendDataLen;index++)
        {
            printf("0x%02x ",*(uint8_t*)(pSendMessage + index));
        }
        SendMessage((void*)pSendMessage);
        printf("\nsend data 3\n");
    }
}
#endif

int main()
{
    
    InitSPP();
    if(!INIT_LOCK())
    {
        printf("\nwhen init lock error occur!\n");
        return;
    }

    int err;
    uint16_t delay = 1;

    sigset_t old_sig_mask;
    sigemptyset(&g_sigset_mask);
    sigaddset(&g_sigset_mask,SIGALRM);

    if(err = pthread_sigmask(SIG_SETMASK,&g_sigset_mask,&old_sig_mask) != 0)
    {
        printf("\nset sig mask error!\n");
        return ;
    }
    
    err = pthread_create(&nRecvThread,NULL,RecvData_thread,NULL);
    if(err != 0)
    {
        printf("\nCan't create multi timer thread!\n");
        return 0;
    }

    err = pthread_create(&nSendThread,NULL,SendData_thread,NULL);
    if(err != 0)
    {
        printf("\nCan't create multi timer thread!\n");
        return 0;
    }
    
    err = pthread_create(&nUserThread,NULL,User_Thread,NULL);
    if(err != 0)
    {
        printf("\nCan't create multi timer thread!\n");
        return 0;
    }
    
    for(delay = 1; delay != 0; delay++);

    err = pthread_create(&nTimerThread,NULL,MultiTimer_thread,NULL);
    if(err != 0)
    {
        printf("\nCan't create multi timer thread!\n");
        return 0;
    }

    pthread_join(nTimerThread,NULL);
    pthread_join(nSendThread,NULL);
    pthread_join(nRecvThread,NULL);
    pthread_join(nUserThread,NULL);

    return 0;
}