#ifndef EMULATED_STDATOMIC_H_INCLUDED_
#define EMULATED_STDATOMIC_H_INCLUDED_

#if __GNUC__ || __clang__
#include "stdatomic_gcc_clang.h"
#elif _WIN32 && !__CYGWIN__
#include "stdatomic_win32.h"
#else
#error Not supported on this platform.
#endif

#endif /* EMULATED_STDATOMIC_H_INCLUDED_ */
