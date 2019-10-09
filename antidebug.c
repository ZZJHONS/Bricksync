/* -----------------------------------------------------------------------------
 *
 * Copyright (c) 2014-2019 Alexis Naveros.
 *
 * This software is provided 'as-is', without any express or implied
 * warranty. In no event will the authors be held liable for any damages
 * arising from the use of this software.
 *
 * Permission is granted to anyone to use this software for any purpose,
 * including commercial applications, and to alter it and redistribute it
 * freely, subject to the following restrictions:
 *
 * 1. The origin of this software must not be misrepresented; you must not
 * claim that you wrote the original software. If you use this software
 * in a product, an acknowledgment in the product documentation would be
 * appreciated but is not required.
 * 2. Altered source versions must be plainly marked as such, and must not be
 * misrepresented as being the original software.
 * 3. This notice may not be removed or altered from any source distribution.
 *
 * -----------------------------------------------------------------------------
 */

#define _WIN32_WINNT 0x0501

#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include <stdint.h>
#include <stdarg.h>
#include <string.h>
#include <math.h>
#include <time.h>

#include "cpuconfig.h"
#include "cc.h"
#include "ccstr.h"
#include "mm.h"
#include "mmatomic.h"
#include "mmbitmap.h"
#include "iolog.h"
#include "cpuinfo.h"
#include "antidebug.h"

#if CC_UNIX
 #include <sys/types.h>
 #include <sys/stat.h>
 #include <unistd.h>
#elif CC_WINDOWS
 #include <windows.h>
 #include <direct.h>
 #include <psapi.h> /* EnumProcessModules */
#else
 #error Unknown/Unsupported platform!
#endif



////



#if defined(__linux__) || defined(__gnu_linux__) || defined(__linux) || defined(__linux)

 #define __USE_GNU
 #include <signal.h>
 #include <sys/ucontext.h>

static void antiDebugHandlerUD( int signo, siginfo_t *si, void *data )
{
  ucontext_t *uc = (ucontext_t *)data;
 #if CPUCONF_WORD_SIZE == 64
  uc->uc_mcontext.gregs[REG_RIP] = uc->uc_mcontext.gregs[REG_RAX];
  uc->uc_mcontext.gregs[REG_RBX] ^= uc->uc_mcontext.gregs[REG_RCX];
 #elif CPUCONF_WORD_SIZE == 32
  uc->uc_mcontext.gregs[REG_EIP] = uc->uc_mcontext.gregs[REG_EAX];
  uc->uc_mcontext.gregs[REG_EBX] ^= uc->uc_mcontext.gregs[REG_ECX];
 #else
  #error Unknown CPUCONF_WORD_SIZE
 #endif
  return;
}

void antiDebugHandleSIGILL()
{
  struct sigaction sa;
  sigemptyset( &sa.sa_mask );
  sa.sa_flags = SA_SIGINFO;
  sa.sa_handler = (void *)antiDebugHandlerUD;
  sigaction( SIGILL, &sa, 0 );
  return;
}

#elif defined(__APPLE__)

 #include <signal.h>
 #include <sys/ucontext.h>

static void antiDebugHandlerUD( int signo, siginfo_t *si, void *data )
{
  ucontext_t *uc = (ucontext_t *)data;
 #if CPUCONF_WORD_SIZE == 64
  uc->uc_mcontext->__ss.__rip = uc->uc_mcontext->__ss.__rax;
  uc->uc_mcontext->__ss.__rbx ^= uc->uc_mcontext->__ss.__rcx;
 #elif CPUCONF_WORD_SIZE == 32
  uc->uc_mcontext->__ss.__eip = uc->uc_mcontext->__ss.__eax;
  uc->uc_mcontext->__ss.__ebx ^= uc->uc_mcontext->__ss.__ecx;
 #else
  #error Unknown CPUCONF_WORD_SIZE
 #endif
  return;
}

void antiDebugHandleSIGILL()
{
  struct sigaction sa;
  sigemptyset( &sa.sa_mask );
  sa.sa_flags = SA_SIGINFO;
  sa.sa_handler = (void *)antiDebugHandlerUD;
  sigaction( SIGILL, &sa, 0 );
  return;
}

#elif defined(_WIN64) || defined(__WIN64__) || defined(WIN64) || defined(_WIN32) || defined(__WIN32__) || defined(WIN32)

 #if !defined(_WIN64) && !defined(__WIN64__) && !defined(WIN64)
static LONG CALLBACK antiDebugVectoredHandlerUD( EXCEPTION_POINTERS *ExceptionInfo )
{
  if( ExceptionInfo->ExceptionRecord->ExceptionCode == EXCEPTION_ILLEGAL_INSTRUCTION )
  {
 #if CPUCONF_WORD_SIZE == 64
    ExceptionInfo->ContextRecord->Rip = ExceptionInfo->ContextRecord->Rax;
    ExceptionInfo->ContextRecord->Rbx ^= ExceptionInfo->ContextRecord->Rcx;
 #elif CPUCONF_WORD_SIZE == 32
    ExceptionInfo->ContextRecord->Eip = ExceptionInfo->ContextRecord->Eax;
    ExceptionInfo->ContextRecord->Ebx ^= ExceptionInfo->ContextRecord->Ecx;
 #else
  #error Unknown CPUCONF_WORD_SIZE
 #endif
    return EXCEPTION_CONTINUE_EXECUTION;
  }
  return EXCEPTION_CONTINUE_SEARCH;
}
 #endif

