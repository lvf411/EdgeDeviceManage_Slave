#include <stdint.h>
#include <stddef.h>
#include "base64.hpp"

static char s_base64Table[] = 
{
    'A','B','C','D','E','F','G','H','I','J',
    'K','L','M','N','O','P','Q','R','S','T',
    'U','V','W','X','Y','Z','a','b','c','d',
    'e','f','g','h','i','j','k','l','m','n',
    'o','p','q','r','s','t','u','v','w','x',
    'y','z','0','1','2','3','4','5','6','7',
    '8','9','+', '/', '\0'
};

/**
 @brief Base64编码
 @param pInData [in] 源字符串
 @param inLen [in] 源字符串长度
 @param pOutData [out] 编码后字符串
 @param pOutLen [out] 解码后字符串长度
 @return 1 - 成功；0 - 失败
*/
uint8_t Base64_Encode(char *pInData, uint32_t inLen, char *pOutData, uint32_t *pOutLen)
{
    if(NULL == pInData || 0 == inLen)
    {
        return 0;
    }

    uint32_t i = 0;
    uint32_t j = 0;
    uint32_t temp = 0;
    // 3字节一组进行转换
    for(i = 0; i < inLen; i += 3) 
    {
        // 获取第一个6位
        temp = (*(pInData + i) >> 2) & 0x3F;
        *(pOutData + j++) = s_base64Table[temp];

        // 获取第二个6位的前两位
        temp = (*(pInData + i) << 4) & 0x30;
        // 如果只有一个字符，那么需要做特殊处理
        if(inLen <= (i + 1)) 
        {
            *(pOutData + j++) = s_base64Table[temp];
            *(pOutData + j++) = '=';
            *(pOutData + j++) = '=';
            break;
        }
        // 获取第二个6位的后四位
        temp |= (*(pInData + i + 1) >> 4) & 0x0F;
        *(pOutData + j++) = s_base64Table[temp];

        // 获取第三个6位的前四位
        temp = (*(pInData+ i + 1) << 2) & 0x3C;
        if(inLen <= (i + 2))
        {
            *(pOutData + j++) = s_base64Table[temp];
            *(pOutData + j++) = '=';
            break;
        }
        // 获取第三个6位的后两位
        temp |= (*(pInData + i + 2) >> 6) & 0x03;
        *(pOutData + j++) = s_base64Table[temp];

        // 获取第四个6位
        temp = *(pInData + i + 2) & 0x3F;
        *(pOutData + j++) = s_base64Table[temp];
    }
    *(pOutData + j) = '\0';
    // 编码后的长度
    *pOutLen = inLen * 8 / 6;
    return 1;
}

/**
 @brief Base64解码
 @param pInData [in] 源字符串
 @param inLen [in] 源字符串长度
 @param pOutData [out] 解码后字符串
 @param pOutLen [out] 解码后字符串长度
 @return 1 - 成功；0 - 失败
*/
uint8_t Base64_Decode(char *pInData, uint32_t inLen, char *pOutData, uint32_t *pOutLen)
{
    if(NULL == pInData || 0 == inLen || inLen % 4 != 0)
    {
        return 0;
    }

    uint32_t i = 0;
    uint32_t j = 0;
    uint32_t k = 0;
    char temp[4] = "";
    // 4字节一组进行转换
    for(i = 0; i < inLen; i += 4)
    {
        // 找到在编码索引表中对应的值
        for(j = 0; j < 64; j++) 
        {
            if(*(pInData + i) == s_base64Table[j])
            {
                temp[0] = j;
            }
        }        
        for(j = 0; j < 64; j++)
        {
            if(*(pInData + i + 1) == s_base64Table[j])
            {
                temp[1] = j;
            }
        }
        for(j = 0; j < 64; j++)
        {
            if(*(pInData + i + 2) == s_base64Table[j])
            {
                temp[2] = j;
            }
        }
        for(j = 0; j < 64; j++) 
        {
            if(*(pInData + i + 3) == s_base64Table[j]) 
            {
                temp[3] = j;
            }
        }

        // 获取第一个6位和第二个6位的前两位组成一个8位
        *(pOutData + k++) = ((temp[0] << 2) & 0xFC) | ((temp[1] >> 4) & 0x03);
        if(*(pInData + i + 2) == '=')
        {
            break;
        }
        // 获取第二个6位的前四位和第三个6位的前四位组成一个8位
        *(pOutData + k++) = ((temp[1] << 4) & 0xF0) | ((temp[2] >> 2) & 0x0F);
        if(*(pInData + i + 3) == '=')
        {
            break;
        }
        // 获取第三个6位的后两位和第四个6位组成一个8位
        *(pOutData + k++) = ((temp[2] << 6) & 0xF0) | (temp[3] & 0x3F);
    }
    *pOutLen = k;
    return 1;
}
