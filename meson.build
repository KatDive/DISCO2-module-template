project('firsttest', 'c', 'cpp')
project_name = 'firsttest'

# Compiler and linker flags
cflags = ['-Wall']

# Dependencies
proto_c_dep = meson.get_compiler('c').find_library('protobuf-c', required: true)
rt_dep = meson.get_compiler('c').find_library('rt', required: true)  # Added for real-time support

# Source files
sources = [
    'src/firsttest.c',  # Module file (REMEMBER TO CHANGE!)
    'src/utils/memory_util.c',
    'src/utils/config_util.c',
    'src/utils/batch_util.c',
    'src/utils/error_util.c',
    'src/utils/metadata_util.c',
    'src/utils/metadata.pb-c.c',
    'src/utils/logger.c'  # Added logger.c for consistency
]

dirs = include_directories(
    'src/include',
    'src/include/utils'
)

# Shared library (SO)
shared_library(project_name, sources,
    include_directories: dirs,
    c_args: cflags + ['-DSHARED_MEMORY=1'],
    dependencies: [proto_c_dep]
)

# **Emulator Executable**
emulator_sources = [
    'src/emulate_camera.c',
    'src/utils/memory_util.c',
    'src/utils/config_util.c',  # Added for consistency with original
    'src/utils/batch_util.c',   # Added for consistency with original
    'src/utils/error_util.c',   # Added for consistency with original
    'src/utils/metadata_util.c',
    'src/utils/metadata.pb-c.c',
    'src/utils/logger.c'  # Added for debugging consistency
]

executable('emulate_camera', emulator_sources,
    include_directories: dirs,
    c_args: cflags + ['-g'],
    dependencies: [proto_c_dep, rt_dep]
)

# Check for cross-compiling
if not get_option('cross-compile')
    # If not cross-compiling, include the executable target
    sources += 'src/test.c'
    sources += 'src/utils/yaml_parser.c'

    libyaml_dep = dependency('yaml-0.1')
    m_dep = meson.get_compiler('c').find_library('m', required: false)

    # Executable
    executable(project_name + '-exec', sources,
        include_directories: dirs,
        c_args: cflags + ['-g', '-DSHARED_MEMORY=0'],
        dependencies: [libyaml_dep, m_dep, proto_c_dep]
    )

    # **Run the emulator as a test**
    test('run_emulator',
        find_program('emulate_camera'),
        args: ['1', '0', meson.project_source_root() + '/image1.bayerRG'],
        workdir: meson.project_source_root()
    )

endif
