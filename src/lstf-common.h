#pragma once

// Win32 compatibility
#if defined(_MSC_VER) && !defined(__clang__)
#define __attribute__(x) 
#endif

#if defined(_WIN32) || defined(_WIN64)
#define fileno _fileno
#define strdup _strdup
#endif

#define LSTF_VM_MAX_CAPTURES (unsigned)UINT8_MAX
