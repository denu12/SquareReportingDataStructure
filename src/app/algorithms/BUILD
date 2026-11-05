load("@rules_cc//cc:defs.bzl", "cc_library")

cc_library(
    name = "algorithm_base_impl_lib",
    srcs = [
        "algorithm_impl.cc",
        "dyn_algorithm_impl.cc",
    ],
    hdrs = [
        "algorithm_impl.h",
        "dyn_algorithm_impl.h",
        "two_phase_algorithm_impl.h",
        "//:build-info.h",
    ],
    visibility = ["//visibility:public"],
    deps = [
        "//app:app_io_cc_proto",
        "//utils:logging",
        "//utils:systeminfo",
        "@abseil-cpp//absl/status:statusor",
        "@protobuf//src/google/protobuf/util:time_util",
    ],
)
