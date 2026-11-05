cc_library(
    name = "reporting",
    srcs = [
        "boost.cc",
        "new_dynamic.cc",
        "new_dynamic.hh",
        "adapter.hh",
        "thst.cc",
    ],
    visibility = ["//visibility:public"],
    deps = [
        "//app/algorithms:algorithm_base_impl_lib",
        "//benchmark",
        "@boost.geometry",
        "@abseil-cpp//absl/container:btree",
        "@thst",
        "@pkd_tree",
        "//third_party/esa2026:nanoflann",
        #"@CGAL",
        # "@GMP",
    ],
    alwayslink = 1,
)

cc_test(name="new_dynamic_test", srcs=["new_dynamic_test.cc","new_dynamic.hh"], deps=["@thst","@pkd_tree","//third_party/esa2026:nanoflann"],linkopts=["-ldl"])

cc_test(name="dynamic_stream_test",srcs=["dynamic_stream_test.cc","new_dynamic.hh","adapter.hh"],deps=["//benchmark","@thst","@pkd_tree","//third_party/esa2026:nanoflann","//app/algorithms:algorithm_base_impl_lib"],linkopts=["-ldl"])

