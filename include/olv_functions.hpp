#ifndef __NN_OLV_FUNCTIONS_H
#define __NN_OLV_FUNCTIONS_H

#include <cstdint>
#include <nn/result.h>

#include "olv_types.hpp"

namespace nn::olv
{
    extern bool g_IsLibraryInitialized;
    extern uint32_t g_ReportTypes;
    extern InternalStruct g_InternalStruct;
    extern InternalClientObject g_ClientObject;

    namespace Report
    {
        void SetReportTypes(uint32_t types);
        uint32_t GetReportTypes();
        void Print(nn::olv::ReportType type, char const *format, ...);
        void PrintBinary(nn::olv::ReportType type, uint8_t *data, uint32_t size);
    }

    bool IsInitialized();

    nn::Result InitializeInternal(nn::olv::InitializeParam const *param, bool a2, int a3, int a4);
    nn::Result Initialize(nn::olv::InitializeParam const *param);
    nn::Result Initialize(nn::olv::MainAppParam *, nn::olv::InitializeParam const *);

    size_t GetNowUsedMemorySize();

    namespace Error
    {
        void SetServerResponseCode(uint32_t responseCode);
        uint32_t GetServerResponseCode();
    }

    nn::Result GetUserAgent(char *userAgent, uint32_t maxSize);

    int GetErrorCode(nn::Result const &result);

    nn::Result CreateParameterPack(uint64_t titleId, uint32_t accessKey, nn::olv::InternalParamPack *ptr);

    nn::Result GetOlvAccessKey(uint32_t *outAccessKey);

}

namespace nn::act
{
    int
    GetErrorCode(nn::Result const &) asm("GetErrorCode__Q2_2nn3actFRCQ2_2nn6Result");

    nn::Result GetParentalControlSlotNoEx(unsigned char *, unsigned char) asm("GetParentalControlSlotNoEx__Q2_2nn3actFPUcUc");

    nn::Result GetTimeZoneId(char *) asm("GetTimeZoneId__Q2_2nn3actFPc");

    nn::Result AcquireIndependentServiceToken(char *outToken, char const *clientId) asm("AcquireIndependentServiceToken__Q2_2nn3actFPcPCc");
    nn::Result AcquireIndependentServiceToken(char *outToken, char const *clientId, unsigned int, bool, bool) asm("AcquireIndependentServiceToken__Q2_2nn3actFPcPCcUibT4");

}

extern "C" char *Base64Encode(char *dest, size_t destSize, uint8_t *src, size_t srcSize, int *outSize);

extern "C" int SCIGetParentalAccountNetworkLauncher(uint8_t *restrictionLevel, uint8_t slotNo);
extern "C" int SCIGetSystemProdArea(uint32_t *outProdArea);
extern "C" int SCIGetLanguage(uint32_t *outLanguage);
extern "C" int SCIGetCafeCountry(uint32_t *outCountry);
extern "C" int SCIGetParentalAccountFriendReg(uint8_t *outFriendRegRestr, uint8_t slotNo);
extern "C" int SCIGetParentalAccountGameRating(uint32_t *outGameRatingRestr, uint8_t slotNo);
extern "C" int SCIGetParentalRatingOrganization(uint32_t *outRatingOrg);

extern "C" int ACPGetOlvAccesskey(uint32_t *);
extern "C" int OSGetConsoleType(void);
extern "C" NSSLError NSSLSetClientPKI(NSSLContextHandle handle, int unk);
extern "C" NSSLError NSSLAddServerPKIGroups(NSSLContextHandle handle, uint32_t flags, int *, int *);

#endif
