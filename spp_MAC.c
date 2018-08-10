#include "spp_include.h"

uint8_t InitMACInstance()
{
    g_sMACInstance = (tMACInstance*)CMALLOC(sizeof(tMACInstance));
    g_sMACInstance->nMACReadStatu = MAC_FRAME_READ_STATUS_IDLE;                                  //读状态机状态
    //g_sMACInstance->aMACReadFrameBuffer = (uint8_t*)CMALLOC(sizeof(uint8_t)*MAC_FRAME_MAX_LENGTH);
    g_sMACInstance->nMACReadFrameRemaining = 0;                         //未读字节流长度
    g_sMACInstance->nMACReadPosition = 0;                               //上一个变量的读位置指针
    g_sMACInstance->nMACReadCRC = 0;                                    //用于计算读到的MAC帧CRC值
    //g_sMACInstance->aMACReadBuffer = (uint8_t*)CMALLOC(sizeof(uint8_t)*LLC_FRAME_MAX_LENGTH);           //保存MAC帧的有效载荷的数组
    g_sMACInstance->pMACReadCompletedCallback = NULL;
    g_sMACInstance->nMACWriteStatu = 0;
    //g_sMACInstance->aMACWriteFrameBuffer = (uint8_t*)CMALLOC(sizeof(uint8_t)*MAC_FRAME_MAX_LENGTH);
    g_sMACInstance->nMACWritePosition = 0;
    g_sMACInstance->nMACWriteLength = 0;

    //滑动窗口

    return 1;
}

bool CheckReadBufferFree(tLLCInstance* pLLCInstance)
{
    if(pLLCInstance->nLLCReadReadPosition <= pLLCInstance->nLLCReadWritePosition)
    {
        if((pLLCInstance->nLLCReadWritePosition - pLLCInstance->nLLCReadReadPosition) > 200)
            return false;
        else
            return true;
    }
    else 
    {
        if((READ_BUFFER_SIZE - pLLCInstance->nLLCReadReadPosition + pLLCInstance->nLLCReadWritePosition ) > 200)
            return false;
        else
            return true;
    }
}

