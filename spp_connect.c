#include "spp_global.h"

static void Timer1_FinialComfirmTimeout(void* pParameter)
{
    if(g_sSPPInstance->nConnectStatus == CONNECT_STATU_WAITING_CONFIRM)
    {
        g_aLLCInstance[0]->nWriteNextWindowFrameId = g_aLLCInstance[0]->nWriteLastAckSentFrameId + 1;
        g_sSPPInstance->nNextMessageHeader = CONNECT_CONFIRM_CONNECT;
        LLCFrameWrite(NULL,0,0,CONNECT_FRAME);
        SetTimer(TIMER_1_CONNECT_CONFIRM,RESENT_CONFIRM_CONNECT_TIMEOUT,true,Timer1_FinialComfirmTimeout,NULL);
    }
}

uint8_t ConnectErrorFrameHandle(uint8_t nMessageHeader)
{
    uint8_t nErrorType;
    nErrorType = nMessageHeader & CONNECT_TYPE_MASK;
    printf("\nTHis is error frame handler! message head : 0x%02x \n",nMessageHeader);

    switch(nErrorType)
    {
        case 0x00:
            printf("It means that when sending disconnect type frame the error occur.\n");
            break;
        case 0xe0:
            printf("It means that when sending connect type frame the error occur.\n");
            break;
        case 0xa0:
            printf("It means that when sending reconnect type frame the error occur.\n");
            break;
        case 0x40:
            printf("It means that when sending data type frame the error occur.\n");
            break;
    }
    
    return 0;
}

