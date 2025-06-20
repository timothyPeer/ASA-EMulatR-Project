#pragma once

#include <QtCore/qglobal.h>

#ifndef BUILD_STATIC
# if defined(AEI_LIB)
#  define AEI_EXPORT Q_DECL_EXPORT
# else
#  define AEI_EXPORT Q_DECL_IMPORT
# endif
#else
# define AEI_EXPORT
#endif
