project(
  'ModelHeader',
  ['cpp'],
  default_options : ['cpp_std=c++1z', 'buildtype=debugoptimized']
)

src = [
  'generator.cc',
]

assimp_dep = dependency('assimp')

executable(
  'modelheader',
  src,
  dependencies: [ assimp_dep ],
  install: true,
)

