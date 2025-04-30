#pragma once

#include <QtCore/qglobal.h>

#ifndef BUILD_STATIC
# if defined(AES_LIB)
#  define AES_EXPORT Q_DECL_EXPORT
# else
#  define AES_EXPORT Q_DECL_IMPORT
# endif
#else
# define AES_EXPORT
#endif
