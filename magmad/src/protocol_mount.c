/*
   MAGMA -- protocol_mount.c
   Copyright (C) 2006-2013 Tx0 <tx0@strumentiresistenti.org>

   Implements NFS mount protocol.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2, or (at your option)
   any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software Foundation,
   Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA. 
*/
/*
 * This file was generated by glibc's rpcgen tool and then customized
 */

#ifdef MAGMA_ENABLE_NFS_INTERFACE

#include "magma.h"


bool_t
mountproc_null_1_svc(void *argp, void *result, struct svc_req *rqstp)
{
	bool_t retval = TRUE;
	(void) argp;
	(void) rqstp;  /* should be used for authentication */
	(void) result;

	dbg(LOG_INFO, DEBUG_PNFS, "Got a NULL request");

	return retval;
}

bool_t
mountproc_mnt_1_svc(dirpath *argp, fhstatus *result, struct svc_req *rqstp)
{
	bool_t retval = TRUE;
	(void) rqstp;  /* should be used for authentication */

	dbg(LOG_INFO, DEBUG_PNFS, "Got a MNT request for %s", (char *) *argp);
	magma_flare_t *flare = magma_search_or_create((char *) *argp);
	if (flare != NULL) {
		result->fhs_status = NFS_OK;
		memcpy(result->fhstatus_u.fhs_fhandle, flare->binhash, SHA_DIGEST_LENGTH+1);
		retval = TRUE;
	} else {
		dbg(LOG_ERR, DEBUG_PNFS, "Can't get flare for %s: ", (char *) *argp);
		result->fhs_status = NFSERR_NOENT;
		memset(result->fhstatus_u.fhs_fhandle, 0, sizeof(fhandle));
		retval = FALSE;
	}

	return retval;
}

bool_t
mountproc_dump_1_svc(void *argp, mountlist *result, struct svc_req *rqstp)
{
	(void) rqstp;  /* should be used for authentication */
	(void) argp;

	dbg(LOG_INFO, DEBUG_PNFS, "Got a DUMP request");

	do {
		result = malloc(sizeof(struct mountbody));
	} while (result == NULL);

	(*result)->ml_hostname = g_strdup(servername);
	(*result)->ml_directory = g_strdup("/");
	(*result)->ml_next = NULL;

	dbg(LOG_INFO, DEBUG_PNFS, "Returning that %s exports just %s",
		(*result)->ml_hostname, (*result)->ml_directory);

	return TRUE;
}

bool_t
mountproc_umnt_1_svc(dirpath *argp, void *result, struct svc_req *rqstp)
{
	(void) rqstp;  /* should be used for authentication */
	(void) result;
	(void) argp;

	dbg(LOG_INFO, DEBUG_PNFS, "Got a UMNT request");
	dbg(LOG_INFO, DEBUG_PNFS, "This call needs more coding like removing client from list...");

	return TRUE;
}

bool_t
mountproc_umntall_1_svc(void *argp, void *result, struct svc_req *rqstp)
{
	bool_t retval = TRUE;
	(void) rqstp;  /* should be used for authentication */
	(void) result;
	(void) argp;

	dbg(LOG_INFO, DEBUG_PNFS, "Got a UMNTALL request");
	dbg(LOG_INFO, DEBUG_PNFS, "This call needs more coding like removing the client from list...");

	return retval;
}

bool_t
mountproc_export_1_svc(void *argp, exports *result, struct svc_req *rqstp)
{
	bool_t retval = TRUE;
	(void) rqstp;  /* should be used for authentication */
	(void) argp;

	dbg(LOG_INFO, DEBUG_PNFS, "Got an EXPORT request");

	result = malloc(sizeof(struct exportnode));
	(*result)->ex_dir = g_strdup("/");

	/* foreach entry in ACL should add a struct groupnode */
	{
		(*result)->ex_groups = malloc(sizeof(struct groupnode));
		if ((*result)->ex_groups != NULL) {
			(*result)->ex_groups->gr_name = g_strdup("");
			(*result)->ex_groups->gr_next = NULL;
		}
	}

	return retval;
}

bool_t
mountproc_exportall_1_svc(void *argp, exports *result, struct svc_req *rqstp)
{
	bool_t retval = TRUE;
	(void) rqstp;  /* should be used for authentication */
	(void) argp;

	dbg(LOG_INFO, DEBUG_PNFS, "Got a EXPORTALL request");

	result = malloc(sizeof(struct exportnode));
	(*result)->ex_dir = g_strdup("/");

	/* foreach entry in ACL should add a struct groupnode */
	{
		(*result)->ex_groups = malloc(sizeof(struct groupnode));
		if ((*result)->ex_groups != NULL) {
			(*result)->ex_groups->gr_name = g_strdup("");
			(*result)->ex_groups->gr_next = NULL;
		}
	}

	return retval;
}

int
mountprog_1_freeresult (SVCXPRT *transp, xdrproc_t xdr_result, caddr_t result)
{
	xdr_free (xdr_result, result);

	(void) transp;
	(void) xdr_result;
	(void) result;

	dbg(LOG_INFO, DEBUG_PNFS, "Got a FREERESULT request");

	return 1;
}

#endif /* MAGMA_ENABLE_NFS_INTERFACE */

// vim:ts=4:nocindent
