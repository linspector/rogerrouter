project('roger', 'c',
  version: '2.5.90',
  license : 'GPL2',
  meson_version: '>= 0.53.0',
  default_options: ['c_std=gnu11',
                    'warning_level=2']
)

gnome = import('gnome')
i18n = import('i18n')
msgfmt = find_program('msgfmt')

application_id = 'org.tabos.roger'
prefix = get_option('prefix')
datadir = join_paths(prefix, get_option('datadir'))

conf = configuration_data()
conf.set_quoted('PACKAGE_NAME', 'Roger Router')
conf.set_quoted('PACKAGE_VERSION', '@VCS_TAG@')
conf.set_quoted('GETTEXT_PACKAGE', meson.project_name())

host_system = host_machine.system()
if host_system == 'windows'
  conf.set_quoted('APP_PLUGINS', join_paths(get_option('libdir'), 'roger'))
  conf.set_quoted('APP_DATA', join_paths(get_option('datadir'), 'roger'))
  conf.set_quoted('APP_LOCALE', join_paths(get_option('localedir')))
else
  conf.set_quoted('APP_PLUGINS', join_paths(get_option('prefix'), get_option('libdir'), 'roger'))
  conf.set_quoted('APP_DATA', join_paths(get_option('prefix'), get_option('datadir'), 'roger'))
  conf.set_quoted('APP_LOCALE', join_paths(get_option('prefix'), get_option('localedir')))
endif

check_headers = [['dlfcn.h','HAVE_DLFCN_H'],
  ['sys/utsname.h', 'HAVE_SYS_UTSNAME_H'],
]

cc = meson.get_compiler('c')
foreach h : check_headers
  if cc.has_header(h.get(0))
    conf.set10(h.get(1), 1)
  endif
endforeach

config_h = declare_dependency(
  sources: vcs_tag(
    input: configure_file(
      output: 'config.h.in',
      configuration: conf
    ),
    output: 'config.h'
  )
)

gtk_dep = dependency('gtk4', version : '>=4.0.0')
libadwaita_dep = dependency('libadwaita-1')
libgs_dep = cc.find_library('gs')
libm_dep = cc.find_library('m', required: true)
librm_dep = dependency('librm', version : '>=2.2.3', fallback : ['librm', 'rm_dep'])
libsoup_dep = dependency('libsoup-2.4')
libtiff_dep = dependency('libtiff-4')

cflags = cc.get_supported_arguments(['-Werror=implicit-function-declaration',
                                     '-Wno-cast-function-type',
                                     '-Wno-missing-field-initializers',
                                     '-Wno-unused-parameter'])
add_project_arguments(cflags,
                      language: 'c')

subdir('build-aux')
subdir('data')
subdir('po')
subdir('plugins')
subdir('src')

if get_option('enable-post-install')
  meson.add_install_script('post_install.py')
endif
