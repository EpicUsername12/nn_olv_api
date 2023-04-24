#include "olv_functions.hpp"
#include <coreinit/ios.h>
#include <coreinit/mcp.h>
#include <coreinit/memexpheap.h>
#include <whb/log.h>
#include <cstdarg>

namespace nn::olv
{
    nn::Result Initialize(nn::olv::InitializeParam const *param)
    {
        return InitializeInternal(param, 0, 1, 1);
    }

    void Report::SetReportTypes(uint32_t types)
    {
        g_ReportTypes = types | 0x1000;
    }

    uint32_t Report::GetReportTypes()
    {
        return g_ReportTypes;
    }

    void Report::Print(nn::olv::ReportType type, char const *format, ...)
    {
        va_list va;
        char buf1[0x800];

        va_start(va, format);
        vsnprintf(buf1, 0x800, format, va);

        WHBLogPrintf("%s", buf1);

        va_end(va);
    }

    bool IsInitialized()
    {
        return g_IsLibraryInitialized;
    }

    size_t GetNowUsedMemorySize()
    {
        size_t total = 0;
        for (int i = 0; i < 3; ++i)
        {
            total += g_InternalStruct.m_UsedMemorySizes[i];
        }
        return total;
    }

    int GetErrorCode(nn::Result const &res)
    {
        int result;

        uint32_t v6; // r31
        uint32_t v7; // r0
        uint32_t v8; // r12
        uint32_t v2 = *(uint32_t *)&res;
        uint32_t v3 = (v2 >> 27) & 3;
        uint32_t v4 = 0x7F00000;

        if (v3 != 3)
            v4 = 0x1FF00000;

        if (v3 != 3)
            v4 = 0x1FF00000;
        if ((v2 & (unsigned int)v4) >> 20 == 7)
        {
            if (v2 == 0xA0757A80 || v2 == 0xA0758480)
            {
                nn::Result v9 = NN_OLV_RESULT_STATUS_CODE(0x1F800);
                result = nn::olv::GetErrorCode(v9);
            }
            else if (v2 == 0xA0757F80 || v2 == 0xA0758980)
            {
                nn::Result v10 = NN_OLV_RESULT_STATUS_CODE(0x1F800);
                result = nn::olv::GetErrorCode(v10);
            }
            else
            {
                nn::act::Initialize();
                v6 = nn::act::GetErrorCode(res);
                nn::act::Finalize();
                result = v6;
            }
        }
        else
        {
            v7 = 133169152;
            if (v3 != 3)
                v7 = 535822336;
            if ((v2 & (unsigned int)v7) >> 20 == 17 && v2 < 0)
            {
                v8 = 1023;
                if (v3 != 3)
                    v8 = 0xFFFFF;
                result = (v2 & v8) / 128 + 1150000;
            }
            else
            {
                result = 1159999;
            }
        }
        return result;
    }

    nn::Result GetUserAgent(char *userAgent, uint32_t maxSize)
    {

        if (nn::olv::IsInitialized())
        {
            if (!userAgent)
                return NN_OLV_RESULT_USAGE_CODE(0x6600);

            if (maxSize <= 0x40 && maxSize)
            {
                snprintf(userAgent, maxSize, "%s", g_ClientObject.userAgent);
                return NN_OLV_RESULT_SUCCESS;
            }
            else
            {
                Report::Print(REPORT_TYPE_2048, "GetUserAgent illegal buffSize!!(%d)\n", maxSize);
                NN_OLV_RESULT_STATUS_CODE(0x3EC00);
            }
        }

        return NN_OLV_RESULT_USAGE_CODE(0x6680);
    }

    uint32_t g_ServerResponseCode;
    void Error::SetServerResponseCode(uint32_t responseCode)
    {
        g_ServerResponseCode = responseCode;
    }

    uint32_t Error::GetServerResponseCode()
    {
        return g_ServerResponseCode;
    }

}