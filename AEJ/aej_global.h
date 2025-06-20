#pragma once

#include <QtCore/qglobal.h>

#ifndef BUILD_STATIC
# if defined(AEJ_LIB)
#  define AEJ_EXPORT Q_DECL_EXPORT
# else
#  define AEJ_EXPORT Q_DECL_IMPORT
# endif
#else
# define AEJ_EXPORT
#endif

