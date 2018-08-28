#include "spp_global.h"

#define IS_NS_FIELD true
#define IS_NR_FIELD false

#define TIMER4_WINDOW_FULL_RESENT   TIMER_4
#define WINDOW_FULL_RESENT_TIMEOUT  700


#ifdef DEBUG_PRINTF
/**
 * @function    调试时候打印信息的辅助函数，确定当前的实体的优先级
 * @parameter1  LLC层处理实体
 * @return      当前实体的优先级
*/
int GetPriorityBypLLCInstance(tLLCInstance* pLLCInstance)
{
    for(int index = 0; index < PRIORITY; index++)
    {
        if(g_aLLCInstance[index] == pLLCInstance)
            return index;
    }
    return -1;
}
#endif

/**
 * @function    定时器2的回调函数，最后一片的发送超时。如果发送的分片是最后一片，那么会设置这个超时任务，超时时间到达之前
 *              如果没有收到最后一片的响应，则会重发最后一片。如果收到了就取消这个定时任务。
 * @parameter
 * @return
*/
static void Timer2_FinialResendTimeout(void *pParameter)
{
    #ifdef DEBUG_PRINTF
    printf("\ntimer2 timeout\n");
    #endif

    for(int index = 0; index < PRIORITY; index++)
    {
        if(g_aLLCInstance[index]->nWriteNextToSendFrameId == g_aLLCInstance[index]->nWriteLastAckSentFrameId + 2)
        {
            //还有一片没有得到接收方确认
            // if((g_aLLCInstance[index]->pLLCFrameWriteCompletedListHead == NULL))
            // {
            //     //如果有未发送的分片，但是已发送链表为空，则出错
            //     #ifdef DEBUG_PRINTF
            //     printf("\nThere is something wrong with protocol stack!\n");
            //     #endif
            //     //杀死进程！
            //     return ;
            // }
            // 因为取消了已发送链表，而用滑动窗口来替代其功能，所以注释掉了

            if(g_aLLCInstance[index]->pLLCFrameWriteCompletedListHead->bIsLastFragment)
            {
                //最后一片的发送超时
                printf("g_aLLCInstance[%d]->pLLCFrameWriteCompletedListHead is bIsLastFragment",index);
                g_aLLCInstance[index]->nWriteNextWindowFrameId = g_aLLCInstance[index]->nWriteLastAckSentFrameId + 1;
                return ;
            }
        }
    }
    return;
}

/**
 * @function    应答超时。应答超时在连接建立之后就开始运行，只要有接收了的分片未响应给发送方，每当应答超时到来，
 *              都会把所有已接收的分片都应答。
 * @parameter
 * @return
*/
void Timer3_ACKTimeout(void* pParameter)
{
    //printf("\ntimer3 timeout\n");
    for(int index = 0; index < PRIORITY; index++)
    {
        if((g_aLLCInstance[index]->nReadNextToReceivedFrameId > (g_aLLCInstance[index]->nReadLastAcknowledgedFrameId + 1)))
        {
            g_aLLCInstance[index]->nNextCtrlFrameToSend = READ_CTRL_FRAME_ACK;
            MACFrameWrite();
        }
        // if(g_aLLCInstance[index]->bIsWaitingLastFragment)
        //     g_aLLCInstance[index]->nNextCtrlFrameToSend = READ_CTRL_FRAME_ACK;
    }
    return;
}

/**
 * @function    滑动窗口满了之后的超时回调函数。如果很不幸，发送的几片分片接收方都没有收到，那么接收方是不知道发送方
 *              发送了数据的，这时候发送方的滑动窗口已经满了，不能再发送，程序就会一直阻塞在这里得不到释放，所以通过
 *              这个超时任务来重发滑动窗口里的内容。一旦滑动窗口不是满的，这个任务就会被取消掉。
 * @parameter   LLC实体，重发的是滑动窗口阻塞了的优先级实体
 * @return
*/
void Timer4_WindowFullResent(void* pParameter)
{
    tLLCInstance *pLLCInstance;
    pLLCInstance = (tLLCInstance*)pParameter;

    //为重发做准备，更改指向下一个要发送的分片的位置
    if(pLLCInstance->bIsWriteWindowsFull)
        pLLCInstance->nWriteNextWindowFrameId = pLLCInstance->nWriteLastAckSentFrameId + 1;

    for(uint32_t index = pLLCInstance->nWriteNextWindowFrameId; index < pLLCInstance->nWriteNextToSendFrameId;index++)
    {
        SPIWriteBytes(pLLCInstance,NULL,0,false);
    }
    return;
}

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
    g_sMACInstance->bIsWriteFramePending = false;

    //滑动窗口

    return 1;
}

