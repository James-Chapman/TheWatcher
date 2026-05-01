load("@rules_cc//cc:defs.bzl", "cc_library")

package(default_visibility = ["//visibility:public"])

# Catch2 v3 amalgamated build.
# The amalgamated .cpp provides the implementation including the default main().
cc_library(
    name = "catch2",
    srcs = ["extras/catch_amalgamated.cpp"],
    hdrs = ["extras/catch_amalgamated.hpp"],
    strip_include_prefix = "extras",
    include_prefix = "catch2",
    copts = select({
        "@platforms//os:windows": ["/EHsc", "/w"],
        "//conditions:default":   ["-w"],
    }),
)
