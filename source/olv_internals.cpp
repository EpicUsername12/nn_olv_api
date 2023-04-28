#include "olv_functions.hpp"
#include <cstdio>
#include <cstdlib>
#include <whb/log.h>
#include <coreinit/memdefaultheap.h>
#include "olv_xml.hpp"

namespace nn::olv
{

    uint32_t g_LastMemoryUsage = 0;
    bool g_IsLibraryInitialized = false;
    uint32_t g_ReportTypes = 0;
    TLogFunction g_LogFunction = nullptr;
    InternalStruct g_InternalStruct;
    bool g_Unk10007886 = false;
    char g_ServiceToken[0x200];
    bool g_ServiceTokenBeingFetched = false;
    bool g_Unk1000B6F0 = false;

    MEMHeapHandle g_ZlibHeap = 0;
    uint8_t g_ZlibHeapData[0x50000];
    uint32_t dword_1005C9EC = 0;
    uint32_t dword_1005C9F0 = 0;
    uint32_t dword_1005C9F4 = 0;

    char g_Unk1000A2DC[1024];
    char g_UnkArray1000A6DC[2048];
    char g_UnkArray1005A1E8[2048];
    char g_UnkArray1000AEF0[2048];

    uint32_t dword_1000B9FC = 0xE1106380;
    char byte_1000B9F8 = 0;

    int g_Unk1000A298 = 0;
    uint8_t g_ForceCancelFlag = 0;

    InternalClientObject g_ClientObject;
    const char *g_DiscoveryProtocol = OLV_DISCOVERY_PROTOCOL;
    const char *g_DiscoveryAddress = OLV_DISCOVERY_ADDRESS;

    InternalConnectionObject g_ConnectionObject;
    NSSLContextHandle g_SslContext = 0;

    void DefaultLogFunction(const char *msg)
    {
        WHBLogPrintf("%s", msg);
        OSReport("%s", msg);
    }

    void *DefaultAllocFunction(uint32_t size)
    {
        return MEMAllocFromExpHeapEx(g_InternalStruct.m_HeapHandle, size, 64);
    }

    void DefaultFreeFunction(void *ptr)
    {
        return MEMFreeToExpHeap(g_InternalStruct.m_HeapHandle, ptr);
    }

    bool IsZlibHeapInitialized()
    {
        return g_ZlibHeap != 0;
    }

    bool Internal_InitializeZlib()
    {
        if (IsZlibHeapInitialized())
        {
            Report::Print(REPORT_TYPE_2, "ZLIB - already initialized.\n");
            return false;
        }
        else
        {
            g_ZlibHeap = MEMCreateExpHeapEx(g_ZlibHeapData, sizeof(g_ZlibHeapData), 4);
            return g_ZlibHeap != 0;
        }
    }

    void DestroyHeap()
    {
        if (g_InternalStruct.m_HeapHandle)
        {
            MEMDestroyExpHeap(g_InternalStruct.m_HeapHandle);
            g_InternalStruct.m_HeapHandle = 0;
        }
    }

    void DestroyHeap2()
    {
        if (IsZlibHeapInitialized())
        {
            dword_1005C9EC = 0;
            dword_1005C9F0 = 0;
            dword_1005C9F4 = 0;
            MEMDestroyExpHeap(g_ZlibHeap);
            g_ZlibHeap = 0;
        }
    }

    bool InitializeLogger()
    {
        g_LogFunction = DefaultLogFunction;
        g_ReportTypes = 0x1fff;
        return true;
    }

    bool InitializeInternalStruct(nn::olv::InitializeParam const *param, int isOfflineModePossible)
    {
        memset(&g_InternalStruct, 0, sizeof(g_InternalStruct)); // Actually memzero/memclr.
        bool offline_mode = true;
        if (!isOfflineModePossible)
            offline_mode = param->m_Flags & InitializeParam::FLAG_OFFLINE_MODE;

        g_InternalStruct.m_OfflineMode = offline_mode;
        IOSHandle mcpHandle = (IOSHandle)MCP_Open();
        if (mcpHandle >= 0)
        {
            uint64_t tid;
            MCPError res = MCP_GetTitleId(mcpHandle, &tid);
            g_InternalStruct.m_TitleId = tid;
            if (res)
            {
                Report::Print(REPORT_TYPE_2048, "MCP_GetTitleId FAILED. result:%d\n");
                MCP_Close(mcpHandle);
                return false;
            }
            else
            {
                MCP_Close(mcpHandle);
                g_InternalStruct.m_HeapHandle = 0;
                dword_1000B9FC = 0xE1106380;
                byte_1000B9F8 = 0;
                g_InternalStruct.m_HeapHandle = MEMCreateExpHeapEx(param->m_Work, param->m_WorkSize, 4);
                if (g_InternalStruct.m_HeapHandle)
                {
                    g_InternalStruct.m_AllocFunc = DefaultAllocFunction;
                    g_InternalStruct.m_FreeFunc = DefaultFreeFunction;
                    return true;
                }
                else
                {
                    return false;
                }
            }
        }
        else
        {
            Report::Print(REPORT_TYPE_2048, "MCP_Open FAILED. result:%d\n", mcpHandle);
        }

        return false;
    }

    uint64_t Internal_GetTitleId()
    {
        return g_InternalStruct.m_TitleId;
    }

    nn::Result InitializeAccount()
    {
        nn::Result res = nn::act::Initialize();
        if (res.IsSuccess())
            g_InternalStruct.m_NumAccountInitialized++;

        return res;
    }

    void FinalizeAccount()
    {
        if (g_InternalStruct.m_NumAccountInitialized)
        {
            for (int i = 0; i < g_InternalStruct.m_NumAccountInitialized; i++)
                nn::act::Finalize();

            g_InternalStruct.m_NumAccountInitialized = 0;
        }
    }

    nn::Result ParentalCheck(bool a1)
    {
        nn::act::SlotNo slotNo = nn::act::GetSlotNo();
        uint8_t level = 0;
        int sciRes = SCIGetParentalAccountNetworkLauncher(&level, slotNo);
        if (sciRes != 1)
        {
            Report::Print(REPORT_TYPE_2048, "SCIGetParentalAccountNetworkLauncher FAILED. sciStatus:%d\n", sciRes);
            return NN_OLV_RESULT_STATUS_CODE(0x22680);
        }

        if (!a1)
        {
            if (level == 1)
            {
                Report::Print(REPORT_TYPE_2048, "Parental Check FAILED. this account is restricted by Level 1.\n");
                return NN_OLV_RESULT_STATUS_CODE(0x1F580);
            }
            if (level != 2)
            {
                return NN_OLV_RESULT_SUCCESS;
            }
        LABEL_9:
            Report::Print(REPORT_TYPE_2048, "Parental Check FAILED. this account is restricted by Level 2.\n");
            return NN_OLV_RESULT_STATUS_CODE(0x1F500);
        }

        if (a1 == true && level == 2)
            goto LABEL_9;

        return NN_OLV_RESULT_SUCCESS;
    }

    nn::Result InitializeZlib()
    {
        if (Internal_InitializeZlib())
        {
            return NN_OLV_RESULT_SUCCESS;
        }
        else
        {
            return NN_OLV_RESULT_FATAL_CODE(0x3400);
        }
    }

    size_t Internal_CopyStr(char *dest, char *src, size_t size)
    {
        size_t result; // r3
        if (!dest || !src || !size)
            return 0;

        result = 0;
        if (size != 1)
        {
            do
            {
                if (!*(src + result))
                    break;
                *(dest + result) = *(src + result);
                ++result;
            } while (result < size - 1);
        }
        *(dest + result) = 0;
        return result;
    }