tLLCInstance* MACFrameRead()
{
    tLLCInstance* pLLCInstance;
    bool bError = false;
    uint8_t nPriority = 0;
    uint8_t nStatus;
    uint8_t nWritePosition = 0;
    uint8_t nReadPosition;
    uint8_t nByteValue;
    uint32_t nByteCount;
    uint8_t* pReadBuffer;
    uint32_t nReadBufferCount;
    uint8_t nCRC = 0;
    uint8_t* pDataRemovedZero;

    uint8_t nCtrlHeader;
    //uint32_t nSHDLCFrameReceptionCounterToCall = 0;

    //tPSHDLCFrameReadCompleted* pReadCallbackFunctionToCall = NULL;
    //void* pReadCallbackParameterToCall = NULL;
    //uint8_t* pReadBufferToCall = NULL;
    //uint8_t nPositionToCall = 0;
    g_sMACInstance->nMACReadStatu = MAC_FRAME_READ_STATUS_WAITING_HEADER;
    g_sMACInstance->nMACReadCRC = 0;

    CDebugAssert(g_sMACInstance != NULL);
    CDebugAssert(g_sMACInstance->nMACReadStatu != MAC_FRAME_READ_STATUS_IDLE);

    nStatus = g_sMACInstance->nMACReadStatu;
    nReadPosition = g_sMACInstance->nMACReadPosition;
    nByteCount = 0;
    nReadBufferCount = g_sMACInstance->nMACReadFrameRemaining;
    pReadBuffer = g_sMACInstance->aMACReadFrameBuffer;
    CDebugAssert(pReadBuffer != NULL);
    pReadBuffer += nReadPosition;

    
      //只要读状态机的状态不是空闲
    while(nStatus != MAC_FRAME_READ_STATUS_IDLE)
    {
        //如果读缓冲区为空，则调用一次读函数 ReadBytes()，并将实际读到的字节数保存在变量pReadBuffer中
        if(nReadBufferCount == 0)
        {

            pReadBuffer = g_sMACInstance->aMACReadFrameBuffer;
            CDebugAssert(pReadBuffer != NULL);
            nReadPosition = 0;
            if( (nReadBufferCount = ReadBytes(pReadBuffer, MAC_FRAME_MAX_LENGTH)) == 0)
            {
               break;
            }

        }
        CDebugAssert(nReadBufferCount <= MAC_FRAME_MAX_LENGTH);

        //nByteValue 为每次取下的一个字节
        nByteValue = *pReadBuffer++;
        //nReadBufferCount 字节流缓存中剩余还没有读取的字节数
        nReadBufferCount--;
        //nByteCount MAC帧字节统计
        nByteCount++;
        //nReadPosition aMACReadFrameBuffer要读取的下一字节位置
        nReadPosition++;
        
        switch(nStatus)
        {
        case MAC_FRAME_READ_STATUS_WAITING_HEADER:
            CDebugAssert( nWritePosition == 0 );
            //如果读到的第一个字节不是 HEADER_SOF 头
            if( nByteValue != HEADER_SOF )
            {
                //printf("\nnByteValue != HEADER_SOF\nbError occur\n");
                bError = true;
            }
            else     //如果读到的第一个字节是 HEADER_SOF 头，那么设置状态为等待字节，即等待有效载荷
            {
               nStatus = MAC_FRAME_READ_STATUS_WAITING_BYTE;
            }
            break;
        case MAC_FRAME_READ_STATUS_WAITING_BYTE:
            //在状态是等待有效载荷的情况下
            if( nByteValue == HEADER_SOF )  // 在等待数据的 case 下，这时候要是有效载荷中出现了 MAC 帧头，则是错误
            {
#ifdef DEBUG_PEINTF
                printf("\nnByteValue == HEADER_SOF\nbError occur\n");
#endif
                bError = true;
            }
            else if( nByteValue == TRAILER_EOF ) // TX 尾部
            {                
                if(nByteCount < 2)
                {
                   // To few data, error skip the frame 
#ifdef DEBUG_PEINTF
                   printf("\nnByteCount < 2\nbError occur\n");
#endif
                   bError = true;
                   break;
                }

                //如果正确处理了最后一帧，设置读状态机的状态为空闲
                nStatus = MAC_FRAME_READ_STATUS_IDLE;                
            }
            else     //有效载荷数据
            {
                if(nWritePosition >= MAC_FRAME_MAX_LENGTH + 1)
                {
                   // 超过一个MAC帧的最大字节数，丢掉
#ifdef DEBUG_PEINTF
                   printf("\nnWritePosition == MAC_FRAME_MAX_LENGTH + 1\nbError occur\n");
#endif
                    bError = true;
                }
                else
                {
                    g_sMACInstance->aMACReadBuffer[nWritePosition++] = nByteValue;
                    //g_sMACInstance->nMACReadCRC ^= nByteValue;                    
                }
            }
            break;
        default:
            //PNALDebugError("static_PSHDLCFrameReadLoopRXTX: Wrong state");
            //CDebugAssert(false);
            return NULL;
        } // switch() 
        
        if(bError)
        {
            //PNALDebugWarning("static_PSHDLCFrameReadLoopRXTX: Frame error detected");
            nWritePosition = 0;
            bError = false;

            g_sMACInstance->nMACReadCRC = 0;

            if( nByteValue == HEADER_SOF )
            {
                nStatus = MAC_FRAME_READ_STATUS_WAITING_BYTE;
                nByteCount = 1;
            }
            else
            {
                nStatus = MAC_FRAME_READ_STATUS_WAITING_HEADER;
                nByteCount = 0;
            }
        }
    } // while() 
    
    //MAC帧要做去0处理的字节数 = nByteCount - 2;
    //把执行了插 0 操作的 LLC 帧复原，然后根据优先级字段选择合适的处理实体。
    pDataRemovedZero = (uint8_t*)CMALLOC(sizeof(uint8_t)*(nByteCount - 2));
    nCRC = static_RemoveInsertedZero(g_sMACInstance->aMACReadBuffer,pDataRemovedZero,(nByteCount - 2));

#ifndef DEBUG_PEINTF
    printf("\ng_sMACInstance->aMACReadBuffer : ");
    for(int index = 0; index < nCRC; index++)
        printf("0x%02x ",g_sMACInstance->aMACReadBuffer[index]);
    printf("\n");

    printf("\npDataRemovedZero : ");
    for(int index = 0; index < nCRC; index++)
        printf("0x%02x ",*(pDataRemovedZero + index));
    printf("\n");
#endif

//=======================对帧进行判断，如果是控制帧，立马处理，如果是信息帧，丢到读缓冲   ====================
    //控制帧
    if(*pDataRemovedZero == 1)
    {
        //if((*(pDataRemovedZero+*pDataRemovedZero+1)) != (*(pDataRemovedZero+*pDataRemovedZero)))
        //     MACFrameRead();
        printf("\nctrl frame!\n");
        //响应
        return NULL;
    }//可能是信息帧，可能是RST帧
    else
    {
        //信息帧
        pLLCInstance = GetCorrespondingLLCInstance(pDataRemovedZero);
        CDebugAssert(pLLCInstance != NULL);
        for(int index = 0; index < *pDataRemovedZero; index++)
        {
            g_sMACInstance->nMACReadCRC ^= *(pDataRemovedZero+index+1);
        }
        nCRC = *(pDataRemovedZero+*pDataRemovedZero+1);
        // if(nCRC != g_sMACInstance->nMACReadCRC)
        //     MACFrameRead();
        nCtrlHeader = *(pDataRemovedZero + 1);
        if((nCtrlHeader & 0xc0) == 0x80)
        {
#ifndef DEBUG_PEINTF
            printf("\n******1*******\npLLCInstance->nLLCReadReadPosition : %d",pLLCInstance->nLLCReadReadPosition);
            printf("\npLLCInstance->nLLCReadWritePosition : %d\n******1*******\n",pLLCInstance->nLLCReadWritePosition);
            printf("\n\nin MACFrameRead() start\n\
nByteCount : %d \n\
LLC Frame effective length : %d\n\
pDataRemovedZero : ",nByteCount,*pDataRemovedZero);
#endif  
            for(int index = 0; index <= *pDataRemovedZero; index++)
            {
                if(pLLCInstance->nLLCReadWritePosition >= READ_BUFFER_SIZE)
                    pLLCInstance->nLLCReadWritePosition = 0;

#ifndef DEBUG_PEINTF
                printf("0x%02x ",*(pDataRemovedZero + index));
#endif

                pLLCInstance->aLLCReadBuffer[(pLLCInstance->nLLCReadWritePosition)++] = *(pDataRemovedZero + index);
            }
#ifndef DEBUG_PEINTF
            printf("\n******2*******\npLLCInstance->nLLCReadReadPosition : %d",pLLCInstance->nLLCReadReadPosition);
            printf("\npLLCInstance->nLLCReadWritePosition : %d\n******2*******\n",pLLCInstance->nLLCReadWritePosition);
#endif
            if(!CheckReadBufferFree(pLLCInstance))//读缓冲区满了
            {
                pLLCInstance->nNextCtrlFrameToSend = READ_CTRL_FRAME_RNR;
                pLLCInstance->bIsReadBufferFull = true;
            }
        }//RST帧
        else 
        {

        }
    }
//=====================================================================================================
    
    g_sMACInstance->nMACReadFrameRemaining = nReadBufferCount;
    g_sMACInstance->pMACReadCompletedCallback = pLLCInstance->pReadHandler;
    g_sMACInstance->nMACReadStatu = nStatus;
    g_sMACInstance->nMACReadPosition = nReadPosition;

    if(g_sMACInstance->pMACReadCompletedCallback != NULL)
    {
        //if(bDirectCall)
        if(1)
        {
            //CDebugAssert(nSHDLCFrameReceptionCounterToCall != 0);

            // pReadCallbackFunctionToCall(pBindingContext,
            //   pReadCallbackParameterToCall,
            //   pReadBufferToCall, nPositionToCall,
            //   nSHDLCFrameReceptionCounterToCall);
            if(g_sMACInstance->pMACReadCompletedCallback != NULL)
                g_sMACInstance->pMACReadCompletedCallback(pLLCInstance);
        }
        else
        {
            // PNALDFCPost4( pBindingContext, P_DFC_TYPE_SHDLC_FRAME,
            //   pReadCallbackFunctionToCall,
            //   pReadCallbackParameterToCall,
            //   pReadBufferToCall, nPositionToCall,
            //   nSHDLCFrameReceptionCounterToCall);
        }
    }
    printf("\nMACFrameRead -> pLLCInstance : 0x%08x\n",pLLCInstance);
    return pLLCInstance;
}

