plugin_deps = [
  gtk3_dep,
  libsoup_dep,
  libhandy_dep,
  librm_dep,
]

plugin_includes = include_directories('.', '..')

subdir('google')

if host_machine.system().contains('linux')
  subdir('evolution')
endif

subdir('keychain')
subdir('osxab')

subdir('thunderbird')
subdir('vcard')

