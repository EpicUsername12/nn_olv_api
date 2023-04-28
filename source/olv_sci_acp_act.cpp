#include "olv_functions.hpp"
#include <cstdio>
#include <whb/log.h>

nn::Result GetSCIError_a071f480()
{
    return nn::Result(nn::Result::LEVEL_STATUS, nn::Result::RESULT_MODULE_NN_ACT, 0x1f480);
}

nn::Result GetSCIError_c070fa80()
{
    return nn::Result(nn::Result::LEVEL_FATAL, nn::Result::RESULT_MODULE_NN_ACT, 0xfa80);
}

nn::Result GetSCIError_C0712C80()
{
    return nn::Result(nn::Result::LEVEL_FATAL, nn::Result::RESULT_MODULE_NN_ACT, 0x12c80);
}

uint8_t internal_SCIGetParentalControlSlot(uint8_t slotNo)
{
    uint8_t parentalControlSlot = 0;
    nn::Result result = nn::act::GetParentalControlSlotNoEx(&parentalControlSlot, slotNo);
    if (result.IsSuccess())
    {
        return parentalControlSlot;
    }

    if (result != GetSCIError_a071f480())
    {
        if (result == GetSCIError_c070fa80())
        {
            OSReport("SCI: Account library is not initialized!\n");
        }
        else
        {
            if (result == GetSCIError_c070fa80())
            {
                OSReport("SCI: Invalid pointer detected!\n");
            }
        }
    }

    return 0;
}

int _SCIReadSysConfig(const char *key, UCDataType type, uint32_t size, void *out)
{
    UCHandle ucHandle = UCOpen();
    if (ucHandle >= 0)
    {
        UCSysConfig config;
        strncpy(config.name, key, sizeof(config.name) - 1);
        config.data = out;
        config.dataSize = size;
        config.access = 0x777;
        config.dataType = type;
        config.error = 0;

        UCError res = UCReadSysConfig(ucHandle, 1, &config);

        UCClose(ucHandle);
        if (res == UC_ERROR_OK)
        {
            return 1;
        }

        if (res == UC_ERROR_KEY_NOT_FOUND)
        {
            return -4;
        }
        else if (res == UC_ERROR_FILE_OPEN)
        {
            return -3;
        }
        else
        {
            OSReport("SCI:ERROR:%s:%d: Couldn't read from system config file (UC Err=%d)\n", "_SCIReadSysConfig", 104, res);
            return -1;
        }
    }
    else
    {
        OSReport("SCI:ERROR:%s:%d: Couldn't get UC handle(%d)\n", "_SCIReadSysConfig", 74, ucHandle);
        UCClose(ucHandle);
        return 0;
    }
}

bool internal_SCIDoSomethingWithLevel(uint8_t *restrictionLevel)
{
    unsigned int v2; // r0
    bool result;     // r3

    v2 = (uint8_t)*restrictionLevel;
    result = 1;
    if (v2 <= 2)
    {
        if ((v2 & 0x80u) != 0)
        {
            result = 0;
            *restrictionLevel = 0;
        }
    }
    else
    {
        result = 0;
        *restrictionLevel = 2;
    }
    return result;
}

int SCIGetParentalAccountNetworkLauncher(uint8_t *restrictionLevel, uint8_t slotNo)
{
    uint8_t parentalControlSlot = internal_SCIGetParentalControlSlot(slotNo);
    if (!parentalControlSlot)
    {
        return -5;
    }

    char setting[64];
    snprintf(setting, sizeof(setting), "p_acct%d.network_launcher", parentalControlSlot);
    int res = _SCIReadSysConfig(setting, UC_DATATYPE_UNSIGNED_BYTE, 1, restrictionLevel);
    internal_SCIDoSomethingWithLevel(restrictionLevel);

    return res;
}