    nn::Result Internal_GetTimeZoneStr(char *str)
    {
        nn::Result res = nn::act::GetTimeZoneId(str);
        if (res.IsFailure())
        {
            int err = nn::act::GetErrorCode(res);
            Report::Print(REPORT_TYPE_2048, "nn::act::GetTimeZoneId Failed - error:%d.\n", err);
        }
        return res;
    }

    bool Internal_IsOnlineMode()
    {
        return g_InternalStruct.m_OfflineMode == 0;
    }

    static nn::olv::InternalParamPack static_ParamPack;

    nn::Result CreateParameterPack(uint64_t titleId, uint32_t accessKey, nn::olv::InternalParamPack *data)
    {

        int errCode;

        nn::olv::InternalParamPack *paramPack = data;
        if (!data)
            paramPack = &static_ParamPack;

        uint32_t language = 0xff;
        uint32_t country;

        int lang_res = SCIGetLanguage(&language);
        if (lang_res != 1)
        {
            Report::Print(REPORT_TYPE_2048, "SCIGetLanguage Failed. error:%d\n", lang_res);
            return NN_OLV_RESULT_STATUS_CODE(0x22680);
        }
        if (language == 255)
        {
            Report::Print(REPORT_TYPE_2048, "[WARNING] language id is invalid:%d\n", 255);
            return NN_OLV_RESULT_STATUS_CODE(0x22680);
        }

        paramPack->languageId = language;

        int coun_res = SCIGetCafeCountry(&country);
        if (coun_res != 1)
        {
            Report::Print(REPORT_TYPE_2048, "SCIGetCafeCountry Failed. error:%d\n", coun_res);
            return NN_OLV_RESULT_STATUS_CODE(0x22680);
        }
        if (!country)
        {
            Report::Print(REPORT_TYPE_2048, "[WARNING] country id is 0.\n");
            return NN_OLV_RESULT_STATUS_CODE(0x22680);
        }

        paramPack->countryId = country;
        paramPack->titleId = titleId;
        paramPack->platformId = 1;

        nn::act::SlotNo slotNo = nn::act::GetSlotNo();
        if (nn::act::IsNetworkAccountEx(slotNo))
        {
            nn::act::SimpleAddressId simpleAddrId;
            nn::Result res = nn::act::GetSimpleAddressId(&simpleAddrId, slotNo);
            if (res.IsFailure())
            {
                paramPack->areaId = 0;
                errCode = nn::act::GetErrorCode(res);
                Report::Print(REPORT_TYPE_2048, "nn::act::GetSimpleAddressIdEx Failed. errorCode:%d\n", errCode);
                return res;
            }
            if (!simpleAddrId)
            {
                Report::Print(REPORT_TYPE_2048, "[WARNING] area id is 0.\n");
                return NN_OLV_RESULT_STATUS_CODE(0x22680);
            }

            paramPack->areaId = (simpleAddrId >> 8) & 0xff;
        }
        else
        {
            Report::Print(REPORT_TYPE_4096, "[WARNING] no %d account is not network account! set area id to 0.\n", slotNo);
            paramPack->areaId = 0;
        }

        uint32_t prodArea;
        int prod_res = SCIGetSystemProdArea(&prodArea);
        if (prod_res != 1)
        {
            Report::Print(REPORT_TYPE_2048, "SCIGetSystemProdArea Failed. error:%d\n", prod_res);
            return NN_OLV_RESULT_STATUS_CODE(0x22680);
        }

        if (prodArea)
        {
            paramPack->regionId = prodArea;
            paramPack->accessKey = accessKey;

            uint8_t level = 0;
            int sci_res = SCIGetParentalAccountNetworkLauncher(&level, slotNo);
            if (sci_res == 1)
            {
                paramPack->networkRestriction = level;
                uint8_t v30;
                int v14 = SCIGetParentalAccountFriendReg(&v30, slotNo);
                if (v14 == 1)
                {
                    uint32_t v34;
                    paramPack->friendRestriction = v30 != 0;
                    int v15 = SCIGetParentalAccountGameRating(&v34, slotNo);
                    if (v15 == 1)
                    {
                        uint32_t v35;
                        paramPack->ratingRestriction = v34;
                        int v16 = SCIGetParentalRatingOrganization(&v35);
                        if (v16 == 1)
                        {
                            paramPack->ratingOrganization = v35;
                            uint32_t unique_id = (titleId >> 8) & 0xFFFFF;
                            nn::act::TransferrableId transferrableId;
                            nn::Result transfRes = nn::act::GetTransferableIdEx(&transferrableId, unique_id, slotNo);
                            if (transfRes.IsSuccess())
                            {
                                nn::Result tzRes = Internal_GetTimeZoneStr(paramPack->tzName);
                                if (tzRes >= 0)
                                {
                                    uint64_t utc_offset = 7200;
                                    paramPack->utcOffset = utc_offset; // TODO:

                                    char paramPackStr[1024];
                                    char paramPackStrEncoded[2048];
                                    snprintf(
                                        paramPackStr,
                                        sizeof(paramPackStr),
                                        "\\%s\\%llu\\%s\\%u\\%s\\%u\\%s\\%d\\%s\\%d\\%s\\%d\\%s\\%d\\%s\\%d\\%s\\%d\\%s\\%u\\%s\\%d\\%s\\%llu\\"
                                        "%s\\%s\\%s\\%lld\\",
                                        "title_id",
                                        paramPack->titleId,
                                        "access_key",
                                        paramPack->accessKey,
                                        "platform_id",
                                        paramPack->platformId,
                                        "region_id",
                                        paramPack->regionId,
                                        "language_id",
                                        paramPack->languageId,
                                        "country_id",
                                        paramPack->countryId,
                                        "area_id",
                                        paramPack->areaId,
                                        "network_restriction",
                                        paramPack->networkRestriction,
                                        "friend_restriction",
                                        paramPack->friendRestriction,
                                        "rating_restriction",
                                        paramPack->ratingRestriction,
                                        "rating_organization",
                                        paramPack->ratingOrganization,
                                        "transferable_id",
                                        paramPack->transferableId,
                                        "tz_name",
                                        paramPack->tzName,
                                        "utc_offset",
                                        paramPack->utcOffset);

                                    int encodedSize = 0;
                                    size_t len = strnlen(paramPackStr, 1024);
                                    Base64Encode(paramPackStrEncoded, 2048, (uint8_t *)paramPackStr, len, &encodedSize);
                                    for (int i = 0; i < encodedSize; i++)
                                    {
                                        if (paramPackStrEncoded[i] == '\n')
                                            paramPackStrEncoded[i] = ' ';
                                    }

                                    Internal_CopyStr(paramPack->encodedParamPack, paramPackStrEncoded, sizeof(paramPack->encodedParamPack));
                                    return NN_OLV_RESULT_SUCCESS;
                                }
                                else
                                {
                                    return tzRes;
                                }
                            }
                            else
                            {
                                errCode = nn::act::GetErrorCode(transfRes);
                                nn::olv::Report::Print(REPORT_TYPE_2048, "nn::act::GetTransferableIdEx Failed. errorCode:%d.\n", errCode);
                                return transfRes;
                            }
                        }
                        else
                        {
                            Report::Print(REPORT_TYPE_2048, "SCIGetParentalRatingOrganization Failed. error:%d\n", v16);
                            return NN_OLV_RESULT_STATUS_CODE(0x22680);
                        }
                    }
                    else
                    {
                        Report::Print(REPORT_TYPE_2048, "SCIGetParentalAccountGameRating Failed. error:%d\n", v15);
                        return NN_OLV_RESULT_STATUS_CODE(0x22680);
                    }
                }
                else
                {
                    Report::Print(REPORT_TYPE_2048, "SCIGetParentalAccountFriendReg Failed. error:%d\n", v14);
                    return NN_OLV_RESULT_STATUS_CODE(0x22680);
                }
            }
            else
            {
                Report::Print(REPORT_TYPE_2048, "SCIGetParentalAccountNetworkLauncher Failed. error:%d\n", sci_res);
                return NN_OLV_RESULT_STATUS_CODE(0x22680);
            }
        }
        else
        {
            Report::Print(REPORT_TYPE_2048, "[WARNING] region id is 0.\n");
            return NN_OLV_RESULT_STATUS_CODE(0x22680);
        }
    }

