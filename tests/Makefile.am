TESTS_ENVIRONMENT = \
	G_TEST_SRCDIR="$(abs_srcdir)"           \
	G_TEST_BUILDDIR="$(abs_builddir)"       \
	G_DEBUG=gc-friendly                     \
	MALLOC_CHECK_=2                         \
	MALLOC_PERTURB_=$$(($${RANDOM:-256} % 256))

TESTS = \
	unit \
	number \
	ingredients

check_PROGRAMS = \
	unit \
	number \
	ingredients

ingredients_SOURCES = ingredients-test.c
ingredients_CFLAGS = \
	$(RECIPES_CFLAGS) 	\
	-DPKGDATADIR=\"$(pkgdatadir)\"  \
	-I$(top_srcdir)/src
ingredients_LDADD = $(RECIPES_LIBS)

number_SOURCES = number.c
number_CFLAGS = \
	$(RECIPES_CFLAGS) 	\
	-DPKGDATADIR=\"$(pkgdatadir)\"  \
	-I$(top_srcdir)/src
number_LDADD = $(RECIPES_LIBS)

unit_SOURCES = unit.c
unit_CFLAGS = \
	$(RECIPES_CFLAGS) 	\
	-DPKGDATADIR=\"$(pkgdatadir)\"  \
	-I$(top_srcdir)/src
unit_LDADD = $(RECIPES_LIBS)

EXTRA_DIST = \
	ingredients-data/valid-1.expected ingredients-data/valid-1.in	\
	ingredients-data/valid-2.expected ingredients-data/valid-2.in	\
	ingredients-data/valid-3.expected ingredients-data/valid-3.in	\
	ingredients-data/valid-4.expected ingredients-data/valid-4.in	\
	number-data/number1.expected number-data/number1.in		\
	unit-data/unit1.expected unit-data/unit1.in

-include $(top_srcdir)/git.mk
