#include "spp_include.h"

uint8_t InitLLCInstance()
{
    for(int index = 0; index < PRIORITY; index++)
    {
        g_aLLCInstance[index] = (tLLCInstance*)CMALLOC(sizeof(tLLCInstance));
        CDebugAssert(g_aLLCInstance[index] != NULL);
        //g_aLLCInstance[index]->aLLCReadBuffer = (uint8_t*)CMALLOC(sizeof(uint8_t)*READ_BUFFER_SIZE);
        g_aLLCInstance[index]->nLLCReadReadPosition = 0;
        g_aLLCInstance[index]->nLLCReadWritePosition = 0;
        g_aLLCInstance[index]->bIsReadBufferFull = false;
        g_aLLCInstance[index]->nReadNextToReceivedFrameId = 0;
        g_aLLCInstance[index]->nReadLastAcknowledgedFrameId = 0;
        g_aLLCInstance[index]->nReadT1Timeout = 0;
        g_aLLCInstance[index]->pReadHandler = NULL;
        g_aLLCInstance[index]->pReadHandlerParameter = NULL;

        //写操作相关
        g_aLLCInstance[index]->nWindowSize = 4;
        g_aLLCInstance[index]->pMessageData = (uint8_t*)CMALLOC(sizeof(uint8_t)*SINGLE_MESSAGE_MAX_LENGTH);
        g_aLLCInstance[index]->nMessageLength = 1;
        g_aLLCInstance[index]->nWritePosition = 0;
        g_aLLCInstance[index]->bIsWriteWindowsFull = false;
        g_aLLCInstance[index]->bIsWriteOtherSideReady = false;    
        g_aLLCInstance[index]->bIsRRFrameAlreadySent = false;
        g_aLLCInstance[index]->nNextCtrlFrameToSend = 0;
        g_aLLCInstance[index]->pWriteHandler = NULL;
        g_aLLCInstance[index]->pWriteHandlerParameter = NULL;

        g_aLLCInstance[index]->pLLCFrameWriteListHead = NULL;
        g_aLLCInstance[index]->pLLCFrameWriteCompletedListHead = NULL;

        //g_aLLCInstance[index]->sLLCFrameNextToSend.aLLCFrameData = (uint8_t*)CMALLOC(sizeof(uint8_t)*LLC_FRAME_MAX_LENGTH);
        g_aLLCInstance[index]->sLLCFrameNextToSend.nLLCFrameLength = 0;

        for(uint8_t nWindowNum = 0; nWindowNum < MAX_WINDOW_SIZE; nWindowNum++)
        {
            g_aLLCInstance[index]->aSlideWindow[nWindowNum] = (tMACWriteContext*)CMALLOC(sizeof(tMACWriteContext));
            g_aLLCInstance[index]->aSlideWindow[nWindowNum]->pFrameBuffer = NULL;
            g_aLLCInstance[index]->aSlideWindow[nWindowNum]->nFrameLength = 0;
            g_aLLCInstance[index]->aSlideWindow[nWindowNum]->pNext = NULL;
        }
    }
    return 1;
}
#define PNALUtilConvertUint32ToPointer( nValue ) \
         ((void*)(((uint8_t*)0) + (nValue)))

void static_AvoidCounterSpin(tLLCInstance* pLLCInstance)
{
   pLLCInstance->nReadNextToReceivedFrameId += 0x20;   //加32？    0x 0010 0000
   pLLCInstance->nReadLastAcknowledgedFrameId += 0x20;
   pLLCInstance->nWriteLastAckSentFrameId += 0x20;
   pLLCInstance->nWriteNextToSendFrameId += 0x20;
   pLLCInstance->nWriteNextWindowFrameId += 0x20;
}

