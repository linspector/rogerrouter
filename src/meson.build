resource_files = files('resources/roger.gresource.xml')
resources = gnome.compile_resources('roger-resources',
  resource_files,
  c_name: 'roger',
  source_dir: 'resources'
)

types_headers = ['preferences/preferences-telephony.h']

enums = gnome.mkenums_simple('roger-type-builtins',
  sources: types_headers
)

roger_sources = [
  'contrib/gd-two-lines-renderer.c',
  'preferences/preferences.c',
  'preferences/preferences-audio.c',
  'preferences/preferences-telephony.c',
  'preferences/preferences-plugins.c',
  'contacts.c',
  'journal.c',
  'roger-assistant.c',
  'roger-contactsearch.c',
  'roger-debug.c',
  'roger-fax.c',
  'roger-phone.c',
  'roger-print.c',
  'roger-settings.c',
  'roger-shell.c',
  'roger-voice-mail.c',
  resources,
  enums
]

if host_system == 'windows'
  roger_sources += roger_win_res
endif

roger_deps = [
  config_h,
  gtk3_dep,
  libsoup_dep,
  libtiff_dep,
  librm_dep,
  libm_dep,
  libgs_dep,
  libhandy_dep
]

roger_includes = include_directories(
  '.',
  '..',
  'contrib',
  'preferences'
)

executable('roger',
  roger_sources,
  dependencies: roger_deps,
  include_directories: roger_includes,
  install: true
)

