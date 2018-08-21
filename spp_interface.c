#include "spp_global.h"

static void Timer0_RequireConnectTimeout(void* pParameter)
{
    if(g_sSPPInstance->nConnectStatus == CONNECT_STATU_WAITING_LINK_CONFIRM)
    {
        g_nReconnectTimes++;
        ConnectToMCU();
    }
}

static void Timer1_RequireDisConnectTimeout(void* pParameter)
{
    if(g_sSPPInstance->nConnectStatus == CONNECT_STATU_WAITING_DISCONNECT_CONFIRM)
    {
        Disconnect();
        g_nReconnectTimes = 0;
    }
}

static void Timer1_RequireResetConnectTimeout(void* pParameter)
{
    if(g_sSPPInstance->nConnectStatus == CONNECT_STATU_WAITING_RESET_CONFIRM)
    {
        g_nReconnectTimes++;
        ResetConnect();
    }
}
/**
 * @function    MPU向MCU发起建立连接请求
 * @description 建立连接的过程是三次握手，通过三次握手，双方都能知道双向数据传输是否联通
 *              这个接口通过发送命令来改变状态变量，通过状态变量的值来确定当前协议中MCU和MPU的状态
 * @parameter   
 * @return      错误码
*/
int ConnectToMCU()
{
    if(g_nReconnectTimes >= 5)
    {
        printf("\n\nConnect Timeout!\n\n");
        return -1;
    }
    //CDebugAssert(g_sSPPInstance->nConnectStatus == CONNECT_STATU_DISCONNECTED);
    g_sSPPInstance->nNextMessageHeader = CONNECT_REQUIRE_CONNECT;
    LLCFrameWrite(NULL,0,0,CONNECT_FRAME);
    g_sSPPInstance->nConnectStatus = CONNECT_STATU_WAITING_LINK_CONFIRM;
    SetTimer(TIMER_0_CONNECT,RESENT_CONNECT_REQUIRE_TIMEOUT,true,Timer0_RequireConnectTimeout,NULL);
    return 0;
}

/**
 * @function    一方向另一方发起断连接请求
 * @description 这个接口通过发送命令来改变状态变量，通过状态变量的值来确定当前协议中MCU和MPU的状态
 * @parameter   
 * @return      错误码
*/
int Disconnect()
{
    g_sSPPInstance->nNextMessageHeader = CONNECT_REQUIRE_DISCONNECT;
    LLCFrameWrite(NULL,0,0,CONNECT_FRAME);
    g_sSPPInstance->nConnectStatus = CONNECT_STATU_WAITING_DISCONNECT_CONFIRM;
    SetTimer(TIMER_0_CONNECT,RESENT_DIS_RESET_CONNECT_TIMEOUT,true,Timer1_RequireDisConnectTimeout,NULL);
    return 0;
}

/**
 * @function    重置连接，清理协议栈的缓存内容
 * @description 直接发送链路层的reset命令来重置链路层的内容
 * @parameter   
 * @return      错误码
*/
int ResetConnect()
{
    g_sSPPInstance->nNextMessageHeader = CONNECT_REQUIRE_RESET;
    LLCFrameWrite(NULL,0,0,CONNECT_FRAME);
    g_sSPPInstance->nConnectStatus = CONNECT_STATU_WAITING_RESET_CONFIRM;
    SetTimer(TIMER_0_CONNECT,RESENT_DIS_RESET_CONNECT_TIMEOUT,true,Timer1_RequireResetConnectTimeout,NULL);
    return 0;
}

/**
 * @function    发送一条message
 * @description 
 * @parameter   
 * @return      错误码
*/
int SendMessage(void* pSendMessage)
{
    uint32_t nMessageLength;
    uint8_t nMessagePriority;
    uint32_t nSentBytes;
    tMessageStruct* pSendMessageWithStruct;

    pSendMessageWithStruct = (tMessageStruct*)pSendMessage;

    // nMessageLength = *(uint32_t*)(pSendMessage + 1);
    // nMessagePriority = *(uint8_t*)(pSendMessage);
    nMessageLength = pSendMessageWithStruct->nMessagelen;
    nMessagePriority = pSendMessageWithStruct->nMessagePriority;

    g_sSPPInstance->nNextMessageHeader = CONNECT_SEND_RECV_MESSAGE;

    nSentBytes = LLCFrameWrite((uint8_t*)pSendMessage,nMessageLength,nMessagePriority,true);
    if(nSentBytes == nMessageLength)
        return 1;
    else return 0;
}

