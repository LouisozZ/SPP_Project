#include "spp_global.h"

/**
 * @function    初始化每个优先级实体，优先级在 spp_def.h 文件中定义，可修改
*/
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
        g_aLLCInstance[index]->bIsWaitingLastFragment = false;
        

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
            //g_aLLCInstance[index]->aSlideWindow[nWindowNum]->pFrameBuffer = NULL;
            g_aLLCInstance[index]->aSlideWindow[nWindowNum]->pFrameBuffer = (uint8_t*)CMALLOC(sizeof(uint8_t)*(MAC_FRAME_MAX_LENGTH));            
            g_aLLCInstance[index]->aSlideWindow[nWindowNum]->nFrameLength = 0;
            g_aLLCInstance[index]->aSlideWindow[nWindowNum]->pNext = NULL;
        }
    }
    return 1;
}

#define PNALUtilConvertUint32ToPointer( nValue ) \
         ((void*)(((uint8_t*)0) + (nValue)))

/**
 * @function    避免ID出错，因为都是无符号类型的变量，所以只要有一个出现了整数溢出，则每个 ID 都加了 0x20 之后都会溢出
 *              保证了他们的大小关系和连续性
 * @parameter1  对应的优先级实体
*/
void static_AvoidCounterSpin(tLLCInstance* pLLCInstance)
{
   pLLCInstance->nReadNextToReceivedFrameId += 0x20;   
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
// uint8_t static_AddToWriteCompletedContextList(tLLCInstance* pLLCInstance, tMACWriteContext* pMACWriteContext,bool bIsAddToHead)
// {
//     tLLCWriteContext** pListHead = NULL;
//     tLLCWriteContext* pWriteContext = pMACWriteContext;
//     tLLCWriteContext* pTempNode = NULL;

//     pListHead = &(pLLCInstance->pLLCFrameWriteCompletedListHead);

//     LOCK_WRITE();
//     //加入到链表末尾
//     if(!bIsAddToHead)
//     {
//         if ( *pListHead == NULL)
//         {
//             *pListHead = pWriteContext;
//             UNLOCK_WRITE();
//             return 1;
//         }

//         while((*pListHead)->pNext != NULL)
//         {
//             pListHead = &(*pListHead)->pNext;
//         }
//         (*pListHead)->pNext = pWriteContext;
//     }//加入到链表头
//     else
//     {
//         pTempNode = *pListHead;
//         *pListHead = pWriteContext;
//         (*pListHead)->pNext = pTempNode;
//     }

//     UNLOCK_WRITE();
//     return 1;
// }

/**
 * @function    在接收到 RST 帧或者是发送 RST 帧后得到了对方响应的 UA 帧之后，重置 ID
 * @parameter1  需要重置 ID 的优先级
 * @parameter2  新的窗口大小
*/
void static_ResetWindowSendRecv(tLLCInstance* pLLCInstance,uint8_t nWindowSize)
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

/**
 * @function    从一个 LLC 优先级实体的读缓冲区中读取出一个 LLC 帧，
 *              这个 LLC 帧结构是 [len] + [llc header] + [pack header] + [payload]
 * @parameter1  对应优先级实体
 * @parameter2  [len] 的值，是 [llc header] + [pack header] + [payload] 的总字节数
 * @return      copy 出来的一帧 LLC 帧的首地址
*/
static uint8_t* static_ReadALLCFrameFromeReadBuffer(tLLCInstance* pLLCInstanceWithPRI,uint8_t nFrameLength)
{
    tLLCInstance* pLLCInstance = pLLCInstanceWithPRI;
    uint8_t* pBuffer;
    uint8_t* pBuffer_return;
    uint8_t nThisLLCFrameLength = 0;
    nThisLLCFrameLength = nFrameLength;

    //这个 LLC 帧结构是 [len] + [llc header] + [pack header] + [payload]
    //[len] 的值，是 [llc header] + [pack header] + [payload] 的总字节数
    //所以要加上一字节的 [len]
    pBuffer = (uint8_t*)CMALLOC(sizeof(uint8_t)*nThisLLCFrameLength + 1);
    //return 的时候的返回值
    pBuffer_return = pBuffer;

    for(int index = 0; index <= nThisLLCFrameLength; index++)
    {
        *pBuffer++ = pLLCInstance->aLLCReadBuffer[pLLCInstance->nLLCReadReadPosition];
        pLLCInstance->aLLCReadBuffer[pLLCInstance->nLLCReadReadPosition++] = 0;
    }

    //因为 nLLCReadReadPosition 是 uint8_t 类型的，且aLLCReadBuffer[READ_BUFFER_SIZE] 最大是256，所以自加溢出自动到头部
    // if((pLLCInstance->nLLCReadReadPosition + nThisLLCFrameLength + 1) >= READ_BUFFER_SIZE)
    //     pLLCInstance->nLLCReadReadPosition = pLLCInstance->nLLCReadReadPosition + nThisLLCFrameLength + 1 - READ_BUFFER_SIZE;
    // else
    //     pLLCInstance->nLLCReadReadPosition = pLLCInstance->nLLCReadReadPosition + nThisLLCFrameLength + 1;
    return pBuffer_return;
}

/**
 * @function    重组 message ，把一个个 LLC 帧，组装成一个完整的 message 放在全局唯一的 g_sSPPInstance->pMessageBuffer中
 * @parameter1  对应的优先级实体
*/
uint8_t LLCReadFrame(tLLCInstance* pLLCInstanceWithPRI)
{
    tLLCInstance* pLLCInstance = pLLCInstanceWithPRI;
    uint8_t* pBuffer = NULL;
    uint8_t* pBufferForFree = pBuffer;
    uint8_t nThisLLCFrameLength = 0;
    uint32_t nReceivedFrameId = 0;
    uint32_t nNonAcknowledgedFrameNumber;

    if(pLLCInstance->nLLCReadReadPosition == pLLCInstance->nLLCReadWritePosition)
    {
        #ifdef DEBUG_PRINTF
        printf("\nread bufffer is empty!\n");
        #endif

        return 0;
    }

    nThisLLCFrameLength = (pLLCInstance->aLLCReadBuffer[pLLCInstance->nLLCReadReadPosition]);

    //CDebugAssert(pLLCInstance != NULL );
    if(pLLCInstance == NULL)
    {
        #ifdef DEBUG_PRINTF
        printf("\npLLCInstance == NULL!\n");
        #endif

        return 0;
    }
    //CDebugAssert(nThisLLCFrameLength >= 1);
    if(nThisLLCFrameLength < 1)
    {
        #ifdef DEBUG_PRINTF
        printf("\nnThisLLCFrameLength < 1 !\n");
        printf("\ng_aLLCInstance[%d]->nLLCReadReadPosition : %d\n",GetPriorityBypLLCInstance(pLLCInstance),pLLCInstance->nLLCReadReadPosition);
        printf("\npLLCInstance->aLLCReadBuffer[pLLCInstance->nLLCReadReadPosition] : %d\n",pLLCInstance->aLLCReadBuffer[pLLCInstance->nLLCReadReadPosition]);
        printf("\nnThisLLCFrameLength : %d\n",nThisLLCFrameLength);
        #endif

        return 0;
    }

    //上层的message结构为空，已经把消息读走，可以写入
    //在等待分片，组织成完整一帧，可以写入
    if((g_sSPPInstance->nMessageLength == 0) || (pLLCInstance->bIsWaitingLastFragment == true))
    {
        LOCK_WRITE();

        g_sSPPInstance->bIsMessageReady = false;
        pBuffer = static_ReadALLCFrameFromeReadBuffer(pLLCInstance,nThisLLCFrameLength); 
        pBuffer++;  //跳过第一个长度字节
        //nReadLastAcknowledgedFrameId 是我已经给对方发送了的已经接受的确认信息
        nNonAcknowledgedFrameNumber = pLLCInstance->nReadNextToReceivedFrameId - pLLCInstance->nReadLastAcknowledgedFrameId;
        
        //如果接收了但是还没有响应的分片数量大于等于滑动窗口大小，则表示发送方的滑动窗口已经因为没有得到接收方响应而满了
        //所以需要响应一个 RR 帧，释放发送方滑动窗口资源
        if( nNonAcknowledgedFrameNumber >= pLLCInstance->nWindowSize )
        {
            pLLCInstance->nNextCtrlFrameToSend = READ_CTRL_FRAME_ACK;
        }
        else if( nNonAcknowledgedFrameNumber == 1 )
        {
            #ifdef DEBUG_PRINTF
            printf("\nalready read the last frame or other side haven't send frame.\n");
            #endif
        }

        //将一个消息分片接在之前分片数据的后面
        for(int index = 2; index < nThisLLCFrameLength; index++)
        {
            *(g_sSPPInstance->pMessageBuffer + g_sSPPInstance->nMessageLength) = pBuffer[index];
            g_sSPPInstance->nMessageLength += 1;
        }

        #ifdef DEBUG_PRINTF
        printf("\n++++++++++++  One of the recved pess :  ++++++++++++\n");
        for(int index = 2; index < nThisLLCFrameLength; index++)
        {
            printf("0x%02x ",pBuffer[index]);
        }
        printf("\n++++++++++++++++++++++++++++++++++++++++++++++++++++\n");
        printf("\ng_sSPPInstance->nMessageLength : %d\n",g_sSPPInstance->nMessageLength);
        #endif

        if((pBuffer[1] & 0x80) == 0)
        {
            //不是最后一片
            #ifdef DEBUG_PRINTF
            printf("\n is waiting last fragment ... \n");
            #endif

            pLLCInstance->bIsWaitingLastFragment = true;
            //组装一个完整的message，需要一直取 LLC 帧
        }
        else
        {
            //是最后一片
            #ifdef DEBUG_PRINTF
            printf("\nthis is the last fragment or this is a single message\n");
            #endif

            pLLCInstance->bIsWaitingLastFragment = false;
            g_sSPPInstance->bIsMessageReady = true;
            //把一帧 LLC 帧解析成message向上传递
        }
        UNLOCK_WRITE();
    }  
    else
    {
        //上层的message结构不为空，还没有把消息读走，不能写入到，不对消息帧做处理
        #ifdef DEBUG_PRINTF
        printf("\n^*^*^*^*^*^*^*^*^*^*^*^*^*^*^*^*^*^*^*^*^*^\n");
        printf("Message still in buffer!!!");
        printf("\n^*^*^*^*^*^*^*^*^*^*^*^*^*^*^*^*^*^*^*^*^*^\n");
        #endif

        return 0;
    }
    #ifdef DEBUG_PRINTF
    printf("\nCFREE(pBufferForFree)\n");
    #endif

    CFREE(pBufferForFree);
    MACFrameWrite();
    return;
}

/**
 * @function    把应用消息分片，并构造成 LLC 帧，按照分片顺序依次放到待发送链表中
 * @parameter1  要发送的应用数据的首地址
 * @parameter2  要发送的应用数据的字节数
 * @parameter3  要发送的应用数据的优先级
 * @parameter4  是否是应用数据，如果不是，则是连接控制命令
 * @return
*/
uint32_t LLCFrameWrite(uint8_t* pSendMessage,uint32_t nMessageLength,uint8_t nMessagePriority,bool bIsData)
{
    uint32_t nWriteByteCount = 0;
    uint8_t nSingleLLCFrameLength = 0;
    uint8_t nCopyPosition = 0;
    uint32_t nRemainingPayload = nMessageLength;
    uint8_t* pSingleLLCFrame = NULL;
    uint8_t* pSendMessageAddress = pSendMessage;
    tLLCWriteContext* pLLCWriteContext = NULL;
    tLLCInstance* pLLCInstance = NULL;
    bool bIsFirstFregment = true;

    //有效载荷总长，需要加上一个Msg Header的长度
    nRemainingPayload++;

    //消息分片
    while(nRemainingPayload > 0)
    {
        pLLCWriteContext = (tLLCWriteContext*)CMALLOC(sizeof(tLLCWriteContext));

        if(nRemainingPayload > LLC_FRAME_MAX_DATA_LENGTH)
        {
            pLLCWriteContext->bIsLastFragment = false;
            //+4 LEN LLCHEADER PACKAGEHEADER 预留一个 CRC 
            pSingleLLCFrame = (uint8_t*)CMALLOC(sizeof(uint8_t)*(LLC_FRAME_MAX_LENGTH));
            //第一个字节是LLC帧的总长，有效载荷加上一个 LLC Header 和 一个 Package Header
            *pSingleLLCFrame = (uint8_t)(LLC_FRAME_MAX_DATA_LENGTH + 2);
            //填充 Package head
            *(pSingleLLCFrame + 2) = 0;             //初始化该字节为0，方面后面进行或运算
            *(pSingleLLCFrame + 2) |= 0x00;         //不是最后一片，分片的 CB 位为0
            if(bIsData)
                *(pSingleLLCFrame + 2) |= 0x40;     //是一个数据帧，DT位1，文档中是CC位，Connect Control
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
        #ifdef DEBUG_PRINTF
        printf("\n!!!!!!!!!   a pess of pSendMessageAddress   !!!!!!!!!!!!!!\n");
        #endif

        if(bIsFirstFregment)
        {
            bIsFirstFregment = false;
            //第一个分片是有Msg Header 的
            *(pSingleLLCFrame + 3) = g_sSPPInstance->nNextMessageHeader;
            for(nSingleLLCFrameLength = 0; nSingleLLCFrameLength <  *pSingleLLCFrame-3; nSingleLLCFrameLength++)
            {
                #ifdef DEBUG_PRINTF
                printf("0x%02x ",*(pSendMessageAddress + nCopyPosition));
                #endif
                
                *(pSingleLLCFrame + 4 + nSingleLLCFrameLength) = *(pSendMessageAddress + nCopyPosition++);
            }
            nSingleLLCFrameLength++;    //加上没有记录的 message header 的长度
        }
        else
        {
            for(nSingleLLCFrameLength = 0; nSingleLLCFrameLength <  *pSingleLLCFrame-2; nSingleLLCFrameLength++)
            {
                #ifdef DEBUG_PRINTF
                printf("0x%02x ",*(pSendMessageAddress + nCopyPosition));
                #endif

                *(pSingleLLCFrame + 3 + nSingleLLCFrameLength) = *(pSendMessageAddress + nCopyPosition++);
            }    
        }
        
        #ifdef DEBUG_PRINTF
        printf("\n!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!\n");     
        printf("\n!!!!!!!!!   this pess in pSingleLLCFrameis   !!!!!!!!!!!!!\n");
        for(int index = 0; index < nSingleLLCFrameLength + 3; index++)
        {
                printf("0x%02x ",*(pSingleLLCFrame + index));
        }
        printf("\n!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!\n"); 
        #endif    

        pLLCWriteContext->pFrameBuffer = pSingleLLCFrame;
        pLLCWriteContext->nFrameLength = *pSingleLLCFrame + 2;//MAC 帧有效载荷是一个LLC帧（从LLC头部开始算起）总长加上他的长度信息（一字节）与crc信息（一字节）
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
            static_AddToWriteContextList(pLLCInstance,pLLCWriteContext,ADD_TO_LIST_TAIL);
        else 
            static_AddToWriteContextList(pLLCInstance,pLLCWriteContext,ADD_TO_LIST_HEAD);
    }
    g_sSPPInstance->nNextMessageHeader = CONNECT_IDLE;
    return nWriteByteCount-1;//in fact the data to be sent don't have message header
}

/**
 * @function    获得对应优先级的处理实体
 * @parameter1  做了去 0 处理的数据
 * @return      对应优先级的处理实体
*/
tLLCInstance* GetCorrespondingLLCInstance(uint8_t* pLLCFrameWithLength)
{
    uint8_t nPriority = 0;
    nPriority = (*(pLLCFrameWithLength+2) & PRIORITY_MASK);
    if(nPriority >= PRIORITY)
        return NULL;

    return g_aLLCInstance[nPriority];
}

