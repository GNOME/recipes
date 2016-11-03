TESTS = ingredients

check_PROGRAMS = ingredients

ingredients_SOURCES = ingredients.c
ingredients_CFLAGS = $(RECIPES_CFLAGS) \
			-I$(top_srcdir)/src
ingredients_LDADD = $(RECIPES_LIBS)

-include $(top_srcdir)/git.mk
