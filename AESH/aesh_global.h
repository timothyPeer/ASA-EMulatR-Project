#pragma once

#include <QtCore/qglobal.h>

#ifndef BUILD_STATIC
# if defined(AESH_LIB)
#  define AESH_EXPORT Q_DECL_EXPORT
# else
#  define AESH_EXPORT Q_DECL_IMPORT
# endif
#else
# define AESH_EXPORT
#endif
