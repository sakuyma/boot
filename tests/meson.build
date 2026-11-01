config_test = executable(
  'test-config',
  'test-config.c',
  wl_proto_headers,
  link_with: [common, tokenize],
  dependencies: [pixman, xkb, fontconfig, wayland_client, fcft, tllist, luajit])

test('config', config_test)