int SCIGetSystemProdArea(uint32_t *outProdArea)
{
    MCPSysProdSettings settings __attribute__((aligned(0x40)));
    memset(&settings, 0, sizeof(settings));

    IOSHandle mcpHandle = (IOSHandle)MCP_Open();
    if (mcpHandle >= 0)
    {
        MCPError err = MCP_GetSysProdSettings(mcpHandle, &settings);
        if (err)
        {
            OSReport("SCI:ERROR:%s:%d: MCP Error loading product information(%d)\n", "SCIGetSystemSettings", 118, err);
            MCP_Close(mcpHandle);
            return -1;
        }
        else
        {
            *outProdArea = (settings.product_area) & 0xff;
            MCP_Close(mcpHandle);
            return 1;
        }
    }
    else
    {
        OSReport("SCI:ERROR:%s:%d: Couldn't get mcp handle(%d)\n", "SCIGetSystemSettings", 109, mcpHandle);
        MCP_Close(mcpHandle);
        return 0;
    }
}

bool Internal_SCICheckLanguage(uint32_t *outLanguage)
{
    if ((*outLanguage < 0xc) && (-1 < (char)*outLanguage))
    {
        return 1;
    }
    *outLanguage = 0xff;
    return 0;
}
int SCIGetLanguage(uint32_t *outLanguage)
{
    int res = _SCIReadSysConfig("cafe.language", UC_DATATYPE_UNSIGNED_INT, 4, outLanguage);
    Internal_SCICheckLanguage(outLanguage);
    return res;
}

bool Internal_SCICheckCafeCountry(uint32_t *outCountry)
{
    bool uVar1;
    uVar1 = true;
    if (*outCountry < 0x100)
    {
        if ((int)*outCountry < 0)
        {
            uVar1 = false;
            *outCountry = 0;
        }
    }
    else
    {
        uVar1 = false;
        *outCountry = 0xff;
    }
    return uVar1;
}

int SCIGetCafeCountry(uint32_t *outCountry)
{
    int res = _SCIReadSysConfig("cafe.cntry_reg", UC_DATATYPE_UNSIGNED_INT, 4, outCountry);
    Internal_SCICheckCafeCountry(outCountry);
    return res;
}

int SCIGetParentalAccountFriendReg(uint8_t *outFriendRegRestr, uint8_t slotNo)
{
    char key[64];
    int parentalSlot = internal_SCIGetParentalControlSlot(slotNo);
    if (!parentalSlot)
        return -5;

    snprintf(key, sizeof(key), "p_acct%d.friend_reg", parentalSlot);
    int result = _SCIReadSysConfig(key, UC_DATATYPE_UNSIGNED_BYTE, 1, outFriendRegRestr);
    if (*outFriendRegRestr > 1u)
        *outFriendRegRestr = 1;

    return result;
}

int SCIGetParentalAccountGameRating(uint32_t *outGameRatingRestr, uint8_t slotNo)
{
    char key[64];
    int parentalSlot = internal_SCIGetParentalControlSlot(slotNo);
    if (!parentalSlot)
        return -5;

    snprintf(key, sizeof(key), "p_acct%d.game_rating", parentalSlot);
    int result = _SCIReadSysConfig(key, UC_DATATYPE_UNSIGNED_INT, 4, outGameRatingRestr);
    if ((uint8_t)*outGameRatingRestr > 1u)
        *outGameRatingRestr = 1;

    return result;
}

bool Internal_SCICheckRatingOrganization(uint32_t *outRatingOrg)
{
    bool v1; // r0
    v1 = true;
    if (*outRatingOrg >= 0x10u)
    {
        v1 = false;
        *outRatingOrg = 255;
    }
    return v1;
}

int SCIGetParentalRatingOrganization(uint32_t *outRatingOrg)
{
    int res = _SCIReadSysConfig("parent.rating_organization", UC_DATATYPE_UNSIGNED_INT, 4, outRatingOrg);
    Internal_SCICheckRatingOrganization(outRatingOrg);
    return res;
}