/**
 * @function    接收一条message
 * @description 
 * @parameter   
 * @return      错误码
*/
int RecvMessage(void* pDataAddress,uint32_t* pMessageLength)
{
    // tLLCInstance* pLLCInstance;

    // for(int nPriority = 0; nPriority < PRIORITY; nPriority++)
    // {
    //     pLLCInstance = g_aLLCInstance[nPriority];
    //     if((pLLCInstance->nMessageLength != 0) && (pLLCInstance->bIsWaitingLastFragment == false))
    //     {
    //         *pMessageLength = pLLCInstance->nMessageLength;
    //         for(int index = 0; index < pLLCInstance->nMessageLength; index++)
    //         {
    //             *((uint8_t*)pDataAddress++) = *(g_sSPPInstance->pMessageBuffer + index);
    //         }
    //         return *(int*)(pDataAddress+4);//return the type field
    //     }
    // }
    // return 0;//没有收到任何消息
    while(g_sSPPInstance->bIsMessageReady != true);
    *pMessageLength = g_sSPPInstance->nMessageLength;
    pDataAddress = g_sSPPInstance->pMessageBuffer;
    // for(int index = 0; index < g_sSPPInstance->nMessageLength; index++)
    // {
    //     *((uint8_t*)pDataAddress++) = *(g_sSPPInstance->pMessageBuffer + index);
    // }
    //return *(int*)(pDataAddress+4);//return the type field
    return 1;

}

uint8_t InitSPP()
{

    uint8_t index = 0;
    //版本信息的确定应该在连接建立的时候确定，暂时写在这里
    g_nVersion = VERSION_1_0;
    

    g_nReconnectTimes = 0;

    for(index = 0;index < MAX_TIMER_UPPER_LIMIT; index++)
    {
        switch(index)
        {
            case 0:
                g_aDefaultTimeout[0] = TIME_OUT_INTERVAL_0;
                g_aTimerID[0] = TIMER_0;
                break;
            case 1:
                g_aDefaultTimeout[1] = TIME_OUT_INTERVAL_1;
                g_aTimerID[1] = TIMER_1;
                break;
            case 2:
                g_aDefaultTimeout[2] = TIME_OUT_INTERVAL_2;
                g_aTimerID[2] = TIMER_2;
                break;
            case 3:
                g_aDefaultTimeout[3] = TIME_OUT_INTERVAL_3;
                g_aTimerID[3] = TIMER_3;
                break;
            case 4:
                g_aDefaultTimeout[4] = TIME_OUT_INTERVAL_4;
                g_aTimerID[4] = TIMER_4;
                break;
            case 5:
                g_aDefaultTimeout[5] = TIME_OUT_INTERVAL_5;
                g_aTimerID[5] = TIMER_5;
                break;
        }
    }

    //SPPInstance初始化
    g_sSPPInstance = (tSPPInstance*)CMALLOC(sizeof(tSPPInstance));
    g_sSPPInstance->pMessageBuffer = (uint8_t*)CMALLOC(sizeof(uint8_t)*SINGLE_MESSAGE_MAX_LENGTH);
    g_sSPPInstance->nConnectStatus = CONNECT_STATU_DISCONNECTED;
    g_sSPPInstance->nNextMessageHeader = CONNECT_IDLE;
    g_sSPPInstance->nMessageLength = 0;
    g_sSPPInstance->bIsMessageReady = false;
    g_sSPPInstance->pConnectCallbackFunction = NULL;
    g_sSPPInstance->pConnectCallbackParameter = NULL;
    g_sSPPInstance->pResetIndicationFunction = NULL;
    g_sSPPInstance->pResetIndicationFunctionParameter = NULL;

    //窗口大小的确定应该通过配置文件来确定，暂时写在这里
    g_sSPPInstance->nWindowSize = 4;
    //MAC初始化
    InitMACInstance();
    //LLC初始化
    InitLLCInstance();
    MultiTimerInit();

    return 0;
}