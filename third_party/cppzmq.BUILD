load("@rules_cc//cc:defs.bzl", "cc_library")

cc_library(
    name = "cppzmq",
    hdrs = glob(["*.hpp"]),
    defines = select({
        "@platforms//os:windows": ["ZMQ_STATIC"],
        "//conditions:default":   [],
    }),
    includes = ["."],
    linkopts = select({
        "@platforms//os:windows": ["ws2_32.lib", "iphlpapi.lib"],
        "//conditions:default":   [],
    }),
    deps = ["@libzmq//:libzmq"],
    visibility = ["//visibility:public"],
)
