#ifndef __SPP_GLOBAL_H__
#define __SPP_GLOBAL_H__

#include "spp_def.h"
#include "spp_include.h"

uint8_t g_nVersion;

tSPPInstance* g_sSPPInstance;
tMACInstance* g_sMACInstance;
tLLCInstance* g_aLLCInstance[PRIORITY];     //下标越小，优先级越高，优先级从0往后一次递减


#endif