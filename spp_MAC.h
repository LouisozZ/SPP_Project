#ifndef __SPP_MAC_H__
#define __SPP_MAC_H__
#include "spp_def.h"
#include "spp_LLC.h"

//typedef void MACFrameReadCompleted(tLLCInstance* pLLCInstance, uint8_t* pLLCFrame,uint8_t nLLCFrameLength);
typedef void MACFrameReadCompleted(tLLCInstance* pLLCInstance);

typedef struct tMACInstance{
    //读状态机
    uint8_t nMACReadStatu;                                  //读状态机状态
    uint8_t aMACReadFrameBuffer[MAC_FRAME_MAX_LENGTH];      //从底层读上来的字节流
    uint8_t nMACReadFrameRemaining;                         //未读字节流长度
    uint8_t nMACReadPosition;                               //上一个变量的读位置指针
    uint8_t nMACReadCRC;                                    //用于计算读到的MAC帧CRC值
    uint8_t aMACReadBuffer[LLC_FRAME_MAX_LENGTH];           //保存MAC帧的有效载荷的数组

    MACFrameReadCompleted* pMACReadCompletedCallback;
    
    //写状态机
    uint8_t nMACWriteStatu;
    uint8_t aMACWriteFrameBuffer[MAC_FRAME_MAX_LENGTH];
    uint8_t nMACWritePosition;
    uint8_t nMACWriteLength;

    /*
    读回调函数定义
    */

}tMACInstance;

tLLCInstance* MACFrameRead();
uint8_t InitMACInstance();
uint8_t static_RemoveInsertedZero(uint8_t* aBufferInsertedZero,uint8_t* pBufferRemovedZero,uint8_t nLengthInsteredZero);
uint8_t static_InsertZero(uint8_t* aMACReadBuffer,uint8_t* pNewBuffer,uint8_t nOldLength);
bool CtrlFrameAcknowledge(uint8_t nCtrlFrame,tLLCInstance *pLLCInstance);


#endif