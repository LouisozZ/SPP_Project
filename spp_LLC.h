#ifndef __SPP_LLC_H__
#define __SPP_LLC_H__
#include "spp_def.h"


/* SHDLC Timeout values in ms */
#define TIMER_T1_TIMEOUT                     200
#define TIMER_T2_TIMEOUT                     50
#define TIMER_T3_TIMEOUT                     50
#define TIMER_T4_TIMEOUT                     5

#define TIMER_T6_TIMEOUT                     2

/* Connect state-machine status types */
#define CONNECT_STATUS_INIT                  0
#define CONNECT_STATUS_SEND_RESET_PENDING    1
#define CONNECT_STATUS_RESET_SENT            2
#define CONNECT_STATUS_SEND_UA_PENDING       3
#define CONNECT_STATUS_UA_SENT               4
#define CONNECT_STATUS_RESET_PENDING         5

/* The frame values and masks */
#define LLC_FRAME_NONE       0x00  /* Meaning: "no frame" */
#define LLC_FRAME_UA        (0xE0 | 0x06)
#define LLC_FRAME_RST       (0xE0 | 0x19)
#define LLC_FRAME_RR        (0xC0 | 0x00)
#define LLC_FRAME_RNR       (0xC0 | 0x10)
#define LLC_FRAME_REJ       (0xC0 | 0x08)
//LLC_FRAME_RR_MASK LLC_FRAME_RNR_MASK LLC_FRAME_REJ_MASK
#define LLC_I_FRAME_MASK  0xC0
#define LLC_S_FRAME_MASK  0xF8

/* The capability flags */
#define FLAG_CAPABILITY_SREJ_SUPPORTED    0x02

/* The default window size defined in the standard */
#define DEFAULT_WINDOW_SIZE         4

#if ((MAX_WINDOW_SIZE * (LLC_FRAME_MAX_LENGTH)) > READ_BUFFER_SIZE)
#error The read buffer should be able to contain the full window size
#endif

#define READ_CTRL_FRAME_NONE    0  /* meaning: no control frame to send */
#define READ_CTRL_FRAME_RNR     LLC_FRAME_RNR       /* with a S-RNR frame */
#define READ_CTRL_FRAME_ACK     LLC_FRAME_RR        /* with a S-RR frame or a I-frame */
#define READ_CTRL_FRAME_REJ     LLC_FRAME_REJ       /* with a S-REJ frame */
#define READ_CTRL_FRAME_UA      LLC_FRAME_UA        /* with a U-UA frame */

typedef void LLCFrameWriteCompleted();
typedef void LLCFrameWriteGetData();
typedef void LLCWriteAcknowledged();
typedef void LLCFrameReadData();

typedef struct tLLCWriteContext
{
   uint8_t*                        pLLCFrameBuffer;
   uint32_t                        nLLCFrameLength;

   LLCFrameWriteCompleted*         pCallbackFunction;
   void*                           pCallbackParameter;

   LLCFrameWriteGetData*           pSreamCallbackFunction;
   void*                           pStreamCallbackParameter;

   struct tLLCWriteContext* pNext;
} tLLCWriteContext;

typedef struct tLLCInstance
{
    //读操作相关
    uint8_t aLLCReadBuffer[READ_BUFFER_SIZE];
    uint8_t nLLCReadReadPosition;
    uint8_t nLLCReadWritePosition;
    bool bIsReadBufferFull;
    uint32_t nReadNextToReceivedFrameId;
    uint32_t nReadLastAcknowledgedFrameId;
    bool bIsWaitingLastFragment;
    uint32_t nReadT1Timeout;
    LLCFrameReadData* pReadHandler;
    void* pReadHandlerParameter;
    
    //写操作相关
    uint8_t nWindowSize;
    uint32_t nWriteLastAckSentFrameId;
    uint32_t nWriteNextToSendFrameId;
    uint32_t nWriteNextWindowFrameId;

    uint8_t *pMessageData;
    uint32_t nMessageLength;
    uint32_t nWritePosition;
    bool bIsWriteBufferFull;
    bool bIsWriteFramePending;
    bool bIsWriteOtherSideReady;    
    bool bIsRRFrameAlreadySent;
    uint8_t nNextCtrlFrameToSend;
    LLCWriteAcknowledged* pWriteHandler;
    void* pWriteHandlerParameter;

    tLLCWriteContext* pLLCFrameWriteListHead;
    tLLCWriteContext* pLLCFrameWriteCompletedListHead;

    struct tLLCFrameNextToSend{
        uint8_t aLLCFrameData[LLC_FRAME_MAX_LENGTH];
        uint8_t nLLCFrameLength;
    }sLLCFrameNextToSend;
}tLLCInstance;

uint8_t LLCReadFrame(tLLCInstance* pLLCInstanceWithPRI);
tLLCInstance* GetCorrespondingLLCInstance(uint8_t* pLLCFrameWithLength);
bool AnalysisReceptionCtrlFrame(tLLCInstance* pLLCInstance,uint8_t nCtrlFrame);
uint8_t InitLLCInstance();

#endif