/**
 * @function    把一个LLC帧的写对象加入到一个LLC实体的写链表中
 * @parameter1  LLC实体对象
 * @parameter2  一个LLC帧写对象
 * @parameter3  插入选项，如果为真就把写对象加入到链表头，否则加入到链表尾
 * @return      错误码 
*/
static uint8_t static_AddToWriteContextList(tLLCInstance* pLLCInstance, tLLCWriteContext* pLLCWriteContext,bool bIsAddToHead)
{
    tLLCWriteContext** pListHead = NULL;
    tLLCWriteContext* pWriteContext = pLLCWriteContext;
    tLLCWriteContext* pTempNode = NULL;

    pListHead = &(pLLCInstance->pLLCFrameWriteListHead);

    //加入到链表末尾
    if(!bIsAddToHead)
    {
        if ( *pListHead == NULL)
        {
            *pListHead = pWriteContext;
            return 1;
        }

        while((*pListHead)->pNext != NULL)
        {
            pListHead = &(*pListHead)->pNext;
        }
        (*pListHead)->pNext = pWriteContext;
    }//加入到链表头
    else
    {
        pTempNode = *pListHead;
        //##加锁
        *pListHead = pWriteContext;
        (*pListHead)->pNext = pTempNode;
        //##解锁
    }

    return 1;
}

/**
 * @function    把一个LLC帧的写对象加入到一个LLC实体的写完成链表中
 * @parameter1  LLC实体对象
 * @parameter2  一个LLC帧写对象
 * @parameter3  插入选项，如果为真就把写对象加入到链表头，否则加入到链表尾
 * @return      错误码 
*/
uint8_t static_AddToWriteCompletedContextList(tLLCInstance* pLLCInstance, tMACWriteContext* pMACWriteContext,bool bIsAddToHead)
{
    tLLCWriteContext** pListHead = NULL;
    tLLCWriteContext* pWriteContext = pMACWriteContext;
    tLLCWriteContext* pTempNode = NULL;

    pListHead = &(pLLCInstance->pLLCFrameWriteCompletedListHead);

    //加入到链表末尾
    if(!bIsAddToHead)
    {
        if ( *pListHead == NULL)
        {
            *pListHead = pWriteContext;
            return 1;
        }

        while((*pListHead)->pNext != NULL)
        {
            pListHead = &(*pListHead)->pNext;
        }
        (*pListHead)->pNext = pWriteContext;
    }//加入到链表头
    else
    {
        pTempNode = *pListHead;
        *pListHead = pWriteContext;
        (*pListHead)->pNext = pTempNode;
    }

    return 1;
}
//这个函数的作用是为接收到的控制帧写响应帧，并把发送动作放到 DFC 链表中
bool static_WriteReceptionCtrlFrame(tLLCInstance* pLLCInstance,uint8_t nCtrlFrame/*,uint32_t nSHDLCFrameReceptionCounter*/)
{
    return true;
}
// static bool static_WriteReceptionCtrlFrame(
//          tLLCInstance* pLLCInstance,
//          uint8_t nCtrlFrame/*,
//          uint32_t nSHDLCFrameReceptionCounter*/)
// {
//    uint8_t nMaskedValue = nCtrlFrame & 0xF8;  /* the next to receive to zero */
//    uint32_t nNextToReceive;
//    bool bAlreadyCalled = false;
//    uint32_t nPosition;

//    CDebugAssert(pLLCInstance != NULL);

//    //PNALDebugTrace("static_WriteReceptionCtrlFrame( nCtrlFrame=0x%02X )", nCtrlFrame);
//    //static_PSHDLCWriteTraceStatus(pLLCInstance, "static_WriteReceptionCtrlFrame()");

//    if((nMaskedValue != FRAME_RNR_MASK)
//    && (nMaskedValue != FRAME_RR_MASK)
//    && (nMaskedValue != FRAME_REJ_MASK))
//    {
//       if(nCtrlFrame == FRAME_UA)
//       {
//          PDebugWarning("Residual UA frame, ignore it");
//       }
//       else
//       {
//          PDebugWarning("Unknown control frame, ignore it");
//       }
//     //   pLLCInstance->nReadFrameLost++;
//     //   pLLCInstance->nReadByteErrorCount ++;
//       return true;
//    }

//    if(nMaskedValue == FRAME_RNR_MASK)
//    {
//       pLLCInstance->bIsWriteOtherSideReady = false;
//    }
//    else
//    {
//       pLLCInstance->bIsWriteOtherSideReady = true;
//    }

//       // nNextToReceive 对方期望接收到的下一帧 ID 
//    nNextToReceive = static_ConvertTo32BitIdentifier(pLLCInstance, nCtrlFrame & 0x07);

