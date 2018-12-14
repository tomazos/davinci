package(default_visibility = ["//visibility:public"])

cc_library(
  name = "relaxng",
  hdrs = [
    "relaxng.h",
  ],
  deps = [
      "//core:json",
  ],
)

cc_binary(
  name = "relaxngc",
  srcs = [
    "relaxngc.cc",
  ],
  linkopts = [
    "-lgflags",
    "-lglog",
    "-lstdc++fs",
  ],
  deps = [
    "//core:file",
    "//core:parser",
    "//core:scanner",
    "//core:json",
  ],
)