uint8_t static_ResetLLC()
{
    printf("\nstatic_ResetLLC()\n");
    tLLCWriteContext* pWaitingToBeFreed = NULL;

    g_sSPPInstance->nConnectStatus = CONNECT_STATU_DISCONNECTED;
    g_sSPPInstance->nNextMessageHeader = CONNECT_IDLE;
    g_sSPPInstance->nMessageLength = 0;
    g_sSPPInstance->bIsMessageReady = false;
    g_sSPPInstance->pConnectCallbackFunction = NULL;
    g_sSPPInstance->pConnectCallbackParameter = NULL;
    g_sSPPInstance->pResetIndicationFunction = NULL;
    g_sSPPInstance->pResetIndicationFunctionParameter = NULL;

    g_sMACInstance->nMACReadStatu = MAC_FRAME_READ_STATUS_IDLE;                                  //读状态机状态
    g_sMACInstance->nMACReadFrameRemaining = 0;                         //未读字节流长度
    g_sMACInstance->nMACReadPosition = 0;                               //上一个变量的读位置指针
    g_sMACInstance->nMACReadCRC = 0;                                    //用于计算读到的MAC帧CRC值
    g_sMACInstance->pMACReadCompletedCallback = NULL;
    g_sMACInstance->nMACWriteStatu = 0;
    g_sMACInstance->nMACWritePosition = 0;
    g_sMACInstance->nMACWriteLength = 0;
    g_sMACInstance->bIsWriteFramePending = false;

    for(int index = 0; index < PRIORITY; index++)
    {
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
        g_aLLCInstance[index]->nMessageLength = 1;
        g_aLLCInstance[index]->nWritePosition = 0;
        g_aLLCInstance[index]->bIsWriteWindowsFull = false;
        g_aLLCInstance[index]->bIsWriteOtherSideReady = false;    
        g_aLLCInstance[index]->bIsRRFrameAlreadySent = false;
        g_aLLCInstance[index]->nNextCtrlFrameToSend = 0;
        g_aLLCInstance[index]->pWriteHandler = NULL;
        g_aLLCInstance[index]->pWriteHandlerParameter = NULL;

        if(g_aLLCInstance[index]->pLLCFrameWriteListHead != NULL)
        {
            while(g_aLLCInstance[index]->pLLCFrameWriteListHead != NULL)
            {
                pWaitingToBeFreed = g_aLLCInstance[index]->pLLCFrameWriteListHead;
                g_aLLCInstance[index]->pLLCFrameWriteListHead = g_aLLCInstance[index]->pLLCFrameWriteListHead->pNext;
                if(pWaitingToBeFreed->pFrameBuffer != NULL)
                {
                    printf("\nCFREE(pWaitingToBeFreed->pFrameBuffer)\n");
                    CFREE(pWaitingToBeFreed->pFrameBuffer);
                    pWaitingToBeFreed->pFrameBuffer = NULL;
                }
                
                if(pWaitingToBeFreed->pCallbackParameter != NULL)
                {
                    printf("\nCFREE(pWaitingToBeFreed->pCallbackParameter)\n");
                    CFREE(pWaitingToBeFreed->pCallbackParameter);
                    pWaitingToBeFreed->pCallbackParameter = NULL;
                }
                
                if(pWaitingToBeFreed->pStreamCallbackParameter != NULL)
                {
                    printf("\nCFREE(pWaitingToBeFreed->pStreamCallbackParameter)\n");
                    CFREE(pWaitingToBeFreed->pStreamCallbackParameter);
                    pWaitingToBeFreed->pStreamCallbackParameter = NULL;
                }
                
                if(pWaitingToBeFreed != NULL)
                {
                    printf("\nCFREE(pWaitingToBeFreed)\n");
                    CFREE(pWaitingToBeFreed);
                    pWaitingToBeFreed = NULL;
                }
            }
        }
        g_aLLCInstance[index]->pLLCFrameWriteListHead = NULL;

        if(g_aLLCInstance[index]->pLLCFrameWriteCompletedListHead != NULL)
        {
            while(g_aLLCInstance[index]->pLLCFrameWriteCompletedListHead != NULL)
            {
                pWaitingToBeFreed = g_aLLCInstance[index]->pLLCFrameWriteCompletedListHead;
                g_aLLCInstance[index]->pLLCFrameWriteCompletedListHead = g_aLLCInstance[index]->pLLCFrameWriteCompletedListHead->pNext;

                if(pWaitingToBeFreed->pFrameBuffer != NULL)
                {
                    printf("\nCFREE(pWaitingToBeFreed->pFrameBuffer)\n");
                    CFREE(pWaitingToBeFreed->pFrameBuffer);
                    pWaitingToBeFreed->pFrameBuffer = NULL;
                }
                
                if(pWaitingToBeFreed->pCallbackParameter != NULL)
                {
                    printf("\nCFREE(pWaitingToBeFreed->pCallbackParameter)\n");
                    CFREE(pWaitingToBeFreed->pCallbackParameter);
                    pWaitingToBeFreed->pCallbackParameter = NULL;
                }
                if(pWaitingToBeFreed->pStreamCallbackParameter != NULL)
                {
                    printf("\nCFREE(pWaitingToBeFreed->pStreamCallbackParameter)\n");
                    CFREE(pWaitingToBeFreed->pStreamCallbackParameter);
                    pWaitingToBeFreed->pStreamCallbackParameter = NULL;
                }
                if(pWaitingToBeFreed != NULL)
                {
                    printf("\nCFREE(pWaitingToBeFreed)\n");
                    CFREE(pWaitingToBeFreed);
                    pWaitingToBeFreed = NULL;
                }
            }
        }
        g_aLLCInstance[index]->pLLCFrameWriteCompletedListHead = NULL;

        g_aLLCInstance[index]->sLLCFrameNextToSend.nLLCFrameLength = 0;

        static_ResetWindowSendRecv(g_aLLCInstance[index],MAX_WINDOW_SIZE);
    }

    return 0;
}

/**
 * @function    把N(S),N(R)这样的三位数据转换成32位的ID数据
 * @parameter1  对应的优先级实体
 * @parameter2  N(S) or N(R) 的值
 * @parameter3  是否是N(S)  
 * @retrun      返回对应的32位ID 
*/
static uint32_t static_ConvertTo32BitIdentifier(tLLCInstance* pLLCInstance,uint8_t n3bitValue,bool bIsNS)
{
    CDebugAssert(pLLCInstance != NULL);
    uint32_t n32bitValue = 0;
    if(bIsNS)
    {
        n32bitValue = pLLCInstance->nReadLastAcknowledgedFrameId;
        CDebugAssert(pLLCInstance->nReadNextToReceivedFrameId > n32bitValue);
    }
    else 
    {
        n32bitValue = pLLCInstance->nWriteLastAckSentFrameId;
        CDebugAssert(pLLCInstance->nWriteNextToSendFrameId >= pLLCInstance->nWriteNextWindowFrameId);
        CDebugAssert(pLLCInstance->nWriteNextWindowFrameId > n32bitValue);
    }
        
   //uint32_t n32bitValue = pLLCInstance->nWriteLastAckSentFrameId;
   if( n3bitValue > (n32bitValue & 0x07))
   {
      n32bitValue = (n32bitValue & 0xFFFFFFF8) | n3bitValue;
   }
   else if(n3bitValue < (n32bitValue & 0x07))
   {
      n32bitValue = ((n32bitValue + 8) & 0xFFFFFFF8) | n3bitValue;
   }

   return n32bitValue;
}

/**
 * @function    模运算，用于定位滑动窗口的元素
*/
static  uint32_t static_Modulo(uint32_t nValue, uint32_t nModulo )
{
   CDebugAssert((nModulo >= 1) && (nModulo <= 4));
   switch(nModulo)
   {
   case 1:
      return 0;
   case 2:
      return nValue & 1;
   case 3:
      return nValue - ((nValue / 3) * 3);
   }
   return nValue & 3;
}

/**
 * @function    检查对应的优先级实体是否读缓冲区满了，这里留了几十个字节的容错，因为是先将读取到的数据放到读缓冲区再判断
*/
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

