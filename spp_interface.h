#ifndef __SPP_INTERFACE_H__
#define __SPP_INTERFACE_H__

typedef void SPPConnectCompleted();
typedef void SPPResetIndication();

typedef struct tSPPInstance{
    uint8_t nConnectStatus;
    uint8_t nWindowSize;
    uint8_t nNextMessageHeader;
    uint8_t* pMessageBuffer;
    uint8_t nMessageLength;
    bool bIsMessageReady;
    SPPConnectCompleted* pConnectCallbackFunction;
    void* pConnectCallbackParameter;

    SPPResetIndication * pResetIndicationFunction;
    void * pResetIndicationFunctionParameter;

}tSPPInstance;

/**
 * @function    MPU向MCU发起建立连接请求
 * @description 建立连接的过程是三次握手，通过三次握手，双方都能知道双向数据传输是否联通
 *              这个接口通过发送命令来改变状态变量，通过状态变量的值来确定当前协议中MCU和MPU的状态
 * @parameter   
 * @return      错误码
*/
int ConnectToMCU();

/**
 * @function    一方向另一方发起断连接请求
 * @description 这个接口通过发送命令来改变状态变量，通过状态变量的值来确定当前协议中MCU和MPU的状态
 * @parameter   
 * @return      错误码
*/
int Disconnect();

/**
 * @function    重置连接，清理协议栈的缓存内容
 * @description 直接发送链路层的reset命令来重置链路层的内容
 * @parameter   
 * @return      错误码
*/
int ResetConnect();

/**
 * @function    发送一条message
 * @description 
 * @parameter   
 * @return      错误码
*/
int SendMessage();

/**
 * @function    接收一条message
 * @description 
 * @parameter   
 * @return      错误码
*/
int RecvMessage(void* pDataAddress,uint32_t* pMessageLength);

uint8_t InitSPP();

#endif