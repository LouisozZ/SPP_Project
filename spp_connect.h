#ifndef __SPP_CONNECT_H__
#define __SPP_CONNECT_H__

#define CONNECT_FRAME false
#define DATA_FRAME    true

#define TIMER_0_CONNECT TIMER_0
#define TIMER_1_CONNECT_CONFIRM TIMER_1

#define RESENT_CONNECT_REQUIRE_TIMEOUT      10000
#define RESENT_CONFIRM_CONNECT_TIMEOUT      5000
#define RESENT_DIS_RESET_CONNECT_TIMEOUT    2000

uint8_t ConnectErrorFrameHandle(uint8_t nMessageHeader);
uint8_t ConnectCtrlFrameACK(uint8_t nMessageHeader);

#endif