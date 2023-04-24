#include <coreinit/title.h>
#include <padscore/kpad.h>
#include <sndcore2/core.h>
#include <sysapp/launch.h>
#include <whb/proc.h>
#include <whb/log_udp.h>
#include <whb/log.h>
#include "olv_functions.hpp"
#include <coreinit/memdefaultheap.h>
#include <coreinit/debug.h>
#include <coreinit/exception.h>
#include <coreinit/context.h>
#include <coreinit/systeminfo.h>
#include <coreinit/dynload.h>
#include <coreinit/core.h>
#include <coreinit/memorymap.h>
#include <coreinit/exit.h>
#include <cstdio>

#define C_UNLESS(expr, code) \
    ({                       \
        if (!(expr))         \
        {                    \
            code;            \
        }                    \
    })

#define R_UNLESS(expr, res) \
    ({                      \
        if (!(expr))        \
        {                   \
            return res;     \
        }                   \
    })

inline bool RunningFromMiiMaker()
{
    return (OSGetTitleID() & 0xFFFFFFFFFFFFF0FFull) == 0x000500101004A000ull;
}

void ExceptionHandler(const char *type, OSContext *context)
{

    char *symbolname = (char *)MEMAllocFromDefaultHeap(129);
    char *symbolname2 = (char *)MEMAllocFromDefaultHeap(129);

    bool valid = OSIsAddressValid(context->srr0);
    C_UNLESS(!valid, OSGetSymbolName(context->srr0, symbolname, 128));
    C_UNLESS(valid, memcpy(symbolname, "???", 19));

    valid = OSIsAddressValid(context->lr);
    C_UNLESS(!valid, OSGetSymbolName(context->lr, symbolname2, 128));
    C_UNLESS(valid, memcpy(symbolname2, "???", 19));

    char *lrBuffer = (char *)MEMAllocFromDefaultHeap(129);
    snprintf(lrBuffer, 0x80, "0x%08x (%s)", context->lr, symbolname2);

    char *buffer = (char *)MEMAllocFromDefaultHeap(0x800);
    snprintf(buffer, 2048,
             "Exception: %s - in %s\n"
             "Title ID: %016llX | Core: %d | UPID: %d\n\n"
             ""
             "An error has occured.\n"
             "Press the POWER button for 4 seconds to shutdown.\n\n"
             ""
             "r%-2d: 0x%08X | r%-2d: 0x%08X | r%-2d: 0x%08X\n"
             "r%-2d: 0x%08X | r%-2d: 0x%08X | r%-2d: 0x%08X\n"
             "r%-2d: 0x%08X | r%-2d: 0x%08X | r%-2d: 0x%08X\n"
             "r%-2d: 0x%08X | r%-2d: 0x%08X | r%-2d: 0x%08X\n"
             "r%-2d: 0x%08X | r%-2d: 0x%08X | r%-2d: 0x%08X\n"
             "r%-2d: 0x%08X | r%-2d: 0x%08X | r%-2d: 0x%08X\n"
             "r%-2d: 0x%08X | r%-2d: 0x%08X | r%-2d: 0x%08X\n"
             "r%-2d: 0x%08X | r%-2d: 0x%08X | r%-2d: 0x%08X\n"
             "r%-2d: 0x%08X | r%-2d: 0x%08X | r%-2d: 0x%08X\n"
             "r%-2d: 0x%08X | r%-2d: 0x%08X | r%-2d: 0x%08X\n"
             "r%-2d: 0x%08X | r%-2d: 0x%08X | LR : %s\n"
             "PC : 0x%08X | DAR: 0x%08X | DSISR: 0x%08X\n",
             type, symbolname, OSGetTitleID(), OSGetCoreId(), OSGetUPID(), 0, context->gpr[0], 11, context->gpr[11], 22, context->gpr[22], 1, context->gpr[1], 12,
             context->gpr[12], 23, context->gpr[23], 2, context->gpr[2], 13, context->gpr[13], 24, context->gpr[24], 3, context->gpr[3], 14, context->gpr[14], 25,
             context->gpr[25], 4, context->gpr[4], 15, context->gpr[15], 26, context->gpr[26], 5, context->gpr[5], 16, context->gpr[16], 27, context->gpr[27], 6,
             context->gpr[6], 17, context->gpr[17], 28, context->gpr[28], 7, context->gpr[7], 18, context->gpr[18], 29, context->gpr[29], 8, context->gpr[8], 19,
             context->gpr[19], 30, context->gpr[30], 9, context->gpr[9], 20, context->gpr[20], 31, context->gpr[31], 10, context->gpr[10], 21, context->gpr[21], lrBuffer,
             context->srr0, context->dar, context->dsisr);

    WHBLogPrintf("%s", buffer);
    OSFatal(buffer);
}

int ExcDSI(OSContext *ctx)
{
    ExceptionHandler("DSI", ctx);
    return 0;
}

int ExcISI(OSContext *ctx)
{
    ExceptionHandler("ISI", ctx);
    return 0;
}

int ExcProgram(OSContext *ctx)
{
    ExceptionHandler("Program", ctx);
    return 0;
}

#include <coreinit/thread.h>
#include <coreinit/time.h>

int main(int argc, char const *argv[])
{
    WHBLogUdpInit();
    WHBProcInit();

    OSSetExceptionCallbackEx(OS_EXCEPTION_MODE_GLOBAL_ALL_CORES, OS_EXCEPTION_TYPE_DSI, ExcDSI);
    OSSetExceptionCallbackEx(OS_EXCEPTION_MODE_GLOBAL_ALL_CORES, OS_EXCEPTION_TYPE_ISI, ExcISI);
    OSSetExceptionCallbackEx(OS_EXCEPTION_MODE_GLOBAL_ALL_CORES, OS_EXCEPTION_TYPE_PROGRAM, ExcProgram);

    void *ptr = MEMAllocFromDefaultHeapEx(0x20000, 0x100);

    NSSLInit();
    nn::olv::InitializeParam param;
    param.SetWork((uint8_t *)ptr, 0x20000);
    param.SetReportTypes(nn::olv::Report::GetReportTypes() | 0x800);

    nn::Result res = nn::olv::Initialize(&param);
    WHBLogPrintf("init: 0x%08x\n", *(uint32_t *)&res);

    int last_tm_sec = -1;
    OSCalendarTime tm;
    while (WHBProcIsRunning())
    {
        OSTicksToCalendarTime(OSGetTime(), &tm);
        if (tm.tm_sec != last_tm_sec)
        {
            WHBLogPrintf("%02d/%02d/%04d %02d:%02d:%02d I'm still here.",
                         tm.tm_mday, tm.tm_mon, tm.tm_year,
                         tm.tm_hour, tm.tm_min, tm.tm_sec);
            last_tm_sec = tm.tm_sec;
        }

        OSSleepTicks(OSMillisecondsToTicks(1000));
    }

    WHBLogPrintf("Exiting ...\n");

    WHBLogUdpDeinit();
    WHBProcShutdown();

    return 0;
}
