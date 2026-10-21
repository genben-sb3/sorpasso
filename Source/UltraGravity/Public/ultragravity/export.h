#pragma once

// Visibility of the package's functions. Unreal builds the package as the module UltraGravity, a DLL in editor builds,
// and defines ULTRAGRAVITY_API as DLLEXPORT inside the module and DLLIMPORT in its users (empty in monolithic
// builds). The package's own sources include no Unreal headers, so those two may be missing there; they are defined
// here the way Unreal defines them. Without Unreal UG_API is empty.
#if defined(ULTRAGRAVITY_API)
#if !defined(DLLEXPORT)
#if defined(_WIN32)
#define DLLEXPORT __declspec(dllexport)
#else
#define DLLEXPORT __attribute__((visibility("default")))
#endif
#endif
#if !defined(DLLIMPORT)
#if defined(_WIN32)
#define DLLIMPORT __declspec(dllimport)
#else
#define DLLIMPORT __attribute__((visibility("default")))
#endif
#endif
#define UG_API ULTRAGRAVITY_API
#else
#define UG_API
#endif