/**
 * @function    处理接收到数据帧后的 ID 变化
 * @parameter1  对应优先级实体
 * @parameter2  LLC Header
 * @return      true    成功更新了 ID 
 *              false   更新 ID 失败
*/
bool DealIDProblemForIFrame(tLLCInstance* pLLCInstance,uint8_t nLLCHeader)
{
    uint8_t nReceivedFrameId;
    uint8_t nTheOtherSideWishToGetID;

    //nReceivedFrameId = (nLLCHeader >> 3) & 0x07;
    nReceivedFrameId = static_ConvertTo32BitIdentifier(pLLCInstance,(nLLCHeader >> 3) & 0x07,IS_NS_FIELD);
    nTheOtherSideWishToGetID = static_ConvertTo32BitIdentifier(pLLCInstance,(nLLCHeader) & 0x07,IS_NR_FIELD);
    if(!((nTheOtherSideWishToGetID > pLLCInstance->nWriteLastAckSentFrameId) && (nTheOtherSideWishToGetID <= pLLCInstance->nWriteNextToSendFrameId)))
    {
        #ifdef DEBUG_PRINTF
        printf("\npLLCInstance->nWriteLastAckSentFrameId : 0x%08x\n",pLLCInstance->nWriteLastAckSentFrameId);
        printf("\nnTheOtherSideWishToGetID : 0x%08x\n",nTheOtherSideWishToGetID);
        printf("\npLLCInstance->nWriteNextToSendFrameId : 0x%08x\n",pLLCInstance->nWriteNextToSendFrameId);
        #endif

        return false;
    }    
   
    //用信息帧携带的 N(R) 字段来构造一个 RR 帧
    nLLCHeader &= 0x07;
    nLLCHeader |= 0xC0;

    //用这个构造出来的 RR 帧更新 ID
    if(!CtrlFrameAcknowledge(nLLCHeader,pLLCInstance))
    {
        return false;
    }

    if(nReceivedFrameId != pLLCInstance->nReadNextToReceivedFrameId)
    {
        #ifdef DEBUG_PRINTF
        printf("\n^^^^^^^^^^^^^^^^  Reject  ^^^^^^^^^^^^^^^^^^^^^^^\n");
        printf("nReceivedFrameId : \n0x%08x\n",nReceivedFrameId);
        printf("\npLLCInstance[%d]->nReadNextToReceivedFrameId : \n0x%08x",GetPriorityBypLLCInstance(pLLCInstance),pLLCInstance->nReadNextToReceivedFrameId);
        printf("\n^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^\n");
        #endif

        pLLCInstance->nNextCtrlFrameToSend = READ_CTRL_FRAME_REJ;
    }
    else
    {
        pLLCInstance->nReadNextToReceivedFrameId++;
        if(pLLCInstance->nReadNextToReceivedFrameId == 0)
        {
            static_AvoidCounterSpin(pLLCInstance);
        }
    }
    return true;
}

