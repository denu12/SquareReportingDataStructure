cc_library(
    name = "static",
    srcs = [
        "morton_simd_adapter.cc",
        "esa2026.cc",
        "SPRKoriginal.cc",
        "stann_fr.cc",
        "ann_fr.cc",
        "boost.cc",
        "cgal.cc",
        "chan_sss.cc",
        "naive.cc",
        "thst.cc",
    ],
    hdrs = [
        "esa2026.hh",
        "stann_fr.hh",
        "ann_fr.hh",
        "naive.hh",
        "adapter.hh",
    ],
    visibility = ["//visibility:public"],
    deps =
        select({
            "//:pargeo": [":static_pargeo"],
            "//conditions:default": [":static_parlay"],
        }) + [
            "//app/algorithms:algorithm_base_impl_lib",
            "//third_party/stann:stann_reporting",
            "//third_party/esa2026:nanoflann",
            "//third_party/ann:ann_euclidean",
            "//benchmark",
            "//morton",
            ":morton_index",
            "@CGAL",
            "@boost.foreach",
            "@boost.geometry",
            "@boost.property_map",
            "@thst",
        ],
    copts = ["-mavx2", "-mbmi2"],
    alwayslink = 1,
)

cc_test(
    name = "chan_sss_test",
    srcs = ["chan_sss_test.cc"],
    deps = [":chan_sss_test_source"],
)

cc_library(
    name = "static_pargeo",
    srcs = [
        "pargeo.cc",
    ],
    hdrs = ["adapter.hh"],
    linkstatic = False,
    visibility = ["//visibility:public"],
    deps = [
        "//app/algorithms:algorithm_base_impl_lib",
        "//benchmark",
        "@pargeo",
    ],
    alwayslink = 1,
)

cc_library(
    name = "static_parlay",
    srcs = [
        "pam.cc",
        "pkd_tree.cc",
    ],
    hdrs = ["adapter.hh"],
    visibility = ["//visibility:public"],
    deps = [
        "//app/algorithms:algorithm_base_impl_lib",
        "//benchmark",
        "@PAM",
        "@pkd_tree",
    ],
    alwayslink = 1,
)

cc_test(
    name = "ann_fr_test",
    srcs = ["ann_fr_test.cc", "ann_fr.hh"],
    deps = ["//benchmark", "//third_party/ann:ann_euclidean"],
)

cc_test(
    name = "stann_fr_test",
    srcs = ["stann_fr_test.cc", "stann_fr.hh"],
    deps = ["//benchmark", "//third_party/stann:stann_reporting"],
)

cc_test(name="esa2026_test", srcs=["esa2026_test.cc","esa2026.hh"], deps=["//benchmark","//third_party/esa2026:nanoflann"], linkopts=["-ldl"])

# Separate translation unit for the compiler-vectorized Morton scan.
cc_library(
    name = "morton_index",
    srcs = ["morton_adapter.cc"],
    hdrs = ["adapter.hh"],
    copts = ["-O3", "-march=native"],
    deps = ["//app/algorithms:algorithm_base_impl_lib", "//benchmark", "//morton"],
    alwayslink = 1,
)

# Expose the single-file index to the existing standalone test.
cc_library(
    name = "chan_sss_test_source",
    textual_hdrs = ["chan_sss.cc"],
    defines = ["DISKREPORT_CHAN_SSS_NO_REGISTRATION"],
    deps = ["//benchmark"],
)
