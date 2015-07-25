#include "config.h"
#include <epan/packet.h>
#include <libmagma/magma.h>

static int proto_magma = -1;

static int hf_magma_op_type = -1;
static int hf_magma_transaction_id = -1;
static int hf_magma_ttl = -1;
static int hf_magma_uid = -1;
static int hf_magma_gid = -1;

static gint ett_magma = -1;

static void dissect_magma(tvbuff_t *tvb, packet_info *pinfo, proto_tree *tree)
{
	gint offset = 0;
	col_set_str(pinfo->cinfo, COL_PROTOCOL, "MAGMA");

	/* Clear out stuff in the info column */
	col_clear(pinfo->cinfo, COL_INFO);
	col_add_fstr(pinfo->cinfo, COL_INFO,
		"Type %s", val_to_str(packet_type, packettypenames, "Unknown (0x%02x)"));

	if (tree) {
		proto_item *ti = NULL;
		proto_tree *magma_tree = NULL;

		ti = proto_tree_add_item(tree, proto_magma, tvb, 0, -1, ENC_NA);
		proto_item_append_text(ti,
			", Type %s", val_to_str(packet_type, packettypenames, "Unknown (0x%02x)"));
		magma_tree = proto_item_add_subtree(ti, ett_magma);

		proto_tree_add_item(magma_tree, hf_magma_op_type, tvb, offset, 1, ENC_BIG_ENDIAN);
		offset += 1;

		proto_tree_add_item(magma_tree, hf_magma_transaction_id, tvb, offset, 2, ENC_BIG_ENDIAN);
		offset += 2;

		proto_tree_add_item(magma_tree, hf_magma_ttl, tvb, offset, 2, ENC_BIG_ENDIAN);
		offset += 2;

		proto_tree_add_item(magma_tree, hf_magma_uid, tvb, offset, 2, ENC_BIG_ENDIAN);
		offset += 2;

		proto_tree_add_item(magma_tree, hf_magma_gid, tvb, offset, 2, ENC_BIG_ENDIAN);
		offset += 2;
	}
}

static const value_string packettypenames[] = {
	{ 1, "GETATTR" },
	{ 2, "READLINK" },
	{ 3, "GETDIR" },			/* deprecated, see readdir */
	{ 4, "MKNOD" },				/* implemented */ 
	{ 5, "MKDIR" },				/* implemented */ 
	{ 6, "UNLINK" },			/* implemented */ 
	{ 7, "RMDIR" },				/* implemented */ 
	{ 8, "SYMLINK" },			/* implemented */ 
	{ 9, "RENAME" },			/* implemented */ 
	{ 10, "LINK" },				/* implemented */ 
	{ 11, "CHMOD" },			/* implemented */ 
	{ 12, "CHOWN" },			/* implemented */ 
	{ 13, "TRUNCATE" },			/* implemented */ 
	{ 14, "UTIME" },			/* implemented */
	{ 15, "OPEN" },				/* implemented */
	{ 16, "READ" },				/* implemented */
	{ 17, "WRITE" },			/* implemented */
	{ 18, "STATFS" },			/* implemented */
	{ 19, "FLUSH" },			/* optional */
	{ 20, "RELEASE" },			/* optional */
	{ 21, "FSYNC" },			/* optional */
	{ 22, "SETXATTR" },			/* optional */
	{ 23, "GETXATTR" },			/* optional */
	{ 24, "LISTXATTR" },		/* optional */
	{ 25, "REMOVEXATTR" },		/* optional */
	{ 26, "OPENDIR" },			/* optional */
	{ 27, "READDIR" },			/* implemented */
	{ 28, "RELEASEDIR" },		/* optional */
	{ 29, "FSYNCDIR" },			/* optional */
	{ 30, "INIT" },				/* optional */
	{ 31, "DESTROY" },			/* optional */
	{ 32, "READDIR_EXTENDED" }, /* implemented */
	{ 33, "READDIR_OFFSET" }, 	/* implemented */

	{ 50, "ADD_FLARE_TO_PARENT" },
	{ 51, "REMOVE_FLARE_FROM_PARENT" },

	{ 60, "F_OPENDIR" },
	{ 61, "F_CLOSEDIR" },
	{ 62, "F_TELLDIR" },
	{ 63, "F_SEEKDIR" },
	{ 64, "F_READDIR" },

	/*
 	 * node protocol
 	 */
	{ 100, "JOIN" },				/* join an existing network */
	{ 101, "FINISH_JOIN" },			/* finish a join procedure */
	{ 105, "TRANSMIT_TOPOLOGY" },	/* transmit the whole topology */
	{ 110, "TRANSMIT_KEY" },		/* transmit a key to its redundant node */
	{ 112, "TRANSMIT_NODE" },		/* transmit profile of a node */
	{ 113, "GET_KEY" },				/* obtain a key from sibling node */
	{ 114, "PUT_KEY" },				/* release a key to sibling node */
	{ 115, "DROP_KEY" },			/* drop key, because it has been assigned to another node */
	{ 116, "GET_KEY_CONTENT" },		/* get key content */
	{ 117, "NETWORK_BUILT" },		/* network loaded and ready to operate */

	/*
	 * generic utilities
	 */
	{ 252, "CLOSECONNECTION" },		/* close connection to MAGMA server */
	{ 253, "SHUTDOWN" },			/* shutdown the lava network */
	{ 254, "HEARTBEAT" },			/* see if server is still available */
};

