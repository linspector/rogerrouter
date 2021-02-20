resource_files = files('resources/roger.gresource.xml')
resources = gnome.compile_resources('roger-resources',
  resource_files,
  c_name : 'roger',
  source_dir : 'resources'
)

types_headers = ['preferences-telephony.h']

enums = gnome.mkenums_simple('roger-type-builtins',
  sources: types_headers
)

sourcelist = []
sourcelist += 'about.c'
sourcelist += 'about.h'
sourcelist += 'main_ui.c'
sourcelist += 'answeringmachine.c'
sourcelist += 'answeringmachine.h'
sourcelist += 'application.c'
sourcelist += 'application.h'
sourcelist += 'assistant.c'
sourcelist += 'assistant.h'
sourcelist += 'contacts.c'
sourcelist += 'contacts.h'
sourcelist += 'contactsearch.c'
sourcelist += 'contactsearch.h'
sourcelist += 'debug.c'
sourcelist += 'debug.h'
sourcelist += 'fax.c'
sourcelist += 'fax.h'
sourcelist += 'gd-two-lines-renderer.c'
sourcelist += 'gd-two-lines-renderer.h'
sourcelist += 'journal.c'
sourcelist += 'journal.h'
sourcelist += 'main.h'
sourcelist += 'main_ui.c'
sourcelist += 'phone.c'
sourcelist += 'phone.h'
sourcelist += 'plugins.c'
sourcelist += 'plugins.h'
sourcelist += 'preferences.c'
sourcelist += 'preferences.h'
sourcelist += 'preferences-audio.c'
sourcelist += 'preferences-audio.h'
sourcelist += 'preferences-telephony.c'
sourcelist += 'preferences-telephony.h'
sourcelist += 'preferences-plugins.c'
sourcelist += 'preferences-plugins.h'
sourcelist += 'print.c'
sourcelist += 'print.h'
sourcelist += 'shortcuts.c'
sourcelist += 'shortcuts.h'
sourcelist += 'uitools.h'
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
