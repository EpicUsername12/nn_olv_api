#include "olv_types.hpp"

namespace nn::olv
{

    InitializeParam::InitializeParam()
    {
        this->m_Flags = 0;
        this->m_ReportTypes = 7039;
        this->m_SysArgsSize = 0;
        this->m_Work = 0;
        this->m_SysArgs = 0;
        this->m_WorkSize = 0;
    }

    nn::Result InitializeParam::SetFlags(uint32_t flags)
    {
        this->m_Flags = flags | 0x1000;
        return NN_OLV_RESULT_SUCCESS;
    }

    nn::Result InitializeParam::SetWork(uint8_t *workData, uint32_t workDataSize)
    {
        if (!workData)
            return NN_OLV_RESULT_FATAL_CODE(0x6600);

        if (workDataSize < 0x10000)
            return NN_OLV_RESULT_FATAL_CODE(0x6580);

        this->m_Work = workData;
        this->m_WorkSize = workDataSize;

        return NN_OLV_RESULT_SUCCESS;
    }

    nn::Result InitializeParam::SetReportTypes(uint32_t reportTypes)
    {
        this->m_ReportTypes = reportTypes;
        return NN_OLV_RESULT_SUCCESS;
    }

    nn::Result InitializeParam::SetSysArgs(void const *sysArgs, uint32_t sysArgsSize)
    {
        if (!sysArgs)
            return NN_OLV_RESULT_FATAL_CODE(0x6600);

        if (!sysArgsSize)
            return NN_OLV_RESULT_FATAL_CODE(0x6480);

        this->m_SysArgs = sysArgs;
        this->m_SysArgsSize = sysArgsSize;

        return NN_OLV_RESULT_SUCCESS;
    }

    nn::Result MainAppParam::GetAppData(uint8_t *appData, uint32_t *appDataSize, uint32_t appDataMaxSize)
    {
        if (!appData)
            return NN_OLV_RESULT_FATAL_CODE(0x6600);

        if (appDataMaxSize)
        {
            uint32_t currentAppDataSize = this->m_AppDataSize;
            if (!this->m_AppDataSize)
                return NN_OLV_RESULT_FATAL_CODE(0x6800);

            if (appDataMaxSize < currentAppDataSize)
                currentAppDataSize = appDataMaxSize;

            memcpy(appData, this->m_AppData, currentAppDataSize);
            if (appDataSize)
                *appDataSize = currentAppDataSize;

            return NN_OLV_RESULT_SUCCESS;
        }

        return NN_OLV_RESULT_FATAL_CODE(0x6580);
    }

}