// Directly copied from pseudocode. (it works though.)
nn::Result nn::olv::GetOlvAccessKey(uint32_t *outAccessKey)
{
    *outAccessKey = 0;
    uint32_t acpRes = (uint32_t)ACPGetOlvAccesskey(outAccessKey);
    if (acpRes == ACP_RESULT_SUCCESS)
        return nn::Result(nn::Result::LEVEL_SUCCESS, nn::Result::RESULT_MODULE_COMMON, 0);

    nn::Result result = NN_OLV_RESULT_STATUS_CODE(0x22700);
    if (acpRes >= 0xFFFFFA88)
    {
        if (acpRes >= 0xFFFFFD44)
        {
            if (acpRes >= 0xFFFFFF33)
            {
                if (acpRes < 0xFFFFFF37)
                {
                    if (acpRes > 0xFFFFFF35)
                    {
                        Report::Print(REPORT_TYPE_2048, "ACPGetOlvAccesskey Failed. error:%d %s\n", acpRes, "ACP_STATUS_INVALID_FILE");
                        result = NN_OLV_RESULT_STATUS_CODE(0x22700);
                    }
                    else if (acpRes == 0xFFFFFF35)
                    {
                        Report::Print(
                            REPORT_TYPE_2048,
                            "ACPGetOlvAccesskey Failed. error:%d %s\n",
                            0xFFFFFF35,
                            "ACP_STATUS_INVALID_XML_FILE");
                        result = NN_OLV_RESULT_STATUS_CODE(0x22700);
                    }
                    else
                    {
                        if (acpRes > 0xFFFFFF33)
                            Report::Print(
                                REPORT_TYPE_2048,
                                "ACPGetOlvAccesskey Failed. error:%d %s\n",
                                acpRes,
                                "ACP_STATUS_INVALID_FILE_ACCESS_MODE");
                        else
                            Report::Print(
                                REPORT_TYPE_2048,
                                "ACPGetOlvAccesskey Failed. error:%d %s\n",
                                -205,
                                "ACP_STATUS_INVALID_NETWORK_TIME");
                        result = NN_OLV_RESULT_STATUS_CODE(0x22700);
                    }
                    return result;
                }
                switch (acpRes)
                {
                case 0xFFFFFFFF:
                    Report::Print(REPORT_TYPE_2048, "ACPGetOlvAccesskey Failed. error:%d %s\n", -1, "ACP_STATUS_FAIL");
                    return NN_OLV_RESULT_STATUS_CODE(0x22700);
                case 0xFFFFFF38:
                    Report::Print(REPORT_TYPE_2048, "ACPGetOlvAccesskey Failed. error:%d %s\n", -200, "ACP_STATUS_INVALID");
                    return NN_OLV_RESULT_STATUS_CODE(0x22700);
                case 0xFFFFFF37:
                    Report::Print(REPORT_TYPE_2048, "ACPGetOlvAccesskey Failed. error:%d %s\n", -201, "ACP_STATUS_INVALID_PARAM");
                    return NN_OLV_RESULT_STATUS_CODE(0x22700);
                }
            }
            else
            {
                if (acpRes >= 0xFFFFFE05)
                {
                    switch (acpRes)
                    {
                    case 0xFFFFFE05:
                        Report::Print(
                            REPORT_TYPE_2048,
                            "ACPGetOlvAccesskey Failed. error:%d %s\n",
                            acpRes,
                            "ACP_STATUS_XML_ITEM_NOT_FOUND");
                        result = NN_OLV_RESULT_STATUS_CODE(0x22700);
                        break;
                    case 0xFFFFFE06:
                        Report::Print(
                            REPORT_TYPE_2048,
                            "ACPGetOlvAccesskey Failed. error:%d %s\n",
                            acpRes,
                            "ACP_STATUS_SYSTEM_CONFIG_NOT_FOUND");
                        result = NN_OLV_RESULT_STATUS_CODE(0x22700);
                        break;
                    case 0xFFFFFE07:
                        Report::Print(
                            REPORT_TYPE_2048,
                            "ACPGetOlvAccesskey Failed. error:%d %s\n",
                            acpRes,
                            "ACP_STATUS_APPLICATION_NOT_FOUND");
                        result = NN_OLV_RESULT_STATUS_CODE(0x22700);
                        break;
                    case 0xFFFFFE08:
                        Report::Print(
                            REPORT_TYPE_2048,
                            "ACPGetOlvAccesskey Failed. error:%d %s\n",
                            acpRes,
                            "ACP_STATUS_TITLE_NOT_FOUND");
                        result = NN_OLV_RESULT_STATUS_CODE(0x22700);
                        break;
                    case 0xFFFFFE09:
                        Report::Print(
                            REPORT_TYPE_2048,
                            "ACPGetOlvAccesskey Failed. error:%d %s\n",
                            acpRes,
                            "ACP_STATUS_DEVICE_NOT_FOUND");
                        result = NN_OLV_RESULT_STATUS_CODE(0x22700);
                        break;
                    case 0xFFFFFE0A:
                        Report::Print(
                            REPORT_TYPE_2048,
                            "ACPGetOlvAccesskey Failed. error:%d %s\n",
                            acpRes,
                            "ACP_STATUS_DIR_NOT_FOUND");
                        result = NN_OLV_RESULT_STATUS_CODE(0x22700);
                        break;
                    case 0xFFFFFE0B:
                        Report::Print(
                            REPORT_TYPE_2048,
                            "ACPGetOlvAccesskey Failed. error:%d %s\n",
                            acpRes,
                            "ACP_STATUS_FILE_NOT_FOUND");
                        result = NN_OLV_RESULT_STATUS_CODE(0x22700);
                        break;
                    case 0xFFFFFE0C:
                        Report::Print(REPORT_TYPE_2048, "ACPGetOlvAccesskey Failed. error:%d %s\n", acpRes, "ACP_STATUS_NOT_FOUND");
                        result = NN_OLV_RESULT_STATUS_CODE(0x22700);
                        break;
                    default:
                        goto LABEL_126;
                    }
                    return result;
                }
                if (acpRes <= 0xFFFFFDA8)
                {
                    if (acpRes == 0xFFFFFDA8)
                    {
                        Report::Print(REPORT_TYPE_2048, "ACPGetOlvAccesskey Failed. error:%d %s\n", -600, "ACP_STATUS_ALREADY_EXISTS");
                        return NN_OLV_RESULT_STATUS_CODE(0x22700);
                    }
                    if (acpRes > 0xFFFFFDA6)
                    {
                        Report::Print(
                            REPORT_TYPE_2048,
                            "ACPGetOlvAccesskey Failed. error:%d %s\n",
                            acpRes,
                            "ACP_STATUS_FILE_ALREADY_EXISTS");
                        return NN_OLV_RESULT_STATUS_CODE(0x22700);
                    }
                    if (acpRes == 0xFFFFFDA6)
                    {
                        Report::Print(
                            REPORT_TYPE_2048,
                            "ACPGetOlvAccesskey Failed. error:%d %s\n",
                            0xFFFFFDA6,
                            "ACP_STATUS_DIR_ALREADY_EXISTS");
                        return NN_OLV_RESULT_STATUS_CODE(0x22700);
                    }
                    if (acpRes == 0xFFFFFD44)
                    {
                        Report::Print(REPORT_TYPE_2048, "ACPGetOlvAccesskey Failed. error:%d %s\n", 0xFFFFFD44, "ACP_STATUS_ALREADY_DONE");
                        return NN_OLV_RESULT_STATUS_CODE(0x22700);
                    }
                }
            }
        }
        else if (acpRes >= 0xFFFFFBB2)
        {
            if (acpRes >= 0xFFFFFC0A)
            {
                switch (acpRes)
                {
                case 0xFFFFFC0A:
                    Report::Print(REPORT_TYPE_2048, "ACPGetOlvAccesskey Failed. error:%d %s\n", acpRes, "ACP_STATUS_DRC_REQUIRED");
                    result = NN_OLV_RESULT_STATUS_CODE(0x22700);
                    break;
                case 0xFFFFFC0B:
                    Report::Print(
                        REPORT_TYPE_2048,
                        "ACPGetOlvAccesskey Failed. error:%d %s\n",
                        acpRes,
                        "ACP_STATUS_DEMO_EXPIRED_NUMBER");
                    result = NN_OLV_RESULT_STATUS_CODE(0x22700);
                    break;
                case 0xFFFFFC0C:
                    Report::Print(REPORT_TYPE_2048, "ACPGetOlvAccesskey Failed. error:%d %s\n", acpRes, "ACP_STATUS_INVALID_LOGO");
                    result = NN_OLV_RESULT_STATUS_CODE(0x22700);
                    break;
                case 0xFFFFFC0D:
                    Report::Print(
                        REPORT_TYPE_2048,
                        "ACPGetOlvAccesskey Failed. error:%d %s\n",
                        acpRes,
                        "ACP_STATUS_INCORRECT_PINCODE");
                    result = NN_OLV_RESULT_STATUS_CODE(0x22700);
                    break;
                case 0xFFFFFC0E:
                    Report::Print(
                        REPORT_TYPE_2048,
                        "ACPGetOlvAccesskey Failed. error:%d %s\n",
                        acpRes,
                        "ACP_STATUS_PINCODE_REQUIRED");
                    result = NN_OLV_RESULT_STATUS_CODE(0x22700);
                    break;
                case 0xFFFFFC0F:
                    Report::Print(REPORT_TYPE_2048, "ACPGetOlvAccesskey Failed. error:%d %s\n", acpRes, "ACP_STATUS_OLV_REQUIRED");
                    result = NN_OLV_RESULT_STATUS_CODE(0x22700);
                    break;
                case 0xFFFFFC10:
                    Report::Print(
                        REPORT_TYPE_2048,
                        "ACPGetOlvAccesskey Failed. error:%d %s\n",
                        acpRes,
                        "ACP_STATUS_BROWSER_REQUIRED");
                    result = NN_OLV_RESULT_STATUS_CODE(0x22700);
                    break;
                case 0xFFFFFC11:
                    Report::Print(
                        REPORT_TYPE_2048,
                        "ACPGetOlvAccesskey Failed. error:%d %s\n",
                        acpRes,
                        "ACP_STATUS_NETACCOUNT_ERROR");
                    result = NN_OLV_RESULT_STATUS_CODE(0x22700);
                    break;
                case 0xFFFFFC12:
                    Report::Print(
                        REPORT_TYPE_2048,
                        "ACPGetOlvAccesskey Failed. error:%d %s\n",
                        acpRes,
                        "ACP_STATUS_NETACCOUNT_REQUIRED");
                    result = NN_OLV_RESULT_STATUS_CODE(0x22700);
                    break;
                case 0xFFFFFC13:
                    Report::Print(
                        REPORT_TYPE_2048,
                        "ACPGetOlvAccesskey Failed. error:%d %s\n",
                        acpRes,
                        "ACP_STATUS_NETSETTING_REQUIRED");
                    result = NN_OLV_RESULT_STATUS_CODE(0x22700);
                    break;
                case 0xFFFFFC14:
                    Report::Print(
                        REPORT_TYPE_2048,
                        "ACPGetOlvAccesskey Failed. error:%d %s\n",
                        acpRes,
                        "ACP_STATUS_PENDING_RATING");
                    result = NN_OLV_RESULT_STATUS_CODE(0x22700);
                    break;
                case 0xFFFFFC15:
                    Report::Print(
                        REPORT_TYPE_2048,
                        "ACPGetOlvAccesskey Failed. error:%d %s\n",
                        acpRes,
                        "ACP_STATUS_NOTPRESENT_RATING");
                    result = NN_OLV_RESULT_STATUS_CODE(0x22700);
                    break;
                case 0xFFFFFC16:
                    Report::Print(
                        REPORT_TYPE_2048,
                        "ACPGetOlvAccesskey Failed. error:%d %s\n",
                        acpRes,
                        "ACP_STATUS_RESTRICTED_RATING");
                    result = NN_OLV_RESULT_STATUS_CODE(0x22700);
                    break;
                case 0xFFFFFC17:
                    Report::Print(
                        REPORT_TYPE_2048,
                        "ACPGetOlvAccesskey Failed. error:%d %s\n",
                        acpRes,
                        "ACP_STATUS_INVALID_REGION");
                    result = NN_OLV_RESULT_STATUS_CODE(0x22700);
                    break;
                case 0xFFFFFC18:
                    Report::Print(REPORT_TYPE_2048, "ACPGetOlvAccesskey Failed. error:%d %s\n", acpRes, "ACP_STATUS_AUTH_ERROR");
                    result = NN_OLV_RESULT_STATUS_CODE(0x22700);
                    break;
                default:
                    goto LABEL_126;
                }
                return result;
            }
            if (acpRes <= 0xFFFFFBB4)
            {
                if (acpRes == 0xFFFFFBB4)
                {
                    Report::Print(REPORT_TYPE_2048, "ACPGetOlvAccesskey Failed. error:%d %s\n", 0xFFFFFBB4, "ACP_STATUS_PERMISSION_ERROR");
                    result = NN_OLV_RESULT_STATUS_CODE(0x22700);
                }
                else
                {
                    if (acpRes > 0xFFFFFBB2)
                        Report::Print(
                            REPORT_TYPE_2048,
                            "ACPGetOlvAccesskey Failed. error:%d %s\n",
                            acpRes,
                            "ACP_STATUS_NO_FILE_PERMISSION");
                    else
                        Report::Print(
                            REPORT_TYPE_2048,
                            "ACPGetOlvAccesskey Failed. error:%d %s\n",
                            -1102,
                            "ACP_STATUS_NO_DIR_PERMISSION");
                    result = NN_OLV_RESULT_STATUS_CODE(0x22700);
                }
                return result;
            }
        }
        else
        {
            switch (acpRes)
            {
            case 0xFFFFFAEC:
                Report::Print(REPORT_TYPE_2048, "ACPGetOlvAccesskey Failed. error:%d %s\n", -1300, "ACP_STATUS_BUSY");
                return NN_OLV_RESULT_STATUS_CODE(0x22700);
            case 0xFFFFFAEB:
                Report::Print(
                    REPORT_TYPE_2048,
                    "ACPGetOlvAccesskey Failed. error:%d %s\n",
                    -1301,
                    "ACP_STATUS_USB_STORAGE_NOT_READY");
                return NN_OLV_RESULT_STATUS_CODE(0x22700);
            case 0xFFFFFA88:
                Report::Print(REPORT_TYPE_2048, "ACPGetOlvAccesskey Failed. error:%d %s\n", -1400, "ACP_STATUS_CANCELED");
                return NN_OLV_RESULT_STATUS_CODE(0x22700);
            }
        }
    LABEL_126:
        Report::Print(REPORT_TYPE_2048, "ACPGetOlvAccesskey Failed. error:%d %s\n", acpRes, "ACP_STATUS_???");
        return NN_OLV_RESULT_STATUS_CODE(0x22700);
    }
    if (acpRes >= 0xFFFFF82C)
    {
        if (acpRes >= 0xFFFFF8F8)
        {
            if (acpRes >= 0xFFFFFA1F)
            {
                switch (acpRes)
                {
                case 0xFFFFFA1F:
                    Report::Print(REPORT_TYPE_2048, "ACPGetOlvAccesskey Failed. error:%d %s\n", acpRes, "ACP_STATUS_IPC_RESOURCE");
                    result = NN_OLV_RESULT_STATUS_CODE(0x22700);
                    break;
                case 0xFFFFFA20:
                    Report::Print(REPORT_TYPE_2048, "ACPGetOlvAccesskey Failed. error:%d %s\n", acpRes, "ACP_STATUS_FS_RESOURCE");
                    result = NN_OLV_RESULT_STATUS_CODE(0x22700);
                    break;
                case 0xFFFFFA21:
                    Report::Print(REPORT_TYPE_2048, "ACPGetOlvAccesskey Failed. error:%d %s\n", acpRes, "ACP_STATUS_SYSTEM_MEMORY");
                    result = NN_OLV_RESULT_STATUS_CODE(0x22700);
                    break;
                case 0xFFFFFA22:
                    Report::Print(REPORT_TYPE_2048, "ACPGetOlvAccesskey Failed. error:%d %s\n", acpRes, "ACP_STATUS_JOURNAL_FULL");
                    result = NN_OLV_RESULT_STATUS_CODE(0x22700);
                    break;
                case 0xFFFFFA23:
                    Report::Print(REPORT_TYPE_2048, "ACPGetOlvAccesskey Failed. error:%d %s\n", acpRes, "ACP_STATUS_DEVICE_FULL");
                    result = NN_OLV_RESULT_STATUS_CODE(0x22700);
                    break;
                case 0xFFFFFA24:
                    Report::Print(
                        REPORT_TYPE_2048,
                        "ACPGetOlvAccesskey Failed. error:%d %s\n",
                        acpRes,
                        "ACP_STATUS_RESOURCE_ERROR");
                    result = NN_OLV_RESULT_STATUS_CODE(0x22700);
                    break;
                default:
                    goto LABEL_126;
                }
                return result;
            }
            switch (acpRes)
            {
            case 0xFFFFF9C0:
                Report::Print(REPORT_TYPE_2048, "ACPGetOlvAccesskey Failed. error:%d %s\n", -1600, "ACP_STATUS_NOT_INITIALIZED");
                return NN_OLV_RESULT_STATUS_CODE(0x22700);
            case 0xFFFFF95C:
                Report::Print(REPORT_TYPE_2048, "ACPGetOlvAccesskey Failed. error:%d %s\n", -1700, "ACP_STATUS_ACCOUNT_ERROR");
                return NN_OLV_RESULT_STATUS_CODE(0x22700);
            case 0xFFFFF8F8:
                Report::Print(REPORT_TYPE_2048, "ACPGetOlvAccesskey Failed. error:%d %s\n", -1800, "ACP_STATUS_UNSUPPORTED");
                return NN_OLV_RESULT_STATUS_CODE(0x22700);
            }
        }
        else
        {
            if (acpRes < 0xFFFFF82F)
            {
                if (acpRes > 0xFFFFF82D)
                {
                    Report::Print(
                        REPORT_TYPE_2048,
                        "ACPGetOlvAccesskey Failed. error:%d %s\n",
                        acpRes,
                        "ACP_STATUS_SLC_DATA_CORRUPTED");
                    result = NN_OLV_RESULT_STATUS_CODE(0x22700);
                }
                else
                {
                    if (acpRes == 0xFFFFF82D)
                        Report::Print(
                            REPORT_TYPE_2048,
                            "ACPGetOlvAccesskey Failed. error:%d %s\n",
                            0xFFFFF82D,
                            "ACP_STATUS_MLC_DATA_CORRUPTED");
                    else
                        Report::Print(
                            REPORT_TYPE_2048,
                            "ACPGetOlvAccesskey Failed. error:%d %s\n",
                            acpRes,
                            "ACP_STATUS_USB_DATA_CORRUPTED");
                    result = NN_OLV_RESULT_STATUS_CODE(0x22700);
                }
                return result;
            }
            if (acpRes == 0xFFFFF830)
            {
                Report::Print(REPORT_TYPE_2048, "ACPGetOlvAccesskey Failed. error:%d %s\n", 0xFFFFF830, "ACP_STATUS_DEVICE_ERROR");
                return NN_OLV_RESULT_STATUS_CODE(0x22700);
            }
            if (acpRes == 0xFFFFF82F)
            {
                Report::Print(REPORT_TYPE_2048, "ACPGetOlvAccesskey Failed. error:%d %s\n", 0xFFFFF82F, "ACP_STATUS_DATA_CORRUPTED");
                return NN_OLV_RESULT_STATUS_CODE(0x22700);
            }
        }
        goto LABEL_126;
    }
    if (acpRes < 0xFFFFF002)
    {
        if (acpRes == 0xFFFFF000)
        {
            Report::Print(REPORT_TYPE_2048, "ACPGetOlvAccesskey Failed. error:%d %s\n", 0xFFFFF000, "ACP_STATUS_FATAL_ERROR");
            return NN_OLV_RESULT_STATUS_CODE(0x22700);
        }
        if (acpRes == 0xFFFFF001)
        {
            Report::Print(
                REPORT_TYPE_2048,
                "ACPGetOlvAccesskey Failed. error:%d %s\n",
                0xFFFFF001,
                "ACP_STATUS_INTERNAL_RESOUCE_ERROR");
            return NN_OLV_RESULT_STATUS_CODE(0x22700);
        }
        goto LABEL_126;
    }
    if (acpRes < 0xFFFFF7C4)
    {
        switch (acpRes)
        {
        case 0xFFFFF768:
            Report::Print(REPORT_TYPE_2048, "ACPGetOlvAccesskey Failed. error:%d %s\n", -2200, "ACP_STATUS_MII_ERROR");
            return NN_OLV_RESULT_STATUS_CODE(0x22700);
        case 0xFFFFF767:
            Report::Print(
                REPORT_TYPE_2048,
                "ACPGetOlvAccesskey Failed. error:%d %s\n",
                -2201,
                "ACP_STATUS_MII_ENCRYPTION_ERROR");
            return NN_OLV_RESULT_STATUS_CODE(0x22700);
        case 0xFFFFF002:
            Report::Print(REPORT_TYPE_2048, "ACPGetOlvAccesskey Failed. error:%d %s\n", -4094, "ACP_STATUS_INTERNAL_ERROR");
            return NN_OLV_RESULT_STATUS_CODE(0x22700);
        }
        goto LABEL_126;
    }
    switch (acpRes)
    {
    case 0xFFFFF7C4:
        Report::Print(
            REPORT_TYPE_2048,
            "ACPGetOlvAccesskey Failed. error:%d %s\n",
            acpRes,
            "ACP_STATUS_USB_MEDIA_WRITE_PROTECTED");
        result = NN_OLV_RESULT_STATUS_CODE(0x22700);
        break;
    case 0xFFFFF7C5:
        Report::Print(
            REPORT_TYPE_2048,
            "ACPGetOlvAccesskey Failed. error:%d %s\n",
            acpRes,
            "ACP_STATUS_MEDIA_WRITE_PROTECTED");
        result = NN_OLV_RESULT_STATUS_CODE(0x22700);
        break;
    case 0xFFFFF7C6:
        Report::Print(REPORT_TYPE_2048, "ACPGetOlvAccesskey Failed. error:%d %s\n", acpRes, "ACP_STATUS_USB_MEDIA_BROKEN");
        result = NN_OLV_RESULT_STATUS_CODE(0x22700);
        break;
    case 0xFFFFF7C7:
        Report::Print(REPORT_TYPE_2048, "ACPGetOlvAccesskey Failed. error:%d %s\n", acpRes, "ACP_STATUS_USB_MEDIA_NOTREADY");
        result = NN_OLV_RESULT_STATUS_CODE(0x22700);
        break;
    case 0xFFFFF7C8:
        Report::Print(REPORT_TYPE_2048, "ACPGetOlvAccesskey Failed. error:%d %s\n", acpRes, "ACP_STATUS_ODD_MEDIA_BROKEN");
        result = NN_OLV_RESULT_STATUS_CODE(0x22700);
        break;
    case 0xFFFFF7C9:
        Report::Print(REPORT_TYPE_2048, "ACPGetOlvAccesskey Failed. error:%d %s\n", acpRes, "ACP_STATUS_ODD_MEDIA_NOTREADY");
        result = NN_OLV_RESULT_STATUS_CODE(0x22700);
        break;
    case 0xFFFFF7CA:
        Report::Print(REPORT_TYPE_2048, "ACPGetOlvAccesskey Failed. error:%d %s\n", acpRes, "ACP_STATUS_MEDIA_BROKEN");
        result = NN_OLV_RESULT_STATUS_CODE(0x22700);
        break;
    case 0xFFFFF7CB:
        Report::Print(REPORT_TYPE_2048, "ACPGetOlvAccesskey Failed. error:%d %s\n", acpRes, "ACP_STATUS_MEDIA_NOTREADY");
        result = NN_OLV_RESULT_STATUS_CODE(0x22700);
        break;
    case 0xFFFFF7CC:
        Report::Print(REPORT_TYPE_2048, "ACPGetOlvAccesskey Failed. error:%d %s\n", acpRes, "ACP_STATUS_MEDIA_ERROR");
        result = NN_OLV_RESULT_STATUS_CODE(0x22700);
        break;
    default:
        goto LABEL_126;
    }
    return result;
}