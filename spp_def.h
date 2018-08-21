#ifndef __SPP_DEF_H__
#define __SPP_DEF_H__

#define TEST_MAIN
#define USER_PRINTF
#define DEBUG_PEINTF

#define SERVICE_CODE 1

//=====================================================================================
//                                变量类型定义
//=====================================================================================
#ifndef __bool_defined
    enum BOOL{false,true};
    typedef enum BOOL bool;
    #define __bool_defined
#endif

#ifndef NULL
    #define NULL (void*)(0)
#endif

typedef unsigned char           uint8_t;  
typedef unsigned short int      uint16_t;  
#ifndef __uint32_t_defined  
typedef unsigned int            uint32_t;  
#define __uint32_t_defined  
#endif  

// #if __WORDSIZE == 64  
// typedef unsigned long int       uint64_t;  
// #else  
// __extension__  
// typedef unsigned long long int  uint64_t;  
// #endif 

#define tMACWriteContext tLLCWriteContext
//******************************  变量类型定义  ****************************************


//=====================================================================================
//                                  常量定义
//=====================================================================================
//-----------------------
//优先级嵌套层次与计算掩码
//-----------------------
#define PRIORITY_MASK   0x07

#define PRIORITY        4

//-----------------------
//滑动窗口
//-----------------------
#define MAX_WINDOW_SIZE      4

//-----------------------
//连接管理状态机
//-----------------------
//状态
#define CONNECT_STATU_DISCONNECTED                  0
#define CONNECT_STATU_WAITING_LINK_CONFIRM          1
#define CONNECT_STATU_WAITING_CONFIRM               2
#define CONNECT_STATU_WAITING_LLC_RESET             3
#define CONNECT_STATU_WAITING_LLC_UA                4
#define CONNECT_STATU_CONNECTED                     5
#define CONNECT_STATU_WAITING_DISCONNECT_CONFIRM    6
#define CONNECT_STATU_WAITING_RESET_CONFIRM         7

//空闲帧
#define CONNECT_IDLE                    0x00
//连接帧
#define CONNECT_REQUIRE_CONNECT         0xfb
#define CONNECT_CONFIRM_CONNECT         0xe4
#define CONNECT_CONFIRM_AGAIN           0xf5
#define CONNECT_CONNECT_ERROR           0xee
//断连接帧
#define CONNECT_REQUIRE_DISCONNECT      0x1b
#define CONNECT_CONFIRM_DISCONNECT      0x04
#define CONNECT_DISCONNECT_ERROR        0x0e
//重启帧
#define CONNECT_REQUIRE_RESET           0xbb
#define CONNECT_CONFIRM_RESET           0xa4
#define CONNECT_RESET_ERROR             0xae
//数据传输帧
#define CONNECT_SEND_RECV_MESSAGE       0x44
#define CONNECT_SEND_RECV_ERROR         0x4e
//掩码
#define CONNECT_TYPE_MASK               0xe0
#define CONNECT_VALUE_MASK              0x1f
#define CONNECT_REQUIRE_VALUE           0x1b
#define CONNECT_CONFIRM_VALUE           0x04
#define CONNECT_ERROR_VALUE             0x0e

//-----------------------
//读状态机
//-----------------------
#define MAC_FRAME_READ_STATUS_IDLE               0
#define MAC_FRAME_READ_STATUS_WAITING_HEADER     1
#define MAC_FRAME_READ_STATUS_WAITING_BYTE       2
#define MAC_FRAME_READ_STATUS_RESET_PENDING      3

#define HEADER_SOF      0x7e
#define TRAILER_EOF     0x7f

//-----------------------
//帧长度
//-----------------------
//#define LLC_FRAME_MAX_LENGTH 30
#define LLC_FRAME_MAX_LENGTH        11
#define LLC_FRAME_MAX_DATA_LENGTH   (LLC_FRAME_MAX_LENGTH-4)
#define MAC_FRAME_MAX_LENGTH        (LLC_FRAME_MAX_LENGTH+4+(LLC_FRAME_MAX_LENGTH/5))
#define SINGLE_MESSAGE_MAX_LENGTH   256

//-----------------------
//缓冲区大小
//-----------------------
#define READ_BUFFER_SIZE    256

//-----------------------
//版本定义
//-----------------------

//  package header  +#***---        说明：  +：CB位，分片信息     #：connect 控制帧     ***：版本信息，占用三位    ---：优先级，占用三位
#define VERSION_FIELD_MASK  0x38

#define VERSION_1_0         0x08

//-----------------------
//定时器
//-----------------------
#define MAX_TIMER_UPPER_LIMIT   6

//#define GetTimerID(x)   TIMER_##x

#define TIMER_0                 0
#define TIMER_1                 1       //timer ID
#define TIMER_2                 2
#define TIMER_3                 3
#define TIMER_4                 4
#define TIMER_5                 5

//#define GetDefaultTimeoutInterval(x)    TIME_OUT_INTERVAL_##x

#define TIME_OUT_INTERVAL_0     100
#define TIME_OUT_INTERVAL_1     100     //超时间隔
#define TIME_OUT_INTERVAL_2     100
#define TIME_OUT_INTERVAL_3     100
#define TIME_OUT_INTERVAL_4     100
#define TIME_OUT_INTERVAL_5     100

#define CANCEL_MODE_IMMEDIATELY         0xf9
#define CANCEL_MODE_AFTER_NEXT_TIMEOUT  0x9f

//******************************    常量定义    ****************************************

#endif