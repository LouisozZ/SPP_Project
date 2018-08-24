#include "spp_global.h"

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
        g_aLLCInstance[index]->bIsFirstFregment = true;

        //写操作相关
        g_aLLCInstance[index]->nWindowSize = g_sSPPInstance->nWindowSize;
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

        static_ResetWindowSendRecv(g_aLLCInstance[index],MAX_WINDOW_SIZE);
        
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

    LOCK_WRITE();

    //加入到链表末尾
    if(!bIsAddToHead)
    {
        if ( *pListHead == NULL)
        {
            *pListHead = pWriteContext;
            UNLOCK_WRITE();
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

    UNLOCK_WRITE();

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

    LOCK_WRITE();
    //加入到链表末尾
    if(!bIsAddToHead)
    {
        if ( *pListHead == NULL)
        {
            *pListHead = pWriteContext;
            UNLOCK_WRITE();
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

    UNLOCK_WRITE();
    return 1;
}

void static_ResetWindowSendRecv(tLLCInstance* pLLCInstance,uint8_t nWindowSize)
{
   pLLCInstance->nWindowSize = nWindowSize;

   /* Reset the connectiion */
   pLLCInstance->nReadNextToReceivedFrameId = 0x20;                      //下一个应该接收的帧ID
   pLLCInstance->nReadLastAcknowledgedFrameId = 0x1F;                    //接收到的最新的帧ID
   pLLCInstance->nReadT1Timeout = (TIMER_T1_TIMEOUT * nWindowSize)>>2;   //T1的接收超时
   pLLCInstance->nWriteLastAckSentFrameId = 0x1F;                        //最新的对发送帧的响应的ID
   printf("\nstatic_ResetWindowSendRecv() : 0x%08x ->nWriteLastAckSentFrameId : 0x%08x\n",pLLCInstance,pLLCInstance->nWriteLastAckSentFrameId);
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

static uint8_t* static_ReadALLCFrameFromeReadBuffer(tLLCInstance* pLLCInstanceWithPRI,uint8_t nFrameLength)
{
    tLLCInstance* pLLCInstance = pLLCInstanceWithPRI;
    uint8_t* pBuffer;
    uint8_t* pBuffer_return;
    uint8_t nThisLLCFrameLength = 0;
    nThisLLCFrameLength = nFrameLength;
    pBuffer = (uint8_t*)CMALLOC(sizeof(uint8_t)*nThisLLCFrameLength);
    pBuffer_return = pBuffer;

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
        g_sSPPInstance->bIsMessageReady = false;
        pBuffer = static_ReadALLCFrameFromeReadBuffer(pLLCInstance,nThisLLCFrameLength);
        nCtrlHeader = *pBuffer;
#ifdef DEBUG_PRINTF
        printf("\npBuffer : ");
        for(int index = 0; index < nThisLLCFrameLength; index++)
        {
            printf("0x%02x ",pBuffer[index]);
        }
        printf("\n");
#endif   
        //得到信息帧的 N(S)，放在nReceivedFrameId中
        DealIDProblemForIFrame(pLLCInstance,nCtrlHeader);
        
        //nReadLastAcknowledgedFrameId 是我已经给对方发送了的已经接受的确认信息
        nNonAcknowledgedFrameNumber = pLLCInstance->nReadNextToReceivedFrameId - pLLCInstance->nReadLastAcknowledgedFrameId;
        
        /* If this number is greater than or equals to the window size */
        if( nNonAcknowledgedFrameNumber >= pLLCInstance->nWindowSize )
        {
           /* Then send an acknowledge *///ACK是I帧或者RR帧
            pLLCInstance->nNextCtrlFrameToSend = READ_CTRL_FRAME_ACK;
        }
        else if( nNonAcknowledgedFrameNumber == 1 )
        {
            printf("\nalready read the last frame or other side haven't send frame.\n");
        }

        /* cancel thr TIMER_T6_SHDLC_RESEND and clear flag if an I-frame
        is received before timer expiration*/
        // if(pLLCInstance->bIsRRFrameAlreadySent == true)
        // {
        //    pLLCInstance->bIsRRFrameAlreadySent = false;
        //    /*T6是应答超时时间，如果T6时间过去了还没有收到发送帧的应答，则重发，如果收到了，则取消T6超时任务*/
        //    //PNALMultiTimerCancel(pBindingContext, TIMER_T6_SHDLC_RESEND);
        // }

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
            g_sSPPInstance->bIsMessageReady = true;
            //把一帧 LLC 帧解析成message向上传递
        }
        for(int index = 2; index < nThisLLCFrameLength; index++)
        {
            *(g_sSPPInstance->pMessageBuffer + g_sSPPInstance->nMessageLength) = pBuffer[index];
            g_sSPPInstance->nMessageLength += 1;
        }

    }  
    else
    {
        //上层的message结构不为空，还没有把消息读走，不能写入到，不对消息帧做处理
        printf("\nmessage still in buffer\n");
        return 0;
    }
    printf("\nCFREE(pBuffer)\n");
    CFREE(pBuffer);
   MACFrameWrite();
   return;
}

uint32_t LLCFrameWrite(uint8_t* pSendMessage,uint32_t nMessageLength,uint8_t nMessagePriority,bool bIsData)
{
    uint32_t nWriteByteCount = 0;
    uint8_t nSingleLLCFrameLength = 0;
    uint32_t nRemainingPayload = nMessageLength;
    uint8_t* pSingleLLCFrame = NULL;
    uint8_t* pSendMessageAddress = pSendMessage;
    tLLCWriteContext* pLLCWriteContext = NULL;
    tLLCInstance* pLLCInstance = NULL;
    bool bIsFirstFregment = true;

    //加上一个消息头的长度
    nRemainingPayload++;

    while(nRemainingPayload > 0)
    {
        pLLCWriteContext = (tLLCWriteContext*)CMALLOC(sizeof(tLLCWriteContext));

        if(nRemainingPayload > LLC_FRAME_MAX_DATA_LENGTH)
        {
            pLLCWriteContext->bIsLastFragment = false;
            //+4 LEN LLCHEADER PACKAGEHEADER 预留一个 CRC 
            pSingleLLCFrame = (uint8_t*)CMALLOC(sizeof(uint8_t)*(LLC_FRAME_MAX_LENGTH));
            *pSingleLLCFrame = (uint8_t)(LLC_FRAME_MAX_DATA_LENGTH + 2);
            //Package head
            *(pSingleLLCFrame + 2) = 0;             //初始化该字节为0，方面后面进行或运算
            *(pSingleLLCFrame + 2) |= 0x00;         //不是最后一片，分片的 CB 位为0
            if(bIsData)
                *(pSingleLLCFrame + 2) |= 0x40;     //是一个数据帧，DT位1
            *(pSingleLLCFrame + 2) |= (g_nVersion & VERSION_FIELD_MASK);   //版本信息
            *(pSingleLLCFrame + 2) |= (nMessagePriority & PRIORITY_MASK);  //优先级信息            
        }
        else
        {
            pLLCWriteContext->bIsLastFragment = true;
            pSingleLLCFrame = (uint8_t*)CMALLOC(sizeof(uint8_t)*(nRemainingPayload + 4));
            *pSingleLLCFrame = (uint8_t)(nRemainingPayload + 2);
            //Package head
            *(pSingleLLCFrame + 2) = 0;             //初始化该字节为0，方面后面进行或运算
            *(pSingleLLCFrame + 2) |= 0x80;         //是最后一片，分片的 CB 位为1
            if(bIsData)
                *(pSingleLLCFrame + 2) |= 0x40;     //是一个数据帧，DT位1
            *(pSingleLLCFrame + 2) |= (g_nVersion & VERSION_FIELD_MASK);   //版本信息
            *(pSingleLLCFrame + 2) |= (nMessagePriority & PRIORITY_MASK);  //优先级信息
        }

        *(pSingleLLCFrame + 1) = 0x80;  //信息帧，N(S)和N(R)字段在发送的时候填充
        if(bIsFirstFregment)
        {
            bIsFirstFregment = false;
            *(pSingleLLCFrame + 3) = g_sSPPInstance->nNextMessageHeader;
            g_sSPPInstance->nNextMessageHeader = CONNECT_IDLE;
            for(nSingleLLCFrameLength = 0; nSingleLLCFrameLength <  *pSingleLLCFrame-3; nSingleLLCFrameLength++)
                *(pSingleLLCFrame + 4 + nSingleLLCFrameLength) = *pSendMessageAddress++;
            nSingleLLCFrameLength++;    //加上没有记录的 message header 的长度
        }
        else
        {
            for(nSingleLLCFrameLength = 0; nSingleLLCFrameLength <  *pSingleLLCFrame-2; nSingleLLCFrameLength++)
                *(pSingleLLCFrame + 3 + nSingleLLCFrameLength) = *pSendMessageAddress++;
        }
        
        

        pLLCWriteContext->pFrameBuffer = pSingleLLCFrame;
        pLLCWriteContext->nFrameLength = *pSingleLLCFrame + 2;//total length conclude len and crc
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
        //data frame add to the bottom, but the connect frame add to the head in order to be found firstly
        if(bIsData)
            static_AddToWriteContextList(pLLCInstance,pLLCWriteContext,0);
        else 
            static_AddToWriteContextList(pLLCInstance,pLLCWriteContext,1);
    }
    return nWriteByteCount-1;//in fact the data to be sent don't have message header
}

tLLCInstance* GetCorrespondingLLCInstance(uint8_t* pLLCFrameWithLength)
{
    uint8_t nPriority = 0;
    nPriority = (*(pLLCFrameWithLength+2) & PRIORITY_MASK);
    CDebugAssert(nPriority < PRIORITY);
    return g_aLLCInstance[nPriority];
}

