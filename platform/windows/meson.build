# Setup nsis installation file
cdata = configuration_data()
cdata.set('PACKAGE_NAME', 'Roger Router')
cdata.set('PACKAGE_VERSION', '@0@'.format(app_version))
cdata.set('PACKAGE_BUGREPORT', '"https://www.tabos.org/forum"')

configure_file(input : 'roger.nsi.in',
  output : 'roger.nsi',
  configuration : cdata)

ico_writer = find_program('gen-ico.py')

ico = custom_target('makeico',
        input : 'roger.ico.in',
        output : 'roger.ico',
        command : [ico_writer, '@INPUT@', '@OUTPUT@'],
        install : false,
        build_always : false)
