#pragma once

#include <QtCore/qglobal.h>

#ifndef BUILD_STATIC
# if defined(AEB_LIB)
#  define AEB_EXPORT Q_DECL_EXPORT
# else
#  define AEB_EXPORT Q_DECL_IMPORT
# endif
#else
# define AEB_EXPORT
#endif
