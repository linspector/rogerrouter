ebook_dep = dependency('libebook-1.2', required: false)

if ebook_dep.found()
  evolution_sources = [
    'evolution.c'
  ]
  evolution_deps = [
    plugin_deps,
    ebook_dep
  ]
  evolution_includes = [include_directories('../../..'), plugin_includes]

  libevolution = shared_module('evolution',
                          evolution_sources,
                          include_directories : evolution_includes,
                          dependencies : evolution_deps,
                          install : true,
                          install_dir : join_paths(get_option('prefix'), get_option('libdir'), 'roger', 'evolution'))

  custom_target('evolution.plugin',
      output : 'evolution.plugin',
      input : 'evolution.desktop.in',
      command : [msgfmt, '--desktop', '--template', '@INPUT@', '-d', podir, '-o', '@OUTPUT@'],
      install : true,
      install_dir : join_paths(get_option('prefix'), get_option('libdir'), 'roger', 'evolution'))

  install_data('org.tabos.roger.plugins.evolution.gschema.xml', install_dir : join_paths(get_option('datadir'), 'glib-2.0', 'schemas'))
endif

