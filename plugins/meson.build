plugins_dep=[]
plugins_dep += dependency('gtk+-3.0')
plugins_dep += dependency('libsoup-2.4')
plugins_dep += dependency('libhandy-1')
plugins_dep += librm_dep
plugins_dep += libhandy_dep

plugins_inc = include_directories('.', '..')

subdir('google')

if host_machine.system().contains('linux')
subdir('indicator')
subdir('evolution')
endif

subdir('keychain')
subdir('osxab')


subdir('thunderbird')
subdir('vcard')