void proto_register_magma(void)
{
	static hf_register_info hf[] = {
		{
			&hf_magma_op_type, {
				"MAGMA OP Type",		// field description
				"magma.optype",			// used in search syntax
				FT_UINT8,				// the field type (unsigned 8 bit)
				BASE_DEC,				// print this field as a decimal
				VALS(packettypenames),
				0x0,
				NULL, HFILL 
			}
		},
		{
			&hf_magma_transaction_id, {
				"MAGMA Transaction ID",	// field description
				"magma.tid",			// used in search syntax
				FT_UINT16,				// the field type (unsigned 8 bit)
				BASE_DEC,				// print this field as a decimal
				NULL, 0x0,
				NULL, HFILL 
			}
		},
		{
			&hf_magma_ttl, {
				"MAGMA TTL",			// field description
				"magma.ttl",			// used in search syntax
				FT_UINT16,				// the field type (unsigned 8 bit)
				BASE_DEC,				// print this field as a decimal
				NULL, 0x0,
				NULL, HFILL 
			}
		},
		{
			&hf_magma_uid, {
				"MAGMA UID",			// field description
				"magma.uid",			// used in search syntax
				FT_UINT16,				// the field type (unsigned 8 bit)
				BASE_DEC,				// print this field as a decimal
				NULL, 0x0,
				NULL, HFILL 
			}
		},
		{
			&hf_magma_gid, {
				"MAGMA GID",			// field description
				"magma.gid",			// used in search syntax
				FT_UINT16,				// the field type (unsigned 8 bit)
				BASE_DEC,				// print this field as a decimal
				NULL, 0x0,
				NULL, HFILL 
			}
		}
	};

	/* Setup protocol subtree array */
	static gint *ett[] = {
		&ett_magma
	};

	proto_magma = proto_register_protocol("Magma protocol", "Magma", "magma");

	proto_register_field_array(proto_magma, hf, array_length(hf));
	proto_register_subtree_array(ett, array_length(ett));
}

void proto_reg_handoff_magma(void)
{
	static dissector_handle_t magma_handle;
	magma_handle = create_dissector_handle(dissect_magma, proto_magma);
	dissector_add_uint("udp.port", MAGMA_NODE_PORT, foo_handle);
	dissector_add_uint("udp.port", MAGMA_FLARE_PORT, foo_handle);
}