    bool Internal_InitClientObject()
    {
        strncpy(g_ClientObject.discoveryUrl, g_DiscoveryProtocol, sizeof(g_ClientObject.discoveryUrl));
        strncat(g_ClientObject.discoveryUrl, g_DiscoveryAddress, sizeof(g_ClientObject.discoveryUrl) - 1);

        int consoleType = OSGetConsoleType();

        char buffer[16]; // In the original this 64 bytes, but since it's never that long
        memset(buffer, 0, sizeof(buffer));

        const char *OlvType = "DOLV";
        if ((consoleType & 0xF0000000) == 0)
            OlvType = "POLV";

        strncpy(buffer, OlvType, sizeof(buffer));

        char version_buffer[16];
        memset(version_buffer, 0, sizeof(version_buffer));
        snprintf(version_buffer, sizeof(version_buffer), "%d.%d.%d", OLV_VERSION_MAJOR, OLV_VERSION_MINOR, OLV_VERSION_PATCH);

        uint64_t titleId = Internal_GetTitleId();
        uint32_t unique_id = (titleId >> 8) & 0xfffff;

        snprintf(g_ClientObject.userAgent, sizeof(g_ClientObject.userAgent) - 1, "%s/%s-%s/%d", "WiiU", buffer, version_buffer, unique_id);
        return true;
    }

    void Internal_ClearClientObject()
    {
        memset(&g_ClientObject, 0, sizeof(g_ClientObject));
        // TODO: memset(dword_10007A8C, 0, 0xA03);
    }

    bool Internal_InitXMLObject()
    {
        return true;
    }

    void Internal_XOR_OfflineMode(uint32_t val)
    {
        g_InternalStruct.m_OfflineMode = val ^ 1;
    }

    nn::Result Internal_CreateServiceToken(bool a1)
    {
        g_ServiceTokenBeingFetched = true;
        if (a1)
        {
            nn::Result res = nn::act::AcquireIndependentServiceToken(g_ServiceToken, OLV_CLIENT_ID);
            if (res.IsFailure())
            {
                int err = nn::act::GetErrorCode(res);
                Report::Print(REPORT_TYPE_2, "Service token create FAILED -ErrorCode:%d\n", err);
            }
            g_ServiceTokenBeingFetched = false;
            return res;
        }
        else
        {
            nn::Result res = nn::act::AcquireIndependentServiceToken(g_ServiceToken, OLV_CLIENT_ID, 43200, false, true);
            if (res.IsFailure())
            {
                int err = nn::act::GetErrorCode(res);
                Report::Print(REPORT_TYPE_2, "Service token create FAILED -ErrorCode:%d\n", err);
            }
            g_ServiceTokenBeingFetched = false;
            return res;
        }
    }

    size_t Internal_CurlWriteDataCb(char *ptr, uint32_t size, uint32_t nmemb, void *pUserdata)
    {
        InternalCURLWriteData *userdata = (InternalCURLWriteData *)pUserdata;

        char *writeDataBuffer;   // r12
        int writeSize_;          // r31
        uint32_t currentOutSize; // r7
        int enableSize;          // r6
        uint32_t writeSize;      // r5

        writeDataBuffer = (char *)userdata->WriteData;
        if (!userdata->WriteData)
        {
            writeDataBuffer = (char *)*userdata->responseBuffer;
            if (!writeDataBuffer)
                return 0;
        }
        writeSize_ = size * nmemb;
        if ((int)(size * nmemb) <= 0)
            return writeSize_;
        currentOutSize = *userdata->outResponseSize;
        enableSize = userdata->responseBufferSize - currentOutSize;
        writeSize = size * nmemb;
        if (enableSize >= writeSize_)
        {
            memcpy(&writeDataBuffer[currentOutSize], ptr, writeSize);
            *userdata->outResponseSize += writeSize_;
            return writeSize_;
        }
        nn::olv::Report::Print(REPORT_TYPE_2048, "not enough response buffer size - writeSize:%d enableSize:%d\n", writeSize, enableSize);
        return 0;
    }

    void Internal_ForceFree(void *ptr)
    {
        if (ptr)
            MEMFreeToDefaultHeap(((uint32_t *)ptr - 2));
    }

    void Internal_Free(void *ptr, HeapId id)
    {
        uint32_t *heapEntryHeader = ((uint32_t *)ptr - 2);
        g_InternalStruct.m_UsedMemorySizes[id] -= heapEntryHeader[0] + 8;
        if (heapEntryHeader[1] != 0xDEADC0DE)
        {
            Report::Print(REPORT_TYPE_2048, "memory corruption detected (magic word is broken: addr is 0x%x)\n", ptr);
            OSPanic("private/olv_Apii.cpp", 2178, "OLV force stop!");
        }

        return g_InternalStruct.m_FreeFunc(heapEntryHeader);
    }

    void *Internal_Alloc(size_t size, HeapId id)
    {
        size_t newSize = size + 8;
        uint32_t *currentUsed = &g_InternalStruct.m_UsedMemorySizes[id];
        uint32_t *currentMax = &g_InternalStruct.m_UsedWorkMaxSizes[id];

        void *ptr = g_InternalStruct.m_AllocFunc(newSize);
        if (ptr)
        {
            memset(ptr, 0, newSize);
            uint32_t v10 = *currentUsed + newSize;
            *currentUsed = v10;
            if (*currentMax < v10)
                *currentMax = v10;

            *((uint32_t *)ptr + 0) = size;
            *((uint32_t *)ptr + 1) = 0xDEADC0DE;

            Report::Print(REPORT_TYPE_128, "%d alloc: %d\n", id, size);
            return (void *)((uint32_t *)ptr + 2);
        }
        else
        {
            Report::Print(REPORT_TYPE_2048, "memory allocation failed (need size:%d)\n", size);
        }
        return nullptr;
    }

