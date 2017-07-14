tests_inc = [include_directories('../src'), top_inc]
env = environment()
env.set('G_TEST_SRCDIR', meson.current_source_dir())
env.set('G_TEST_BUILDDIR', meson.current_build_dir())
env.set('G_DEBUG', 'gc-friendly')
env.set('MALLOC_CHECK_', '2')
# FIXME: Add a meson helper to get a random number
env.set('MALLOC_PERTURB_', '113') # Guaranteed random!

ingredients = executable('ingredients', 'ingredients-test.c', src_incs,
                         include_directories : tests_inc,
                         dependencies: deps)
test('ingredients', ingredients, env : env)

number = executable('number', 'number.c',
                     include_directories : tests_inc,
                     link_with: librecipes,
                     dependencies: deps)
test('number', number, env : env)

unit = executable('unit', 'unit.c',
                  include_directories : tests_inc,
                  link_with: librecipes,
                  dependencies: deps)
test('unit', unit, env : env)

ids = executable('ids', 'ids.c',
                  include_directories : tests_inc,
                  link_with: librecipes,
                  dependencies: deps)
test('ids', ids, env : env)

strv = executable('strv', 'strv.c',
                  include_directories : tests_inc,
                  link_with: librecipes,
                  dependencies: deps)
test('strv', strv, env : env)
