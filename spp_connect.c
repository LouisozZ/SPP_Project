#include "spp_global.h"

uint8_t ConnectErrorFrameHandle(uint8_t nMessageHeader)
{
    printf("\nTHis is error frame handler! message head : 0x%02x \n",nMessageHeader);
    return 0;
}

uint8_t ConnectCtrlFrameACK(uint8_t nMessageHeader)
{
    printf("\nTHis is Connect ctrl ack frame handler! message head : 0x%02x \n",nMessageHeader);
    return 0;
}