uint8_t static_RemoveInsertedZero(uint8_t* aBufferInsertedZero,uint8_t* pBufferRemovedZero,uint8_t nLengthInsteredZero)
{
#ifndef TEST_MAIN
    return 0x00;
#endif
#ifndef TEST_MAIN
    return 0x00;
#endif
    uint8_t nByteValue;
    uint8_t n5Conut = 0;
    uint8_t nOutBitCount = 0;
    uint8_t *pOutByte = pBufferRemovedZero;
    uint8_t nBitValue;
    uint8_t nIgnore = 0;
    *pOutByte = 0;
    for(int index = 0; index < nLengthInsteredZero; index++)
    {
        nByteValue = *aBufferInsertedZero++;
        for(int bits = 0; bits < 8; bits++)
        {
            if(nByteValue & (0x80 >> bits))
            {
                if(nIgnore == 1)
                {
                    printf("the process inserting zero error!\nreturn...\n");
                    return ;
                }
                n5Conut++;
                nBitValue = 0x80;
            }
            else
            {
                n5Conut = 0;
                if(nIgnore == 1)
                {
                    nIgnore = 0;
                    continue;
                }
                nBitValue = 0;
            }
            
            if(((nOutBitCount % 8) == 0) && (nOutBitCount != 0))
            {
                pOutByte++;
                *pOutByte = 0;
            }
            
            *pOutByte |= (nBitValue >> (nOutBitCount++)%8);
            if(n5Conut >= 5)
            {
                nIgnore = 1;
            }
        }
    }
    if((nOutBitCount % 8) == 0)
        return nOutBitCount/8;
    else 
        return (nOutBitCount/8 + 1);
}

