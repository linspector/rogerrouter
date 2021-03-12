resource_files = files('resources/roger.gresource.xml')
resources = gnome.compile_resources('roger-resources',
  resource_files,
  c_name : 'roger',
  source_dir : 'resources'
)

types_headers = ['preferences/preferences-telephony.h']

enums = gnome.mkenums_simple('roger-type-builtins',
  sources: types_headers
)

sourcelist = []
sourcelist += 'contrib/gd-two-lines-renderer.c'
sourcelist += 'contrib/gd-two-lines-renderer.h'
sourcelist += 'preferences/preferences.c'
sourcelist += 'preferences/preferences.h'
sourcelist += 'preferences/preferences-audio.c'
sourcelist += 'preferences/preferences-audio.h'
sourcelist += 'preferences/preferences-telephony.c'
sourcelist += 'preferences/preferences-telephony.h'
sourcelist += 'preferences/preferences-plugins.c'
sourcelist += 'preferences/preferences-plugins.h'
sourcelist += 'application.c'
sourcelist += 'application.h'
sourcelist += 'contacts.c'
sourcelist += 'contacts.h'
sourcelist += 'journal.c'
sourcelist += 'journal.h'
sourcelist += 'roger-assistant.c'
sourcelist += 'roger-assistant.h'
sourcelist += 'roger-contactsearch.c'
sourcelist += 'roger-contactsearch.h'
sourcelist += 'roger-debug.c'
sourcelist += 'roger-debug.h'
sourcelist += 'roger-fax.c'
sourcelist += 'roger-fax.h'
sourcelist += 'roger-phone.c'
sourcelist += 'roger-phone.h'
sourcelist += 'roger-print.c'
sourcelist += 'roger-print.h'
sourcelist += 'roger-voice-mail.c'
sourcelist += 'roger-voice-mail.h'
sourcelist += resources
sourcelist += enums

if host_system == 'windows'
  sourcelist += roger_win_res
endif

roger_dep = []
roger_dep += dependency('gtk+-3.0', version : '>=3.22.0')
roger_dep += dependency('libsoup-2.4')
roger_dep += dependency('libtiff-4')
roger_dep += librm_dep
roger_dep += cc.find_library('m', required: true)
roger_dep += cc.find_library('gs', required: false)
roger_dep += libhandy_dep

roger_inc = include_directories('.', '..', 'contrib', 'preferences')

executable('roger', sources : sourcelist, dependencies : roger_dep, include_directories : roger_inc, gui_app : true, install : true)

i18n.merge_file(
    input: 'org.tabos.roger.desktop.in',
    output: 'org.tabos.roger.desktop',
    type: 'desktop',
    po_dir: '../po',
    install: true,
    install_dir: join_paths(get_option('datadir'), 'applications')
)

i18n.merge_file(
    input: 'org.tabos.roger.appdata.xml.in',
    output: 'org.tabos.roger.appdata.xml',
    type: 'xml',
    po_dir: '../po',
    install: true,
    install_dir: join_paths(get_option('datadir'), 'appdata')
)

images = []
images += 'resources/images/org.tabos.roger.svg'
install_data(images, install_dir : get_option('datadir') + '/icons/hicolor/scalable/apps/')

images = []
images += 'resources/images/org.tabos.roger-symbolic.svg'
install_data(images, install_dir : get_option('datadir') + '/icons/hicolor/symbolic/apps/')

subdir('data')
