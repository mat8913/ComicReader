comicreader_sources = [
  'main.c',
  'comicreader-application.c',
  'comicreader-window.c',
  'comicreader-imagedisplay.c',
  'comicreader-imageloader.c',
  'comicreader-directoryimageloader.c',
  'comicreader-backgroundimageloader.c',
]

cc = meson.get_compiler('c')

comicreader_deps = [
  cc.find_library('m', required: true),
  dependency('gtk4'),
  dependency('libadwaita-1', version: '>= 1.4'),
]

comicreader_sources += gnome.compile_resources('comicreader-resources',
  'comicreader.gresource.xml',
  c_name: 'comicreader'
)

executable('comicreader', comicreader_sources,
  dependencies: comicreader_deps,
       install: true,
)