//       //需要弄懂 nWriteNextWindowFrameId 这个值的计算与含义
//    if((nNextToReceive <= pLLCInstance->nWriteNextWindowFrameId) &&
//       (pLLCInstance->nWriteLastAckSentFrameId < nNextToReceive))
//    {
//       if(nNextToReceive < pLLCInstance->nWriteNextWindowFrameId)
//       {
//          uint32_t nFrameLost = pLLCInstance->nWriteNextWindowFrameId - nNextToReceive;
//          /* In normal case, the  frameToSend is processed after received and analysing
//            I-frame and RR-frame.
//           In few case the  frameToSend is processed after analysing Received RR-frame
//           and before analysing Received I-frame, in such case (i.e FrameLost == 0x01),
//           we consider the I-frame analysis is delayed, so no frame is lost*/
//          if(nFrameLost == 0x01)
//          {
//             //PNALDebugWarning("I-frame analysis was delayed after frame send process");
//             pLLCInstance->nWriteLastAckSentFrameId = nNextToReceive - 1;
//             return true;
//          }
//          //PNALDebugWarning("Sent frames are lost (%d frames)", nFrameLost);
//          //pLLCInstance->nWriteFrameLost += nFrameLost;

//          for(nPosition = nNextToReceive; nPosition < pLLCInstance->nWriteNextWindowFrameId; nPosition++)
//          {
//             pLLCInstance->nWriteByteErrorCount += pLLCInstance->aWriteFrameArray[
//                static_Modulo(nPosition, pLLCInstance->nWindowSize)].nLength;
//          }
//       }

//       /* Parse all the acknowledged frames to send the frame counter of the acknoledgement */
//       for(nPosition = pLLCInstance->nWriteLastAckSentFrameId + 1;
//          nPosition < nNextToReceive;
//          nPosition++)
//       {
//          CDebugAssert(pLLCInstance->aWriteFrameArray[
//                static_Modulo(nPosition, pLLCInstance->nWindowSize)].nLength >= 2);

//          if(((pLLCInstance->aWriteFrameArray[
//                static_Modulo(nPosition, pLLCInstance->nWindowSize)].aData[1]) & 0x80) != 0)
//          {
//             bAlreadyCalled = true;
//       //pLLCInstance->pWriteHandler = static_PHCIFrameSHDLCWriteAcknowledged
//             // PNALDFCPost2( pBindingContext, P_DFC_TYPE_SHDLC,
//             //    pLLCInstance->pWriteHandler,
//             //    pLLCInstance->pWriteHandlerParameter,
//             //    PNALUtilConvertUint32ToPointer(nSHDLCFrameReceptionCounter));
//          }
//       }

//       pLLCInstance->nWriteLastAckSentFrameId = nNextToReceive - 1;
//       pLLCInstance->nWriteNextWindowFrameId = nNextToReceive;

//       if(pLLCInstance->nWriteNextToSendFrameId == nNextToReceive)
//       {
//          /* All frames are acknlowledged */

//          /* Cancel the guard/transmit timeout */
//          //PNALMultiTimerCancel( pBindingContext, TIMER_T2_SHDLC_RESEND );

//          /* Informs the caller if the buffer was full */
//          if( pLLCInstance->bIsWriteWindowsFull )
//          {
//             pLLCInstance->bIsWriteWindowsFull = false;

//             if(bAlreadyCalled == false)
//             {
//             //    PNALDFCPost2( pBindingContext, P_DFC_TYPE_SHDLC,
//             //       pLLCInstance->pWriteHandler,
//             //       pLLCInstance->pWriteHandlerParameter,
//             //       (void*)0);  /* Zero means: no acknoledgment of a complete message */
//             }
//          }
//       }
//       else
//       {
//          /* Some frames still needs to be send */
//          static_PSHDLCWriteTransmit(pBindingContext, pLLCInstance, false);
//       }
//    }
//    else
//    {
//       PNALDebugWarning("Out of range NR value in the frame, ignore it");
//       pLLCInstance->nReadFrameLost++;
//       pLLCInstance->nReadByteErrorCount ++;

//       return false;
//    }

//    return true;
// }

