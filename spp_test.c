#include "spp_global.h"

#include "sys/time.h"
#include "pthread.h"

#if SERVICE_CODE
//服务器端，接受数据线程
void* User_Thread(void* parameter)
{
    printf("\nthis is user thread!\n");
    if(signal(SIGALRM,SYSTimeoutHandler) == SIG_ERR)
    {
        printf("\nwhen set timeout handler the error occur!\n");
        return ((void*)0);
    }

    void *pRecvedData;
    uint32_t nRecvedDataLen;
    tMessageStruct* pRecvedMessage;
    while(1)
    {
        //printf("\nservice loop\n");
        
        if(g_sSPPInstance->nConnectStatus != CONNECT_STATU_CONNECTED)
            continue;
        if(RecvMessage(pRecvedData,&nRecvedDataLen))
        {
            pRecvedMessage = (tMessageStruct*)CMALLOC(sizeof(uint8_t)*nRecvedDataLen);
            for(int index = 0; index < nRecvedDataLen; index++)
            {
                *(uint8_t*)(pRecvedMessage + index) = *(uint8_t*)(pRecvedData + index);
            }
            printf("\npriority : 0x%02x\nlength : 0x%02x",pRecvedMessage->nMessagePriority,pRecvedMessage->nMessagelen);
            if(*(pRecvedMessage->pMessageData) == 0x18)
            {
                SendMessage((void*)pRecvedMessage);
            }
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
    if(signal(SIGALRM,SYSTimeoutHandler) == SIG_ERR)
    {
        printf("\nwhen set timeout handler the error occur!\n");
        return ((void*)0);
    }

    void *pSendData;
    uint32_t nSendDataLen;
    tMessageStruct* pSendMessage;

    ConnectToMCU();

    while(1)
    {
        //printf("\nclient loop\n");

        if(g_sSPPInstance->nConnectStatus != CONNECT_STATU_CONNECTED)
            continue;
        else 
            break;
    }
    {
        nSendDataLen = 9;
        pSendMessage->pMessageData = (uint8_t*)CMALLOC(sizeof(uint8_t)*9);
        pSendMessage->nMessagelen = sizeof(pSendMessage);
        pSendMessage->nMessagePriority = 3;
        for(int index = 0;index < nSendDataLen;index++)
        {
            *(uint8_t*)(pSendMessage->pMessageData + index) = 0x00+index;
        }
        SendMessage((void*)pSendMessage);
    }
}
#endif

int main()
{
    if(signal(SIGALRM,SYSTimeoutHandler) == SIG_ERR)
    {
        printf("\nwhen set timeout handler the error occur!\n");
        return ((void*)0);
    }
    InitSPP();
    if(!INIT_LOCK())
    {
        printf("\nwhen init lock error occur!\n");
        return;
    }

    int err;
    
    err = pthread_create(&nRecvThread,NULL,RecvData_thread,NULL);
    if(err != 0)
    {
        printf("\nCan't create multi timer thread!\n");
        return 0;
    }

    struct itimerval new_time_value,old_time_value;
    new_time_value.it_interval.tv_sec = 0;
    new_time_value.it_interval.tv_usec = 0;
    new_time_value.it_value.tv_sec = 3;
    new_time_value.it_value.tv_usec = 0;

    //getitimer(ITIMER_REAL, &new_time_value);
    setitimer(ITIMER_REAL, &new_time_value,NULL);
    pause();

    err = pthread_create(&nSendThread,NULL,SendData_thread,NULL);
    if(err != 0)
    {
        printf("\nCan't create multi timer thread!\n");
        return 0;
    }
    err = pthread_create(&nTimerThread,NULL,MultiTimer_thread,NULL);
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


    pthread_join(nTimerThread,NULL);
    pthread_join(nSendThread,NULL);
    pthread_join(nRecvThread,NULL);
    pthread_join(nUserThread,NULL);

    return 0;
}

// int main()
// {
//     tLLCInstance* pLLCInstance;
//     int lccframelength = 0;
//     int oldlength;

//     //uint8_t olddata[13] = {0x09,0x80,0x00,0x15,0x23,0x46,0x73,0x20,0x7e,0xff,0x32};     //11
//     //uint8_t olddata[13] = {0x09,0x80,0x00,0x49,0x7f,0x7f,0x7e,0x7f,0x29,0x43,0x6e};   //11
//     //uint8_t olddata[13] = {0x02,0xcd,0x80,0x31};             //4    REJ
//      //uint8_t olddata[13] = {0x02,0xf9,0x02,0x31};             //4    RSET
//     // uint8_t olddata[13] = {0x02,0xc1,0x80,0x31};             //4    RR
//     // uint8_t olddata[13] = {0x02,0xD2,0x80,0x31};             //4    RNR
//     // uint8_t olddata[13] = {0x01,0xE6,0xE6};                  //3    UA
//     uint8_t olddata[19] = {0x15,0x23,0x46,0x73,0x20,0x7e,0xff,0x49,0x7f,0x7f,0x7e,0x7f,0x29,0x43,0x21,0x44,0x95,0x08,0x18};                  //3    UA


//     // uint8_t newdata[13] = {0};
//     // uint8_t removezero[13] = {0};

//     // oldlength = 4;

//     // lccframelength = static_InsertZero(olddata,newdata,oldlength);
//     // for(int i = 0; i < oldlength; i++)
//     //     printf("0x%02x ",olddata[i]);
//     // printf("\n");
//     // for(int i = 0; i < lccframelength; i++)
//     //     printf("0x%02x ",newdata[i]);
//     // printf("\n===============================================================\n");
//     // oldlength = static_RemoveInsertedZero(newdata,removezero,lccframelength);
//     // for(int i = 0; i < lccframelength; i++)
//     //     printf("0x%02x ",newdata[i]);
//     // printf("\n");
//     // for(int i = 0; i < oldlength; i++)
//     //     printf("0x%02x ",removezero[i]);

//     InitSPP();
//     // ConnectToMCU();
//     // Disconnect();
//     // ResetConnect();
//     //SendMessage((void*)olddata);

//     // g_sSPPInstance->nNextMessageHeader = CONNECT_REQUIRE_CONNECT;
//     // LLCFrameWrite(NULL,0,0,false);

//     // g_sSPPInstance->nNextMessageHeader = CONNECT_REQUIRE_DISCONNECT;
//     // LLCFrameWrite(NULL,0,0,false);

//     // for(int index = 0; index < 8; index++)
//     // {
//     //     MACFrameWrite();
//     //     if(index == 3)
//     //         g_aLLCInstance[0]->nNextCtrlFrameToSend = READ_CTRL_FRAME_RNR;
//     // }
//     printf("\n\n======================================================================\n\n");
//     for(int i = 0; i < 3; i++)
//     {
//         pLLCInstance = MACFrameRead();
//         //printf("g_aLLCInstance[0]->nLLCReadReadPosition : %d\n",g_aLLCInstance[0]->nLLCReadReadPosition);
//         // lccframelength = g_aLLCInstance[0]->aLLCReadBuffer[g_aLLCInstance[0]->nLLCReadReadPosition];
//         // lccframelength++;
        
//         // if(lccframelength > 22)
//         //     continue;
//         // printf("\n*******************   读缓存中的一帧   **********************************\n\
//         // lccframelength : %d\n",lccframelength);
//         // for(int index = 0; index < lccframelength; index++)
//         // {
//         //     printf("0x%02x ",g_aLLCInstance[0]->aLLCReadBuffer[g_aLLCInstance[0]->nLLCReadReadPosition+index]);
//         // }
//         // printf("\n=========================================================\n");
//         if(pLLCInstance != NULL)
//             LLCReadFrame(pLLCInstance);
//     }
//     printf("\ng_sSPPInstance->nMessageLength : %d\ng_sSPPInstance->pMessageBuffer : ",g_sSPPInstance->nMessageLength);
//     for(int index = 0; index < g_sSPPInstance->nMessageLength; index++)
//     {
//         printf("0x%02x ",g_sSPPInstance->pMessageBuffer[index]);
//     }
//     printf("\n");

//     printf("\ng_sSPPInstance->nConnectStatus : 0x%02x\n",g_sSPPInstance->nConnectStatus);
//     printf("\n*****************************************************\n");
//     static_ResetLLC();
//     SendMessage((void*)olddata);

//     for(int i = 0; i < 2; i++)
//     {
//         pLLCInstance = MACFrameRead();
//         if(pLLCInstance != NULL)
//             LLCReadFrame(pLLCInstance);
//     }
//     printf("\ng_sSPPInstance->nMessageLength : %d\ng_sSPPInstance->pMessageBuffer : ",g_sSPPInstance->nMessageLength);
//     for(int index = 0; index < g_sSPPInstance->nMessageLength; index++)
//     {
//         printf("0x%02x ",g_sSPPInstance->pMessageBuffer[index]);
//     }
//     printf("\n");

// }

// // int cancelflag = 0;

// // void Timer1_Timeout_CallBack(void* pParameter)
// // {
// //     int timerid = 0;
// //     timerid = (int*)(pParameter);
// //     printf("\nthis is the timer1 %d callbackfunciton!\n",timerid);
// // }
// // void Timer2_Timeout_CallBack(void* pParameter)
// // {
// //     int timerid = 0;
// //     cancelflag++;
// //     timerid = (int*)(pParameter);
// //     printf("\nthis is the timer2 %d callbackfunciton!\n",timerid);
// // }
// // void Timer3_Timeout_CallBack(void* pParameter)
// // {
// //     int timerid = 0;
// //     timerid = (int*)(pParameter);
// //     printf("\nthis is the timer3 %d callbackfunciton!\n",timerid);
// // }
// // void Timer4_Timeout_CallBack(void* pParameter)
// // {
// //     int timerid = 0;
// //     timerid = (int*)(pParameter);
// //     printf("\nthis is the timer4 %d callbackfunciton!\n",timerid);
// // }
// // void Timer5_Timeout_CallBack(void* pParameter)
// // {
// //     int timerid = 0;
// //     timerid = (int*)(pParameter);
// //     printf("\nthis is the timer5 %d callbackfunciton!\n",timerid);
// // }
// // void Timer0_Timeout_CallBack(void* pParameter)
// // {
// //     int timerid = 0;
// //     timerid = (int*)(pParameter);
// //     printf("\nthis is the timer0 %d callbackfunciton!\n",timerid);
// // }
// // int main()
// // {
// //     tSppMultiTimer* pEarliestTimer = NULL;
// //     int nPara = 78;
// //     uint8_t tiems = 0;

// //     InitSPP();
// //     MultiTimerInit();

// //     uint16_t sec = 0;
// //     uint8_t sadf = 0;
// //     uint8_t re = 0;
// //     SetTimer(TIMER_1,3000,false,Timer1_Timeout_CallBack,(void*)nPara);
// //     SetTimer(TIMER_2,2648,false,Timer2_Timeout_CallBack,(void*)nPara);
// //     SetTimer(TIMER_3,4215,false,Timer3_Timeout_CallBack,(void*)nPara);

// //     while(1)
// //     {
// //         sec++;
// //         if(sec == 0)
// //         {
// //             sadf++;
// //         }

// //         if(sadf >= 10)
// //         {
// //             sadf = 0;
// //             SYSTimeoutHandler();
// //             if((cancelflag == 3) && (re != 2))
// //             {
// //                 re = CancelTimerTask(TIMER_2,CANCEL_MODE_IMMEDIATELY);
// //                 //CANCEL_MODE_AFTER_NEXT_TIMEOUT
// //             }
// //         }
// //     }
// // }