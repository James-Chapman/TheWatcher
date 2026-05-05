project('libcbor', 'c', version: '0.13.0')

cbor_sources = files(
  'src/allocators.c',
  'src/cbor.c',
  'src/cbor/arrays.c',
  'src/cbor/bytestrings.c',
  'src/cbor/callbacks.c',
  'src/cbor/common.c',
  'src/cbor/encoding.c',
  'src/cbor/floats_ctrls.c',
  'src/cbor/ints.c',
  'src/cbor/maps.c',
  'src/cbor/serialization.c',
  'src/cbor/streaming.c',
  'src/cbor/strings.c',
  'src/cbor/tags.c',
  'src/cbor/internal/builder_callbacks.c',
  'src/cbor/internal/encoders.c',
  'src/cbor/internal/loaders.c',
  'src/cbor/internal/memory_utils.c',
  'src/cbor/internal/stack.c',
  'src/cbor/internal/unicode.c',
)

# Provide pre-generated headers (from third_party/libcbor/generated).
inc = include_directories('src', '.')

libcbor = static_library(
  'cbor',
  cbor_sources,
  include_directories: inc,
  c_args: ['-DCBOR_STATIC_DEFINE', '-DCBOR_BUILD_STATIC=1'],
  install: false,
)

libcbor_dep = declare_dependency(
  link_with: libcbor,
  include_directories: inc,
  compile_args: ['-DCBOR_STATIC_DEFINE', '-DCBOR_BUILD_STATIC=1'],
)