    size_t Internal_CurlReadHeaderCb(char *ptr, uint32_t size, uint32_t nmemb, void *p_userdata)
    {
        int v5;           // r31
        int v6;           // r3
        unsigned int v10; // r5
        void *v11;        // r3
        uint32_t v12;     // r5
        int v13;          // r5
        unsigned int v15; // r6
        int v16;          // r5

        InternalCURLHeaderData *userdata = (InternalCURLHeaderData *)p_userdata;

        v5 = size * nmemb;
        memcpy(g_Unk1000A2DC, ptr, size * nmemb);

        g_Unk1000A2DC[v5] = 0;
        v6 = strnlen("Content-Length: ", 17);
        if (strncmp(g_Unk1000A2DC, "Content-Length: ", v6) != 0)
            return v5;

        long contentLength = strtol(&g_Unk1000A2DC[v6], nullptr, 10);

        v10 = contentLength;
        userdata->totalReadSize = v10;
        nn::olv::Report::Print(REPORT_TYPE_4, "content size: %d\n", v10);

        if (g_ConnectionObject.curlData.write.responseBuffer)
        {
            v11 = Internal_Alloc(v10 + 1, HEAP_MAIN);
            v12 = userdata->totalReadSize;
            *g_ConnectionObject.curlData.write.responseBuffer = v11;

            if (!v11)
            {
                nn::olv::Report::Print(REPORT_TYPE_32, "content size > allocatable size. content size:%d\n", v12);
                v13 = userdata->totalReadSize;
                if (v13)
                    userdata->totalReadSize = v13 + 0x2000;
                userdata->result1 = NN_OLV_RESULT_FATAL_CODE(0x3280);
                return 0;
            }

            g_ConnectionObject.curlData.write.responseBufferSize = v12;
            return v5;
        }
        v15 = userdata->readSize;
        if (v15 >= v10)
            return v5;
        nn::olv::Report::Print(REPORT_TYPE_32, "content size > buffer size. content size:%d  buffer size:%d\n", v10, v15);

        v16 = userdata->totalReadSize;
        if (v16)
            userdata->totalReadSize = v16 + 0x2000;
        userdata->result1 = NN_OLV_RESULT_FATAL_CODE(0x3280);
        return 0;
    }

    void Internal_DestroySSLContext()
    {
        if (g_SslContext != -1)
        {
            NSSLDestroyContext(g_SslContext);
            g_SslContext = -1;
        }
    }

    char *additionalUrlParams = nullptr;
    char g_TempUrlArray[0x200];
    void Internal_InitConnectionObject()
    {
        g_ConnectionObject.curl = nullptr;
        g_ConnectionObject.slist = 0;
        g_ConnectionObject.isPostRequest = 0;
        g_ConnectionObject.WriteDataCallback = Internal_CurlWriteDataCb;
        g_ConnectionObject.ReadHeaderCallback = Internal_CurlReadHeaderCb;
        g_ConnectionObject.curlData.write.WriteData = 0;
        g_ConnectionObject.curlData.write.outResponseSize = 0;
        memset(g_TempUrlArray, 0, 0x200u);
        additionalUrlParams = 0;
        g_SslContext = -1;
        g_Unk1000B6F0 = false;
    }

    nn::Result Internal_ConnectionObjectInit()
    {
        if (g_Unk1000B6F0)
            return NN_OLV_RESULT_USAGE_CODE(0x6700);

        Internal_InitConnectionObject();

        NSSLContextHandle v1 = g_SslContext;
        if (g_SslContext >= 0 || (v1 = NSSLCreateContext(0), g_SslContext = v1, v1 >= 0))
        {
            NSSLError v2 = NSSLSetClientPKI(v1, 7);
            if (v2)
            {
                Report::Print(REPORT_TYPE_2, "ERROR: NSSLSetClientPKI failed. errorCode:%d\n", v2);
                Internal_DestroySSLContext();
                return NN_OLV_RESULT_USAGE_CODE(0x3380);
            }
            else
            {
                NSSLError v3 = NSSLAddServerPKI(g_SslContext, NSSLServerCertId::NSSL_SERVER_CERT_NINTENDO_CLASS2_CA_G3);
                if (v3)
                {
                    Report::Print(REPORT_TYPE_2, "ERROR: NSSLAddServerPKI failed (NINTENDO_CLASS2_CA_G3). errorCode:%d\n", v3);
                    Internal_DestroySSLContext();
                    return NN_OLV_RESULT_USAGE_CODE(0x3380);
                }
                else
                {
                    int v6, v5;
                    int v4 = NSSLAddServerPKIGroups(g_SslContext, 3, &v6, &v5);
                    if (v4)
                    {
                        Report::Print(REPORT_TYPE_2, "ERROR: NSSLAddServerPKIGroups failed. errorCode:%d\n", v4);
                        Internal_DestroySSLContext();
                        return NN_OLV_RESULT_USAGE_CODE(0x3380);
                    }
                    else if (v5 <= 0)
                    {
                        g_Unk1000B6F0 = true;
                        return NN_OLV_RESULT_SUCCESS;
                    }
                    else
                    {
                        Report::Print(REPORT_TYPE_2, "ERROR: NSSLAddServerPKIGroups error occured. error Count:%d\n", v5);
                        Internal_DestroySSLContext();
                        return NN_OLV_RESULT_USAGE_CODE(0x3380);
                    }
                }
            }
        }
        else
        {
            Report::Print(REPORT_TYPE_2, "ERROR: NSSLCreateContext failed. code:%d\n", v1);
            return NN_OLV_RESULT_USAGE_CODE(0x3380);
        }
    }

    void sub_2006198(int code, nn::Result *outResult)
    {
        if (code)
        {
            if (code >= 400)
                *(uint32_t *)outResult = (((code << 7) - 0x5EE83000) & 0xFFFFF) | 0xA1100000;
            else
                *(uint32_t *)outResult = 0x1100080;
        }
        else
        {
            *(uint32_t *)outResult = 0xA113E980;
        }
    }

    void Internal_SetupCURLWriteData(void *writeData, size_t responseBufferSize, size_t *responseSize)
    {
        g_ConnectionObject.curlData.write.WriteData = writeData;
        g_ConnectionObject.curlData.write.responseBufferSize = responseBufferSize;
        g_ConnectionObject.curlData.write.outResponseSize = responseSize;
    }

    void Internal_AppendSList(const char *header)
    {
        g_ConnectionObject.slist = curl_slist_append(g_ConnectionObject.slist, header);
    }

    void Internal_SetupHeaders()
    {
        char headerBuffer[1024];

        const char *serviceToken = g_ServiceToken;
        snprintf(headerBuffer, sizeof(headerBuffer), "X-Nintendo-ServiceToken: %s", serviceToken);
        Internal_AppendSList(headerBuffer);

        memset(headerBuffer, 0, sizeof(headerBuffer));

        const char *paramPack = static_ParamPack.encodedParamPack;
        snprintf(headerBuffer, sizeof(headerBuffer), "X-Nintendo-ParamPack: %s", paramPack);
        Internal_AppendSList(headerBuffer);
    }

    curl_httppost *g_CurlHttpPost = nullptr;

