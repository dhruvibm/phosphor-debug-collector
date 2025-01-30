gtest_dep = dependency('gtest', main: true, disabler: true, required: false)
gmock_dep = dependency('gmock', disabler: true, required: false)
if not gtest_dep.found() or not gmock_dep.found()
    gtest_proj = import('cmake').subproject('googletest', required: false)
    if gtest_proj.found()
        gtest_dep = declare_dependency(
            dependencies: [
                dependency('threads'),
                gtest_proj.dependency('gtest'),
                gtest_proj.dependency('gtest_main'),
            ],
        )
        gmock_dep = gtest_proj.dependency('gmock')
    else
        assert(
            not get_option('tests').allowed(),
            'Googletest is required if tests are enabled',
        )
    endif
endif

tests = {
    'core_dump_retry_test': ['../core_dump_retry.cpp'],
    'debug_inif_test': ['../dump_serialize.cpp'],
    'op_dump_policy_test': [],
}

foreach t, sources : tests
    test(
        t,
        executable(
            t.underscorify(),
            t + '.cpp',
            sources,
            include_directories: ['.', '../'],
            implicit_include_directories: false,
            dependencies: [
                gtest_dep,
                gmock_dep,
                phosphor_dbus_interfaces_dep,
                phosphor_logging_dep,
                cereal_dep,
            ],
        ),
        workdir: meson.current_source_dir(),
    )
endforeach
