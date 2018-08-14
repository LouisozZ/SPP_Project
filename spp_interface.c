#include "spp_include.h"

/**
 * @function    MPU向MCU发起建立连接请求
 * @description 建立连接的过程是三次握手，通过三次握手，双方都能知道双向数据传输是否联通
 *              这个接口通过发送命令来改变状态变量，通过状态变量的值来确定当前协议中MCU和MPU的状态
 * @parameter   
 * @return      错误码
*/
int ConnectToMCU()
{

}

/**
 * @function    一方向另一方发起断连接请求
 * @description 这个接口通过发送命令来改变状态变量，通过状态变量的值来确定当前协议中MCU和MPU的状态
 * @parameter   
 * @return      错误码
*/
int Disconnect()
{

}

/**
 * @function    重置连接，清理协议栈的缓存内容
 * @description 直接发送链路层的reset命令来重置链路层的内容
 * @parameter   
 * @return      错误码
*/
int ResetConnect()
{

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

    // nMessageLength = *(uint32_t*)(pSendMessage + 1);
    // nMessagePriority = *(uint8_t*)(pSendMessage);
    nMessageLength = 27;
    nMessagePriority = 0;

    nSentBytes = LLCFrameWrite((uint8_t*)pSendMessage,nMessageLength,nMessagePriority);
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
    tLLCInstance* pLLCInstance;

    for(int nPriority = 0; nPriority < PRIORITY; nPriority++)
    {
        pLLCInstance = g_aLLCInstance[nPriority];
        if((pLLCInstance->nMessageLength != 0) && (pLLCInstance->bIsWaitingLastFragment == false))
        {
            *pMessageLength = pLLCInstance->nMessageLength;
            for(int index = 0; index < pLLCInstance->nMessageLength; index++)
            {
                *((uint8_t*)pDataAddress++) = *(g_sSPPInstance->pMessageBuffer + index);
            }
            return *(int*)(pDataAddress+4);//return the type field
        }
    }
    return 0;//没有收到任何消息
}

uint8_t InitSPP()
{

    //版本信息的确定应该在连接建立的时候确定，暂时写在这里
    g_nVersion = VERSION_1_0;

    //SPPInstance初始化
    g_sSPPInstance = (tSPPInstance*)CMALLOC(sizeof(tSPPInstance));
    g_sSPPInstance->pMessageBuffer = (uint8_t*)CMALLOC(sizeof(uint8_t)*SINGLE_MESSAGE_MAX_LENGTH);
    g_sSPPInstance->nConnectStatus = 0;
    g_sSPPInstance->nConnectNextFrameToSend = 0;
    g_sSPPInstance->nMessageLength = 0;
    g_sSPPInstance->pConnectCallbackFunction = NULL;
    g_sSPPInstance->pConnectCallbackParameter = NULL;
    g_sSPPInstance->pResetIndicationFunction = NULL;
    g_sSPPInstance->pResetIndicationFunctionParameter = NULL;

    //MAC初始化
    InitMACInstance();
    //LLC初始化
    InitLLCInstance();

    return 0;
}