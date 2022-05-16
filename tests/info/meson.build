
test_units = [
  'test-info-cleanup'
]

includes = [top_inc, include_directories('../../panels/system')]
cflags = '-DTEST_SRCDIR="@0@"'.format(meson.current_source_dir())

foreach unit: test_units
  exe = executable(
                    unit,
           [unit + '.c'],
    include_directories : includes,
           dependencies : common_deps,
              link_with : [system_panel_lib],
                 c_args : cflags
  )

  test(unit, exe)
endforeach

