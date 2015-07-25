CFLAGS=-I../../src/ -D_DEBUG_STDERR -Wall $(GLIB_CFLAGS)
LDFLAGS=-lm -lpthread -lssl $(GLIB_LIBS)

bin_PROGRAMS = file_flare dir_flare block_flare char_flare fifo_flare symlink_flare flare_create destroy_flare

file_flare_SOURCES = file_flare.c 
file_flare_CFLAGS = -DMAGMA_SERVER_NODE -DINCLUDE_FLARE_INTERNALS $(GLIB_CFLAGS) 
file_flare_LDADD = -lm $(GLIB_LIBS)

dir_flare_SOURCES =  dir_flare.c 
dir_flare_CFLAGS = -DMAGMA_SERVER_NODE -DINCLUDE_FLARE_INTERNALS $(GLIB_CFLAGS) 
dir_flare_LDADD = -lm $(GLIB_LIBS)

block_flare_SOURCES =  block_flare.c 
block_flare_CFLAGS = -DMAGMA_SERVER_NODE -DINCLUDE_FLARE_INTERNALS $(GLIB_CFLAGS) 
block_flare_LDADD = -lm $(GLIB_LIBS)

char_flare_SOURCES =  char_flare.c 
char_flare_CFLAGS = -DMAGMA_SERVER_NODE -DINCLUDE_FLARE_INTERNALS $(GLIB_CFLAGS) 
char_flare_LDADD = -lm $(GLIB_LIBS)

fifo_flare_SOURCES =  fifo_flare.c 
fifo_flare_CFLAGS = -DMAGMA_SERVER_NODE -DINCLUDE_FLARE_INTERNALS $(GLIB_CFLAGS) 
fifo_flare_LDADD = -lm $(GLIB_LIBS)

symlink_flare_SOURCES =  symlink_flare.c 
symlink_flare_CFLAGS = -DMAGMA_SERVER_NODE -DINCLUDE_FLARE_INTERNALS $(GLIB_CFLAGS) 
symlink_flare_LDADD = -lm $(GLIB_LIBS)

flare_create_SOURCES =  flare_create.c 
flare_create_CFLAGS = -DMAGMA_SERVER_NODE -DINCLUDE_FLARE_INTERNALS $(GLIB_CFLAGS) 
flare_create_LDADD = -lm $(GLIB_LIBS)

destroy_flare_SOURCES =  destroy_flare.c 
destroy_flare_CFLAGS = -DMAGMA_SERVER_NODE -DINCLUDE_FLARE_INTERNALS $(GLIB_CFLAGS) 
destroy_flare_LDADD = -lm $(GLIB_LIBS)
