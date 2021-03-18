project('roger', 'c', version : '2.2.1', license : 'GPL2')

gnome = import('gnome')
i18n = import('i18n')
msgfmt = find_program('msgfmt')

app_version = meson.project_version()
version_arr = app_version.split('.')
app_version_major = version_arr[0]
app_version_minor = version_arr[1]
app_version_micro = version_arr[2]

application_id = 'org.tabos.roger'
prefix = get_option('prefix')
datadir = join_paths(prefix, get_option('datadir'))

conf = configuration_data()
conf.set('PACKAGE_NAME', '"Roger Router"')
conf.set('PACKAGE_VERSION', '"@0@"'.format(app_version))
conf.set('PACKAGE_BUGREPORT', '"https://www.tabos.org/forum"')
conf.set('GETTEXT_PACKAGE', '"roger"')

host_system = host_machine.system()
if host_system == 'windows'
  conf.set('APP_PLUGINS', '"' + get_option('libdir') + '/roger/"')
  conf.set('APP_DATA', '"' + get_option('datadir') + '/roger/"')
  conf.set('APP_LOCALE', '"' + get_option('localedir') + '"')
else
  conf.set('APP_PLUGINS', '"' + get_option('prefix') + '/' + get_option('libdir') + '/roger/"')
  conf.set('APP_DATA', '"' + get_option('prefix') + '/' + get_option('datadir') + '/roger/"')
  conf.set('APP_LOCALE', '"' + get_option('prefix') + '/' + get_option('localedir') + '"')
endif

check_headers = [['dlfcn.h','HAVE_DLFCN_H'],
  ['sys/utsname.h', 'HAVE_SYS_UTSNAME_H'],
]

cc = meson.get_compiler('c')
foreach h : check_headers
  if cc.has_header(h.get(0))
    conf.set(h.get(1), 1)
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

gtk3_dep = dependency('gtk+-3.0', version : '>=3.22.0')
libgs_dep = cc.find_library('gs', required: false)
libhandy_dep = dependency('libhandy-1', version: '>= 0.90.0')
libm_dep = cc.find_library('m', required: true)
librm_dep = dependency('librm', version : '>=2.1.4', fallback : ['librm', 'rm_dep'])
libsoup_dep = dependency('libsoup-2.4')
libtiff_dep = dependency('libtiff-4')

cflags = cc.get_supported_arguments(['-Werror=implicit-function-declaration',
                                     '-Wno-cast-function-type',
                                     '-Wno-missing-field-initializers',
                                     '-Wno-unused-parameter'])
add_project_arguments(cflags,
                      language: 'c')

subdir('data')
subdir('po')
subdir('plugins')
subdir('platform')
subdir('src')

if get_option('enable-post-install')
  meson.add_install_script('meson_post_install.sh')
endif

