#pragma once

#include <QtCore/qglobal.h>

#ifndef BUILD_STATIC
# if defined(AEC_LIB)
#  define AEC_EXPORT Q_DECL_EXPORT
# else
#  define AEC_EXPORT Q_DECL_IMPORT
# endif
#else
# define AEC_EXPORT
#endif
