#ifndef __SPP_GLOBAL_H__
#define __SPP_GLOBAL_H__

#include "spp_def.h"
#include "spp_include.h"

uint8_t g_nVersion;
uint32_t g_nAbsoluteTime;

uint8_t g_aTimerID[MAX_TIMER_UPPER_LIMIT];
uint32_t g_aDefaultTimeout[MAX_TIMER_UPPER_LIMIT];

tSPPInstance* g_sSPPInstance;
tMACInstance* g_sMACInstance;
tLLCInstance* g_aLLCInstance[PRIORITY];     //下标越小，优先级越高，优先级从0往后一次递减
tSppMultiTimer* g_aSPPMultiTimer[MAX_TIMER_UPPER_LIMIT];


#endif