static LONG WINAPI antiDebugHandlerUD( EXCEPTION_POINTERS * ExceptionInfo )
{
  if( ExceptionInfo->ExceptionRecord->ExceptionCode == EXCEPTION_ILLEGAL_INSTRUCTION )
  {
 #if CPUCONF_WORD_SIZE == 64
    ExceptionInfo->ContextRecord->Rip = ExceptionInfo->ContextRecord->Rax;
    ExceptionInfo->ContextRecord->Rbx ^= ExceptionInfo->ContextRecord->Rcx;
 #elif CPUCONF_WORD_SIZE == 32
    ExceptionInfo->ContextRecord->Eip = ExceptionInfo->ContextRecord->Eax;
    ExceptionInfo->ContextRecord->Ebx ^= ExceptionInfo->ContextRecord->Ecx;
 #else
  #error Unknown CPUCONF_WORD_SIZE
 #endif
    return EXCEPTION_CONTINUE_EXECUTION;
  }
  return EXCEPTION_EXECUTE_HANDLER;
}

static void antiDebugInvalidParameter( const wchar_t* expression, const wchar_t* function,  const wchar_t* file,  unsigned int line,  uintptr_t pReserved )
{
  return;
}

static inline void antiDebugSetHandleInvalidParameter()
{
  int i, modulecount;
  HINSTANCE psapilib;
  HANDLE processhandle;
  HMODULE *modulelist;
  DWORD listmemsize;
  DWORD listusedsize;
  void *(__cdecl *setfunction)( void *pNew );
  BOOL (WINAPI *enumprocessmodules)( HANDLE hProcess, HMODULE *lphModule, DWORD cb, LPDWORD lpcbNeeded );

  psapilib = LoadLibraryA( "PSAPI.DLL" );
  if( !( psapilib ) )
    return;
  enumprocessmodules = (void *)GetProcAddress( psapilib, "EnumProcessModules" );
  if( !( enumprocessmodules ) )
    goto done;
  processhandle = GetCurrentProcess();
  listmemsize = 0;
  listusedsize = 0;
  if( !( enumprocessmodules( processhandle, NULL, 0, &listmemsize ) ) )
    goto done;
  modulelist = malloc( listmemsize );
  if( enumprocessmodules( processhandle, modulelist, listmemsize, &listusedsize ) )
  {
    modulecount = CC_MIN( listmemsize, listusedsize ) / sizeof(HMODULE);
    for( i = 0 ; i < modulecount ; i++ )
    {
      setfunction = (void *)GetProcAddress( modulelist[i], "_set_invalid_parameter_handler" );
      if( setfunction )
      {
        setfunction( (void *)antiDebugInvalidParameter );
 #if 0
        printf( "Fixed parameter handler, module %d / %d\n", i, modulecount );
 #endif
      }
    }
  }
  free( modulelist );
  done:
  FreeLibrary( psapilib );
  return;
}

 #if !defined(_WIN64) && !defined(__WIN64__) && !defined(WIN64)
static inline int antiDebugDetectWin64()
{
  BOOL win64flag;
  BOOL (WINAPI *iswow64process)( HANDLE hProcess, PBOOL Wow64Process );

  win64flag = FALSE;
  HMODULE module = GetModuleHandleA( "kernel32" );
  if( module )
  {
    iswow64process = (void *)GetProcAddress( module, "IsWow64Process" );
    if( iswow64process )
    {
      iswow64process( GetCurrentProcess(), &win64flag );
 #if 0
      printf( "IsWin64 : %d\n", (int)win64flag );
 #endif
    }
  }
  return (int)win64flag;
}
 #endif

void antiDebugHandleSIGILL()
{
  antiDebugSetHandleInvalidParameter();
 #if !defined(_WIN64) && !defined(__WIN64__) && !defined(WIN64)
  OSVERSIONINFO osvi;
  ZeroMemory( &osvi, sizeof(OSVERSIONINFO) );
  osvi.dwOSVersionInfoSize = sizeof(OSVERSIONINFO);
  GetVersionEx( &osvi );
  if( !( antiDebugDetectWin64() ) && ( osvi.dwMajorVersion >= 6 ) )
    AddVectoredExceptionHandler( 1, antiDebugVectoredHandlerUD );
  else
    SetUnhandledExceptionFilter( antiDebugHandlerUD );
 #else
  SetUnhandledExceptionFilter( antiDebugHandlerUD );
 #endif
  return;
}

#else

 #error Unknown platform

#endif