uint8_t ConnectCtrlFrameACK(uint8_t nMessageHeader)
{
    uint8_t nErrorMessageHeader = 0;
    
    printf("\nTHis is Connect ctrl ack frame handler! message head : 0x%02x \n",nMessageHeader);

    if(g_sSPPInstance->nConnectStatus == CONNECT_STATU_DISCONNECTED)
    {
        if(nMessageHeader == CONNECT_REQUIRE_DISCONNECT)    //接收断连接之后响应的确认断连接帧丢失，断连接请求方未收到断连接确认，重发断连接请求
        {
            g_sSPPInstance->nConnectStatus == CONNECT_STATU_DISCONNECTED;
            g_sSPPInstance->nNextMessageHeader = CONNECT_CONFIRM_DISCONNECT;
            LLCFrameWrite(NULL,0,0,CONNECT_FRAME);
            CancleAllTimerTask();
            return 1;
        }
        else if(nMessageHeader == CONNECT_REQUIRE_RESET)    //接收重启请求之后响应的确认重启帧丢失，重启请求方未收到重启确认，重发重启请求
        {
            g_sSPPInstance->nConnectStatus == CONNECT_STATU_DISCONNECTED;
            g_sSPPInstance->nNextMessageHeader = CONNECT_CONFIRM_RESET;
            LLCFrameWrite(NULL,0,0,CONNECT_FRAME);
            //关闭所有的定时任务
            CancleAllTimerTask();
            return 1;
        }
        else if(nMessageHeader == CONNECT_REQUIRE_CONNECT)  //在未连接状态收到对方的连接请求，转换状态，建立连接
        {
            g_sSPPInstance->nConnectStatus = CONNECT_STATU_WAITING_CONFIRM;
            g_sSPPInstance->nNextMessageHeader = CONNECT_CONFIRM_CONNECT;
            LLCFrameWrite(NULL,0,0,CONNECT_FRAME);
            SetTimer(TIMER_1_CONNECT_CONFIRM,RESENT_CONFIRM_CONNECT_TIMEOUT,true,Timer1_FinialComfirmTimeout,NULL);
            return 1;
        }
        else
        {
            nErrorMessageHeader = CONNECT_ERROR_VALUE | (nMessageHeader & CONNECT_TYPE_MASK);
            g_sSPPInstance->nNextMessageHeader = nErrorMessageHeader;
            LLCFrameWrite(NULL,0,0,CONNECT_FRAME);
            return 0;
        }
    }
    else if(g_sSPPInstance->nConnectStatus == CONNECT_STATU_WAITING_LINK_CONFIRM)
    {
        if(nMessageHeader == CONNECT_REQUIRE_DISCONNECT)    //收到断连接请求
        {
            g_sSPPInstance->nConnectStatus == CONNECT_STATU_DISCONNECTED;
            g_sSPPInstance->nNextMessageHeader = CONNECT_CONFIRM_DISCONNECT;
            LLCFrameWrite(NULL,0,0,CONNECT_FRAME);
            //关闭所有的定时任务
            CancleAllTimerTask();
            return 1;
        }
        else if(nMessageHeader == CONNECT_CONFIRM_CONNECT)
        {
            //g_sSPPInstance->nConnectStatus = CONNECT_STATU_WAITING_LLC_RESET;
            g_sSPPInstance->nConnectStatus = CONNECT_STATU_CONNECTED;
            CancelTimerTask(TIMER_0_CONNECT,CANCEL_MODE_IMMEDIATELY);
            SetTimer(TIMER3_ACK_TIMEOUT,SEND_ACK_TIMEOUT,false,Timer3_ACKTimeout,NULL);
            g_sSPPInstance->nNextMessageHeader = CONNECT_CONFIRM_AGAIN;
            LLCFrameWrite(NULL,0,0,CONNECT_FRAME);
            return 1;
            //等待对方发送reset帧，初始化LLC层
            //在状态为 CONNECT_STATU_WAITING_LLC_RESET 并且收到了 reset 帧的情况下，连接状态为已连接 CONNECT_STATU_CONNECTED
        }
        else 
        {
            nErrorMessageHeader = CONNECT_ERROR_VALUE | (nMessageHeader & CONNECT_TYPE_MASK);
            g_sSPPInstance->nNextMessageHeader = nErrorMessageHeader;
            LLCFrameWrite(NULL,0,0,CONNECT_FRAME);
            return 0;
        }
    }
    else if(g_sSPPInstance->nConnectStatus == CONNECT_STATU_WAITING_CONFIRM)
    {
        if(nMessageHeader == CONNECT_CONFIRM_AGAIN)             //收到对方的最终确认帧，转换状态，完成连接的建立
        {
            g_sSPPInstance->nConnectStatus = CONNECT_STATU_CONNECTED;
            //g_sSPPInstance->nConnectStatus = CONNECT_STATU_WAITING_LLC_UA;
            //发送reset帧，初始化LLC层
            //g_aLLCInstance[0]->nNextCtrlFrameToSend = LLC_FRAME_RST;
            g_aLLCInstance[0]->nNextCtrlFrameToSend = CONNECT_IDLE;
            CancelTimerTask(TIMER_1_CONNECT_CONFIRM,CANCEL_MODE_IMMEDIATELY);
            SetTimer(TIMER3_ACK_TIMEOUT,SEND_ACK_TIMEOUT,false,Timer3_ACKTimeout,NULL);  
            return 1;          
            //在状态为 CONNECT_STATU_WAITING_LLC_UA 并且收到了 ua 帧的情况下，连接状态为已连接 CONNECT_STATU_CONNECTED
        }
        else if(nMessageHeader == CONNECT_REQUIRE_CONNECT)      //响应的确认建立连接帧丢失，对方重发了请求帧
        {
            g_sSPPInstance->nNextMessageHeader = CONNECT_CONFIRM_CONNECT;
            LLCFrameWrite(NULL,0,0,CONNECT_FRAME);
            SetTimer(TIMER_1_CONNECT_CONFIRM,RESENT_CONFIRM_CONNECT_TIMEOUT,true,Timer1_FinialComfirmTimeout,NULL);
            return 1;
        }
        else if(nMessageHeader == CONNECT_REQUIRE_DISCONNECT)   //收到断连接请求
        {
            g_sSPPInstance->nConnectStatus == CONNECT_STATU_DISCONNECTED;
            g_sSPPInstance->nNextMessageHeader = CONNECT_CONFIRM_DISCONNECT;
            LLCFrameWrite(NULL,0,0,CONNECT_FRAME);
            //关闭所有的定时任务
            CancleAllTimerTask();
            return 1;
        }
        else
        {
            nErrorMessageHeader = CONNECT_ERROR_VALUE | (nMessageHeader & CONNECT_TYPE_MASK);
            g_sSPPInstance->nNextMessageHeader = nErrorMessageHeader;
            LLCFrameWrite(NULL,0,0,CONNECT_FRAME);
            return 0;
        }
    }
    else if(g_sSPPInstance->nConnectStatus == CONNECT_STATU_CONNECTED)
    {
        if(nMessageHeader == CONNECT_SEND_RECV_MESSAGE)         //收到一帧数据帧
        {
            //这里应该是进不来的
            return 1;
        }
        else if(nMessageHeader == CONNECT_REQUIRE_DISCONNECT)   //收到一个断连接请求
        {
            g_sSPPInstance->nConnectStatus == CONNECT_STATU_DISCONNECTED;
            g_sSPPInstance->nNextMessageHeader = CONNECT_CONFIRM_DISCONNECT;
            LLCFrameWrite(NULL,0,0,CONNECT_FRAME);
            //关闭所有的定时任务
            CancleAllTimerTask();
            return 1;
        }
        else if(nMessageHeader == CONNECT_REQUIRE_RESET)        //收到一个重启请求
        {
            g_sSPPInstance->nConnectStatus == CONNECT_STATU_DISCONNECTED;
            g_sSPPInstance->nNextMessageHeader = CONNECT_CONFIRM_RESET;
            LLCFrameWrite(NULL,0,0,CONNECT_FRAME);
            //关闭所有的定时任务
            CancleAllTimerTask();
            return 1;
        }
        else if(nMessageHeader == CONNECT_REQUIRE_CONNECT)
        {
            g_sSPPInstance->nNextMessageHeader = CONNECT_CONFIRM_CONNECT;
            LLCFrameWrite(NULL,0,0,CONNECT_FRAME);
            return 1;
        }
        else if(nMessageHeader == CONNECT_CONFIRM_AGAIN)
        {
            return 1;
        }
        else 
        {
            nErrorMessageHeader = CONNECT_ERROR_VALUE | (nMessageHeader & CONNECT_TYPE_MASK);
            g_sSPPInstance->nNextMessageHeader = nErrorMessageHeader;
            LLCFrameWrite(NULL,0,0,CONNECT_FRAME);
            return 0;
        }
    }
    else if(g_sSPPInstance->nConnectStatus == CONNECT_STATU_WAITING_DISCONNECT_CONFIRM)
    {
        if(nMessageHeader == CONNECT_CONFIRM_DISCONNECT)
        {
            g_sSPPInstance->nConnectStatus == CONNECT_STATU_DISCONNECTED;
            //关闭所有的定时任务
            CancleAllTimerTask();
            return 1;
        }
        else 
        {
            nErrorMessageHeader = CONNECT_ERROR_VALUE | (nMessageHeader & CONNECT_TYPE_MASK);
            g_sSPPInstance->nNextMessageHeader = nErrorMessageHeader;
            LLCFrameWrite(NULL,0,0,CONNECT_FRAME);
            return 0;
        }
    }
    else if(g_sSPPInstance->nConnectStatus == CONNECT_STATU_WAITING_RESET_CONFIRM)
    {
        if(nMessageHeader == CONNECT_CONFIRM_RESET)             //
        {
            g_sSPPInstance->nConnectStatus == CONNECT_STATU_DISCONNECTED;
            //关闭所有的定时任务
            CancleAllTimerTask();
            ConnectToMCU();
            return 1;
        }
        else if(nMessageHeader == CONNECT_REQUIRE_DISCONNECT)   //收到断连接请求
        {
            g_sSPPInstance->nConnectStatus == CONNECT_STATU_DISCONNECTED;
            g_sSPPInstance->nNextMessageHeader = CONNECT_CONFIRM_DISCONNECT;
            LLCFrameWrite(NULL,0,0,CONNECT_FRAME);
            //关闭所有的定时任务
            CancleAllTimerTask();
            return 1;
        }
        else 
        {
            nErrorMessageHeader = CONNECT_ERROR_VALUE | (nMessageHeader & CONNECT_TYPE_MASK);
            g_sSPPInstance->nNextMessageHeader = nErrorMessageHeader;
            LLCFrameWrite(NULL,0,0,CONNECT_FRAME);
            return 0;
        }
    }
}