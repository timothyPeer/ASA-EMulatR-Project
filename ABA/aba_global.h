#pragma once

#include <QtCore/qglobal.h>

#ifndef BUILD_STATIC
# if defined(ABA_LIB)
#  define ABA_EXPORT Q_DECL_EXPORT
# else
#  define ABA_EXPORT Q_DECL_IMPORT
# endif
#else
# define ABA_EXPORT
#endif