static void static_ResetWindowSendRecv(tLLCInstance* pLLCInstance,uint8_t nWindowSize)
{
   pLLCInstance->nWindowSize = nWindowSize;

   /* Reset the connectiion */
   pLLCInstance->nReadNextToReceivedFrameId = 0x20;                      //下一个应该接收的帧ID
   pLLCInstance->nReadLastAcknowledgedFrameId = 0x1F;                    //接收到的最新的帧ID
   pLLCInstance->nReadT1Timeout = (TIMER_T1_TIMEOUT * nWindowSize)>>2;   //T1的接收超时
   pLLCInstance->nWriteLastAckSentFrameId = 0x1F;                        //最新的对发送帧的响应的ID
   pLLCInstance->nWriteNextToSendFrameId = 0x20;                         //下一个要发送的帧ID
   pLLCInstance->nWriteNextWindowFrameId = 0x20;                         //下一个要被写的窗口ID

   pLLCInstance->bIsWriteOtherSideReady = true;                          //通信的另一方是否准备就绪
}

static bool static_CheckIfRSTFrame(tLLCInstance* pLLCInstance,uint8_t* pBuffer,uint32_t nLength)
{
   CDebugAssert(pLLCInstance != NULL );
   CDebugAssert(nLength >= 1);

   if((nLength <= 2) && (pBuffer[0] == LLC_FRAME_RST))
   {
      uint8_t nWindowSize = DEFAULT_WINDOW_SIZE;

      if(nLength == 2)
      {
         nWindowSize = pBuffer[1];
         if(nWindowSize == 0)
         {
            /* Error in the frame format */
            return false;
         }
      }

      if(nWindowSize <= MAX_WINDOW_SIZE)
      {
         static_ResetWindowSendRecv(pLLCInstance, nWindowSize);
         return true;
      }
   }
   return false;
}
static bool static_PSHDLCReadWriteInBuffer(
         tLLCInstance* pLLCInstance,
         uint8_t* pFrameBuffer,
         uint32_t nFrameLength,
         uint32_t nSHDLCFrameReceptionCounter)
{
//    uint32_t nDataLength;
//    uint32_t nNextPosition;

//    CDebugAssert(pLLCInstance != NULL );
//    CDebugAssert(nFrameLength > 0);
//    CDebugAssert(nFrameLength <= 0xFF);

//    nDataLength = pLLCInstance->nReadBufferDataLength;
//    nNextPosition = pLLCInstance->nReadNextPositionToRead + nDataLength;
//    nNextPosition &= (P_SHDLC_READ_BUFFER_SIZE - 1);

//    pLLCInstance->aReadBuffer[nNextPosition++] = (uint8_t)nFrameLength;
//    nNextPosition &= (P_SHDLC_READ_BUFFER_SIZE - 1);
//    pLLCInstance->aReadBuffer[nNextPosition++] = (uint8_t)((nSHDLCFrameReceptionCounter >> 24) & 0xFF);
//    nNextPosition &= (P_SHDLC_READ_BUFFER_SIZE - 1);
//    pLLCInstance->aReadBuffer[nNextPosition++] = (uint8_t)((nSHDLCFrameReceptionCounter >> 16) & 0xFF);
//    nNextPosition &= (P_SHDLC_READ_BUFFER_SIZE - 1);
//    pLLCInstance->aReadBuffer[nNextPosition++] = (uint8_t)((nSHDLCFrameReceptionCounter >> 8) & 0xFF);
//    nNextPosition &= (P_SHDLC_READ_BUFFER_SIZE - 1);
//    pLLCInstance->aReadBuffer[nNextPosition++] = (uint8_t)(nSHDLCFrameReceptionCounter & 0xFF);
//    nNextPosition &= (P_SHDLC_READ_BUFFER_SIZE - 1);

//    if(nNextPosition + nFrameLength <= P_SHDLC_READ_BUFFER_SIZE)
//    {
//       CNALMemoryCopy(
//          &pLLCInstance->aReadBuffer[nNextPosition],
//          pFrameBuffer, nFrameLength);
//    }
//    else
//    {
//       uint32_t nCopyLength = P_SHDLC_READ_BUFFER_SIZE - nNextPosition;

//       CNALMemoryCopy(
//          &pLLCInstance->aReadBuffer[nNextPosition],
//          pFrameBuffer, nCopyLength);

//       CNALMemoryCopy(
//          &pLLCInstance->aReadBuffer[0],
//          pFrameBuffer + nCopyLength, nFrameLength - nCopyLength);
//    }

//    pLLCInstance->nReadBufferDataLength = nDataLength + nFrameLength + 5;

//    return (nDataLength == 0)?true:false;
}

