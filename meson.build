# Follow: https://github.com/tiernemi/meson-sample-project

project('p4db', 'cpp',
    version: '1.0',
    meson_version : '>= 0.55.0',
    default_options: ['warning_level=3', 'buildtype=release', 'b_lto=true', 'cpp_std=c++2a'],
    # default_options: ['warning_level=3', 'buildtype=release', 'cpp_std=c++2a'],
    # default_options: ['warning_level=3', 'buildtype=debugoptimized', 'cpp_std=c++2a'],
    # default_options: ['warning_level=3', 'buildtype=debug', 'b_sanitize=address', 'cpp_std=c++2a'],
    # default_options: ['warning_level=3', 'buildtype=debug', 'b_sanitize=address,undefined', 'cpp_std=c++2a'],
)
add_global_arguments('-march=native', language : ['cpp', 'c']) #vs add_global_link_arguments
# add_project_arguments ???
# add_global_arguments('-fno-omit-frame-pointer', language : ['cpp', 'c'])
# add_global_arguments('-g', language : ['cpp', 'c'])
# add_global_arguments('-fmax-errors=1', language : ['cpp', 'c'])

# add_global_arguments('-mno-tbm', language : ['cpp', 'c'])
# add_global_arguments('-DRTE_LIBRTE_MBUF_DEBUG', language : ['cpp', 'c'])
# add_global_arguments('-DRTE_LIBRTE_MEMPOOL_DEBUG', language : ['cpp', 'c'])
# add_global_link_arguments('-DRTE_LIBRTE_MBUF_DEBUG', language : ['cpp', 'c'])
# add_global_link_arguments('-DRTE_LIBRTE_MEMPOOL_DEBUG', language : ['cpp', 'c'])

# disable in config/common_base
# CONFIG_RTE_MBUF_DEBUG=n
# CONFIG_RTE_LIBRTE_MEMPOOL_DEBUG=y



project_headers = []
project_sources = []
project_includes = []
project_libs = []
project_deps = []



threads_dep = dependency('threads')

fmt_proj = subproject('fmt')
fmt_dep = fmt_proj.get_variable('fmt_dep')

cxxopts_proj = subproject('cxxopts')
cxxopts_dep = cxxopts_proj.get_variable('cxxopts_dep')

dpdk_dep = dependency('libdpdk', required: true)
# dpdk_proj = subproject('dpdk')
# dpdk_dep = dpdk_proj.get_variable('static_dep')
# dpdk_dep = dependency('libdpdk', fallback : ['dpdk', 'static_dep'])






cxx = meson.get_compiler('cpp')
# tbb_dep = cxx.find_library('tbb', required : true)


# python3 = find_program('python3', 'python')
# res = run_command(python3, '-c', 'import os; print(os.environ["VTUNE_PROFILER_2020_DIR"], end="")')
# if res.returncode() == 0
#     VTUNE_DIR = res.stdout()
#     message('Found Vtune: ', VTUNE_DIR)
#     inc +=  join_paths(VTUNE_DIR, 'include/')

#     cpp = meson.get_compiler('cpp')
#     vtune_dep = cpp.find_library('libittnotify', required: true, dirs: [
#         join_paths(VTUNE_DIR, 'sdk/lib64/')
#     ])
#     dl_dep = cpp.find_library('dl', required: true)
# else 
#     message('VTUNE_PROFILER_2020_DIR not set')
# endif


project_includes += [
    include_directories('src'),
    fmt_proj.get_variable('fmt_inc')
]

project_deps += [
    threads_dep,
    fmt_dep,
    cxxopts_dep,
    dpdk_dep,
    # tbb_dep
    # vtune_dep,
    # dl_dep
]




subdir('src')


p4db_bin = executable('p4db', 
    main_source + project_sources, 
    include_directories : project_includes, 
    dependencies : project_deps,
    link_with : project_libs
)


# This adds the clang format file to the build directory
configure_file(input : '.clang-format',
               output : '.clang-format',
	       copy: true)
run_target('format',
  command : ['clang-format','-i','-style=file', main_source, project_sources, project_headers])

# # This regex excludes any sources from the third_party, tests, benchmarks 
# # and gtest related files.
# regex = '^((?!(third_party|tests|benchmarks|gtest)).)*$'
# # This adds clang tidy support
# configure_file(input : '.clang-tidy',
#                output : '.clang-tidy',
#                copy : true)
# run_target('tidy',
#   command : ['run-clang-tidy.py','-fix', '-j' , '8', 'files', regex ,'-format', '-p='+ meson.build_root()])


 #clang-tidy --checks='clang-diagnostic-*,clang-analyzer-*,-*,bugprone*,modernize*,performance*,-modernize-pass-by-value,-modernize-use-auto,-modernize-use-using,-modernize-use-trailing-return-type,-modernize-avoid-c-arrays' --header-filter='.*' -p=. ../src/main.cpp

