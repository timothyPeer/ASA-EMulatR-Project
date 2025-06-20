#pragma once

#include <QtCore/qglobal.h>

#ifndef BUILD_STATIC
# if defined(AEU_LIB)
#  define AEU_EXPORT Q_DECL_EXPORT
# else
#  define AEU_EXPORT Q_DECL_IMPORT
# endif
#else
# define AEU_EXPORT
#endif
