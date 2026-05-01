load("@rules_foreign_cc//foreign_cc:defs.bzl", "cmake")

filegroup(
    name = "all_srcs",
    srcs = glob(["**"]),
    visibility = ["//visibility:public"],
)

cmake(
    name = "libzmq",
    lib_source = ":all_srcs",
    cache_entries = {
        "BUILD_SHARED":   "OFF",
        "BUILD_TESTS":    "OFF",
        "WITH_LIBSODIUM": "ON",
        "ENABLE_CURVE":   "ON",
        "CMAKE_BUILD_TYPE": "Release",
    },
    out_static_libs = select({
        "@platforms//os:windows": ["libzmq-mt-s-4_3_5.lib"],
        "//conditions:default":   ["libzmq.a"],
    }),
    deps = ["@libsodium//:libsodium"],
    visibility = ["//visibility:public"],
)
