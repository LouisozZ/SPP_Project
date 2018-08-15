#include "spp_include.h"

void AddTimerToCheckList(tSppMultiTimer* pTimer)
{
    tSppMultiTimer* pEarliestTimer = NULL;
    
    CDebugAssert(pTimer->nInterval != 0);

    pTimer->nTimeStamp = g_nAbsoluteTime + pTimer->nInterval;
    if(pTimer->nTimeStamp < g_nAbsoluteTime)
        pTimer->bIsOverflow = !(pTimer->bIsOverflow);
    if(g_pTimeoutCheckListHead == NULL)
    {
        g_pTimeoutCheckListHead = pTimer;
        g_pTimeoutCheckListHead->pNextTimer = NULL;
        g_pTimeoutCheckListHead->pPreTimer = NULL;
        g_pTimeoutCheckListHead->pNextHandle = NULL;
    }
    else
    {
        pEarliestTimer = g_pTimeoutCheckListHead;
        while(pEarliestTimer != NULL)
        {
            if((pEarliestTimer->bIsOverflow != pTimer->bIsOverflow) || (pEarliestTimer->nTimeStamp < pTimer->nTimeStamp))
                pEarliestTimer = pEarliestTimer->pNextTimer;
            else
            {
                if(pEarliestTimer->nTimeStamp == pTimer->nTimeStamp)
                {
                    pTimer->pNextHandle = pEarliestTimer->pNextHandle;
                    pEarliestTimer->pNextHandle = pTimer;
                    break;
                }
                else
                {
                    if(pEarliestTimer->pPreTimer == NULL)
                    {
                        pEarliestTimer->pPreTimer = pTimer;
                        pTimer->pNextTimer = pEarliestTimer;
                        pTimer->pPreTimer = NULL;
                        g_pTimeoutCheckListHead = pTimer;
                        break;
                    }
                    else
                    {
                        pEarliestTimer->pPreTimer->pNextTimer = pTimer;
                        pTimer->pNextTimer = pEarliestTimer;
                        pTimer->pPreTimer = pEarliestTimer->pPreTimer;
                        pEarliestTimer->pPreTimer = pTimer;
                        break;
                    }
                }
            }        
        }
    }
}

/**
 * @function    定时器处理函数，用于检测是否有定时任务超时，如果有则调用该定时任务的回调函数，并更新超时检测链表
 *              更新动作：如果超时的那个定时任务不是一次性的，则将新的节点加入到检测超时链表中，否则直接删掉该节点；
 * @parameter   
 * @return
*/
void SYSTimeoutHandler()
{
    tSppMultiTimer* pEarliestTimer = NULL;
    pEarliestTimer = g_pTimeoutCheckListHead;

    if(g_pTimeoutCheckListHead != NULL)
    {
        if((g_pTimeoutCheckListHead->nTimeStamp <= g_nAbsoluteTime) && (g_pTimeoutCheckListHead->bIsOverflow == g_bIs_g_nAbsoluteTimeOverFlow))
        {
            while(pEarliestTimer != NULL)
            {
                if(!(pEarliestTimer->bIsSingleUse))
                    AddTimerToCheckList(pEarliestTimer);
                pEarliestTimer = pEarliestTimer->pNextHandle;
            }
            pEarliestTimer = g_pTimeoutCheckListHead;
            g_pTimeoutCheckListHead = g_pTimeoutCheckListHead->pNextTimer;
            if(g_pTimeoutCheckListHead != NULL)
                g_pTimeoutCheckListHead->pPreTimer = NULL;
            while(pEarliestTimer != NULL)
            {
                pEarliestTimer->pTimeoutCallbackfunction(pEarliestTimer->pTimeoutCallbackParameter);
                pEarliestTimer = pEarliestTimer->pNextHandle;
            }
        }
    }
    
    g_nAbsoluteTime++;
    if(g_nAbsoluteTime == 0)
        g_bIs_g_nAbsoluteTimeOverFlow = !g_bIs_g_nAbsoluteTimeOverFlow;
}

void MultiTimerInit()
{
    g_pTimeoutCheckListHead = NULL;
    g_bIs_g_nAbsoluteTimeOverFlow = false;
    g_nAbsoluteTime = 0;
    /*  如果预先规定了一些定时器，这个时候可以初始化除时间戳以外的其他值  */
}