windows = import('windows')

# Setup nsis installation file
cdata = configuration_data()
cdata.set('PACKAGE_NAME', 'Roger Router')
cdata.set('PACKAGE_VERSION', '@0@'.format(meson.project_version()))
cdata.set('PACKAGE_BUGREPORT', '"https://www.tabos.org/forum"')
cdata.set('ROGER_VERSION_MAJOR', '2')
cdata.set('ROGER_VERSION_MINOR', '3')
cdata.set('ROGER_VERSION_MICRO', '90')

configure_file(input : 'roger.nsi.in',
  output : 'roger.nsi',
  configuration : cdata)

ico_writer = find_program('gen-ico.py')

ico = custom_target('makeico',
        input : 'roger.ico.in',
        output : 'roger.ico',
        command : [ico_writer, '@INPUT@', '@OUTPUT@'],
        install : false)

roger_win_rc = configure_file(
  input: 'roger_winres.rc.in',
  output: 'roger_winres.rc',
  configuration : cdata,
)

roger_win_res = windows.compile_resources(
  roger_win_rc,
  depends: ico,
  include_directories: include_directories('.')
)