static uint8_t* static_ReadALLCFrameFromeReadBuffer(tLLCInstance* pLLCInstanceWithPRI,uint8_t nFrameLength)
{
    tLLCInstance* pLLCInstance = pLLCInstanceWithPRI;
    uint8_t* pBuffer;
    uint8_t* pBuffer_return;
    uint8_t nThisLLCFrameLength = 0;
    nThisLLCFrameLength = nFrameLength;
    pBuffer = (uint8_t*)CMALLOC(sizeof(uint8_t)*nThisLLCFrameLength);
    pBuffer_return = pBuffer;

    //printf("\nstatic_ReadALLCFrameFromeReadBuffer -> pLLCInstance : 0x%08x\n",pLLCInstance);

    for(int index = 0; index < nThisLLCFrameLength; index++)
    {
        *pBuffer++ = pLLCInstance->aLLCReadBuffer[pLLCInstance->nLLCReadReadPosition+index+1];
    }

    if((pLLCInstance->nLLCReadReadPosition + nThisLLCFrameLength + 1) >= READ_BUFFER_SIZE)
        pLLCInstance->nLLCReadReadPosition = pLLCInstance->nLLCReadReadPosition + nThisLLCFrameLength + 1 - READ_BUFFER_SIZE;
    else
        pLLCInstance->nLLCReadReadPosition = pLLCInstance->nLLCReadReadPosition + nThisLLCFrameLength + 1;

    return pBuffer_return;
}

