#include "spp_include.h"

int main()
{
    tLLCInstance* pLLCInstance;
    int lccframelength = 0;
    int oldlength;

    //uint8_t olddata[13] = {0x09,0x80,0x00,0x15,0x23,0x46,0x73,0x20,0x7e,0xff,0x32};     //11
    //uint8_t olddata[13] = {0x09,0x80,0x00,0x49,0x7f,0x7f,0x7e,0x7f,0x29,0x43,0x6e};   //11
    //uint8_t olddata[13] = {0x02,0xcd,0x80,0x31};             //4    REJ
     //uint8_t olddata[13] = {0x02,0xf9,0x02,0x31};             //4    RSET
    // uint8_t olddata[13] = {0x02,0xc1,0x80,0x31};             //4    RR
    // uint8_t olddata[13] = {0x02,0xD2,0x80,0x31};             //4    RNR
     uint8_t olddata[13] = {0x01,0xE6,0xE6};                  //3    UA



    uint8_t newdata[13] = {0};
    uint8_t removezero[13] = {0};

    oldlength = 4;

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

    for(int i = 0; i < 8; i++)
    {
        pLLCInstance = MACFrameRead();
        //printf("g_aLLCInstance[0]->nLLCReadReadPosition : %d\n",g_aLLCInstance[0]->nLLCReadReadPosition);
        lccframelength = g_aLLCInstance[0]->aLLCReadBuffer[g_aLLCInstance[0]->nLLCReadReadPosition];
        lccframelength++;
        
        // if(lccframelength > 22)
        //     continue;
        // printf("\n*******************   读缓存中的一帧   **********************************\n\
        // lccframelength : %d\n",lccframelength);
        // for(int index = 0; index < lccframelength; index++)
        // {
        //     printf("0x%02x ",g_aLLCInstance[0]->aLLCReadBuffer[g_aLLCInstance[0]->nLLCReadReadPosition+index]);
        // }
        // printf("\n=========================================================\n");
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