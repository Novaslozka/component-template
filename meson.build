# (0) choose an appropriate name for the component
project('comp-templ', 'c', version : '0.1', default_options : 'default_library=shared', meson_version : '>=0.61.2')
project_name = 'comp-templ'

# (1) enable SALLY CSP support, if needed
#     you will have to make sure that libcsp is available!
component_deps = []
if get_option('enable_csp')
  component_deps += dependency('csp')
endif

platform = get_option('build_platform')
class = get_option('build_class')
backend_defs = []
backend_src = files()

# (2) create the platform-specific configuration
if platform == 'Linux'
  # (2.1) set platform-specific backend sources
  backend_src += files(
    'lib/sally-backends/fs/posix_backend.c',
    'lib/sally-backends/os/posix_backend.c',
    'lib/sally-backends/com/posix_tty_backend.c',
    'lib/sally-backends/com/memcom_backend.c',
  )

  # (2.2) set the definitions that can be used in code
  backend_defs += '-DFS_BACKEND=backend_fs_posix'
  backend_defs += '-DOS_BACKEND=backend_os_posix'
  backend_defs += '-DCOM_BACKEND=backend_posix_tty'

  # (2.3) set the libraries that are needed for backends 
  component_deps += dependency('threads')

  # (2.4) enable building autotest binaries for this platform (set by option)
  # (2.5) disable building commandline commands for this platform (set by option)

else
  warning('Unknown platform: "' + platform + '", build will likely fail!')

endif

# (3) explicitly set the sources
client_lib_src = files('cli/src/client.c')
server_lib_src = files('srv/src/server.c')

# (3.1) add sources common to both client and server
# NOTE: if lpldgen is used, do not add it here, as it is handled automatically!
common_src = files('common/src/common.c')

# (3.2) if client commands are to be built, specify the sources and add console dep
if get_option('build_cmdline') != 'none'
  client_cmd_src = files('cli/cmd/cli_cmd.c')
  component_deps += dependency('console')
endif

# (3.3) if some autotests are to be built, specify the sources
if get_option('build_autotest')
  client_bin_src = files('autotest/cli/client.c')
  server_bin_src = files('autotest/srv/server.c')
  combined_bin_src = files('autotest/comb/combined.c')
endif

# (4) set the project-specific configuration
#     this takes options from meson_options.txt (or provided from commandline)
#     and builds the configuration file based on the .in template
common_config_path = 'common/include/templ'
common_config = configuration_data()
common_config.set('TEMPLATE_OPTION', get_option('template_option'))

# (5) any miscellanous user configuration should go here
#     e.g., additional dependencies based on user options

# (6) end of user configuration

# get the git revision
gitrev = 'unknown'
res = run_command(['git', 'describe', '--always', '--dirty'], capture : true, check : false)
if res.returncode() != 0
  warning('Failed to parse git revision, most likely not running in git repository!')
else
  gitrev = res.stdout()
endif

add_project_arguments(
  '-DVERSION="@0@"'.format(meson.project_version()),
  '-DGITREV="@0@"'.format(gitrev.strip()),
  language : 'c',
)

# SALLY is always needed
component_deps += dependency('srv-abs')

# check build class
warning('Unknown build class: "' + class + '", build will likely fail!')

# set paths to the different build classes
# since meson doesn't allow globbing, there are several variables to keep track of source files:
# - client_lib_src: sources for the client library
# - client_bin_src: sources for the client autotest binary
# - server_lib_src: sources for the server library
# - server_bin_src: sources for the server autotest binary
# - client_cmd_src: sources for the client commands
# - combined_bin_src: sources for the combined autotest binary

inc_dirs = ['common/include']
lib_src = backend_src + common_src
bin_src = backend_src + common_src
lib_name = project_name
if class == 'client'
  inc_dirs += 'cli/include'
  lib_src += client_lib_src
  if get_option('build_cmdline') != 'none'
    lib_src += client_cmd_src
  endif

  lib_name += '-cli'
  if get_option('build_autotest')
    bin_src += client_bin_src
  endif

