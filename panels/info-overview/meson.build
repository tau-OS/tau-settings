panels_list += cappletname
desktop = 'gnome-@0@-panel.desktop'.format(cappletname)

desktop_in = configure_file(
  input: desktop + '.in.in',
  output: desktop + '.in',
  configuration: desktop_conf
)

i18n.merge_file(
  type: 'desktop',
  input: desktop_in,
  output: desktop,
  po_dir: po_dir,
  install: true,
  install_dir: control_center_desktopdir
)

cflags += [
  '-DDATADIR="@0@"'.format(control_center_datadir),
  '-DLIBEXECDIR="@0@"'.format(control_center_libexecdir),
]

sources = files(
  'cc-info-overview-panel.c'
)

resource_data = files(
  'cc-info-overview-panel.ui'
)

sources += gnome.compile_resources(
  'cc-' + cappletname + '-resources',
  cappletname + '.gresource.xml',
  c_name: 'cc_' + cappletname.underscorify (),
  dependencies: resource_data,
  export: true
)

panels_libs += static_library(
  cappletname,
  sources: sources,
  include_directories: [ top_inc, common_inc ],
  dependencies: deps,
  c_args: cflags
)

install_data(
  [ 'gnome-logo-text.svg', 'gnome-logo-text-dark.svg' ],
  install_dir: join_paths(control_center_datadir, 'icons')
)

subdir('icons')