//static uint8_t static_InsertZero(uint8_t* aMACReadBuffer,uint8_t nOldLength)
uint8_t static_InsertZero(uint8_t* aMACReadBuffer,uint8_t* pNewBuffer,uint8_t nOldLength)
{
#ifndef TEST_MAIN
    return 0x00;
#endif
    uint8_t nByteValue;
    uint8_t n5Conut = 0;
    uint8_t nOutBitCount = 0;
    uint8_t *pOutByte = pNewBuffer;
    uint8_t nBitValue;
    *pOutByte = 0;
    for(int index = 0; index < nOldLength; index++)
    {
        nByteValue = *aMACReadBuffer++;
        for(int bits = 0; bits < 8; bits++)
        {
            if(nByteValue & (0x80 >> bits))
            {
                n5Conut++;
                nBitValue = 0x80;
            }
            else
            {
                n5Conut = 0;
                nBitValue = 0;
            }
            
            if(((nOutBitCount % 8) == 0) && (nOutBitCount != 0))
            {
                pOutByte++;
                *pOutByte = 0;
            }
            
            if(n5Conut < 5)
            {
                *pOutByte |= (nBitValue >> (nOutBitCount++)%8);
            }
            else
            {
                *pOutByte |= (nBitValue >> (nOutBitCount++)%8);
                if((nOutBitCount % 8) == 0)
                {
                    pOutByte++;
                    *pOutByte = 0;
                }
                *pOutByte |= (0 >> (nOutBitCount++)%8);
                n5Conut = 0;
            }
        }
    }
    if((nOutBitCount % 8) == 0)
        return nOutBitCount/8;
    else 
        return (nOutBitCount/8 + 1);
}