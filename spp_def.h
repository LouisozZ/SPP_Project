#ifndef __SPP_DEF_H__
#define __SPP_DEF_H__

#define TEST_MAIN
#define USER_PRINTF
#define DEBUG_PEINTF

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
#if __WORDSIZE == 64  
typedef unsigned long int       uint64_t;  
#else  
//__extension__  
typedef unsigned long long int  uint64_t;  
#endif 
//******************************  变量类型定义  ****************************************


//=====================================================================================
//                                  常量定义
//=====================================================================================
//优先级嵌套层次与计算掩码
#define PRIORITY_MASK   0x07

#define PRIORITY        4

//滑动窗口
#define MAX_WINDOW_SIZE      4

//读状态机
#define MAC_FRAME_READ_STATUS_IDLE               0
#define MAC_FRAME_READ_STATUS_WAITING_HEADER     1
#define MAC_FRAME_READ_STATUS_WAITING_BYTE       2
#define MAC_FRAME_READ_STATUS_RESET_PENDING      3

#define HEADER_SOF      0x7e
#define TRAILER_EOF     0x7f

//帧长度
//#define LLC_FRAME_MAX_LENGTH 30
#define LLC_FRAME_MAX_LENGTH 4
#define MAC_FRAME_MAX_LENGTH LLC_FRAME_MAX_LENGTH+4+(LLC_FRAME_MAX_LENGTH/5)
#define SINGLE_MESSAGE_MAX_LENGTH 256

//缓冲区大小
#define READ_BUFFER_SIZE    256

//版本定义
#define VERSION_FIELD_MASK  0x78

#define VERSION_1_0         0x08
//******************************    常量定义    ****************************************

#endif