uint8_t LLCReadFrame(tLLCInstance* pLLCInstanceWithPRI)
{
    tLLCInstance* pLLCInstance = pLLCInstanceWithPRI;
    uint8_t* pBuffer = NULL;
    uint8_t nThisLLCFrameLength = 0;
    uint8_t nCtrlHeader;
    uint32_t nReceivedFrameId = 0;
    uint32_t nNonAcknowledgedFrameNumber;

    if(pLLCInstance->nLLCReadReadPosition == pLLCInstance->nLLCReadWritePosition)
    {
        printf("\nread bufffer is empty!\n");
        return 0;
    }

    bool bForceTransmit = false;
    nThisLLCFrameLength = (pLLCInstance->aLLCReadBuffer[pLLCInstance->nLLCReadReadPosition]);

    CDebugAssert(pLLCInstance != NULL );
    CDebugAssert(nThisLLCFrameLength >= 1);

    //上层的message结构为空，已经把消息读走，可以写入
    //在等待分片，组织成完整一帧，可以写入
    if((g_sSPPInstance->nMessageLength == 0) || (pLLCInstance->bIsWaitingLastFragment == true))
    {
        //printf("\nLLCReadFrame -> pLLCInstance : 0x%08x\n",pLLCInstance);
        pBuffer = static_ReadALLCFrameFromeReadBuffer(pLLCInstance,nThisLLCFrameLength);
        nCtrlHeader = *pBuffer;

        printf("\npBuffer : ");
        for(int index = 0; index < nThisLLCFrameLength; index++)
        {
            printf("0x%02x ",pBuffer[index]);
        }
        printf("\n");
        
        //得到信息帧的 N(S)，放在nReceivedFrameId中
        nReceivedFrameId = (nCtrlHeader >> 3) & 0x07;

        /* build a RR frame with the same "next to receive identifier" */
        nCtrlHeader &= 0x07;
        nCtrlHeader |= 0xC0;
        /* Send this fake acknowledgememnt piggy-backed with the I-Frame
         * to the the write state machine
        */
        //根据帧头 nCtrlHeader 来写响应
        //CtrlFrameAcknowledge(nCtrlHeader,pLLCInstance)
        if (static_WriteReceptionCtrlFrame(pLLCInstance, nCtrlHeader) != true)
        {
            //PNALDebugWarning("No more processing of this frame...");
            //return;
        }
        // if(nReceivedFrameId != (pLLCInstance->nReadNextToReceivedFrameId & 0x07))
        // {
        //     /* No, at least a frame is missing, reject the frame */
        //     //PNALDebugWarning("The received frame id does not match the expected id, drop it with REJ");
        //     pLLCInstance->nNextCtrlFrameToSend = READ_CTRL_FRAME_REJ;
        //     //bForceTransmit = true;
        //     // pLLCInstance->nReadFrameLost++;
        //     goto function_return;
        // }

        //检查SPPInstance的pMessageBuffer中是否有内容未被读走，若是，则不对LLC帧做处理
        // if(static_PSHDLCReadCheckFreeSize(pLLCInstance, nLength) == false)
        // {
        //    PNALDebugWarning("Not enough space to store the received data, drop it");
        // //    pLLCInstance->nReadFrameLost++;
        // //    pLLCInstance->nReadByteErrorCount += nLength;
        //     /* Not enough space to store the data */
        //     pLLCInstance->nNextCtrlFrameToSend = READ_CTRL_FRAME_RNR;
        //     pLLCInstance->bIsReadBufferFull = true;

        //    goto function_return;
        // }


        /* Compute the number of non-acknoledged frames */
        nNonAcknowledgedFrameNumber = pLLCInstance->nReadNextToReceivedFrameId - pLLCInstance->nReadLastAcknowledgedFrameId;
        
        /* If this number is greater than or equals to the window size */
        if( nNonAcknowledgedFrameNumber >= pLLCInstance->nWindowSize )
        {
           /* Then send an acknowledge *///ACK是I帧或者RR帧
            pLLCInstance->nNextCtrlFrameToSend = READ_CTRL_FRAME_ACK;
        }
        else if( nNonAcknowledgedFrameNumber == 1 )
        {
           /* It's the first frame in the window, start the acknowledge timer */
            // PNALMultiTimerSet( pBindingContext,
            //   TIMER_T1_SHDLC_ACK, pLLCInstance->nReadT1Timeout,
            //   &static_PSHDLCReadTimerCompleted,
            //   NULL );
        }

        pLLCInstance->nReadNextToReceivedFrameId++;
        if(pLLCInstance->nReadNextToReceivedFrameId == 0)
        {
            static_AvoidCounterSpin(pLLCInstance);
        }


        /* cancel thr TIMER_T6_SHDLC_RESEND and clear flag if an I-frame
        is received before timer expiration*/
        if(pLLCInstance->bIsRRFrameAlreadySent == true)
        {
           pLLCInstance->bIsRRFrameAlreadySent = false;
           /*T6是应答超时时间，如果T6时间过去了还没有收到发送帧的应答，则重发，如果收到了，则取消T6超时任务*/
           //PNALMultiTimerCancel(pBindingContext, TIMER_T6_SHDLC_RESEND);
        }

        /* If the I-frame is HCI chained, force the transmition of the next RR */
        if((pBuffer[1] & 0x80) == 0)
        {
            bForceTransmit = true;
            printf("\n is waiting last fragment ... \n");
            pLLCInstance->bIsWaitingLastFragment = true;
            //组装一个完整的message，需要一直取 LLC 帧
        }
        else
        {
            printf("\nthis is the last fragment or this is a single message\n");
            pLLCInstance->bIsWaitingLastFragment = false;
            //把一帧 LLC 帧解析成message向上传递
        }
        for(int index = 2; index < nThisLLCFrameLength; index++)
        {
            *(g_sSPPInstance->pMessageBuffer + g_sSPPInstance->nMessageLength) = pBuffer[index];
            g_sSPPInstance->nMessageLength += 1;
        }
        
        //    /* The buffer was empty, send the data ready event */
        //    PNALDFCPost1(
        //       pBindingContext, P_DFC_TYPE_SHDLC,
        //       pLLCInstance->pReadHandler,
        //       pLLCInstance->pReadHandlerParameter);
    }  
    else
    {
        //上层的message结构不为空，还没有把消息读走，不能写入到，不对消息帧做处理
        printf("\nmessage still in buffer\n");
        return 0;
    }
function_return:
   //static_PSHDLCWriteTransmit(pBindingContext, pLLCInstance, bForceTransmit);
   return;
}

