#pragma once

#if defined(_WIN32) || defined(_WIN64)
#include <windows.h>
#define fileno _fileno
#define strdup _strdup
#endif

// Win32 compatibility
#if defined(_MSC_VER) && !defined(__clang__)
#define __attribute__(x) 
#endif

#define LSTF_VM_MAX_CAPTURES (unsigned)UINT8_MAX
