vcard_sources = [
  'vcard.c'
]
vcard_deps = [
  plugin_deps
]
vcard_includes = [
  plugin_includes
]

libvcard = shared_module('vcard',
                        vcard_sources,
                        include_directories : vcard_includes,
                        dependencies : vcard_deps,
                        install : true,
                        install_dir : get_option('prefix') + '/' + get_option('libdir') + '/roger/vcard/')

custom_target('vcard.plugin',
    output : 'vcard.plugin',
    input : 'vcard.desktop.in',
    command : [msgfmt, '--desktop', '--template', '@INPUT@', '-d', podir, '-o', '@OUTPUT@'],
    install : true,
    install_dir : get_option('prefix') + '/' + get_option('libdir') + '/roger/vcard/')

install_data('org.tabos.roger.plugins.vcard.gschema.xml', install_dir : 'share/glib-2.0/schemas')
