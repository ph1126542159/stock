//
// OpenTelemetry.h
//
// Library: OpenTelemetry
// Package: OpenTelemetry
// Module:  OpenTelemetry
//
// Basic definitions for the Poco OpenTelemetry library.
// This file must be the first file included by every other OpenTelemetry
// header file.
//

#ifndef OpenTelemetry_OpenTelemetry_INCLUDED
#define OpenTelemetry_OpenTelemetry_INCLUDED

#include "Poco/Poco.h"

#if defined(_WIN32) && defined(POCO_DLL)
    #if defined(OpenTelemetry_EXPORTS)
        #define OpenTelemetry_API __declspec(dllexport)
    #else
        #define OpenTelemetry_API __declspec(dllimport)
    #endif
#endif

#if !defined(OpenTelemetry_API)
    #define OpenTelemetry_API
#endif

#if defined(_MSC_VER)
    #if !defined(POCO_NO_AUTOMATIC_LIBS) && !defined(OpenTelemetry_EXPORTS)
        #pragma comment(lib, "PocoOpenTelemetry" POCO_LIB_SUFFIX)
    #endif
#endif

#endif // OpenTelemetry_OpenTelemetry_INCLUDED
