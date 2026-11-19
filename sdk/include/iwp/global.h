#pragma once
#include <QtGlobal>

#if defined(IWP_SDK_LIBRARY)
#  define IWP_SDK_EXPORT Q_DECL_EXPORT
#else
#  define IWP_SDK_EXPORT Q_DECL_IMPORT
#endif
