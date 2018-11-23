project(
  'ModelHeader',
  ['cpp'],
  default_options : ['cpp_std=c++1z', 'buildtype=debugoptimized']
)

src = [
  'src/main.cc',
]

cc = meson.get_compiler('cpp')
m_dep = cc.find_library('m', required : false)
glm_dep = dependency('glm')
assimp_dep = dependency('assimp')

incdir = include_directories('src')

executable(
  'modelheader',
  src,
  dependencies: [
    m_dep,
    glm_dep,
    assimp_dep,
  ],
  include_directories: [incdir],
  install: true,
)

