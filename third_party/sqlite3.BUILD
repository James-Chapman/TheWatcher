load("@rules_cc//cc:defs.bzl", "cc_library")

cc_library(
    name = "sqlite3",
    srcs  = ["sqlite3.c"],
    hdrs  = ["sqlite3.h"],
    copts = [
        "-DSQLITE_THREADSAFE=1",
        "-DSQLITE_ENABLE_JSON1",
        "-DSQLITE_ENABLE_WAL",
        "-DSQLITE_DEFAULT_WAL_SYNCHRONOUS=1",
    ],
    visibility = ["//visibility:public"],
)