elif class == 'server'
  inc_dirs += 'srv/include'
  lib_src += server_lib_src
  lib_name += '-srv'
  if get_option('build_autotest')
    bin_src += server_bin_src
  endif

elif class == 'combined'
  inc_dirs += 'cli/include'
  inc_dirs += 'srv/include'
  bin_src += combined_bin_src

endif

# configure lpldgen if it is used
if get_option('enable_lpldgen')
  pld = dependency('pld')
  component_deps += pld

  if get_option('build_cmdline') != 'none'
    component_deps += dependency('commands')
  endif

  lpldgen = find_program(pld.get_variable('lpldgen'), native : true)
  pld_src = custom_target(
    'pld_' + project_name,
    input : 'common/lpldgen/config.json',
    output : ['pld_' + project_name + '.h', 'pld_' + project_name + '.c'],
    depend_files : ['common/lpldgen/module.json'],
    command : [lpldgen, '--config', '@INPUT@', '--output-inc', '@OUTPUT0@', '--output-src', '@OUTPUT1@'],
  )

  lib_src += pld_src
  bin_src += pld_src
  common_src += pld_src
  inc_dirs += 'common/lpldgen/include'
endif

# set name for the static library and autotest executable
if get_option('build_autotest')
  autotest_exe = project_name + '-bin'
endif

# install the project-specific configuration
subdir(common_config_path)
install_subdir(common_config_path, install_dir : get_option('includedir'), exclude_files : 'meson.build')

# add both targets
if class == 'client' or class == 'server'
  # non-combined build, prepare the library
  if get_option('default_library') == 'shared' or get_option('build_cmdline') == 'plugin'
    backend_defs += '-DSHARED_BUILD'
  endif

  if get_option('build_cmdline') == 'plugin'
    # when building as plugin, we need to enfore shared library
    # because default_library will be set to static
    lib = shared_library(
            lib_name,
            lib_src, 
            dependencies: component_deps,
            include_directories: inc_dirs,
            c_args: backend_defs
          )
  
  else
    lib = library(
            lib_name,
            lib_src, 
            dependencies: component_deps,
            include_directories: inc_dirs,
            c_args: backend_defs,
            install : get_option('install_lib')
          )
    if get_option('install_lib')
      foreach dir : inc_dirs
        install_subdir(dir, install_dir : get_option('includedir'), strip_directory: true, exclude_files : 'meson.build')
      endforeach
    endif

  endif
  
  # if this is an autotest build, create the client/server executable as well
  if get_option('build_autotest')
    inc_dirs += 'autotest/munit'
    bin_src += files('autotest/munit/munit.c')
    exe = executable(lib_name + '-bin', bin_src,
            dependencies: component_deps,
            include_directories : inc_dirs,
            c_args: backend_defs,
            link_with: lib)
  endif

  # if the component is being built as statically linked library,
  # it needs to be declared as a dependency
  if get_option('default_library') == 'static'
    lib_dep = declare_dependency(
      include_directories : inc_dirs,
      link_with : lib
    )
    meson.override_dependency(lib_name, lib_dep)

    pkgconfig = import('pkgconfig')
    pkgconfig.generate(lib)
  endif

else
  # for combined build, we need server and client libs and combined executable
  lib_cli = library(
              project_name + '-cli',
              client_lib_src + common_src, 
              dependencies: component_deps,
              include_directories: inc_dirs,
              c_args: backend_defs
            )
  
  lib_srv = library(
              project_name + '-srv',
              server_lib_src + common_src, 
              dependencies: component_deps,
              include_directories: inc_dirs,
              c_args: backend_defs
            )
  
  inc_dirs += 'autotest/munit'
  bin_src += files('autotest/munit/munit.c')
  exe = executable(
          autotest_exe,
          bin_src,
          dependencies: component_deps,
          include_directories : inc_dirs,
          c_args: backend_defs,
          link_with: [lib_cli, lib_srv]
        )

endif

