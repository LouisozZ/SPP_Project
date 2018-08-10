#include "spp_include.h"

int main()
{
    tLLCInstance* pLLCInstance;
    int lccframelength = 0;
    int oldlength;

    //uint8_t olddata[13] = {0x09,0x80,0x00,0x15,0x23,0x46,0x73,0x20,0x7e,0xff,0x32};     //11
    //uint8_t olddata[13] = {0x09,0x80,0x00,0x49,0x7f,0x7f,0x7e,0x7f,0x29,0x43,0x6e};   //11
    uint8_t olddata[13] = {0x07,0x80,0x80,0x21,0x44,0x95,0x08,0x18,0x44};             //9

    uint8_t newdata[13] = {0};
    uint8_t removezero[13] = {0};

    oldlength = 9;

    lccframelength = static_InsertZero(olddata,newdata,oldlength);
    for(int i = 0; i < oldlength; i++)
        printf("0x%02x ",olddata[i]);
    printf("\n");
    for(int i = 0; i < lccframelength; i++)
        printf("0x%02x ",newdata[i]);
    printf("\n===============================================================\n");
    oldlength = static_RemoveInsertedZero(newdata,removezero,lccframelength);
    for(int i = 0; i < lccframelength; i++)
        printf("0x%02x ",newdata[i]);
    printf("\n");
    for(int i = 0; i < oldlength; i++)
        printf("0x%02x ",removezero[i]);

    InitSPP();

    for(int i = 0; i < 4; i++)
    {
        pLLCInstance = MACFrameRead();
        printf("g_aLLCInstance[0]->nLLCReadReadPosition : %d\n",g_aLLCInstance[0]->nLLCReadReadPosition);
        lccframelength = g_aLLCInstance[0]->aLLCReadBuffer[g_aLLCInstance[0]->nLLCReadReadPosition];
        lccframelength++;
        
        // if(lccframelength > 22)
        //     continue;
        printf("\n*******************   读缓存中的一帧   **********************************\n\
        lccframelength : %d\n",lccframelength);
        for(int index = 0; index < lccframelength; index++)
        {
            printf("0x%02x ",g_aLLCInstance[0]->aLLCReadBuffer[g_aLLCInstance[0]->nLLCReadReadPosition+index]);
        }
        printf("\n=========================================================\n");
        // if(g_aLLCInstance[0]->nLLCReadReadPosition + lccframelength >= READ_BUFFER_SIZE)
        //     g_aLLCInstance[0]->nLLCReadReadPosition = g_aLLCInstance[0]->nLLCReadReadPosition + lccframelength - READ_BUFFER_SIZE;
        // else g_aLLCInstance[0]->nLLCReadReadPosition += lccframelength;
        if(pLLCInstance != NULL)
            LLCReadFrame(pLLCInstance);
    }
    printf("\ng_sSPPInstance->nMessageLength : %d\ng_sSPPInstance->pMessageBuffer : ",g_sSPPInstance->nMessageLength);
    for(int index = 0; index < g_sSPPInstance->nMessageLength; index++)
    {
        printf("0x%02x ",g_sSPPInstance->pMessageBuffer[index]);
    }
    printf("\n");
}