import os

from conan import ConanFile
from conan.errors import ConanInvalidConfiguration
from conan.tools.build import check_min_cppstd, cross_building
from conan.tools.cmake import CMake, CMakeDeps, CMakeToolchain, cmake_layout
from conan.tools.files import apply_conandata_patches, copy, export_conandata_patches, get, rmdir
from conan.tools.microsoft import is_msvc, is_msvc_static_runtime
from conan.tools.scm import Version


class ConanUStore(ConanFile):

    exports = 'VERSION', 'LICENSE', 'README.md'
    exports_sources = 'CMakeLists.txt', 'src/*', 'include/*', 'cmake/*', 'VERSION'
    name = 'ustore_deps'
    version = open('VERSION').read()
    # Take just the first line
    license = open('LICENSE').read().split('\n', 1)[0]
    description = open('README.md').read()
    url = 'https://github.com/unum-cloud/ustore.git'
    homepage = 'https://unum.cloud/ustore'

    # Complete list of possible settings:
    # https://docs.conan.io/en/latest/extending/custom_settings.html
    settings = {
        # https://docs.conan.io/en/latest/introduction.html#all-platforms-all-build-systems-and-compilers
        'os': ['Linux'],
        # https://docs.conan.io/en/latest/integrations/compilers.html
        'compiler': [
            'gcc', 'clang', 'intel',
            # Not fully supported yet:
            # 'intel-cc'
        ],
        # https://github.com/conan-io/conan/issues/2500
        'build_type': ['Release'],
        'arch': ['x86', 'x86_64', 'armv8', 'armv8_32', 'armv8.3'],
    }
    generators = 'CMakeDeps', 'deploy'
    options = {'with_arrow': [True, False]}
    default_options = {
        'with_arrow': False,
    }

    def layout(self):
        cmake_layout(self)

    def generate(self):
        tc = CMakeToolchain(self)
        if cross_building(self):
            cmake_system_processor = {
                "armv8": "aarch64",
                "armv8.3": "aarch64",
            }.get(str(self.settings.arch), str(self.settings.arch))
            if cmake_system_processor == "aarch64":
                tc.variables["ARROW_CPU_FLAG"] = "armv8"

        tc.variables["ARROW_DEPENDENCY_SOURCE"] = "AUTO"
        tc.variables['ARROW_BUILD_STATIC'] = True
        tc.variables['ARROW_BUILD_SHARED'] = False
        tc.variables['ARROW_SIMD_LEVEL'] = "AVX2"
        tc.variables["ARROW_DEPENDENCY_USE_SHARED"] = False
        tc.variables["ARROW_OPENSSL_USE_SHARED"] = True

        tc.variables["ARROW_BUILD_TESTS"] = False
        tc.variables["ARROW_ENABLE_TIMING_TESTS"] = False
        tc.variables["ARROW_BUILD_EXAMPLES"] = False
        tc.variables["ARROW_BUILD_BENCHMARKS"] = False
        tc.variables["ARROW_BUILD_INTEGRATION"] = False
        tc.variables["ARROW_EXTRA_ERROR_CONTEXT"] = False

        tc.variables['ARROW_DATASET'] = True
        tc.variables['ARROW_PARQUET'] = True
        tc.variables['ARROW_WITH_RE2'] = True
        tc.variables['ARROW_COMPUTE'] = True
        tc.variables['ARROW_FLIGHT'] = True
        tc.variables['ARROW_WITH_UTF8PROC'] = True

        tc.variables["PARQUET_REQUIRE_ENCRYPTION"] = bool(
            self.options['arrow:encryption'])  # False
        tc.variables["ARROW_CUDA"] = False
        tc.variables["ARROW_JEMALLOC"] = False
        tc.variables["ARROW_IPC"] = False
        tc.variables["ARROW_JSON"] = False
        tc.variables["ARROW_CSV"] = True
        tc.variables["ARROW_FLIGHT_SQL"] = False
        tc.variables["ARROW_WITH_UCX"] = False
        tc.variables["ARROW_WITH_SNAPPY"] = True
        tc.variables["ARROW_BUILD_UTILITIES"] = bool(
            self.options['arrow:cli'])  # False
        tc.variables["ARROW_GANDIVA"] = False
        tc.variables["ARROW_S3"] = False

        tc.variables["ABS_VENDORED"] = True

        tc.variables["ARROW_DEPENDENCY_SOURCE"] = "BUNDLED"
        tc.variables["c-ares_SOURCE"] = "BUNDLED"
        tc.variables["re2_SOURCE"] = "BUNDLED"
        tc.variables["absl_SOURCE"] = "BUNDLED"
        tc.variables["Protobuf_SOURCE"] = "BUNDLED"
        tc.variables["Snappy_SOURCE"] = "BUNDLED"
        tc.variables["gRPC_SOURCE"] = "BUNDLED"
        tc.variables["ZLIB_SOURCE"] = "BUNDLED"
        tc.variables["Thrift_SOURCE"] = "BUNDLED"
        tc.variables["utf8proc_SOURCE"] = "BUNDLED"

        tc.variables["ARROW_INCLUDE_DIR"] = True
        tc.variables["ARROW_WITH_THRIFT"] = self._with_thrift()
        tc.variables["ARROW_UTF8PROC_USE_SHARED"] = False
        tc.variables["Thrift_SOURCE"] = "BUNDLED"
        if self._with_thrift():
            # a recent thrift does not require boost
            tc.variables["THRIFT_VERSION"] = bool(
                self.dependencies["thrift"].ref.version)
            tc.variables["ARROW_THRIFT_USE_SHARED"] = bool(
                self.dependencies["thrift"].options.shared)

        tc.cache_variables["ENABLE_STATIC"] = "ON"
        tc.cache_variables["ENABLE_BSON"] = "ON"
        tc.cache_variables["ENABLE_TESTS"] = "OFF"
        tc.cache_variables["ENABLE_EXAMPLES"] = "OFF"
        tc.cache_variables["ENABLE_TRACING"] = "OFF"
        tc.cache_variables["ENABLE_COVERAGE"] = "OFF"
        tc.cache_variables["ENABLE_SHM_COUNTERS"] = "OFF"
        tc.cache_variables["ENABLE_MONGOC"] = "OFF"
        tc.cache_variables["ENABLE_MAN_PAGES"] = "OFF"
        tc.cache_variables["ENABLE_HTML_DOCS"] = "OFF"
        tc.generate()


    def configure(self):
        self.options["openssl"].shared = False

        self.options["pcre2"].fPIC = True
        self.options["pcre2"].support_jit = True
        self.options["pcre2"].build_pcre2grep = True
        
        self.options['re2'].shared = False
        self.options['xsimd'].xtl_complex = False
        
        self.options['arrow'].shared = True

        
        self.options["arrow"].shared = False
        self.options['arrow'].with_grpc = 'auto'
        self.options["arrow"].with_orc = False
        self.options["arrow"].deprecated = True
        self.options["arrow"].with_protobuf = 'auto'
        self.options["arrow"].parquet = True
        self.options["arrow"].dataset_modules = True
        self.options["arrow"].with_re2 = True
        self.options["arrow"].compute = True
        self.options["arrow"].with_flight_rpc = True
        self.options["arrow"].with_utf8proc = True
        self.options["arrow"].with_openssl = True
        self.options["arrow"].encryption = False
        self.options["arrow"].with_jemalloc = False
        self.options["arrow"].with_json = False
        self.options["arrow"].with_csv = True
        self.options["arrow"].simd_level = 'avx2'
        self.options["arrow"].with_flight_sql = False
        self.options["arrow"].with_snappy = True
        self.options["arrow"].cli = True
        self.options["arrow"].gandiva = False
        self.options["arrow"].with_s3 = False
        
        self.options['grpc'].shared = False
        
        self.options['googleapis'].shared = False
        self.options['googleapis'].fPIC = False

        self.options["mongo-c-driver"].with_ssl = False
        self.options["mongo-c-driver"].with_sasl = False
        self.options["mongo-c-driver"].srv = False
        self.options["mongo-c-driver"].with_snappy = False
        self.options["mongo-c-driver"].with_zlib = False
        self.options["mongo-c-driver"].with_zstd = False
        
        self.options["leveldb"].shared = False
        self.options["leveldb"].fPIC = True
        self.options["leveldb"].with_snappy = False
        self.options["leveldb"].with_crc32c = False
        
        self.options["abseil"].shared = False
        
        # self.options["gtest"].shared = False

    def requirements(self):
        self.requires('rocksdb/7.10.2@unum/ustore')
        self.requires('arrow/10.0.0')
        self.requires('openssl/1.1.1o')
        self.requires('pcre2/10.42')
        self.requires('fmt/9.1.0')
        self.requires('mongo-c-driver/1.23.2')
        self.requires('nlohmann_json/3.11.2')
        self.requires('yyjson/0.6.0')
        self.requires('simdjson/3.1.7')
        self.requires('jemalloc/5.3.0')
        self.requires('clipp/1.2.3')
        # self.requires('gtest/1.13.0')
        self.requires('benchmark/1.7.1')
        self.requires('argparse/2.9')
        self.requires('re2/20220601')
        self.requires('xsimd/9.0.1')
        
        self.requires('leveldb/1.23')
        self.requires('grpc/1.50.1')
        self.requires('zlib/1.2.13')
        
        self.requires('abseil/20230125.3')
        self.requires('libcurl/7.80.0')
        # https://conan.io/center/openssl

    # def build(self):
    #     cmake = CMake(self)
    #     cmake.configure()
    #     cmake.build()

    def system_requirements(self):
        pass

    def package_info(self):

        self.cpp_info.components["libarrow_flight.a"].set_property(
            "pkg_config_name", "flight_rpc")
        self.cpp_info.components["libarrow_flight.a"].libs = [
            f"arrow_flight.a"]

    def package(self):
        if self.options['arrow'].shared:
            self.copy(pattern="*.dll", dst="bin", keep_path=False)
            self.copy(pattern="*.dylib", dst="lib", keep_path=False)
        self.copy(pattern="*.a", dst="lib", keep_path=False)
        self.copy(pattern="*.h", dst="lib", keep_path=False)
        self.copy(pattern="*.hpp", dst="lib", keep_path=False)
        
        # self.cpp_info.components["libarrow"].set_property("pkg_config_name", "arrow")
        # self.cpp_info.components["libarrow"].libs = [f"arrow{suffix}"]
        # if not self.options['arrow'].shared:
        #     self.cpp_info.components["libarrow"].defines = ["ARROW_STATIC"]
        #     if self.settings.os in ["Linux", "FreeBSD"]:
        #         self.cpp_info.components["libarrow"].system_libs = ["pthread", "m", "dl", "rt"]
        # if self.options['arrow'].gandiva:
        #     self.cpp_info.components["libparquet"].set_property("pkg_config_name", "parquet")
        #     self.cpp_info.components["libparquet"].libs = [f"parquet{suffix}"]
        #     self.cpp_info.components["libparquet"].requires = ["libarrow"]
        #     if not self.options.shared:
        #         self.cpp_info.components["libparquet"].defines = ["PARQUET_STATIC"]
        # if self.options['arrow'].gandiva:
        #     self.cpp_info.components["libgandiva"].set_property("pkg_config_name", "gandiva")
        #     self.cpp_info.components["libgandiva"].libs = [f"gandiva{suffix}"]
        #     self.cpp_info.components["libgandiva"].requires = ["libarrow"]
        #     if not self.options['arrow'].shared:
        #         self.cpp_info.components["libgandiva"].defines = ["GANDIVA_STATIC"]
        # if self.options['arrow'].with_flight_rpc:
        #     self.cpp_info.components["libarrow_flight"].set_property("pkg_config_name", "flight_rpc")
        #     self.cpp_info.components["libarrow_flight"].libs = [f"arrow_flight{suffix}"]
        #     self.cpp_info.components["libarrow_flight"].requires = ["libarrow"]
        # if self.options['arrow'].with_flight_sql:
        #     self.cpp_info.components["libarrow_flight_sql"].set_property("pkg_config_name", "flight_sql")
        #     self.cpp_info.components["libarrow_flight_sql"].libs = [f"arrow_flight_sql{suffix}"]
        #     self.cpp_info.components["libarrow_flight_sql"].requires = ["libarrow", "libarrow_flight"]
        # if self.options['arrow'].dataset:
        #     self.cpp_info.components["dataset"].libs = ["arrow_dataset"]
        #     if self._parquet():
        #         self.cpp_info.components["dataset"].requires = ["libparquet"]
        # if self.options['arrow'].cli and (self.options['arrow'].with_cuda or self.options['arrow'].with_flight_rpc or self.options['arrow'].parquet):
        #     binpath = os.path.join(self.package_folder, "bin")
        #     self.output.info(f"Appending PATH env var: {binpath}")
        #     self.env_info.PATH.append(binpath)
            
        # if self.options['arrow'].openssl:
        #     self.cpp_info.components["libarrow"].requires.append("openssl::openssl")
        # if self.options['with_gflags']:
        #     self.cpp_info.components["libarrow"].requires.append("gflags::gflags")
        # if self.options['arrow'].with_jemalloc:
        #     self.cpp_info.components["libarrow"].requires.append("jemalloc::jemalloc")
        # if self.options['arrow'].with_re2:
        #     if self.options['arrow'].gandiva:
        #         self.cpp_info.components["libgandiva"].requires.append("re2::re2")
        #     if self.options['arrow'].parquet:
        #         self.cpp_info.components["libparquet"].requires.append("re2::re2")
        #     self.cpp_info.components["libarrow"].requires.append("re2::re2")
        # if self.options['arrow'].with_protobuf:
        #     self.cpp_info.components["libarrow"].requires.append("protobuf::protobuf")
        # if self.options['arrow'].with_utf8proc:
        #     self.cpp_info.components["libarrow"].requires.append("utf8proc::utf8proc")
        # if self.options['arrow'].with_thrift:
        #     self.cpp_info.components["libarrow"].requires.append("thrift::thrift")
       
        # if self.options['arrow'].with_cuda:
        #     self.cpp_info.components["libarrow"].requires.append("cuda::cuda")
       
        
        # if self.options['arrow'].with_zlib:
        #     self.cpp_info.components["libarrow"].requires.append("zlib::zlib")

        # if self.options['arrow'].with_boost:
        #     self.cpp_info.components["libarrow"].requires.append("boost::boost")
        # if self.options['arrow'].with_grpc:
        #     self.cpp_info.components["libarrow"].requires.append("grpc::grpc")
        # if self.options['arrow'].with_flight_rpc:
        #     self.cpp_info.components["libarrow_flight"].requires.append("protobuf::protobuf")

    def _with_thrift(self, required=False):
        # No self.options.with_thift exists
        return bool(required or self._parquet())

    def _parquet(self, required=False):
        if required or self.options['arrow'].parquet == "auto":
            return bool(self.options.get_safe("substrait", False))
        else:
            return bool(self.options['arrow'].parquet)
