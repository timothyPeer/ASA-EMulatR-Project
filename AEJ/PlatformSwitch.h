#pragma once


#if !defined(ALPHA_BUILD) && !defined(TRU64_BUILD)
#error "You must define one of ALPHA_BUILD or TRU64_BUILD"
#elif defined(ALPHA_BUILD) && defined(TRU64_BUILD)
#error "Only one of ALPHA_BUILD or TRU64_BUILD may be defined"
#endif
