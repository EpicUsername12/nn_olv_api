#ifndef __NN_OLV_TYPES_H__
#define __NN_OLV_TYPES_H__

#include <cstring>
#include <cstdint>
#include <nn/result.h>

#include <coreinit/userconfig.h>
#include <coreinit/debug.h>
#include <coreinit/memexpheap.h>
#include <nn/act.h>
#include <nn/acp.h>

#include <nsysnet/_socket.h>
#include <nsysnet/nssl.h>

#include <curl/curl.h>

#ifdef __PRETENDO__

#define OLV_DISCOVERY_PROTOCOL "https://"
#define OLV_DISCOVERY_ADDRESS "discovery.olv.pretendo.cc/v1/endpoint"

#else

#define OLV_DISCOVERY_PROTOCOL "https://"
#define OLV_DISCOVERY_ADDRESS "discovery.olv.nintendo.net/v1/endpoint"

#endif

#define OLV_CLIENT_ID "87cd32617f1985439ea608c2746e4610"

#define OLV_VERSION_MAJOR 5
#define OLV_VERSION_MINOR 0
#define OLV_VERSION_PATCH 3

#define NN_OLV_RESULT_SUCCESS (nn::Result(nn::Result::LEVEL_SUCCESS, nn::Result::RESULT_MODULE_NN_OLV, 128))
#define NN_OLV_RESULT_SUCCESS_CODE(code) (nn::Result(nn::Result::LEVEL_SUCCESS, nn::Result::RESULT_MODULE_NN_OLV, code))
#define NN_OLV_RESULT_FATAL_CODE(code) (nn::Result(nn::Result::LEVEL_FATAL, nn::Result::RESULT_MODULE_NN_OLV, code))
#define NN_OLV_RESULT_USAGE_CODE(code) (nn::Result(nn::Result::LEVEL_USAGE, nn::Result::RESULT_MODULE_NN_OLV, code))
#define NN_OLV_RESULT_STATUS_CODE(code) (nn::Result(nn::Result::LEVEL_STATUS, nn::Result::RESULT_MODULE_NN_OLV, code))

namespace nn::olv
{
    enum ReportType : uint32_t
    {
        REPORT_TYPE_2 = (1 << 1),
        REPORT_TYPE_4 = (1 << 2),
        REPORT_TYPE_32 = (1 << 5),
        REPORT_TYPE_128 = (1 << 7),
        REPORT_TYPE_2048 = (1 << 11),
        REPORT_TYPE_4096 = (1 << 12),
    };

    typedef void (*TLogFunction)(const char *message);
    typedef void *(*TMemAllocFunction)(uint32_t size);
    typedef void (*TMemFreeFunction)(void *ptr);
    typedef size_t (*TCurlCallback)(char *ptr, uint32_t size, uint32_t nmemb, void *userdata);

    struct InternalParamPack
    {
        uint64_t titleId __attribute__((packed));
        uint32_t accessKey;
        uint32_t platformId;
        uint8_t regionId, languageId, countryId, areaId;
        uint8_t networkRestriction, friendRestriction;
        uint32_t ratingRestriction;
        uint8_t ratingOrganization;
        uint64_t transferableId;
        char tzName[72];
        uint64_t utcOffset __attribute__((packed));
        char encodedParamPack[512];
    };

    static_assert(sizeof(nn::olv::InternalParamPack) == 0x278, "sizeof(nn::olv::InternalParamPack) != 0x278");

    struct InternalClientObject
    {
        char discoveryUrl[0x80];
        uint32_t unk_0x80;
        char userAgent[0x40];
        char apiEndpoint[0x100];
        char urlFormatBuffer[0x800];
        char unk_end[685];
    } __attribute__((packed));

    static_assert(sizeof(nn::olv::InternalClientObject) == 0xC71, "nn::olv::InternalClientObject) != 0xC71");

    struct InternalConnectionObject
    {
        CURL *curl;
        curl_slist *slist;
        TCurlCallback WriteDataCallback;
        TCurlCallback ReadHeaderCallback;
        uint32_t isPostRequest;
        void *WriteData;
        uint32_t responseBufferSize;
        uint32_t *responseSize;
        void **responseBuffer;
        char CurlHeaderData;
        int responseCode;
        uint32_t readSize;
        uint32_t totalReadSize;
        nn::Result result1 = nn::Result(nn::Result::LEVEL_FATAL, nn::Result::RESULT_MODULE_COMMON, 0xfffff);
        nn::Result result2 = nn::Result(nn::Result::LEVEL_FATAL, nn::Result::RESULT_MODULE_COMMON, 0xfffff);
    };

    class InitializeParam
    {
    public:
        static const uint32_t FLAG_OFFLINE_MODE = (1 << 0);

        InitializeParam();

        nn::Result SetFlags(uint32_t flags);
        nn::Result SetWork(uint8_t *workData, uint32_t workDataSize);
        nn::Result SetReportTypes(uint32_t reportTypes);
        nn::Result SetSysArgs(void const *sysArgs, uint32_t sysArgsSize);

        uint32_t m_Flags;
        uint32_t m_ReportTypes;
        uint8_t *m_Work;
        uint32_t m_WorkSize;
        const void *m_SysArgs;
        uint32_t m_SysArgsSize;
        char unk[0x28];
    };
    static_assert(sizeof(nn::olv::InitializeParam) == 0x40, "sizeof(nn::olv::InitializeParam) != 0x40");

    class MainAppParam
    {
    public:
        MainAppParam()
        {
            this->m_AppDataSize = 0;
            this->m_CommunityId = 0;
            memset(this->m_PostId, 0, sizeof(this->m_PostId));
            memset(this->m_AppData, 0, sizeof(this->m_AppData));
        }

        bool TestFlags(uint32_t flags) const
        {
            return (this->m_Flags & flags) != 0;
        }

        uint32_t GetCommunityId() const
        {
            return this->m_CommunityId;
        }

        const char *GetPostId() const
        {
            return this->m_PostId;
        }

        nn::Result GetAppData(uint8_t *appData, uint32_t *appDataSize, uint32_t appDataMaxSize);

    private:
        uint32_t m_Flags;
        uint32_t m_CommunityId;
        char m_PostId[32];
        uint8_t m_AppData[0x400];
        uint32_t m_AppDataSize;
        char unk4[0x1000 - 0x42c];
    };

    static_assert(sizeof(nn::olv::MainAppParam) == 0x1000, "sizeof(nn::olv::MainAppParam) != 0x1000");

    struct InternalStruct
    {
        InternalStruct()
        {
            m_TitleId = 0;
            m_OfflineMode = 0;
            m_HeapHandle = 0;
            m_AllocFunc = nullptr;
            m_FreeFunc = nullptr;
            memset(&m_UsedMemorySizes, 0, sizeof(m_UsedMemorySizes));
            memset(&m_UsedWorkMaxSizes, 0, sizeof(m_UsedWorkMaxSizes));
            m_NumAccountInitialized = 0;
            memset(unk, 0, sizeof(unk));
        }

    public:
        uint64_t m_TitleId __attribute__((packed));
        uint64_t m_OfflineMode __attribute__((packed));
        MEMHeapHandle m_HeapHandle;
        TMemAllocFunction m_AllocFunc;
        TMemFreeFunction m_FreeFunc;
        uint32_t m_UsedMemorySizes[3];
        uint32_t m_UsedWorkMaxSizes[3];
        int m_NumAccountInitialized;
        char unk[0x1360c];
    };

    static_assert(sizeof(nn::olv::InternalStruct) == 0x13644, "sizeof(nn::olv::InternalStruct) != 0x13644");

}

#endif