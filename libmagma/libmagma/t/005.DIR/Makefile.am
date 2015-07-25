CFLAGS=-I../../src/ -D_DEBUG_STDERR -Wall $(GLIB_CFLAGS)
LDFLAGS=-lm -lpthread -lssl $(GLIB_LIBS)

bin_PROGRAMS = basic_add_and_remove page_size_check generic

basic_add_and_remove_SOURCES = basic_add_and_remove.c 
basic_add_and_remove_CFLAGS = -DMAGMA_SERVER_NODE -DINCLUDE_FLARE_INTERNALS $(GLIB_CFLAGS) 
basic_add_and_remove_LDADD = -lm $(GLIB_LIBS)

page_size_check_SOURCES = page_size_check.c 
page_size_check_CFLAGS = -DMAGMA_SERVER_NODE -DINCLUDE_FLARE_INTERNALS $(GLIB_CFLAGS) 
page_size_check_LDADD = -lm $(GLIB_LIBS)

generic_SOURCES = generic.c 
generic_CFLAGS = -DMAGMA_SERVER_NODE -DINCLUDE_FLARE_INTERNALS $(GLIB_CFLAGS) 
generic_LDADD = -lm $(GLIB_LIBS)
