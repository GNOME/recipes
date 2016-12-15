TESTS_ENVIRONMENT = \
	G_TEST_SRCDIR="$(abs_srcdir)"           \
	G_TEST_BUILDDIR="$(abs_builddir)"       \
	G_DEBUG=gc-friendly                     \
	MALLOC_CHECK_=2                         \
	MALLOC_PERTURB_=$$(($${RANDOM:-256} % 256))

TESTS = ingredients

check_PROGRAMS = ingredients

ingredients_SOURCES = ingredients.c
ingredients_CFLAGS = \
	$(RECIPES_CFLAGS) 	\
	-DPKGDATADIR=\"$(pkgdatadir)\"  \
	-I$(top_srcdir)/src

ingredients_LDADD = $(RECIPES_LIBS)

EXTRA_DIST = \
	data/valid-1.expected data/valid-1.in	\
	data/valid-2.expected data/valid-2.in	\
	data/valid-3.expected data/valid-3.in	\
	data/valid-4.expected data/valid-4.in	\
	data/valid-5.expected data/valid-5.in

-include $(top_srcdir)/git.mk