uint32_t LLCFrameWrite(uint8_t* pSendMessage,uint32_t nMessageLength,uint8_t nMessagePriority)
{
    uint32_t nWriteByteCount = 0;
    uint8_t nSingleLLCFrameLength = 0;
    int nRemainingPayload = nMessageLength;
    uint8_t* pSingleLLCFrame = NULL;
    uint8_t* pSendMessageAddress = pSendMessage;
    tLLCWriteContext* pLLCWriteContext = NULL;
    tLLCInstance* pLLCInstance = NULL;

    while(nRemainingPayload > 0)
    {
        if(nRemainingPayload > LLC_FRAME_MAX_DATA_LENGTH)
        {
            //+4 LEN LLCHEADER PACKAGEHEADER 预留一个 CRC 
            pSingleLLCFrame = (uint8_t*)CMALLOC(sizeof(uint8_t)*(LLC_FRAME_MAX_LENGTH));
            *pSingleLLCFrame = (uint8_t)(LLC_FRAME_MAX_DATA_LENGTH + 2);
            //Package head
            *(pSingleLLCFrame + 2) = 0;             //初始化该字节为0，方面后面进行或运算
            *(pSingleLLCFrame + 2) |= 0x00;         //不是最后一片，分片的 CB 位为0
            *(pSingleLLCFrame + 2) |= (g_nVersion & VERSION_FIELD_MASK);   //版本信息
            *(pSingleLLCFrame + 2) |= (nMessagePriority & PRIORITY_MASK);  //优先级信息            
        }
        else
        {
            pSingleLLCFrame = (uint8_t*)CMALLOC(sizeof(uint8_t)*(nRemainingPayload + 4));
            *pSingleLLCFrame = (uint8_t)(nRemainingPayload + 2);
            //Package head
            *(pSingleLLCFrame + 2) = 0;             //初始化该字节为0，方面后面进行或运算
            *(pSingleLLCFrame + 2) |= 0x80;         //是最后一片，分片的 CB 位为1
            *(pSingleLLCFrame + 2) |= (g_nVersion & VERSION_FIELD_MASK);   //版本信息
            *(pSingleLLCFrame + 2) |= (nMessagePriority & PRIORITY_MASK);  //优先级信息
        }

        *(pSingleLLCFrame + 1) = 0x80;  //信息帧，N(S)和N(R)字段在发送的时候填充
        for(nSingleLLCFrameLength = 0; nSingleLLCFrameLength <  *pSingleLLCFrame-2; nSingleLLCFrameLength++)
        {
            *(pSingleLLCFrame + 3 + nSingleLLCFrameLength) = *pSendMessageAddress++;
        }
        pLLCWriteContext = (tLLCWriteContext*)CMALLOC(sizeof(tLLCWriteContext));

        pLLCWriteContext->pFrameBuffer = pSingleLLCFrame;
        pLLCWriteContext->nFrameLength = *pSingleLLCFrame + 2;
        //pLLCWriteContext->pCallbackFunction = (LLCFrameWriteCompleted*)CMALLOC(sizeof(LLCFrameWriteCompleted));
        pLLCWriteContext->pCallbackFunction = NULL;
        pLLCWriteContext->pCallbackParameter = (void*)CMALLOC(sizeof(void));        
        pLLCWriteContext->pCallbackParameter = NULL;
        //pLLCWriteContext->pSreamCallbackFunction = (LLCFrameWriteGetData*)CMALLOC(sizeof(LLCFrameWriteGetData));
        pLLCWriteContext->pSreamCallbackFunction = NULL;
        pLLCWriteContext->pStreamCallbackParameter = (void*)CMALLOC(sizeof(void));        
        pLLCWriteContext->pStreamCallbackParameter = NULL;
        //pLLCWriteContext->pNext = (tLLCWriteContext*)CMALLOC(sizeof(tLLCWriteContext));
        pLLCWriteContext->pNext = NULL;

        nWriteByteCount += nSingleLLCFrameLength;
        nRemainingPayload -= nSingleLLCFrameLength;

        pLLCInstance = GetCorrespondingLLCInstance(pSingleLLCFrame);
        static_AddToWriteContextList(pLLCInstance,pLLCWriteContext,0);
    }
    return nWriteByteCount;
}

tLLCInstance* GetCorrespondingLLCInstance(uint8_t* pLLCFrameWithLength)
{
    uint8_t nPriority = 0;
    nPriority = (*(pLLCFrameWithLength+2) & PRIORITY_MASK);
    CDebugAssert(nPriority < PRIORITY);
    return g_aLLCInstance[nPriority];
}

