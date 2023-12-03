# JSON++ is header only library, so we don't need a compiler,
project('json')

add_languages('cpp', native: true)

if not get_option('json_no_install')
    install_headers('include/json.hpp')
endif

incl = include_directories('include')
json_dep = declare_dependency(include_directories: incl)
