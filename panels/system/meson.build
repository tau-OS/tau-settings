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

resource_data = files(
  'cc-system-panel.ui',
)

sources =  files(
  'cc-system-panel.c',
  'info-cleanup.c'
)

sources += gnome.compile_resources(
  'cc-' + cappletname + '-resources',
  cappletname + '.gresource.xml',
  c_name: 'cc_' + cappletname,
  dependencies: resource_data,
  export: true
)

cflags += [
  '-DDATADIR="@0@"'.format(control_center_datadir),
  '-DLIBEXECDIR="@0@"'.format(control_center_libexecdir),
]

deps = common_deps + [
  dependency('udisks2', version: '>= 2.8.2'),
  dependency('libgtop-2.0'),
  gudev_dep,
]

system_panel_lib = static_library(
  cappletname,
  sources: sources,
  include_directories: [ top_inc, common_inc ],
  dependencies: deps,
  c_args: cflags,
)

panels_libs += system_panel_lib

print_renderer_deps = [
  dependency('gtk4'),
  epoxy_dep
]

executable(
  'gnome-control-center-print-renderer',
  'gnome-control-center-print-renderer.c',
  include_directories: [ top_inc ],
  dependencies: [ print_renderer_deps ],
  c_args: cflags,
  install: true,
  install_dir: control_center_libexecdir,
)