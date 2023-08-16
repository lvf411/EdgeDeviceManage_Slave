#ifndef __BASE64_HPP
#define __BASE64_HPP

#include <stdint.h>

uint8_t Base64_Encode(char *pInData, uint32_t inLen, char *pOutData, uint32_t *pOutLen);
uint8_t Base64_Decode(char *pInData, uint32_t inLen, char *pOutData, uint32_t *pOutLen);

#endif // __BASE64_HPP