    nn::Result Internal_DoRequest()
    {
        if (g_ConnectionObject.curlData.write.WriteData)
        {
            if (!g_ConnectionObject.curlData.write.responseBufferSize)
            {
                nn::olv::Report::Print(REPORT_TYPE_2048, "Connect failed. responseBufferSize is 0.\n");
                return NN_OLV_RESULT_USAGE_CODE(0x3400);
            }
        }
        else if (!g_ConnectionObject.curlData.write.responseBuffer)
        {
            nn::olv::Report::Print(REPORT_TYPE_2048, "Connect failed. responseBuffer not set.\n");
            return NN_OLV_RESULT_USAGE_CODE(0x3400);
        }
        if (!g_ConnectionObject.curlData.write.outResponseSize)
        {
            nn::olv::Report::Print(REPORT_TYPE_2048, "Connect failed. responseSize not set.\n");
            return NN_OLV_RESULT_USAGE_CODE(0x3400);
        }

        memset(&g_ConnectionObject.curlData.header.CurlHeaderData, 0, 6 * 4);
        g_ConnectionObject.curlData.header.readSize = g_ConnectionObject.curlData.write.responseBufferSize;
        g_ConnectionObject.curlData.header.result1 = NN_OLV_RESULT_SUCCESS;

        Internal_CopyStr(g_UnkArray1000AEF0, g_UnkArray1000A6DC, 2048);
        if (g_ConnectionObject.curlData.write.outResponseSize)
            *g_ConnectionObject.curlData.write.outResponseSize = 0;

        char userAgent[64];
        nn::olv::GetUserAgent(userAgent, 64);

        g_ConnectionObject.curl = curl_easy_init();
        curl_easy_setopt(g_ConnectionObject.curl, CURLOPT_FOLLOWLOCATION, 1);
        curl_easy_setopt(g_ConnectionObject.curl, CURLOPT_WRITEFUNCTION, g_ConnectionObject.WriteDataCallback);
        curl_easy_setopt(g_ConnectionObject.curl, CURLOPT_WRITEDATA, &g_ConnectionObject.curlData.write);
        curl_easy_setopt(g_ConnectionObject.curl, CURLOPT_USERAGENT, userAgent);
        curl_easy_setopt(g_ConnectionObject.curl, CURLOPT_HEADERFUNCTION, g_ConnectionObject.ReadHeaderCallback);
        curl_easy_setopt(g_ConnectionObject.curl, CURLOPT_HEADERDATA, &g_ConnectionObject.curlData.header);
        curl_easy_setopt(g_ConnectionObject.curl, CURLOPT_TIMEOUT, 300);
        curl_easy_setopt(g_ConnectionObject.curl, CURLOPT_CONNECTTIMEOUT, 30);
        curl_easy_setopt(g_ConnectionObject.curl, CURLOPT_LOW_SPEED_LIMIT, 1);
        curl_easy_setopt(g_ConnectionObject.curl, CURLOPT_LOW_SPEED_TIME, 60);
        // Nintendo SSL
        curl_easy_setopt(g_ConnectionObject.curl, (CURLoption)210, g_SslContext);
        curl_easy_setopt(g_ConnectionObject.curl, (CURLoption)211, 3);

        if (g_ConnectionObject.isPostRequest == 1)
        {
            curl_easy_setopt(g_ConnectionObject.curl, CURLOPT_POST, 1);
            curl_easy_setopt(g_ConnectionObject.curl, CURLOPT_HTTPPOST, g_CurlHttpPost);
            g_ConnectionObject.slist = curl_slist_append(g_ConnectionObject.slist, "Expect:");
            curl_easy_setopt(g_ConnectionObject.curl, CURLOPT_URL, g_UnkArray1000AEF0);
        }
        else
        {
            if (additionalUrlParams)
                snprintf(g_UnkArray1000AEF0, sizeof(g_UnkArray1000AEF0), "%s?%s", g_UnkArray1000A6DC, additionalUrlParams);
        }

        curl_easy_setopt(g_ConnectionObject.curl, CURLOPT_URL, g_UnkArray1000AEF0);
        if (g_ConnectionObject.slist)
            curl_easy_setopt(g_ConnectionObject.curl, CURLOPT_HTTPHEADER, g_ConnectionObject.slist);

        g_Unk1000A298 = 0;
        g_ForceCancelFlag = 0;

        CURLcode v7 = CURLcode::CURLE_OK;
        int res = 0;
        CURLMsg *msg = nullptr;
        curl_slist *slist = g_ConnectionObject.slist;
        CURLM *curlm = curl_multi_init();
        if (curlm)
        {
            if (curl_multi_add_handle(curlm, g_ConnectionObject.curl))
            {
                g_ConnectionObject.curlData.header.result1 = NN_OLV_RESULT_FATAL_CODE(0x3280);
                goto LABEL_53;
            }
            curl_multi_setopt(curlm, CURLMOPT_MAXCONNECTS, 4);
            while (!g_ForceCancelFlag)
            {
                if (curl_multi_perform(curlm, &g_Unk1000A298) != -1)
                {
                    if (g_ForceCancelFlag || !g_Unk1000A298)
                        break;

                    nsysnet_fd_set read_fd_set, write_fd_set, exc_fd_set;
                    NSYSNET_FD_ZERO(&read_fd_set);
                    NSYSNET_FD_ZERO(&write_fd_set);
                    NSYSNET_FD_ZERO(&exc_fd_set);
                    int max_fd;

                    nsysnet_timeval tv = {.tv_sec = 1, .tv_usec = 0};
                    curl_multi_fdset(
                        curlm,
                        (fd_set *)&read_fd_set,
                        (fd_set *)&write_fd_set,
                        (fd_set *)&exc_fd_set,
                        &max_fd);
                    res = __rplwrap_select(max_fd + 1, &read_fd_set, &write_fd_set, &exc_fd_set, &tv);
                    if (res == -1)
                    {
                        g_Unk1000A298 = 0;
                        msg = curl_multi_info_read(curlm, &res);
                        if (!msg)
                            goto LABEL_39;

                        goto LABEL_37;
                    }
                    while (!g_ForceCancelFlag && curl_multi_perform(curlm, &g_Unk1000A298) == -1)
                        ;
                    if (!g_Unk1000A298)
                        break;
                }
            }
            msg = curl_multi_info_read(curlm, &res);
            if (!msg)
                goto LABEL_39;

        LABEL_37:
            v7 = msg->data.result;
            if (v7)
                nn::olv::Report::Print(REPORT_TYPE_4, "result:%d\n", v7);
        LABEL_39:
            if (g_ForceCancelFlag)
            {
                g_ConnectionObject.curlData.header.result1 = NN_OLV_RESULT_STATUS_CODE(0x9680);
                Report::Print(REPORT_TYPE_32, "connection cancel.\n");
                goto LABEL_53;
            }
            if (v7)
            {
                Report::Print(REPORT_TYPE_32, "ERROR: Error fetching URL: %s reason: %s\n", g_UnkArray1000AEF0, curl_easy_strerror(v7));
                g_ConnectionObject.curlData.header.result1 = nn::Result(
                    nn::Result::LEVEL_STATUS,
                    nn::Result::RESULT_MODULE_NN_OLV,
                    ((v7 << 7) - 0x5EEA2400));
            }
            else if (g_ConnectionObject.curlData.write.WriteData && g_ConnectionObject.curlData.header.totalReadSize < g_ConnectionObject.curlData.header.readSize)
            {
                *(char *)((uint32_t)g_ConnectionObject.curlData.write.WriteData + g_ConnectionObject.curlData.header.totalReadSize) = 0;
            }

            long ssl_verifyresult;
            curl_easy_getinfo(g_ConnectionObject.curl, CURLINFO_SSL_VERIFYRESULT, &ssl_verifyresult);
            if (ssl_verifyresult)
                Report::Print(REPORT_TYPE_2048, "ssl_verifyresult:%d\n", ssl_verifyresult);

            curl_easy_getinfo(g_ConnectionObject.curl, CURLINFO_RESPONSE_CODE, &g_ConnectionObject.curlData.header.responseCode);
            if (g_ConnectionObject.curlData.header.responseCode != 200)
                nn::olv::Report::Print(REPORT_TYPE_2048, "response code: %d\n", g_ConnectionObject.curlData.header.responseCode);

            sub_2006198(g_ConnectionObject.curlData.header.responseCode, &g_ConnectionObject.curlData.header.result2);
            nn::olv::Error::SetServerResponseCode(g_ConnectionObject.curlData.header.responseCode);
            goto LABEL_53;
        }

        g_ConnectionObject.curlData.header.result1 = NN_OLV_RESULT_STATUS_CODE(0x3280);

    LABEL_53:
        if (slist)
        {
            curl_slist_free_all(slist);
            g_ConnectionObject.slist = nullptr;
        }

        curl_multi_remove_handle(curlm, g_ConnectionObject.curl);
        curl_multi_cleanup(curlm);
        curl_easy_cleanup(g_ConnectionObject.curl);
        g_ConnectionObject.isPostRequest = 0;

        g_ConnectionObject.curlData.write.WriteData = 0;
        g_ConnectionObject.curlData.write.responseBufferSize = 0;
        g_ConnectionObject.curlData.write.responseBuffer = 0;
        g_ConnectionObject.curlData.write.outResponseSize = 0;

        additionalUrlParams = 0;
        return g_ConnectionObject.curlData.header.result1;
    }

