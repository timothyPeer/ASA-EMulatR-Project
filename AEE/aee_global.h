#pragma once

#include <QtCore/qglobal.h>

#ifndef BUILD_STATIC
# if defined(AEE_LIB)
#  define AEE_EXPORT Q_DECL_EXPORT
# else
#  define AEE_EXPORT Q_DECL_IMPORT
# endif
#else
# define AEE_EXPORT
#endif
