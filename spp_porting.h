#ifndef __SPP_PORTING_H__
#define __SPP_PORTING_H__

#define CDebugAssert(value) if(!(value)){printf("\n%s is not true!\nreturn...\n",#value);return;}

/**
 * @function    调用底层SPI的读函数读取字节
 * @description 
 * @parameter
 * @return      实际读到的字节数，如果读出错则返回 -1
*/
uint8_t ReadBytes(uint8_t *pBuffer,uint8_t nReadLength);
void* CMALLOC(uint32_t length);
uint8_t CFREE(void* pFreeAddress);

#endif