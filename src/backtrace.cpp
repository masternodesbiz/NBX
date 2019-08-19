// Copyright (c) 2018-2019 Netbox.Global
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "backtrace.h"

#ifdef WIN32
#include <psapi.h>
#include <dbghelp.h>


#if defined(_M_X64) || defined(__x86_64__)
typedef DWORD64 MACH_DWORD;
#else
typedef DWORD MACH_DWORD;
#endif

void LoadWinProcessModules()
{
    DWORD cnt = 0;
    if (FALSE == EnumProcessModules(GetCurrentProcess(), NULL, 0, &cnt))
        return;
    cnt /= sizeof(HMODULE);
    HMODULE *modules = new HMODULE[cnt + 32];
    if (FALSE == EnumProcessModules(GetCurrentProcess(), modules, (cnt + 32)*sizeof(HMODULE), &cnt))
        return;
    cnt /= sizeof(HMODULE);
    SymInitialize(GetCurrentProcess(), NULL, FALSE);
    SymSetOptions(SYMOPT_UNDNAME | SYMOPT_NO_PROMPTS | SYMOPT_LOAD_LINES | SYMOPT_LOAD_ANYTHING | SYMOPT_FAIL_CRITICAL_ERRORS | SYMOPT_CASE_INSENSITIVE);
    char *buf = new char[10240];
    for (DWORD i = 0; i < cnt; i++){
        MODULEINFO info;
        std::string fullPath;
        std::string name;
        DWORD len;
        len = GetModuleFileNameA(modules[i], buf, 10240);
        if (0 != len){
            CharLowerA(buf);
            fullPath = std::string(buf, len);
            size_t pos = fullPath.find_last_of("\\/");
            if (std::string::npos != pos)
                name = fullPath.substr(pos + 1);
        }
        if (FALSE == GetModuleInformation(GetCurrentProcess(), modules[i], &info, sizeof(info)))
            continue;
        DWORD64 base = (DWORD64)info.lpBaseOfDll;
        DWORD size = info.SizeOfImage;
        SymLoadModule64(GetCurrentProcess(), NULL, fullPath.empty() ? NULL : (PSTR)fullPath.c_str(), name.empty() ? NULL : (PSTR)name.c_str(), base, size);
    }
    delete buf;
}

std::string addr2hex(MACH_DWORD num, bool trim = true)
{
    const char * alpha = "0123456789abcdef";
    std::string res;
    res.reserve(sizeof(MACH_DWORD) * 2);
    for (int i = sizeof(MACH_DWORD) - 1; i >= 0; i--){
        int high = ((unsigned char*)&num)[i] >> 4;
        int low = ((unsigned char*)&num)[i] & 0xf;
        if (!trim || !res.empty() || high)
            res += alpha[high];
        if (!trim || !res.empty() || low)
            res += alpha[low];
    }
    return res;
}

std::string uint2str(unsigned long number)
{
    static char buf[11];
    char * tmp = buf + 10;
    *tmp-- = 0;
    do{
        *tmp-- = number % 10 + '0';
        number /= 10;
    } while (number != 0);
    return tmp + 1;
}

std::string GetWinAddrDescription(DWORD64 addr)
{
    static SYMBOL_INFO * symbol = (SYMBOL_INFO*)new char[sizeof(SYMBOL_INFO) + 10240 * sizeof(char)];
    std::string res;
    HMODULE module = (HMODULE)SymGetModuleBase64(GetCurrentProcess(), addr);
    // offset to symbol
    DWORD64 offset64 = 0;
    memset(symbol, 0, sizeof(*symbol) + 10240 * sizeof(char));
    symbol->SizeOfStruct = sizeof(*symbol);
    symbol->MaxNameLen = 10240;
    if (FALSE != SymFromAddr(GetCurrentProcess(), addr, &offset64, symbol)){
        res += symbol->Name;
        if (offset64){
            res += "+0x";
            res += addr2hex(offset64);
        }
        if (0 != module){
            DWORD len = GetModuleFileNameA(module, (char*)symbol, 10240);
            if (0 != len){
                CharLowerA((char*)symbol);
                res += " in ";
                res += (char*)symbol;
            }
        }
    }
    else {
        // offset to module
        if (0 != module){
            DWORD len = GetModuleFileNameA(module, (char*)symbol, 10240);
            if (0 != len){
                CharLowerA((char*)symbol);
                res += (char*)symbol;
                res += "+0x";
                res += addr2hex(addr - (DWORD64)module);
            }
            else {
                res += "0x";
                res += addr2hex(addr, false);
            }
        }
        else {
            // raw offset
            res += "0x";
            res += addr2hex(addr, false);
        }
    }
    // source line
    DWORD offset = 0;
    IMAGEHLP_LINE64 line = {};
    line.SizeOfStruct = sizeof(line);
    if (FALSE != SymGetLineFromAddr64(GetCurrentProcess(), addr, &offset, &line)){
        res += " at ";
        res += line.FileName;
        res += ":";
        res += uint2str(line.LineNumber);
    }
    return res;
}

std::string CBacktrace::GetWinBacktrace(LPCONTEXT context)
{
    LoadWinProcessModules();
    if (!context){
        CONTEXT contextTmp = {};
        RtlCaptureContext(&contextTmp);
        context = &contextTmp;
    }
    STACKFRAME64 frame = {};
#if defined(_M_X64) || defined(__x86_64__)
    frame.AddrPC.Offset = context->Rip;
    frame.AddrStack.Offset = context->Rsp;
    frame.AddrFrame.Offset = context->Rbp;
#else
    frame.AddrPC.Offset = context->Eip;
    frame.AddrStack.Offset = context->Esp;
    frame.AddrFrame.Offset = context->Ebp;
#endif
    frame.AddrPC.Mode = AddrModeFlat;
    frame.AddrStack.Mode = AddrModeFlat;
    frame.AddrFrame.Mode = AddrModeFlat;
    std::string res;
    do{
        if (FALSE == StackWalk64(
#if defined(_M_X64) || defined(__x86_64__)
                IMAGE_FILE_MACHINE_AMD64
#else
                IMAGE_FILE_MACHINE_I386
#endif
                , GetCurrentProcess(), GetCurrentThread(), &frame, context, NULL, SymFunctionTableAccess64, SymGetModuleBase64, NULL))
            break;
        if (frame.AddrPC.Offset == 0)
            res += "0\n";
        else {
            res += GetWinAddrDescription(frame.AddrPC.Offset);
            res += "\n";
        }
    } while (frame.AddrReturn.Offset);
    if (!res.empty())
        res.resize(res.size() - 1);
    return res;
}

#else

#include <stdlib.h>
#include <execinfo.h>
std::string CBacktrace::GetLinBacktrace()
{
    std::string res;
    void *buffer[64];
    int size = backtrace(buffer, 64);
    char **strings = backtrace_symbols(buffer, size);
    if (strings == NULL)
        res = "unknown";
    else {
        for (int i = 0; i < size; i++) {
            res += strings[i];
            res += "\n";
        }
        free(strings);
    }
    return res;
}

#endif
