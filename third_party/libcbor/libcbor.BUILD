load("@rules_cc//cc:cc_library.bzl", "cc_library")

package(default_visibility = ["//visibility:public"])

cc_library(
    name = "cbor",
    srcs = [
        "src/allocators.c",
        "src/cbor.c",

        "src/cbor/arrays.c",
        "src/cbor/bytestrings.c",
        "src/cbor/callbacks.c",
        "src/cbor/common.c",
        "src/cbor/encoding.c",
        "src/cbor/floats_ctrls.c",
        "src/cbor/ints.c",
        "src/cbor/maps.c",
        "src/cbor/serialization.c",
        "src/cbor/streaming.c",
        "src/cbor/strings.c",
        "src/cbor/tags.c",

        "src/cbor/internal/builder_callbacks.c",
        "src/cbor/internal/encoders.c",
        "src/cbor/internal/loaders.c",
        "src/cbor/internal/memory_utils.c",
        "src/cbor/internal/stack.c",
        "src/cbor/internal/unicode.c",
    ],
    hdrs = glob([
        "src/*.h",
        "src/cbor/*.h",
        "src/cbor/internal/*.h",
    ]) + [
        "cbor/cbor_export.h",
        "cbor/configuration.h",
    ],
    includes = [
        "src",
        ".",
    ],
    defines = [
        "CBOR_STATIC_DEFINE",
    ],
    copts = select({
        "@platforms//os:windows": [
            "/D_CRT_SECURE_NO_WARNINGS",
            "/DCBOR_RESTRICT_SPECIFIER=",
        ],
        "//conditions:default": [
            "-DCBOR_RESTRICT_SPECIFIER=restrict",
        ],
    }),
)