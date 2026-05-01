load("@rules_cc//cc:defs.bzl", "cc_library")
load("@rules_foreign_cc//foreign_cc:defs.bzl", "configure_make")

filegroup(
    name = "all_srcs",
    srcs = glob(["**"]),
    visibility = ["//visibility:public"],
)

# Linux / BSD: autotools build.
# Windows: run `bazel build --config=windows` after installing MSYS2 and
#           setting BAZEL_SH in .bazelrc.user. See README for details.
genrule(
    name = "version_header",
    srcs = ["builds/msvc/version.h"],
    outs = ["generated/sodium/version.h"],
    cmd_bash = "cp $(location builds/msvc/version.h) $@",
    cmd_bat = "copy /Y \"$(location builds/msvc/version.h)\" \"$@\" >NUL",
)

cc_library(
    name = "libsodium_msvc",
    srcs = glob(["src/libsodium/**/*.c"]),
    hdrs = glob(["src/libsodium/**/*.h"]) + [":version_header"],
    copts = [
        "/wd4146",
        "/wd4244",
    ],
    defines = ["SODIUM_STATIC"],
    includes = [
        "generated",
        "generated/sodium",
        "src/libsodium/include",
        "src/libsodium/include/sodium",
    ],
    local_defines = [
        "CONFIGURED=1",
        "inline=__inline",
        "NATIVE_LITTLE_ENDIAN",
        "_CRT_SECURE_NO_WARNINGS",
    ],
    linkopts = ["advapi32.lib"],
)

configure_make(
    name = "libsodium_autotools",
    lib_source = ":all_srcs",
    configure_in_place = True,
    configure_options = [
        "--enable-static",
        "--disable-shared",
        "--enable-pic",
        "--disable-debug",
        "--disable-dependency-tracking",
    ],
    out_static_libs = ["libsodium.a"],
)

alias(
    name = "libsodium",
    actual = select({
        "@platforms//os:windows": ":libsodium_msvc",
        "//conditions:default": ":libsodium_autotools",
    }),
    visibility = ["//visibility:public"],
)