    void Internal_CopyCURLHeaderData(InternalCURLHeaderData *out)
    {
        memcpy(out, &g_ConnectionObject.curlData.header, sizeof(InternalCURLHeaderData));
    }

    nn::Result Internal_GetErrorFromHeaderData()
    {
        InternalCURLHeaderData header;
        Internal_CopyCURLHeaderData(&header);
        return header.result2;
    }

    unsigned int Internal_StringCopy(char *dest, char *src, unsigned int srcLen, unsigned int destLen)
    {
        unsigned int result; // r3
        unsigned int i;      // r12

        if (!dest || !src || !destLen)
            return 0;
        if (srcLen)
        {
            if (srcLen > destLen)
                srcLen = destLen - 1;
            for (i = 0; i < srcLen; ++i)
                dest[i] = src[i];
            dest[i] = 0;
            result = i;
        }
        else
        {
            *dest = 0;
            result = 0;
        }
        return result;
    }

    char *Internal_CopyToLocalBuffer(char *content, uint32_t length)
    {
        static char s_LocalBuffer[0x40];

        if (length >= 0x40)
            return nullptr;

        strncpy(s_LocalBuffer, content, length);
        s_LocalBuffer[length] = '\0';

        return s_LocalBuffer;
    }

    int Internal_ToInteger(char *content, uint32_t length)
    {
        char *v2;   // r3
        int result; // r3
        if (content && (v2 = Internal_CopyToLocalBuffer(content, length)) != 0)
            result = strtol(v2, 0, 10);
        else
            result = 0;
        return result;
    }

    void Internal_WriteOliveError(int code, uint32_t *outResult)
    {
        if (code)
        {
            if (code >= 400)
                *outResult = (((code << 7) - 0x5EE83000) & 0xFFFFF) | 0xA1100000;
            else
                *outResult = 0x1100080;
        }
        else
        {
            *outResult = 0xA113E980;
        }
    }

    bool CalenderStringCheck(char *calendarString)
    {
        static char CALENDAR_STRING_VALIDATOR[] = {0, 0, 0, 0, 1, 0, 0, 1, 0, 0, 2, 0, 0, 3, 0, 0, 3, 0, 0, 0};

        char *puVar2;
        char *puVar3;
        int iVar4;
        char uStack_1c[4];
        char local_18[20];
        uint8_t bVar1;

        puVar2 = uStack_1c;
        iVar4 = 5;
        puVar3 = CALENDAR_STRING_VALIDATOR;
        do
        {
            puVar3 = (char *)((int)puVar3 + 4);
            puVar2 = puVar2 + 4;
            *(uint32_t *)puVar2 = *(uint32_t *)puVar3;
            iVar4 = iVar4 + -1;
        } while (iVar4 != 0);
        iVar4 = strnlen(calendarString, 0x14);
        if (iVar4 != 0x13)
        {
            nn::olv::Report::Print(REPORT_TYPE_2048, "CalenderStringCheck FAILED Illegal Length -%d\n", iVar4);
            return 0;
        }
        iVar4 = 0;
        do
        {
            bVar1 = local_18[iVar4];
            if (bVar1 == 0)
            {
                if (9 < *calendarString - '0')
                    goto LAB_0201f618;
            }
            else if (bVar1 == 1)
            {
                if (*calendarString != '-')
                    goto LAB_0201f618;
            }
            else if (bVar1 < 3)
            {
                if (*calendarString != ' ')
                    goto LAB_0201f618;
            }
            else
            {
                if (bVar1 != 3)
                {
                    nn::olv::Report::Print(REPORT_TYPE_2048, "CalenderStringCheck FAILED. Illegal Character -[%c]\n", *calendarString);
                    return 0;
                }
                if (*calendarString != ':')
                {
                LAB_0201f618:
                    nn::olv::Report::Print(REPORT_TYPE_2048, "CalenderStringCheck FAILED. Illegal Character -[%c]\n", *calendarString);
                    return 0;
                }
            }
            iVar4 = iVar4 + 1;
            calendarString++;
            if (0x12 < iVar4)
            {
                return 1;
            }
        } while (true);
    }

    bool TransformDateStringToCalender(char *dateString, OSCalendarTime *outCalendar)
    {
        if (!dateString || !outCalendar || !CalenderStringCheck(dateString))
            return false;

        memset(outCalendar, 0, sizeof(OSCalendarTime));

        return true;
    }

    void Internal_ParseOliveErrorEach(char *tagName, char *tagContent, uint32_t tagContentLen, nn::Result *error)
    {

        WHBLogPrintf("%.*s -> %.*s", xml::GetTagNameLength(), tagName, tagContentLen, tagContent);

        if (xml::CompareAgainstTagName(tagName, "has_error"))
        {
            int has_error = Internal_ToInteger(tagContent, tagContentLen);
            if (has_error == 1)
            {
                if (xml::SelectNodeByName("code"))
                {
                    char *codeContent = xml::GetTagContent();
                    uint32_t codeContentLen = xml::GetTagContentLength();
                    int code = Internal_ToInteger(codeContent, codeContentLen);
                    Internal_WriteOliveError(code, (uint32_t *)error);
                }
                if (xml::SelectNodeByName("error_code"))
                {
                    char *codeContent = xml::GetTagContent();
                    uint32_t codeContentLen = xml::GetTagContentLength();
                    int code = Internal_ToInteger(codeContent, codeContentLen);
                    *(uint32_t *)error = (((code << 7) - 0x5EE63C00) & 0xFFFFF) | 0xA1100000;
                }
            }
        }
        else if (xml::CompareAgainstTagName(tagName, "version"))
        {
            int version = Internal_ToInteger(tagContent, tagContentLen);
            if (version != strtol("1", nullptr, 10))
            {
                *(uint32_t *)error = 0xA113E880;
            }
        }
        else if (xml::CompareAgainstTagName(tagName, "expire"))
        {
            char buffer[256];
            Internal_StringCopy(buffer, tagContent, tagContentLen, sizeof(buffer));

            OSCalendarTime expiryCalendarTime;
            if (TransformDateStringToCalender(buffer, &expiryCalendarTime))
            {
                ACPNetworkTime acpNetworkTime;
                uint8_t unk[4];
                ACPResult acpStatus = ACPGetNetworkTime(&acpNetworkTime, unk);
                if (acpStatus)
                {
                    nn::olv::Report::Print(REPORT_TYPE_2048, "ACPGetNetworkTime FAILED. acpStatus:%d\n", acpStatus);
                    *(uint32_t *)error = 0xA1122700;
                }
                else
                {
                    OSCalendarTime networkCalendarTime;
                    ACPConvertNetworkTimeToOSCalendarTime(acpNetworkTime, &networkCalendarTime);
                    if (OSCalendarTimeToTicks(&networkCalendarTime) > OSCalendarTimeToTicks(&expiryCalendarTime))
                    {
                        nn::olv::Report::Print(REPORT_TYPE_2048, "EXPIRE PATHED!\n");
                        *(uint32_t *)error = 0xA113E900;
                    }
                }
            }
            else
            {
                nn::olv::Report::Print(REPORT_TYPE_2048, "TransformDateStringToCalender FAILED [%s]\n", tagContent);
                *(uint32_t *)error = 0xC1106500;
            }
        }
        else if (xml::CompareAgainstTagName(tagName, "request_language_id"))
        {
            int request_language_id = Internal_ToInteger(tagContent, tagContentLen);
            if (request_language_id != static_ParamPack.languageId)
            {
                nn::olv::Report::Print(REPORT_TYPE_2048, "XML Illegal Language Id!!\n");
                *(uint32_t *)error = 0xA113EC80;
            }
        }
    }

