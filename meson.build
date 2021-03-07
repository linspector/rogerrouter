project('roger', 'c', version : '2.2.1', license : 'GPL2')

gnome = import('gnome')

msgfmt = find_program('msgfmt')

result = run_command(msgfmt, ['--version'])

app_version = meson.project_version()

version_arr = app_version.split('.')
app_version_major = version_arr[0]
app_version_minor = version_arr[1]
app_version_micro = version_arr[2]

cc = meson.get_compiler('c')

#intltool_merge = find_program('intltool-merge')

cdata = configuration_data()
cdata.set('PACKAGE_NAME', '"Roger Router"')
cdata.set('PACKAGE_VERSION', '"@0@"'.format(app_version))
cdata.set('PACKAGE_BUGREPORT', '"https://www.tabos.org/forum"')
cdata.set('GETTEXT_PACKAGE', '"roger"')

host_system = host_machine.system()

if host_system == 'windows'
cdata.set('APP_PLUGINS', '"' + get_option('libdir') + '/roger/"')
cdata.set('APP_DATA', '"' + get_option('datadir') + '/roger/"')
cdata.set('APP_LOCALE', '"' + get_option('localedir') + '"')
else
cdata.set('APP_PLUGINS', '"' + get_option('prefix') + '/' + get_option('libdir') + '/roger/"')
cdata.set('APP_DATA', '"' + get_option('prefix') + '/' + get_option('datadir') + '/roger/"')
cdata.set('APP_LOCALE', '"' + get_option('prefix') + '/' + get_option('localedir') + '"')
endif

gslib = cc.find_library('gs')

check_headers = [['dlfcn.h','HAVE_DLFCN_H'],
  ['sys/utsname.h', 'HAVE_SYS_UTSNAME_H'],
]

foreach h : check_headers
  if cc.has_header(h.get(0))
    cdata.set(h.get(1), 1)
  endif
endforeach

configure_file(
  output : 'config.h',
  configuration : cdata)

librm_dep = dependency('librm', version : '>=2.1.4', fallback : ['librm', 'rm_dep'])
libhandy_dep = dependency('libhandy-1', version: '>= 0.90.0')

subdir('po')
subdir('share')
subdir('plugins')
subdir('platform')
subdir('src')

if get_option('enable-post-install')
  meson.add_install_script('meson_post_install.sh')
endif

#rpm = import('rpm')
#rpm.generate_spec_template()
