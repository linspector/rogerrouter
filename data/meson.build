install_data('org.tabos.roger.gschema.xml', install_dir : 'share/glib-2.0/schemas')

icondir = join_paths('icons', 'hicolor', 'scalable', 'apps')
install_data(
  join_paths(icondir, ('@0@.svg').format(application_id)),
  install_dir: join_paths(datadir, icondir),
)

icondir = join_paths('icons', 'hicolor', 'symbolic', 'apps')
install_data(
  join_paths(icondir, 'org.tabos.roger-symbolic.svg'),
  install_dir: join_paths(datadir, icondir),
  rename: '@0@-symbolic.svg'.format(application_id)
)

desktop = i18n.merge_file(
    input: 'org.tabos.roger.desktop.in',
    output: 'org.tabos.roger.desktop',
    type: 'desktop',
    po_dir: '../po',
    install: true,
    install_dir: join_paths(get_option('datadir'), 'applications')
)

appdata = i18n.merge_file(
    input: 'org.tabos.roger.appdata.xml.in',
    output: 'org.tabos.roger.appdata.xml',
    type: 'xml',
    po_dir: '../po',
    install: true,
    install_dir: join_paths(get_option('datadir'), 'appdata')
)

desktop_file_validate = find_program('desktop-file-validate', required: false)
if desktop_file_validate.found()
  test(
    'validate-desktop',
    desktop_file_validate,
    args: desktop.full_path(),
    depends: desktop
  )
endif

appstream_util = find_program('appstream-util', required: false)
if appstream_util.found()
  test(
    'validate-appdata', appstream_util,
    args: [
      'validate-relax', '--nonet', appdata.full_path()
    ],
    depends: appdata
  )
endif

