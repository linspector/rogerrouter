thunderbird_sources = [
  'thunderbird.c'
]
thunderbird_deps = [
  plugin_deps
]
thunderbird_includes = [
  plugin_includes
]

libthunderbird = shared_module('thunderbird',
                        thunderbird_sources,
                        include_directories : thunderbird_includes,
                        dependencies : thunderbird_deps,
                        install : true,
                        install_dir : get_option('prefix') + '/' + get_option('libdir') + '/roger/thunderbird/')

custom_target('thunderbird.plugin',
    output : 'thunderbird.plugin',
    input : 'thunderbird.desktop.in',
    command : [msgfmt, '--desktop', '--template', '@INPUT@', '-d', podir, '-o', '@OUTPUT@'],
    install : true,
    install_dir : get_option('prefix') + '/' + get_option('libdir') + '/roger/thunderbird/')

install_data('org.tabos.roger.plugins.thunderbird.gschema.xml', install_dir : 'share/glib-2.0/schemas')
