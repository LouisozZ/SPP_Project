#include "spp_global.h"

static int GetPriorityBypLLCInstance(tLLCInstance* pLLCInstance)
{
    for(int index = 0; index < PRIORITY; index++)
    {
        if(g_aLLCInstance[index] == pLLCInstance)
            return index;
    }
    return -1;
}

static void Timer2_FinialResendTimeout(void *pParameter)
{
    printf("\ntimer2 timeout\n");
    for(int index = 0; index < PRIORITY; index++)
    {
        if(g_aLLCInstance[index]->nWriteNextToSendFrameId == g_aLLCInstance[index]->nWriteLastAckSentFrameId + 2)
        {
            if(g_aLLCInstance[index]->pLLCFrameWriteCompletedListHead == NULL)
            {
                printf("\nThere is something wrong with protocol stack!\n");
                //杀死进程！
                return ;
            }

            if(g_aLLCInstance[index]->pLLCFrameWriteCompletedListHead->bIsLastFragment)
            {
                //最后一片的发送超时
                g_aLLCInstance[index]->nWriteNextWindowFrameId = g_aLLCInstance[index]->nWriteLastAckSentFrameId + 1;
                return ;
            }
        }
    }
    return;
}

void Timer3_ACKTimeout(void* pParameter)
{
    for(int index = 0; index < PRIORITY; index++)
    {
        if(g_aLLCInstance[index]->nReadNextToReceivedFrameId > g_aLLCInstance[index]->nReadLastAcknowledgedFrameId + 1)
            g_aLLCInstance[index]->nNextCtrlFrameToSend = READ_CTRL_FRAME_ACK;
        // if(g_aLLCInstance[index]->bIsWaitingLastFragment)
        //     g_aLLCInstance[index]->nNextCtrlFrameToSend = READ_CTRL_FRAME_ACK;
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
static uint32_t static_ConvertTo32BitIdentifier(tLLCInstance* pLLCInstance,uint8_t n3bitValue )
{
   uint32_t n32bitValue = pLLCInstance->nWriteLastAckSentFrameId;

   CDebugAssert(pLLCInstance != NULL);
   CDebugAssert(pLLCInstance->nWriteNextToSendFrameId >= pLLCInstance->nWriteNextWindowFrameId);
   CDebugAssert(pLLCInstance->nWriteNextWindowFrameId > n32bitValue);
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

bool DealIDProblemForIFrame(tLLCInstance* pLLCInstance,uint8_t nLLCHeader)
{
    uint8_t nReceivedFrameId;

    //nReceivedFrameId = (nLLCHeader >> 3) & 0x07;
    nReceivedFrameId = static_ConvertTo32BitIdentifier(pLLCInstance,(nLLCHeader >> 3) & 0x07);
    if(!((nReceivedFrameId > pLLCInstance->nWriteLastAckSentFrameId) && (nReceivedFrameId <= pLLCInstance->nWriteNextToSendFrameId)))
        return false;
    /* build a RR frame with the same "next to receive identifier" */
    nLLCHeader &= 0x07;
    nLLCHeader |= 0xC0;

    //根据帧头 nCtrlHeader 来写响应
    if(!CtrlFrameAcknowledge(nLLCHeader,pLLCInstance))
    {
        return false;
    }

    if(nReceivedFrameId != pLLCInstance->nReadNextToReceivedFrameId)
    {
        printf("\n^^^^^^^^^^^^^^^^  Reject  ^^^^^^^^^^^^^^^^^^^^^^^\n");
        printf("nReceivedFrameId : \n0x%08x\n",nReceivedFrameId);
        printf("\npLLCInstance->nReadNextToReceivedFrameId : \n0x%08x",pLLCInstance->nReadNextToReceivedFrameId);
        printf("\n^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^\n");

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
    return;
}

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
                    //如果正确处理了最后一帧，设置读状态机的状态为空闲
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
    g_sMACInstance->nMACReadFrameRemaining = nReadBufferCount;
    g_sMACInstance->nMACReadStatu = nStatus;
    g_sMACInstance->nMACReadPosition = nReadPosition;
    //MAC帧要做去0处理的字节数 = nByteCount - 2;去掉 SOF 和 EOF
    //把执行了插 0 操作的 LLC 帧复原，然后根据优先级字段选择合适的处理实体。
    pDataRemovedZero = (uint8_t*)CMALLOC(sizeof(uint8_t)*(nByteCount - 2));

    if((nLength = static_RemoveInsertedZero(g_sMACInstance->aMACReadBuffer,pDataRemovedZero,(nByteCount - 2))) == 0)
    {
        printf("\nbit error!\n");
        return NULL;
    }
    g_sMACInstance->nMACReadCRC = 0;
    for(int index = 0; index < *pDataRemovedZero + 1; index++)
    {
        g_sMACInstance->nMACReadCRC ^= *(pDataRemovedZero+index);
    }
    nCRC = *(pDataRemovedZero+*pDataRemovedZero+1);
    if(nCRC != g_sMACInstance->nMACReadCRC)
    {
        printf("\nCRC error!\n");
        return NULL;
    }
#ifndef DEBUG_PRINTF
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
#ifndef DEBUG_PRINTF
        printf("\nctrl frame! 0x%02x\n",(nCtrlHeader & 0xf8));
#endif
        if(nCtrlHeader != LLC_FRAME_RST)//不是RSET帧
        {
            pLLCInstance = GetCorrespondingLLCInstance(pDataRemovedZero);
            if(pLLCInstance == NULL)
                return NULL;
            //g_sMACInstance->pMACReadCompletedCallback = pLLCInstance->pReadHandler;

            if(!CtrlFrameAcknowledge(nCtrlHeader,pLLCInstance))
            {
                printf("\nAcknowledge ctrl frame false!\n");
                return NULL;
            }
            return pLLCInstance;
        }//是reset帧
        else
        {
            if(*(pDataRemovedZero + 2) > g_sSPPInstance->nWindowSize)
            {
                g_aLLCInstance[0]->nNextCtrlFrameToSend = LLC_FRAME_RST;
                //设置 RESET 的响应超时，还可用的定时器，timer4 和 timer5
                //SetTimer(timerid,timeroutvalue,is,nextctrlframeisrst,null);
            }
            else
            {
                g_aLLCInstance[0]->nNextCtrlFrameToSend = LLC_FRAME_UA;
                LOCK_WRITE();
                printf("\nrecv reset and the window size is ok\n");
                static_ResetLLC();
                UNLOCK_WRITE();
            }
        }        
        //响应
        return NULL;
    }//UA帧 只有UA帧和RESET帧没有优先级信息，所以只有UA帧可能是一字节长
    else if(*pDataRemovedZero == 1)
    {        
        SetTimer(TIMER3_ACK_TIMEOUT,SEND_ACK_TIMEOUT,false,Timer3_ACKTimeout,NULL);
        LOCK_WRITE();
        printf("\nrecv ua\n");
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
            printf("Befor deal ID , the 0x%08x->nReadNextToReceivedFrameId : 0x%08x",pLLCInstance,pLLCInstance->nReadNextToReceivedFrameId);
            if(!DealIDProblemForIFrame(pLLCInstance,nCtrlHeader))
            {
                printf("\nDealIDProblemForIFrame() error !\n");
                return NULL;
            }
            printf("After deal ID , the 0x%08x->nReadNextToReceivedFrameId : 0x%08x",pLLCInstance,pLLCInstance->nReadNextToReceivedFrameId);
            //===========================================   
                
            //g_sMACInstance->pMACReadCompletedCallback = pLLCInstance->pReadHandler;
            
            nMessageHeader = *(pDataRemovedZero + 3);
            nPackageHeader = *(pDataRemovedZero + 2);

            if(((nPackageHeader & 0x80) == 0x80) && ((nPackageHeader & 0x40) == 0x00))
            {
                //只有一个分片，且不是数据帧
                if((nMessageHeader & CONNECT_VALUE_MASK) == CONNECT_ERROR_VALUE)   //错误帧
                {
                    ConnectErrorFrameHandle(nMessageHeader);
                    return NULL;
                }
                else    //正常的控制帧
                {
                    if(!ConnectCtrlFrameACK(nMessageHeader))
                        return NULL;
                }
            }
            else if((nPackageHeader & 0x40) == 0x40)
            {
                if(pLLCInstance->bIsFirstFregment)
                {
                    pLLCInstance->bIsFirstFregment = false;
                    nMessageHeader = *(pDataRemovedZero + 3);
                    if(nMessageHeader == CONNECT_SEND_RECV_MESSAGE)     //正常的数据帧
                    {  
                        nLength = *pDataRemovedZero;
                        *pDataRemovedZero = *pDataRemovedZero - 1;
                        for(int index = 0; index <= nLength; index++)
                        {
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
            printf("\nUnknown frame type!\n");
        }
    }
    //产生了LLC ctrl 帧，所以调用一次写函数，把命令（响应）发送出去
    printf("\nCFREE(pDataRemovedZero)\n");
    CFREE(pDataRemovedZero);
    MACFrameWrite();
    return pLLCInstance;
}

uint8_t MACFrameWrite()
{
    //取得优先级最高的LLC实体，把它的写链表中最早的数据写到滑动窗口上
    //printf("\nMACFrameWrite\n");
    uint8_t nInsertedZeroFrameLength = 0;
    uint8_t* pCtrlLLCHeader = NULL;
    uint8_t* pCtrlFrameData = NULL;
    uint8_t nCtrlFrameLength = 0;
    tLLCInstance* pLLCInstance = NULL;
    tMACWriteContext* pSingleMACFrame = NULL;
    tLLCWriteContext* pSendLLCFrame = NULL;
    uint8_t nCRC = 0;
    int nPriority;
    //the connect ctrl frame belong to priority 0, so if there is any connect ctrl frame, is must be found firstly
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
        return 0;
    }
    //滑动窗口是否满了
    int npLLCnum = 0;
    if(pLLCInstance->bIsWriteWindowsFull)
    {
        if(pLLCInstance->nWriteNextToSendFrameId - pLLCInstance->nWriteLastAckSentFrameId - 1 >= pLLCInstance->nWindowSize)
        {
            pLLCInstance->bIsWriteWindowsFull = true;
            printf("\nThe slide window is full!\n");
            return 0;
        }    
        else
            pLLCInstance->bIsWriteWindowsFull = false;
    }

    if(g_sMACInstance->bIsWriteFramePending)
    {
        printf("\nThe wirte mechine is pending!\n");
        return 0;
    }

    if(pLLCInstance->nNextCtrlFrameToSend == READ_CTRL_FRAME_NONE)
    {
        pSingleMACFrame = pLLCInstance->aSlideWindow[static_Modulo(pLLCInstance->nWriteNextToSendFrameId,pLLCInstance->nWindowSize)];
        if(pSingleMACFrame->pFrameBuffer != NULL)
        {
            printf("\nCFREE((void*)(pSingleMACFrame->pFrameBuffer))\n");
            CFREE((void*)(pSingleMACFrame->pFrameBuffer));
            pSingleMACFrame->pFrameBuffer = NULL;
            pSingleMACFrame->nFrameLength = 0;
        }
        pSendLLCFrame = pLLCInstance->pLLCFrameWriteListHead;
        
        //(pSendLLCFrame->nFrameLength + 3)
        pSingleMACFrame->pFrameBuffer = (uint8_t*)CMALLOC(sizeof(uint8_t)*MAC_FRAME_MAX_LENGTH);
        
        *(pSingleMACFrame->pFrameBuffer) = HEADER_SOF;
        //填充 N(S) 和 N(R) 字段 
        *(pSendLLCFrame->pFrameBuffer + 1) |= ((pLLCInstance->nWriteNextToSendFrameId & 0x07) << 3);
        *(pSendLLCFrame->pFrameBuffer + 1) |= (pLLCInstance->nReadNextToReceivedFrameId & 0x07);

        nCRC = 0;
        for(int index = 0; index < pSendLLCFrame->nFrameLength-1; index++)
        {
            nCRC ^= *(pSendLLCFrame->pFrameBuffer + index);
        }
        *(pSendLLCFrame->pFrameBuffer + pSendLLCFrame->nFrameLength - 1) = nCRC;
        printf("\npDataHaventInsertZero : ");
        for(int index = 0; index < pSendLLCFrame->nFrameLength; index++)
            printf("0x%02x ",*(pSendLLCFrame->pFrameBuffer + index));
        printf("\n");
        nInsertedZeroFrameLength = static_InsertZero(pSendLLCFrame->pFrameBuffer,pSingleMACFrame->pFrameBuffer+1,pSendLLCFrame->nFrameLength);
        
        *(pSingleMACFrame->pFrameBuffer + nInsertedZeroFrameLength + 1) = TRAILER_EOF;
        //len + 2 : SOF EOF
        pSingleMACFrame->nFrameLength = nInsertedZeroFrameLength + 2;

        pLLCInstance->pLLCFrameWriteListHead = pSendLLCFrame->pNext;
        pSendLLCFrame->pNext = NULL;
        //##加锁
        static_AddToWriteCompletedContextList(pLLCInstance,pSingleMACFrame,ADD_TO_LIST_HEAD);
        //##解锁
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
        return 0;
    }//控制帧优先级最高，直接发送
    else
    {
        if(pLLCInstance->nNextCtrlFrameToSend != LLC_FRAME_UA)
        {
            //预留一个 CRC 字节
            pCtrlLLCHeader = (uint8_t*)CMALLOC(sizeof(uint8_t)*4);
            pCtrlFrameData = (uint8_t*)CMALLOC(sizeof(uint8_t)*7);   //SOF LEN + LLCCRTL WINDOWSIZE [INSERTED ZERO] + CRC EOF
            
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
            printf("\npDataHaventInsertZero : ");
            for(int index = 0; index < 4; index++)
                printf("0x%02x ",*(pCtrlLLCHeader + index));
            printf("\n");
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
            printf("\npDataHaventInsertZero : ");
            for(int index = 0; index < 3; index++)
                printf("0x%02x ",*(pCtrlLLCHeader + index));
            printf("\n");
            nInsertedZeroFrameLength = static_InsertZero(pCtrlLLCHeader,pCtrlFrameData+1,3);
            printf("\nCFREE(pCtrlLLCHeader)\n");
            CFREE(pCtrlLLCHeader);
        }
        *(pCtrlFrameData) = HEADER_SOF;
        *(pCtrlFrameData + nInsertedZeroFrameLength + 1) = TRAILER_EOF;
        nCtrlFrameLength = nInsertedZeroFrameLength + 2;

        SPIWriteBytes(pLLCInstance,pCtrlFrameData,nCtrlFrameLength,1);
        printf("\nCFREE(pCtrlFrameData)\n");
        CFREE(pCtrlFrameData);
        printf("\nCFREE(pCtrlLLCHeader)\n");
        CFREE(pCtrlLLCHeader);
        pLLCInstance->nNextCtrlFrameToSend = READ_CTRL_FRAME_NONE;
        return 0;
    }
}

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
    
    nOtherWantToRecvNextId = static_ConvertTo32BitIdentifier(pLLCInstance,nN_R_Value);
        
    if(!((nOtherWantToRecvNextId > nWriteLastACKId) && (nOtherWantToRecvNextId <= nWriteNextToSendId)))
    {
        //如果收到对方发来的期望接收的下一帧ID不在对方已经确认收到的最大ID和我即将发送的ID之间，
        //则表示这个ID越界了，不对这个控制帧做响应，并且记录这种情况连续出现的次数，如果连续出现多次，
        //则认为协议栈的ID管理已经崩溃，无法再保证消息的可靠传输，保持各个缓冲区的数据不变，重置序号，
        //发送端把滑动窗口中的帧全重发。（接收端做重复对比？）
#ifndef DEBUG_PRINTF
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
        printf("\nRR : 0x%08x ->nWriteLastAckSentFrameId : 0x%08x\n",pLLCInstance,pLLCInstance->nWriteLastAckSentFrameId);

        //##加锁
        for(int times = 0; times < nAckedFrameNum; times++)
        {
            if(!RemoveACompleteSentFrame(pLLCInstance))
            {
                printf("\nThe count of completed write context is less than recved N(R) - lastAck.\n");
                return false;
            }
        }
        //##解锁
        printf("\n-------------->RR : update LastACK\t\tN(R) : 0x%08x\n",nOtherWantToRecvNextId);
        return true;
    }//REJ帧
    else if((nCtrlFrame & LLC_S_FRAME_MASK) == READ_CTRL_FRAME_REJ)
    {
        //更新LastACK
        nAckedFrameNum = nOtherWantToRecvNextId - 1 - pLLCInstance->nWriteLastAckSentFrameId;
        pLLCInstance->nWriteLastAckSentFrameId = nOtherWantToRecvNextId - 1;

        //##加锁
        for(int times = 0; times < nAckedFrameNum; times++)
        {
            if(!RemoveACompleteSentFrame(pLLCInstance))
            {
                printf("\nThe count of completed write context is less than recved N(R) - lastAck.\n");
                return false;
            }
        }
        //##解锁
        pLLCInstance->nWriteNextWindowFrameId = nOtherWantToRecvNextId;
        printf("\nREJ : 0x%08x ->nWriteNextToSendFrameId : 0x%08x\n",pLLCInstance,pLLCInstance->nWriteNextToSendFrameId);
        printf("\nREJ : 0x%08x ->nWriteNextWindowFrameId : 0x%08x\n",pLLCInstance,pLLCInstance->nWriteNextWindowFrameId);
        printf("\nREJ : 0x%08x ->nWriteLastAckSentFrameId : 0x%08x\n",pLLCInstance,pLLCInstance->nWriteLastAckSentFrameId);

        printf("\n-------------->REJ : update NextWindowToSend\t\tN(R) : 0x%02x\n",nOtherWantToRecvNextId);
        return true;
    }//RNR帧
    else if((nCtrlFrame & LLC_S_FRAME_MASK) == READ_CTRL_FRAME_RNR)
    {
        //更新NextWindowToSend
        pLLCInstance->bIsWriteOtherSideReady = false;
        nAckedFrameNum = nOtherWantToRecvNextId - 1 - pLLCInstance->nWriteLastAckSentFrameId;
        
        pLLCInstance->nWriteLastAckSentFrameId = nOtherWantToRecvNextId - 1;
        printf("\nRNR : 0x%08x ->nWriteLastAckSentFrameId : 0x%08x\n",pLLCInstance,pLLCInstance->nWriteLastAckSentFrameId);

        //##加锁
        for(int times = 0; times < nAckedFrameNum; times++)
         {
            if(!RemoveACompleteSentFrame(pLLCInstance))
            {
                printf("\nThe count of completed write context is less than recved N(R) - lastAck.\n");
                return false;
            }
        }
        //##解锁
        printf("\n-------------->RNR : pLLCInstance->bIsWriteOtherSideReady = %d\t\tN(R) : 0x%02x\n",pLLCInstance->bIsWriteOtherSideReady,nOtherWantToRecvNextId);
        return true;
    }
    if(pLLCInstance->nWriteNextToSendFrameId - pLLCInstance->nWriteLastAckSentFrameId - 1 >= pLLCInstance->nWindowSize)
        pLLCInstance->bIsWriteWindowsFull = true;
    else
        pLLCInstance->bIsWriteWindowsFull = false;
    return false;
}

uint8_t SPIWriteBytes(tLLCInstance* pLLCInstance,uint8_t* pData,uint8_t nLength,bool bIsCtrlFrame)
{
    tMACWriteContext* pSendMACFrame = NULL;
    g_sMACInstance->bIsWriteFramePending = true;

    printf("Befor SPIWriteBytes(),0x%08x->nWriteNextWindowFrameId : 0x%08x",pLLCInstance,pLLCInstance->nWriteNextWindowFrameId);

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
    g_sMACInstance->bIsWriteFramePending = false;

    printf("After SPIWriteBytes(),0x%08x->nWriteNextWindowFrameId : 0x%08x",pLLCInstance,pLLCInstance->nWriteNextWindowFrameId);

    return 0;
}

uint8_t RemoveACompleteSentFrame(tLLCInstance* pLLCInstance)
{
    tMACWriteContext* pWriteContext;
    pWriteContext = pLLCInstance->pLLCFrameWriteCompletedListHead;

    tMACWriteContext* pPreWriteContext;

    LOCK_WRITE();

    if(pWriteContext == NULL)
    {
        UNLOCK_WRITE();
        return 0;
    }
    else
    {
        pPreWriteContext = pWriteContext;
        pWriteContext = pPreWriteContext->pNext;
        if(pWriteContext == NULL)
        {
            printf("\nCFREE((void*)(pPreWriteContext))\n");
            CFREE((void*)(pPreWriteContext->pFrameBuffer));
            CFREE((void*)(pPreWriteContext));
            pPreWriteContext = NULL;
            pLLCInstance->pLLCFrameWriteCompletedListHead = NULL;
        }
        else
        {
            while(pWriteContext->pNext != NULL)
            {
                pPreWriteContext = pWriteContext;
                pWriteContext = pPreWriteContext->pNext;
            }
            printf("\nCFREE((void*)(pWriteContext))\n");
            CFREE((void*)(pWriteContext->pFrameBuffer));
            CFREE((void*)(pWriteContext));
            pWriteContext = NULL;
            pPreWriteContext->pNext = NULL;
        }
        UNLOCK_WRITE();

        return 1;
    }

}

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