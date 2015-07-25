#ifndef MAGMA_SERVER_NODE
#define MAGMA_SERVER_NODE
#endif

#include "../../config.h"
#include "../magma.h"
uint16_t debug;

// #include "../src/parseargs.c"
// #include "../src/sha1.c"
// #include "../src/utils.c"
// #include "../src/routing.c"
// #include "../src/magma_flare.c"
// #include "../src/net_layer.c"
// #include "../src/vulcano.c"
// #include "../src/protocol.c"
// #include "../src/protocol_pkt.c"
// #include "../src/protocol_console.c"
// #include "../src/protocol_flare.c"
// #include "../src/protocol_mount.c"
// #include "../src/protocol_nfs.c"
// #include "../src/protocol_node.c"

#define HASHPATH "/tmp/magma-test"

void cleanup(int s) {}
int shutting_down = 0;
void exit_this_thread() {}

magma_environment_t magma_environment;

void test_init(int debug_flag)
{
	magma_environment.progname = "test";					/* saving program name */
	magma_environment.log = 0;								/* default is NO debug */
	magma_environment.port = MAGMA_PORT;					/* default port */
	magma_environment.servername = g_strdup("localhost");	/* default server name */
	magma_environment.nickname = g_strdup("magma");			/* default server nickname */
	magma_environment.ipaddr = g_strdup("127.0.0.1");		/* default server IP address */
	magma_environment.hashpath = g_strdup(HASHPATH);		/* default server path for local storage */
	magma_environment.bootserver = g_strdup("127.0.0.1");	/* default remote boot server */
	magma_environment.secretkey = NULL;
	magma_environment.bandwidth = 1;						/**< Declared bandwidth */
	magma_environment.storage = 1;							/**< Declared storage */
	magma_environment.bootstrap = 1;						/**< If true, this node should bootstrap a new network, if false this node should join an existing one */

	debug = DEBUG_ERR|debug_flag;
	magma_check_filesystem();

	/* clean filesystem from previous operations */
	DIR *d;
	if (! (d = opendir(HASHPATH)) ) {
		dbg(LOG_ERR, DEBUG_ERR, "Can't open %s for cleaning", HASHPATH);
		return;
	}

	struct dirent *de;
	while ((de = readdir(d)) != NULL) {
		char *epath;
		epath = g_strdup_printf("%s/%s/%s", HASHPATH, de->d_name, "contents");
		unlink(epath);
		free(epath);

		epath = g_strdup_printf("%s/%s/%s", HASHPATH, de->d_name, "metadata");
		unlink(epath);
		free(epath);

		epath = g_strdup_printf("%s/%s", HASHPATH, de->d_name);
		rmdir(epath);
		free(epath);
	}
	closedir(d);
}

// vim:ts=4:nocindent:autoindent
