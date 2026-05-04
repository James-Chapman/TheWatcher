#pragma once

#if defined(_WIN32) || defined(__CYGWIN__)
  #ifdef CBOR_STATIC_DEFINE
    #define CBOR_EXPORT
    #define CBOR_NO_EXPORT
  #else
    #ifdef cbor_EXPORTS
      #define CBOR_EXPORT __declspec(dllexport)
    #else
      #define CBOR_EXPORT __declspec(dllimport)
    #endif
    #define CBOR_NO_EXPORT
  #endif
#else
  #define CBOR_EXPORT __attribute__((visibility("default")))
  #define CBOR_NO_EXPORT __attribute__((visibility("hidden")))
#endif