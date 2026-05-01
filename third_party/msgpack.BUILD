load("@rules_cc//cc:defs.bzl", "cc_library")

cc_library(
    name = "msgpack",
    hdrs = glob([
        "include/**/*.hpp",
        "include/**/*.h",
    ]),
    defines = ["MSGPACK_NO_BOOST"],
    includes = ["include"],
    visibility = ["//visibility:public"],
)