/**
 * @function    MAC 帧读函数。通过 MAC 读状态机读取从硬件得到的字节流中的一帧 MAC 帧，然后解析出 LLC 帧，
 *              如果是控制帧，不论是 LLC 控制帧还是连接控制帧，都直接响应，如果是数据帧，则往上丢给 LLC 层处理
*/
tLLCInstance* MACFrameRead()
{
    tLLCInstance* pLLCInstance = NULL;
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
    uint8_t nLength = 0;
    uint8_t nPackageHeader = 0;
    uint8_t nMessageHeader = 0;
    uint8_t nRecvedWindowSize = 0;
    uint8_t nN_S,nN_R;

    uint8_t nCtrlHeader;

    //初始化 MAC 帧读状态机，g_sMACInstance->nMACReadStatu 的状态值，只能在这里被设置为 WAITING_HEADER
    g_sMACInstance->nMACReadStatu = MAC_FRAME_READ_STATUS_WAITING_HEADER;
    g_sMACInstance->nMACReadCRC = 0;

    CDebugAssert(g_sMACInstance != NULL);
    CDebugAssert(g_sMACInstance->nMACReadStatu != MAC_FRAME_READ_STATUS_IDLE);

    nStatus = g_sMACInstance->nMACReadStatu;
    //MAC 读缓存的读取起始位置，如果一次读取的多个字节中，读到了 EOF 之后还有剩余的字节，下一次需要从这里开始读取 MAC 帧
    nReadPosition = g_sMACInstance->nMACReadPosition;
    nByteCount = 0;
    nReadBufferCount = g_sMACInstance->nMACReadFrameRemaining;
    //g_sMACInstance->aMACReadFrameBuffer 的值不被更新，一直保存的是 MAC 读缓存的起始地址
    pReadBuffer = g_sMACInstance->aMACReadFrameBuffer;
    CDebugAssert(pReadBuffer != NULL);
    //根据读缓存的读取起始位置来重定位缓存区
    pReadBuffer += nReadPosition;

    
    //只要读状态机的状态不是空闲
    while(nStatus != MAC_FRAME_READ_STATUS_IDLE)
    {
        //如果读缓冲区为空，则调用一次读函数 ReadBytes()，并将实际读到的字节数保存在变量pReadBuffer中
        if(nReadBufferCount == 0)
        {
            pReadBuffer = g_sMACInstance->aMACReadFrameBuffer;
            //g_sMACInstance->aMACReadFrameBuffer这个变量保存的始终是mac层缓存的首地址
            CDebugAssert(pReadBuffer != NULL);
            nReadPosition = 0;
            if( (nReadBufferCount = ReadBytes(pReadBuffer, MAC_FRAME_MAX_LENGTH)) == 0)
            {
                //printf("\nReadBytes read nothing!\n");
                return NULL;
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
        
        //MAC 读状态机
        switch(nStatus)
        {
            case MAC_FRAME_READ_STATUS_WAITING_HEADER:
                CDebugAssert( nWritePosition == 0 );
                //如果读到的第一个字节不是 HEADER_SOF 头
                if( nByteValue != HEADER_SOF )
                {
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
                    bError = true;
                }
                else if( nByteValue == TRAILER_EOF ) // 
                {                
                    if(nByteCount < 2)
                    {
                    // 没有有效载荷 
                        bError = true;
                        break;
                    }
                    //如果正确地读到了最后一字节，设置读状态机的状态为空闲
                    nStatus = MAC_FRAME_READ_STATUS_IDLE;                
                }
                else     //有效载荷数据
                {
                    if(nWritePosition >= MAC_FRAME_MAX_LENGTH + 1)
                    {
                        // 超过一个MAC帧的最大字节数，丢掉
                        bError = true;
                    }
                    else
                    {
                        g_sMACInstance->aMACReadBuffer[nWritePosition++] = nByteValue;                   
                    }
                }
                break;
            default:
                return NULL;
        } // switch() 
        
        if(bError)
        {
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
    //更新 MAC 读状态机
    g_sMACInstance->nMACReadFrameRemaining = nReadBufferCount;
    g_sMACInstance->nMACReadStatu = nStatus;
    g_sMACInstance->nMACReadPosition = nReadPosition;
    
    //MAC帧要做去0处理的字节数 = nByteCount - 2;去掉 SOF 和 EOF
    pDataRemovedZero = (uint8_t*)CMALLOC(sizeof(uint8_t)*(nByteCount - 2));
    //做去 0 处理
    if((nLength = static_RemoveInsertedZero(g_sMACInstance->aMACReadBuffer,pDataRemovedZero,(nByteCount - 2))) == 0)
    {
        #ifdef DEBUG_PRINTF
        printf("\nbit error!\n");
        #endif

        return NULL;
    }
    //校验 crc
    g_sMACInstance->nMACReadCRC = 0;
    for(int index = 0; index < *pDataRemovedZero + 1; index++)
    {
        g_sMACInstance->nMACReadCRC ^= *(pDataRemovedZero+index);
    }
    nCRC = *(pDataRemovedZero+*pDataRemovedZero+1);
    if(nCRC != g_sMACInstance->nMACReadCRC)
    {
        #ifdef DEBUG_PRINTF
        printf("\nCRC error!\n");
        #endif

        return NULL;
    }

    #ifdef DEBUG_PRINTF
    printf("\npDataRemovedZero : ");
    for(int index = 0; index < nLength; index++)
        printf("0x%02x ",*(pDataRemovedZero + index));
    printf("\n\n");
    #endif

//=======================对帧进行判断，如果是控制帧，立马处理，如果是信息帧，丢到读缓冲   ====================
    
    nCtrlHeader = *(pDataRemovedZero + 1);

    //控制帧    控制帧带了package头，因为有优先级信息
    if(*pDataRemovedZero == 2)
    {
        //RR RNR REJ RSET
        #ifdef DEBUG_PRINTF
        printf("\nctrl frame! 0x%02x\n",(nCtrlHeader & 0xf8));
        #endif

        if(nCtrlHeader != LLC_FRAME_RST)//不是RSET帧
        {
            //根据优先级字段选择合适的处理实体
            pLLCInstance = GetCorrespondingLLCInstance(pDataRemovedZero);
            if(pLLCInstance == NULL)
                return NULL;
            //g_sMACInstance->pMACReadCompletedCallback = pLLCInstance->pReadHandler;

            if(!CtrlFrameAcknowledge(nCtrlHeader,pLLCInstance))
            {
                #ifdef DEBUG_PRINTF
                printf("\nAcknowledge ctrl frame false!\n");
                #endif

                return NULL;
            }
            return pLLCInstance;
        }//是reset帧
        else
        {
            if(*(pDataRemovedZero + 2) > g_sSPPInstance->nWindowSize)
            {
                g_aLLCInstance[0]->nNextCtrlFrameToSend = LLC_FRAME_RST;
                //设置 RESET 的响应超时，还可用的定时器，timer5
                //SetTimer(timerid,timeroutvalue,is,nextctrlframeisrst,null);
            }
            else
            {
                #ifdef DEBUG_PRINTF
                printf("\nrecv reset and the window size is ok\n");
                #endif

                g_aLLCInstance[0]->nNextCtrlFrameToSend = LLC_FRAME_UA;

                LOCK_WRITE();
                static_ResetLLC();
                UNLOCK_WRITE();
            }
        }        

        return NULL;

    }//UA帧 只有UA帧和RESET帧没有优先级信息，所以只有UA帧可能是一字节长
    else if(*pDataRemovedZero == 1)
    {        
        #ifdef DEBUG_PRINTF
        printf("\nrecv ua\n");
        #endif

        SetTimer(TIMER3_ACK_TIMEOUT,SEND_ACK_TIMEOUT,false,Timer3_ACKTimeout,NULL);
        LOCK_WRITE();
        static_ResetLLC();
        UNLOCK_WRITE();
    }    
    else
    {
        //信息帧
        if((nCtrlHeader & 0xc0) == 0x80)
        {
            pLLCInstance = GetCorrespondingLLCInstance(pDataRemovedZero);
            if(pLLCInstance == NULL)
                return NULL;
            
            //===========================================
            //      更新相关的 ID 数据，及时响应发送方
            //===========================================
            #ifdef DEBUG_PRINTF
            printf("\nBefor deal ID , g_aLLCInstance[%d]->nReadNextToReceivedFrameId : 0x%08x\n",GetPriorityBypLLCInstance(pLLCInstance),pLLCInstance->nReadNextToReceivedFrameId);
            #endif

            if(!DealIDProblemForIFrame(pLLCInstance,nCtrlHeader))
            {
                #ifdef DEBUG_PRINTF
                printf("\nDealIDProblemForIFrame() error !\n");
                #endif

                return NULL;
            }

            #ifdef DEBUG_PRINTF
            printf("\nAfter deal ID , g_aLLCInstance[%d]->nReadNextToReceivedFrameId : 0x%08x\n",GetPriorityBypLLCInstance(pLLCInstance),pLLCInstance->nReadNextToReceivedFrameId);
            #endif
            //===========================================   
                
            //g_sMACInstance->pMACReadCompletedCallback = pLLCInstance->pReadHandler;
            
            nMessageHeader = *(pDataRemovedZero + 3);
            nPackageHeader = *(pDataRemovedZero + 2);

            if(((nPackageHeader & 0x80) == 0x80) && ((nPackageHeader & 0x40) == 0x00))
            {
                //只有一个分片，且不是数据帧
                if((nMessageHeader & CONNECT_VALUE_MASK) == CONNECT_ERROR_VALUE)   //连接错误帧
                {
                    ConnectErrorFrameHandle(nMessageHeader);
                    return NULL;
                }
                else    //正常的连接控制帧
                {
                    if(!ConnectCtrlFrameACK(nMessageHeader))
                        return NULL;
                }
            }
            else if((nPackageHeader & 0x40) == 0x40)
            {
                //是应用数据帧,往上递交，放到读缓冲区
                if(pLLCInstance->bIsFirstFregment)
                {
                    //第一个分片，有 Message Header 字段
                    pLLCInstance->bIsFirstFregment = false;
                    nMessageHeader = *(pDataRemovedZero + 3);

                    if(nMessageHeader == CONNECT_SEND_RECV_MESSAGE)     //正常的应用数据帧
                    {  
                        //nLength 是数据帧的长度，从 LLC Header 到 有效载荷最后一个字节，这个长度比应用数据有效载荷多了两个字节， LLC Header 和 Package Header
                        nLength = *pDataRemovedZero;
                        //去掉一个字节的 message header 长度
                        *pDataRemovedZero = *pDataRemovedZero - 1;
                        for(int index = 0; index <= nLength; index++)
                        {
                            //在重组应用数据的时候，忽略掉 message header
                            if(index == 3)
                                continue;
                            if(pLLCInstance->nLLCReadWritePosition >= READ_BUFFER_SIZE)
                                pLLCInstance->nLLCReadWritePosition = 0;

                            pLLCInstance->aLLCReadBuffer[(pLLCInstance->nLLCReadWritePosition)++] = *(pDataRemovedZero + index);
                        }

                        if(!CheckReadBufferFree(pLLCInstance))//读缓冲区满了
                        {
                            pLLCInstance->nNextCtrlFrameToSend = READ_CTRL_FRAME_RNR;
                            pLLCInstance->bIsReadBufferFull = true;
                        }
                    }
                    else
                    {
                        printf("\nmessage header is error!\n");
                    }
                }
                else
                {
                    //不是第一个分片，没有 message header ，直接把 LLC 帧向上递交
                    for(int index = 0; index <= *pDataRemovedZero; index++)
                    {
                        if(pLLCInstance->nLLCReadWritePosition >= READ_BUFFER_SIZE)
                            pLLCInstance->nLLCReadWritePosition = 0;

                        pLLCInstance->aLLCReadBuffer[(pLLCInstance->nLLCReadWritePosition)++] = *(pDataRemovedZero + index);
                    }

                    if(!CheckReadBufferFree(pLLCInstance))//读缓冲区满了
                    {
                        pLLCInstance->nNextCtrlFrameToSend = READ_CTRL_FRAME_RNR;
                        pLLCInstance->bIsReadBufferFull = true;
                    }
                }
                if((nPackageHeader & 0x80) == 0x80)
                {
                    pLLCInstance->bIsFirstFregment = true;
                }
            }
            
        }//未知帧类型
        else 
        {
            #ifdef DEBUG_PRINTF
            printf("\nUnknown frame type!\n");
            #endif
        }
    }
    //产生了LLC ctrl 帧，所以调用一次写函数，把命令（响应）发送出去
    #ifdef DEBUG_PRINTF
    printf("\nCFREE(pDataRemovedZero)\n");
    #endif

    CFREE(pDataRemovedZero);
    MACFrameWrite();
    return pLLCInstance;
}

/**
 * @function    MAC 帧写函数。把 LLC 帧链表中的数据，即一帧 LLC 帧，写到滑动窗口中
*/
uint8_t MACFrameWrite()
{
    //如果此时是读挂起，则返回，等待上一帧写结束
    if(g_sMACInstance->bIsWriteFramePending)
    {
        #ifdef DEBUG_PRINTF
        printf("\nThe wirte mechine is pending!\n");
        #endif

        return 0;
    }

    uint8_t nInsertedZeroFrameLength = 0;
    uint8_t* pCtrlLLCHeader = NULL;
    uint8_t* pCtrlFrameData = NULL;
    uint8_t nCtrlFrameLength = 0;
    tLLCInstance* pLLCInstance = NULL;
    tMACWriteContext* pSingleMACFrame = NULL;
    tLLCWriteContext* pSendLLCFrame = NULL;
    uint8_t nCRC = 0;
    int nPriority;
    //自定义了连接控制帧的优先级是 0 ，所以如果有连接控制帧，一定是最高优先级，最早被处理

    g_sMACInstance->bIsWriteFramePending = true;

    //取得优先级最高的LLC实体，把它的写链表中最早的数据写到滑动窗口上
    for(nPriority = 0; nPriority < PRIORITY; nPriority++)
    {
        if((g_aLLCInstance[nPriority]->pLLCFrameWriteListHead != NULL) || (g_aLLCInstance[nPriority]->nNextCtrlFrameToSend != READ_CTRL_FRAME_NONE))
        {
            pLLCInstance = g_aLLCInstance[nPriority];
            break;
        }
    }
    if(pLLCInstance == NULL)
    {
        //printf("\nthere is no frame to send!\n");
        g_sMACInstance->bIsWriteFramePending = false;
        return 0;
    }
    //滑动窗口是否满了
    int npLLCnum = 0;
    if(pLLCInstance->bIsWriteWindowsFull)
    {
        if(pLLCInstance->nWriteNextToSendFrameId - pLLCInstance->nWriteLastAckSentFrameId - 1 >= pLLCInstance->nWindowSize)
        {
            pLLCInstance->bIsWriteWindowsFull = true;
            //printf("\ng_sSPPInstance[%d] : The slide window is full!\n",GetPriorityBypLLCInstance(pLLCInstance));
            SetTimer(TIMER4_WINDOW_FULL_RESENT,WINDOW_FULL_RESENT_TIMEOUT,true,Timer4_WindowFullResent,pLLCInstance);
            g_sMACInstance->bIsWriteFramePending = false;
            return 0;
        }    
        else
        {
            pLLCInstance->bIsWriteWindowsFull = false;
            CancelTimerTask(TIMER4_WINDOW_FULL_RESENT,CANCEL_MODE_IMMEDIATELY);
        }
            
    }

    if(pLLCInstance->nNextCtrlFrameToSend == READ_CTRL_FRAME_NONE)
    {
        //获取一个滑动窗口实体，往里面写要发送的数据
        pSingleMACFrame = pLLCInstance->aSlideWindow[static_Modulo(pLLCInstance->nWriteNextToSendFrameId,pLLCInstance->nWindowSize)];
        pSendLLCFrame = pLLCInstance->pLLCFrameWriteListHead;
        //暂时不更新 pLLCInstance->pLLCFrameWriteListHead ，如果发送失败，下一次还需要重发这一帧

        //MAC 帧头
        *(pSingleMACFrame->pFrameBuffer) = HEADER_SOF;
        //填充 N(S) 和 N(R) 字段 
        *(pSendLLCFrame->pFrameBuffer + 1) |= ((pLLCInstance->nWriteNextToSendFrameId & 0x07) << 3);
        *(pSendLLCFrame->pFrameBuffer + 1) |= (pLLCInstance->nReadNextToReceivedFrameId & 0x07);

        //计算 crc，由于是demo，就没有采用 crc16
        nCRC = 0;
        for(int index = 0; index < pSendLLCFrame->nFrameLength-1; index++)
        {
            nCRC ^= *(pSendLLCFrame->pFrameBuffer + index);
        }
        *(pSendLLCFrame->pFrameBuffer + pSendLLCFrame->nFrameLength - 1) = nCRC;

        #ifdef DEBUG_PRINTF
        printf("\npDataHaventInsertZero : ");
        for(int index = 0; index < pSendLLCFrame->nFrameLength; index++)
            printf("0x%02x ",*(pSendLLCFrame->pFrameBuffer + index));
        printf("\n");
        #endif

        //把sof 和 eof 之间的字节进行插 0 处理
        nInsertedZeroFrameLength = static_InsertZero(pSendLLCFrame->pFrameBuffer,pSingleMACFrame->pFrameBuffer+1,pSendLLCFrame->nFrameLength);
        *(pSingleMACFrame->pFrameBuffer + nInsertedZeroFrameLength + 1) = TRAILER_EOF;
        //len + 2 : SOF EOF
        pSingleMACFrame->nFrameLength = nInsertedZeroFrameLength + 2;

        //更新 pLLCInstance->pLLCFrameWriteListHead，摘掉第一帧
        pLLCInstance->pLLCFrameWriteListHead = pSendLLCFrame->pNext;
        pSendLLCFrame->pNext = NULL;

        //这里没有写到已发送链表，是因为滑动窗口已经被设计为全局的 WriteContext 实体，他来充当 WriteCompletedContextList 的角色
        //##加锁
        //static_AddToWriteCompletedContextList(pLLCInstance,pSingleMACFrame,ADD_TO_LIST_HEAD);
        //##解锁

        //更新ID
        pLLCInstance->nWriteNextToSendFrameId += 1;

        if(pLLCInstance->nWriteNextToSendFrameId == 0)
            static_AvoidCounterSpin(pLLCInstance);

        if(pLLCInstance->nWriteNextToSendFrameId - pLLCInstance->nWriteLastAckSentFrameId - 1 >= pLLCInstance->nWindowSize)
            pLLCInstance->bIsWriteWindowsFull = true;
        else
            pLLCInstance->bIsWriteWindowsFull = false;

        SPIWriteBytes(pLLCInstance,pSingleMACFrame->pFrameBuffer,pSingleMACFrame->nFrameLength,0);
        if(pSendLLCFrame->bIsLastFragment)//最后一片，设置重发超时
        {
            SetTimer(TIMER2_SENDTIMEOUT,RESEND_FINIAL_FRAME_TIMEOUT,true,Timer2_FinialResendTimeout,(void*)pLLCInstance);//RESEND_TIMEOUT
        }
        g_sMACInstance->bIsWriteFramePending = false;
        return 0;
    }//控制帧优先级最高，直接发送
    else
    {
        //控制帧没有 ID 信息，因为控制帧的发送是在满足条件之后发送的，如果控制帧丢失，则没有响应，那么发送方的控制帧发送条件一直满足，会自动重发的
        if(pLLCInstance->nNextCtrlFrameToSend != LLC_FRAME_UA)
        {
            //RR RNR REJ RST 当前版本，暂不支持 SREJ
            //预留一个 CRC 字节
            pCtrlLLCHeader = (uint8_t*)CMALLOC(sizeof(uint8_t)*4);
            pCtrlFrameData = (uint8_t*)CMALLOC(sizeof(uint8_t)*7);   //SOF LEN + LLCCRTL WINDOWSIZE [INSERTED ZERO] + CRC EOF
            
            // LLC Header 和 Package Header
            *pCtrlLLCHeader = 0x02;
            
            if(pLLCInstance->nNextCtrlFrameToSend == LLC_FRAME_RST)
            {
                *(pCtrlLLCHeader + 1) = LLC_FRAME_RST;
                *(pCtrlLLCHeader + 2) = g_sSPPInstance->nWindowSize;
            }
            else
            {
                switch(pLLCInstance->nNextCtrlFrameToSend)
                {
                    case READ_CTRL_FRAME_RNR:
                        *(pCtrlLLCHeader + 1) = READ_CTRL_FRAME_RNR;
                        *(pCtrlLLCHeader + 1) |= (pLLCInstance->nReadNextToReceivedFrameId & 0x07);
                        pLLCInstance->nReadLastAcknowledgedFrameId = pLLCInstance->nReadNextToReceivedFrameId - 1;
                        break;
                    case READ_CTRL_FRAME_ACK:
                        *(pCtrlLLCHeader + 1) = READ_CTRL_FRAME_ACK;
                        *(pCtrlLLCHeader + 1) |= (pLLCInstance->nReadNextToReceivedFrameId & 0x07);
                        pLLCInstance->nReadLastAcknowledgedFrameId = pLLCInstance->nReadNextToReceivedFrameId - 1;
                        break;
                    case READ_CTRL_FRAME_REJ:
                        *(pCtrlLLCHeader + 1) = READ_CTRL_FRAME_REJ;
                        *(pCtrlLLCHeader + 1) |= (pLLCInstance->nReadNextToReceivedFrameId & 0x07);
                        pLLCInstance->nReadLastAcknowledgedFrameId = pLLCInstance->nReadNextToReceivedFrameId - 1;
                        break;
                }
                *(pCtrlLLCHeader + 2) = 0;
                *(pCtrlLLCHeader + 2) |= 0x80;  //just one fragment
                *(pCtrlLLCHeader + 2) |= (g_nVersion & VERSION_FIELD_MASK);   //版本信息
                *(pCtrlLLCHeader + 2) |= (nPriority & PRIORITY_MASK);  //优先级信息  
            }
            nCRC = 0;
            for(uint8_t index = 0; index < 3; index++)
                nCRC ^= *(pCtrlLLCHeader + index);
            *(pCtrlLLCHeader + 3) = nCRC;

            #ifdef DEBUG_PRINTF
            printf("\npDataHaventInsertZero : ");
            for(int index = 0; index < 4; index++)
                printf("0x%02x ",*(pCtrlLLCHeader + index));
            printf("\n");
            #endif

            nInsertedZeroFrameLength = static_InsertZero(pCtrlLLCHeader,pCtrlFrameData+1,4);
            printf("\nCFREE(pCtrlLLCHeader)\n");
            CFREE(pCtrlLLCHeader);
        }
        else
        {
            //预留一个 CRC 字节
            pCtrlLLCHeader = (uint8_t*)CMALLOC(sizeof(uint8_t)*3);
            pCtrlFrameData = (uint8_t*)CMALLOC(sizeof(uint8_t)*6);   //SOF LEN + LLCCRTL [INSERTED ZERO] + CRC EOF
            
            *pCtrlLLCHeader = 0x01;
            *(pCtrlLLCHeader + 1) = 0x00;
            *(pCtrlLLCHeader + 1) = READ_CTRL_FRAME_UA;
                   
            nCRC ^= *(pCtrlLLCHeader);
            nCRC ^= *(pCtrlLLCHeader + 1);
            *(pCtrlLLCHeader + 2) = nCRC;

            #ifdef DEBUG_PRINTF
            printf("\npDataHaventInsertZero : ");
            for(int index = 0; index < 3; index++)
                printf("0x%02x ",*(pCtrlLLCHeader + index));
            printf("\n");
            #endif

            nInsertedZeroFrameLength = static_InsertZero(pCtrlLLCHeader,pCtrlFrameData+1,3);
            printf("\nCFREE(pCtrlLLCHeader)\n");
            CFREE(pCtrlLLCHeader);
        }

        //添加 MAC 帧的头和尾
        *(pCtrlFrameData) = HEADER_SOF;
        *(pCtrlFrameData + nInsertedZeroFrameLength + 1) = TRAILER_EOF;
        nCtrlFrameLength = nInsertedZeroFrameLength + 2;

        SPIWriteBytes(pLLCInstance,pCtrlFrameData,nCtrlFrameLength,1);
        printf("\nCFREE(pCtrlFrameData)\n");
        CFREE(pCtrlFrameData);
        
        pLLCInstance->nNextCtrlFrameToSend = READ_CTRL_FRAME_NONE;
        g_sMACInstance->bIsWriteFramePending = false;
        return 0;
    }
}

/**
 * @function    处理 LLC 控制帧
 * @parameter1  LLC Header
 * @parameter2  优先级实体
 * @return      true    正确处理了 LLC 控制帧
 *              false   处理失败
*/
bool CtrlFrameAcknowledge(uint8_t nCtrlFrame, tLLCInstance *pLLCInstance)
{
    uint8_t nN_R_Value;
    uint32_t nWriteLastACKId = pLLCInstance->nWriteLastAckSentFrameId;
    uint32_t nWriteNextWindowId = pLLCInstance->nWriteNextWindowFrameId;
    uint32_t nWriteNextToSendId = pLLCInstance->nWriteNextToSendFrameId;
    uint32_t nOtherWantToRecvNextId = 0;
    uint32_t nTheIdOtherSend = 0;
    uint32_t nAckedFrameNum = 0;
    nN_R_Value = (nCtrlFrame & 0x07);
    
    nOtherWantToRecvNextId = static_ConvertTo32BitIdentifier(pLLCInstance,nN_R_Value,IS_NR_FIELD);
        
    if(!((nOtherWantToRecvNextId > nWriteLastACKId) && (nOtherWantToRecvNextId <= nWriteNextToSendId)))
    {
        //如果收到对方发来的期望接收的下一帧ID不在对方已经确认收到的最大ID和我即将发送的ID之间，
        //则表示这个ID越界了，不对这个控制帧做响应，并且记录这种情况连续出现的次数，如果连续出现多次，
        //则认为协议栈的ID管理已经崩溃，无法再保证消息的可靠传输，保持各个缓冲区的数据不变，重置序号，
        //发送端把滑动窗口中的帧全重发。（接收端做重复对比？）
        #ifdef DEBUG_PRINTF
        printf("\nID error!\n");
        printf("\nnWriteLastACKId : 0x%08x\n",nWriteLastACKId);
        printf("\nnWriteNextWindowId : 0x%08x\n",nWriteNextWindowId);
        printf("\nnReceivedId : 0x%08x\n",nOtherWantToRecvNextId);
        #endif

        return false;
    }
    
    //RR帧
    if((nCtrlFrame & LLC_S_FRAME_MASK) == READ_CTRL_FRAME_ACK)
    {
        nAckedFrameNum = nOtherWantToRecvNextId - 1 - pLLCInstance->nWriteLastAckSentFrameId;        
        pLLCInstance->nWriteLastAckSentFrameId = nOtherWantToRecvNextId - 1;
        
        #ifdef DEBUG_PRINTF
        printf("\nRR : g_aLLCInstance[%d]->nWriteLastAckSentFrameId : 0x%08x\n",GetPriorityBypLLCInstance(pLLCInstance),pLLCInstance->nWriteLastAckSentFrameId);

        //##加锁
        // for(int times = 0; times < nAckedFrameNum; times++)
        // {
        //     if(!RemoveACompleteSentFrame(pLLCInstance))
        //     {
        //         printf("\nThe count of completed write context is less than recved N(R) - lastAck.\n");
        //         return false;
        //     }
        // }
        //##解锁
        printf("\n-------------->RR : update LastACK\t\tN(R) : 0x%08x\n",nOtherWantToRecvNextId);
        #endif
        
    }//REJ帧
    else if((nCtrlFrame & LLC_S_FRAME_MASK) == READ_CTRL_FRAME_REJ)
    {
        //更新LastACK
        nAckedFrameNum = nOtherWantToRecvNextId - 1 - pLLCInstance->nWriteLastAckSentFrameId;
        pLLCInstance->nWriteLastAckSentFrameId = nOtherWantToRecvNextId - 1;

        //##加锁
        // for(int times = 0; times < nAckedFrameNum; times++)
        // {
        //     if(!RemoveACompleteSentFrame(pLLCInstance))
        //     {
        //         printf("\nThe count of completed write context is less than recved N(R) - lastAck.\n");
        //         return false;
        //     }
        // }
        //##解锁
        pLLCInstance->nWriteNextWindowFrameId = nOtherWantToRecvNextId;

        #ifdef DEBUG_PRINTF
        printf("\nREJ : g_aLLCInstance[%d]->nWriteNextToSendFrameId : 0x%08x\n",GetPriorityBypLLCInstance(pLLCInstance),pLLCInstance->nWriteNextToSendFrameId);
        printf("\nREJ : g_aLLCInstance[%d]->nWriteNextWindowFrameId : 0x%08x\n",GetPriorityBypLLCInstance(pLLCInstance),pLLCInstance->nWriteNextWindowFrameId);
        printf("\nREJ : g_aLLCInstance[%d]->nWriteLastAckSentFrameId : 0x%08x\n",GetPriorityBypLLCInstance(pLLCInstance),pLLCInstance->nWriteLastAckSentFrameId);
        printf("\n-------------->REJ : update NextWindowToSend\t\tN(R) : 0x%02x\n",nOtherWantToRecvNextId);
        #endif
    }//RNR帧
    else if((nCtrlFrame & LLC_S_FRAME_MASK) == READ_CTRL_FRAME_RNR)
    {
        //更新NextWindowToSend
        pLLCInstance->bIsWriteOtherSideReady = false;
        nAckedFrameNum = nOtherWantToRecvNextId - 1 - pLLCInstance->nWriteLastAckSentFrameId;
        
        pLLCInstance->nWriteLastAckSentFrameId = nOtherWantToRecvNextId - 1;

        #ifdef DEBUG_PRINTF
        printf("\nRNR : g_aLLCInstance[%d]->nWriteLastAckSentFrameId : 0x%08x\n",GetPriorityBypLLCInstance(pLLCInstance),pLLCInstance->nWriteLastAckSentFrameId);

        //##加锁
        // for(int times = 0; times < nAckedFrameNum; times++)
        //  {
        //     if(!RemoveACompleteSentFrame(pLLCInstance))
        //     {
        //         printf("\nThe count of completed write context is less than recved N(R) - lastAck.\n");
        //         return false;
        //     }
        // }
        //##解锁
        printf("\n-------------->RNR : pLLCInstance->bIsWriteOtherSideReady = %d\t\tN(R) : 0x%02x\n",pLLCInstance->bIsWriteOtherSideReady,nOtherWantToRecvNextId);
        #endif
    }
    if(pLLCInstance->nWriteNextToSendFrameId - pLLCInstance->nWriteLastAckSentFrameId - 1 >= pLLCInstance->nWindowSize)
        pLLCInstance->bIsWriteWindowsFull = true;
    else
        pLLCInstance->bIsWriteWindowsFull = false;
    return true;
}

/**
 * @function    发送一帧 MAC 帧，如果是 LLC 层的控制帧（RR，RNR，REJ，RST，UA）,则直接发送，
 *              否则发送下一帧需要被发送的 MAC 帧，nWriteNextWindowFrameId，这个值可能被 REJ 帧改变
 * @parameter1  发送的数据帧对应的优先级实体（一个优先级一个 tLLCInstance 指针对象）
 * @parameter2  要发送的数据
 * @parameter3  要发送数据的长度，字节数
 * @parameter4  是否是 LLC 控制帧
*/
uint8_t SPIWriteBytes(tLLCInstance* pLLCInstance,uint8_t* pData,uint8_t nLength,bool bIsCtrlFrame)
{
    tMACWriteContext* pSendMACFrame = NULL;
    

    printf("Befor SPIWriteBytes(),g_aLLCInstance[%d]->nWriteNextWindowFrameId : 0x%08x",GetPriorityBypLLCInstance(pLLCInstance),pLLCInstance->nWriteNextWindowFrameId);

    if(bIsCtrlFrame)
    {
        //直接发送数据;
        SPI_SEND_BYTES(pData,nLength);
    }
    else
    {
        pSendMACFrame = pLLCInstance->aSlideWindow[static_Modulo(pLLCInstance->nWriteNextWindowFrameId,pLLCInstance->nWindowSize)];
        SPI_SEND_BYTES(pSendMACFrame->pFrameBuffer,pSendMACFrame->nFrameLength);
        pLLCInstance->nWriteNextWindowFrameId += 1;
        if(pLLCInstance->nWriteNextWindowFrameId == 0)
            static_AvoidCounterSpin(pLLCInstance);
    }

    printf("After SPIWriteBytes(),g_aLLCInstance[%d]->nWriteNextWindowFrameId : 0x%08x",GetPriorityBypLLCInstance(pLLCInstance),pLLCInstance->nWriteNextWindowFrameId);

    return 0;
}

// uint8_t RemoveACompleteSentFrame(tLLCInstance* pLLCInstance)
// {
//     tMACWriteContext* pWriteContext;
//     pWriteContext = pLLCInstance->pLLCFrameWriteCompletedListHead;

//     tMACWriteContext* pPreWriteContext;

//     LOCK_WRITE();

//     if(pWriteContext == NULL)
//     {
//         UNLOCK_WRITE();
//         return 0;
//     }
//     else
//     {
//         pPreWriteContext = pWriteContext;
//         pWriteContext = pPreWriteContext->pNext;
//         if(pWriteContext == NULL)
//         {
//             printf("\nCFREE((void*)(pPreWriteContext))\n");
//             CFREE((void*)(pPreWriteContext->pFrameBuffer));
//             CFREE((void*)(pPreWriteContext));
//             pPreWriteContext = NULL;
//             pLLCInstance->pLLCFrameWriteCompletedListHead = NULL;
//         }
//         else
//         {
//             while(pWriteContext->pNext != NULL)
//             {
//                 pPreWriteContext = pWriteContext;
//                 pWriteContext = pPreWriteContext->pNext;
//             }
//             printf("\nCFREE((void*)(pWriteContext))\n");
//             CFREE((void*)(pWriteContext->pFrameBuffer));
//             CFREE((void*)(pWriteContext));
//             pWriteContext = NULL;
//             pPreWriteContext->pNext = NULL;
//         }
//         UNLOCK_WRITE();

//         return 1;
//     }
// }

/**
 * @function    把做了插 0 处理的数据复原
 * @parameter1  做了插 0 处理的数据
 * @parameter2  保存去 0 结果的地址
 * @parameter3  做了插 0 处理的数据的长度
 * @return      去 0 之后的数据长度
*/
uint8_t static_RemoveInsertedZero(uint8_t* aBufferInsertedZero,uint8_t* pBufferRemovedZero,uint8_t nLengthInsteredZero)
{
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
                    return 0;
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

/**
 * @function    插 0 处理
 * @parameter1  未插 0 的数据
 * @parameter2  完成插 0 操作后的数据的首地址
 * @parameter3  未插 0 前的字节数
 * @return      完成插 0 后的字节数 
*/
uint8_t static_InsertZero(uint8_t* aMACReadBuffer,uint8_t* pNewBuffer,uint8_t nOldLength)
{
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