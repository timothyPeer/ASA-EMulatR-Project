#pragma once

#include <QtCore/qglobal.h>

#ifndef BUILD_STATIC
# if defined(AEM_LIB)
#  define AEM_EXPORT Q_DECL_EXPORT
# else
#  define AEM_EXPORT Q_DECL_IMPORT
# endif
#else
# define AEM_EXPORT
#endif
