if host_machine.system() == 'windows'

gtknotify_gresources = gnome.compile_resources(
  'gtknotify-gresources', 'gtknotify.gresource.xml',
  source_dir : '.',
  c_name : '')


gtknotify_sources = []
gtknotify_sources += 'gtknotify.c'
gtknotify_sources += gtknotify_gresources

gtknotify_dep = []
gtknotify_dep += plugin_deps

gtknotify_inc = [plugin_includes]

libgtknotify = shared_module('gtknotify',
                        gtknotify_sources,
                        include_directories : gtknotify_inc,
                        dependencies : gtknotify_dep,
                        install : true,
                        install_dir : get_option('prefix') + '/' + get_option('libdir') + '/roger/gtknotify/')

custom_target('gtknotify.plugin',
    output : 'gtknotify.plugin',
    input : 'gtknotify.desktop.in',
    command : [msgfmt, '--desktop', '--template', '@INPUT@', '-d', podir, '-o', '@OUTPUT@'],
    install : true,
    install_dir : get_option('prefix') + '/' + get_option('libdir') + '/roger/gtknotify/')

endif