    int Internal_ParseOliveError(int unk, xml::XMLNode *node, void *userptr)
    {

        nn::Result *userResult = (nn::Result *)userptr;
        char *tagName = xml::GetTagName();
        char *tagContent = xml::GetTagContent();
        uint32_t tagContentLen = xml::GetTagContentLength();

        Internal_ParseOliveErrorEach(tagName, tagContent, tagContentLen, userResult);

        int result = 0;
        if (*userResult != NN_OLV_RESULT_SUCCESS)
            result = 3;

        return result;
    }

    bool Internal_CheckResponse(nn::Result &outResult, void *responseBuffer, uint32_t responseSize)
    {
        if (nn::olv::Error::GetServerResponseCode() == 200)
            return true;

        InternalCURLHeaderData header;
        Internal_CopyCURLHeaderData(&header);

        int res;
        if (!header.totalReadSize)
            goto LABEL_8;

        res = xml::ParseXML((char *)responseBuffer, responseSize, Internal_ParseOliveError, &outResult);
        if (res == 1)
            outResult = NN_OLV_RESULT_STATUS_CODE(0x3EA00);
        else if (res == 2)
            outResult = NN_OLV_RESULT_FATAL_CODE(0x3280);

        if (outResult.IsSuccess())
        {
        LABEL_8:
            outResult = Internal_GetErrorFromHeaderData();
        }

        return false;
    }

    nn::Result Internal_SearchHomeRegion(bool a1)
    {
        if (!a1 && strnlen(g_UnkArray1005A1E8, 2048))
            return nn::Result(nn::Result::LEVEL_SUCCESS, nn::Result::RESULT_MODULE_COMMON, 0);

        void *responseBuffer = (void *)Internal_Alloc(0x2800, HEAP_MAIN);
        if (!responseBuffer)
            return NN_OLV_RESULT_USAGE_CODE(0x3280);

        memset(responseBuffer, 0, 0x2800);
        strncpy(g_UnkArray1000A6DC, g_ClientObject.discoveryUrl, 2048);

        size_t resSize = 0;
        Internal_SetupCURLWriteData(responseBuffer, 0x2800, &resSize);
        Internal_SetupHeaders();
        nn::Result result = Internal_DoRequest();
        if (result.IsFailure())
        {
            int err = nn::olv::GetErrorCode(result);
            nn::olv::Report::Print(REPORT_TYPE_2, "Search Home Region FAILED. connection failed: errorCode:%d\n", err);
        }
        else if (Internal_CheckResponse(result, responseBuffer, resSize) && result.IsSuccess())
        {
        }

        Internal_Free(responseBuffer, HEAP_MAIN);
        return result;
    }

    nn::Result Internal_InitConnect(bool a1)
    {
        if (!nn::olv::IsInitialized())
            return NN_OLV_RESULT_STATUS_CODE(0x6680);

        if (Internal_IsOnlineMode())
            return NN_OLV_RESULT_USAGE_CODE(0x6700);

        nn::Result serviceTokenStatus = Internal_CreateServiceToken(a1);
        if (serviceTokenStatus.IsSuccess())
        {
            nn::Result connRes = Internal_ConnectionObjectInit();
            if (connRes.IsFailure())
            {
                int err = nn::olv::GetErrorCode(connRes);
                Report::Print(REPORT_TYPE_2, "Connection object init failed - ErrorCode:%d\n", err);
                return connRes;
            }

            nn::Result searchRes = Internal_SearchHomeRegion(a1);
            if (searchRes.IsFailure())
            {
                int err = nn::olv::GetErrorCode(searchRes);
                Report::Print(REPORT_TYPE_2, "Search Home Region failed - ErrorCode:%d\n", err);
                return searchRes;
            }

            Internal_XOR_OfflineMode(1);
            g_LastMemoryUsage = nn::olv::GetNowUsedMemorySize();

            return NN_OLV_RESULT_SUCCESS;
        }

        return serviceTokenStatus;
    }

