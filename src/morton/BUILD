cc_library(
    name = "morton",
    textual_hdrs = ["2d/morton.cc", "2d/mortonSIMD.cc", "3d/morton3D.cc", "3d/mortonSIMD3D.hpp", "d_dim/morton_d_dim.cc", "2d/mortonplus.hpp", "3d/mortonplus3d.hpp", "d_dim/mortonplus_d_dim.hpp"],
    visibility = ["//visibility:public"],
)

cc_test(
    name = "morton_ddim_test",
    srcs = ["d_dim/morton_d_dim_test.cc"],
    deps = [":morton"],
    copts = ["-std=c++20", "-mavx2"],
)

cc_test(name="mortonplus_test", srcs=["2d/mortonplus_test.cc"], deps=[":morton"], copts=["-std=c++20", "-mavx512f", "-mbmi2"], linkopts=["-pthread"])

cc_test(name="mortonplus3d_test", srcs=["3d/mortonplus3d_test.cc"], deps=[":morton"], copts=["-std=c++20", "-mavx512f", "-mbmi2"], linkopts=["-pthread"])

cc_test(name="mortonplus_ddim_test", srcs=["d_dim/mortonplus_d_dim_test.cc"], deps=[":morton"], copts=["-std=c++20", "-mavx2"])

cc_test(name="morton_simd_batch_test", srcs=["2d/morton_simd_batch_test.cc"], deps=[":morton"], copts=["-std=c++20", "-mavx512f", "-mbmi2"])

cc_test(name="morton_test", srcs=["2d/morton_test.cc"], deps=[":morton"], copts=["-std=c++20", "-mbmi2"])

cc_test(name="morton_3d_variants_test", srcs=["3d/morton_3d_variants_test.cc"], deps=[":morton"], copts=["-std=c++20", "-mavx512f", "-mbmi2"])