    nn::Result InitializeInternal(nn::olv::InitializeParam const *param, bool isOfflineModePossible, int checkParentalControls, int a4)
    {

        if (g_IsLibraryInitialized)
            return NN_OLV_RESULT_USAGE_CODE(0x6700);

        InitializeLogger();
        Report::SetReportTypes(param->m_ReportTypes);
        Report::Print(REPORT_TYPE_4096, "--------------------------------\n");
        Report::Print(REPORT_TYPE_4096, "olv version: %d.%d.%d\n", OLV_VERSION_MAJOR, OLV_VERSION_MINOR, OLV_VERSION_PATCH);
        Report::Print(REPORT_TYPE_4096, "--------------------------------\n");

        bool res = InitializeInternalStruct(param, isOfflineModePossible);
        if (!res)
            return NN_OLV_RESULT_FATAL_CODE(0x3380);

        nn::Result accInitStatus = InitializeAccount();
        if (accInitStatus.IsSuccess())
        {
            if (checkParentalControls)
            {
                nn::Result parentalCheck = ParentalCheck(true);
                if (parentalCheck.IsFailure())
                {
                    Report::Print(REPORT_TYPE_2048, "Initialize - Parental check FAILED.\n");
                    DestroyHeap();
                    DestroyHeap2();
                    FinalizeAccount();
                    g_IsLibraryInitialized = 0;
                    return parentalCheck;
                }
            }

            nn::Result zlibInitStatus = InitializeZlib();
            if (zlibInitStatus.IsFailure())
            {
                Report::Print(REPORT_TYPE_2048, "Initialize - ZLib Initialize FAILED\n");
                DestroyHeap();
                DestroyHeap2();
                FinalizeAccount();
                g_IsLibraryInitialized = 0;
                return zlibInitStatus;
            }

            if (!param->m_Work)
            {
                DestroyHeap();
                DestroyHeap2();
                FinalizeAccount();
                g_IsLibraryInitialized = 0;
                return NN_OLV_RESULT_USAGE_CODE(0x6600);
            }

            if (param->m_WorkSize < 0x10000)
            {
                DestroyHeap();
                DestroyHeap2();
                FinalizeAccount();
                g_IsLibraryInitialized = 0;
                return NN_OLV_RESULT_USAGE_CODE(0x6880);
            }

            nn::Result createParamPackResult = NN_OLV_RESULT_SUCCESS;
            if (a4)
            {
                uint32_t accessKey;
                nn::Result olvAccessKeyStatus = GetOlvAccessKey(&accessKey);
                if (olvAccessKeyStatus.IsFailure())
                {
                    DestroyHeap();
                    DestroyHeap2();
                    FinalizeAccount();
                    g_IsLibraryInitialized = 0;
                    return olvAccessKeyStatus;
                }

                uint64_t tid = Internal_GetTitleId();
                createParamPackResult = CreateParameterPack(tid, accessKey, 0);
                if (createParamPackResult.IsFailure())
                {
                    int errCode = nn::olv::GetErrorCode(createParamPackResult);
                    nn::olv::Report::Print(REPORT_TYPE_2, "Parameter pack create failed - ErrorCode:%d\n", errCode);
                    DestroyHeap();
                    DestroyHeap2();
                    FinalizeAccount();
                    g_IsLibraryInitialized = 0;
                    return createParamPackResult;
                }

                bool onlineMode = Internal_IsOnlineMode();
                if (!onlineMode)
                {
                    nn::olv::Report::Print(REPORT_TYPE_2, "--------------------------------\n");
                    nn::olv::Report::Print(REPORT_TYPE_2, "            WARNING             \n");
                    nn::olv::Report::Print(REPORT_TYPE_2, "                                \n");
                    nn::olv::Report::Print(REPORT_TYPE_2, "      THIS IS OFFLINE MODE      \n");
                    nn::olv::Report::Print(REPORT_TYPE_2, "                                \n");
                    nn::olv::Report::Print(REPORT_TYPE_2, "--------------------------------\n\n");
                }

                if (Internal_InitClientObject())
                {
                    if (Internal_InitXMLObject())
                    {
                        g_IsLibraryInitialized = true;
                        g_Unk10007886 = (param->m_Flags & 2) != 0;
                        if (onlineMode != 0)
                        {
                            Internal_XOR_OfflineMode(0);
                            nn::Result initConnRes = Internal_InitConnect(g_Unk10007886);

                            if (initConnRes.IsSuccess())
                            {
                                Report::Print(REPORT_TYPE_2, "Initialized successfully.\n");
                                g_LastMemoryUsage = nn::olv::GetNowUsedMemorySize();
                            }
                            else
                            {
                                // TODO: free and clear stuff
                            }
                            return initConnRes;
                        }
                        return createParamPackResult;
                    }
                    else
                    {
                        Report::Print(REPORT_TYPE_2, "XMLParser object init failed.\n");
                        Internal_ClearClientObject();
                        DestroyHeap();
                        DestroyHeap2();
                        FinalizeAccount();
                        g_IsLibraryInitialized = 0;
                        return NN_OLV_RESULT_USAGE_CODE(0x3380);
                    }
                }
                else
                {
                    Report::Print(REPORT_TYPE_2, "Client object init failed.\n");
                    Internal_ClearClientObject();
                    DestroyHeap();
                    DestroyHeap2();
                    FinalizeAccount();
                    g_IsLibraryInitialized = 0;
                    return NN_OLV_RESULT_USAGE_CODE(0x3380);
                }
            }
            return createParamPackResult;
        }
        else
        {
            int errCode = nn::act::GetErrorCode(accInitStatus);
            Report::Print(REPORT_TYPE_2048, "Initialize - First act Initialize FAILED -errorCode:%d\n", errCode);
            DestroyHeap();
            DestroyHeap2();
            FinalizeAccount();
            g_IsLibraryInitialized = 0;
            return accInitStatus;
        }
    }
}

bool EnsureNotOverflow(char *pDest, uint32_t destSize, int currentSize)
{
    if (!pDest || (uint32_t)currentSize != destSize)
        return true;
    nn::olv::Report::Print(nn::olv::ReportType::REPORT_TYPE_2048, "ToBase64String Error! size is FULL.\n");
    return false;
}

const char BASE64_TRANSFORM_ARRAY[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
bool ToBase64String(uint8_t *pSrc, size_t srcSize, char *pDest, size_t destSize, int *outSize)
{
    uint8_t *v5;  // r31
    int v9;       // r26
    uint8_t *v10; // r23
    int v11;      // r24
    int v12;      // r28
    int v13;      // r22
    int v15;      // r24
    int v16;      // r28

    v5 = pSrc;
    v9 = 0;
    v10 = &pSrc[srcSize];
    v11 = 0;
    v12 = 0;
    v13 = 0;
    if (!pSrc)
    {
        nn::olv::Report::Print(nn::olv::ReportType::REPORT_TYPE_2048, "ToBase64String pSrc is NULL.\n");
        return 0;
    }
    while (v5 != v10)
    {
        v13 += 2;
        v11 = (v11 << 8) | *v5;
        if (v9 == 76)
        {
            if (!EnsureNotOverflow(pDest, destSize, v12))
                return 0;
            v9 = 0;
            if (pDest)
                pDest[v12] = 10;
            ++v12;
        }
        if (!EnsureNotOverflow(pDest, destSize, v12))
            return 0;
        if (pDest)
        {
            ++v9;
            pDest[v12++] = BASE64_TRANSFORM_ARRAY[(v11 >> v13) & 0x3F];
            if (v13 < 6)
                goto LABEL_25;
        }
        else
        {
            ++v9;
            ++v12;
            if (v13 < 6)
                goto LABEL_25;
        }
        v13 -= 6;
        if (v9 == 76)
        {
            if (!EnsureNotOverflow(pDest, destSize, v12))
                return 0;
            v9 = 0;
            if (pDest)
                pDest[v12] = 10;
            ++v12;
        }
        if (!EnsureNotOverflow(pDest, destSize, v12))
            return 0;
        ++v9;
        if (pDest)
            pDest[v12] = BASE64_TRANSFORM_ARRAY[(v11 >> v13) & 0x3F];
        ++v12;
    LABEL_25:
        ++v5;
    }
    if (v13 <= 0)
        goto LABEL_41;
    v15 = v11 << (6 - v13);
    if (v9 != 76)
        goto LABEL_34;
    if (!EnsureNotOverflow(pDest, destSize, v12))
        return 0;
    v9 = 0;
    if (pDest)
        pDest[v12] = 10;
    ++v12;
LABEL_34:
    if (!EnsureNotOverflow(pDest, destSize, v12))
        return 0;
    if (!pDest)
        goto LABEL_40;
    ++v9;
    pDest[v12++] = BASE64_TRANSFORM_ARRAY[v15 & 0x3F];
LABEL_41:
    while ((v9 & 3) != 0)
    {
        if (!EnsureNotOverflow(pDest, destSize, v12))
            return 0;
        if (pDest)
        {
            ++v9;
            pDest[v12++] = 61;
        }
        else
        {
        LABEL_40:
            ++v9;
            ++v12;
        }
    }
    if (!EnsureNotOverflow(pDest, destSize, v12))
        return 0;
    if (pDest)
    {
        pDest[v12] = 0;
        v16 = v12 + 1;
        if (outSize)
        LABEL_48:
            *outSize = v16;
    }
    else
    {
        v16 = v12 + 1;
        if (outSize)
            goto LABEL_48;
    }
    return 1;
}

char *Base64Encode(char *dest, size_t destSize, uint8_t *src, size_t srcSize, int *outSize)
{
    if (ToBase64String(src, srcSize, dest, destSize, outSize))
        return dest;
    nn::olv::Report::Print(nn::olv::ReportType::REPORT_TYPE_2048, "Base64Encode Error! operation failed.\n");
    return 0;
}