/*
   MAGMA -- sql.c
   Copyright (C) 2006-2015 Tx0 <tx0@strumentiresistenti.org>

   SQL flare serialization code

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

#include "../magma.h"

#if LIBDBI_VERSION >= ((0 * 10000) + (9 * 100) + (0))
#define MAGMA_DBI_REENTRANT 1
dbi_inst dbi_instance;
#else
#define MAGMA_DBI_REENTRANT 0
#endif

dbi_conn dbi;
GMutex magma_sql_mutex;

dbi_conn magma_sql_connect()
{
	dbg(LOG_INFO, DEBUG_SQL, "Initializing SQL layer");

#if MAGMA_DBI_REENTRANT
	dbi_initialize_r(NULL, &dbi_instance);
	dbi_conn dbi = dbi_conn_new_r("sqlite3", dbi_instance);
#else
	dbi_initialize(NULL);
	dbi_conn dbi = dbi_conn_new("sqlite3");
#endif
	dbi_conn_set_option(dbi, "dbname", "store.sql");
	dbi_conn_set_option(dbi, "sqlite3_dbdir", magma_environment.hashpath);

	dbg(LOG_INFO, DEBUG_SQL, "SQL layer connected");

	return (dbi);
}

#if 0
dbi_result magma_sql_query_on_connection(gchar *query, dbi_conn dbi, GMutex *mutex)
{
	g_mutex_lock(mutex);
	const char *error_message;

	if (!dbi_conn_ping(dbi) && dbi_conn_connect(dbi) < 0) {
		dbg(LOG_ERR, DEBUG_SQL, "ERROR! DBI Connection has gone!");
		g_mutex_unlock(mutex);
		return (NULL);
	}

	dbg(LOG_INFO, DEBUG_SQL, "SQL statement: %s", query);

	dbi_result result = dbi_conn_query(dbi, query);

    if (!result) {
    	dbi_conn_error(dbi, &error_message);
    	dbg(LOG_ERR, DEBUG_SQL, "Error running query: %s", error_message);
    }

    g_mutex_unlock(mutex);
    return (result);
}
#endif

dbi_result magma_sql_query(gchar *query)
{
	g_mutex_lock(&magma_sql_mutex);
	const char *error_message;

	if (!dbi_conn_ping(dbi) && dbi_conn_connect(dbi) < 0) {
		dbg(LOG_ERR, DEBUG_SQL, "ERROR! DBI Connection has gone!");
		g_mutex_unlock(&magma_sql_mutex);
		return (NULL);
	}

	dbg(LOG_INFO, DEBUG_SQL, "SQL statement: %s", query);

	dbi_result result = dbi_conn_query(dbi, query);

    if (!result) {
    	dbi_conn_error(dbi, &error_message);
    	dbg(LOG_ERR, DEBUG_SQL, "Error running query: %s", error_message);
    }

    g_mutex_unlock(&magma_sql_mutex);
    return (result);
}

void magma_init_sql()
{
    dbi = magma_sql_connect();
    gchar *stmt;

    /*
     * create lava table
     */
    stmt = g_strdup_printf(
    	"create table if not exists lava_%s ("
    		"nickname char(1024) primary key,"
    		"fqdn char(1024),"
    		"ipaddr char(15),"
    		"ipport int,"
    		"start_key char(40),"
    		"stop_key char(40),"
    		"load double,"
    		"storage int,"
    		"bandwidth int"
    	")",
    	magma_environment.nickname);

    if (!magma_sql_query(stmt)) exit (1);
    g_free(stmt);

    /*
     * create the flare table
     */
    stmt = g_strdup_printf(
    	"create table if not exists flare_%s ("
    		"hash char[40], "
    		"type char(1)," // r(egular), d(ir), c(har), b(lock), l(ink), p(ipe), s(ocket)
    		"path char(1024) primary key,"
    		"uid int,"
    		"gid int,"
    		"commit_path char(1024),"
    		"commit_time char(16) not null default ((julianday('now') - 2440587.5) * 86400.0)"
    	")",
    	magma_environment.nickname);

    if (!magma_sql_query(stmt)) exit (1);
    g_free(stmt);

    /*
     * create the journal table
     */
    stmt = g_strdup_printf(
    	"create table if not exists journal_%s ("
    		"id             integer not null    primary key autoincrement, "
    		"optype         int not null, "
    		"path           char not null, "
    		"to_path        char not null       default '', " // magma_rename(), magma_symlink()
    		"mode           int not null        default 0, "  // magma_chmod(),  magma_mknod()
    		"rdev           int not null        default 0, "  // magma_mknod()
    		"new_uid        int not null        default 0, "  // magma_chown()
    		"new_gid        int not null        default 0, "  // magma_chown()
    		"atime          int not null        default 0, "  // magma_utime()
    		"mtime          int not null        default 0, "  // magma_utime()
    		"write_offset   bigint not null     default 0, "  // magma_write()
    		"write_size     int not null        default 0  "  // magma_write()
    	")",
    	magma_environment.nickname);

    if (!magma_sql_query(stmt)) exit (1);
    g_free(stmt);

    dbg(LOG_INFO, DEBUG_SQL, "SQL layer initialized");
}

guint32 magma_sql_fetch_integer(dbi_result result, int index)
{
	guint32 i = 0;

    unsigned int type = dbi_result_get_field_type_idx(result, index);

    if (type == DBI_TYPE_INTEGER) {
        unsigned int size = dbi_result_get_field_attribs_idx(result, index);
        unsigned int is_unsigned = size & DBI_INTEGER_UNSIGNED;
        size = size & DBI_INTEGER_SIZEMASK;

        switch (size) {
            case DBI_INTEGER_SIZE8:
                if (is_unsigned)
                    i = dbi_result_get_ulonglong_idx(result, index);
                else
                    i = dbi_result_get_longlong_idx(result, index);
                break;
            case DBI_INTEGER_SIZE4:
            case DBI_INTEGER_SIZE3:
                if (is_unsigned)
                    i = dbi_result_get_uint_idx(result, index);
                else
                    i = dbi_result_get_int_idx(result, index);
                break;
            case DBI_INTEGER_SIZE2:
                if (is_unsigned)
                    i = dbi_result_get_ushort_idx(result, index);
                else
                    i = dbi_result_get_short_idx(result, index);
                break;
            case DBI_INTEGER_SIZE1:
                if (is_unsigned)
                    i = dbi_result_get_uchar_idx(result, index);
                else
                    i = dbi_result_get_char_idx(result, index);
                break;
        }
    } else if (type == DBI_TYPE_DECIMAL) {
        return (0);
    } else if (type == DBI_TYPE_STRING) {
        const gchar *int_string = dbi_result_get_string_idx(result, index);
        i = atoi(int_string);
        dbg('s', LOG_INFO, "magma_sql_fetch_integer called on non integer field");
    }

    return (i);
}

gchar *magma_sql_fetch_string(dbi_result result, int index)
{
    gchar *result_string = dbi_result_get_string_copy_idx(result, index);
    dbg('s', LOG_INFO, "Returning string: %s", result_string);
    return (result_string);
}

double magma_sql_fetch_double(dbi_result result, int index)
{
	double d = 0;
    unsigned int type = dbi_result_get_field_type_idx(result, index);
	if (type == DBI_TYPE_DECIMAL) {
		d = dbi_result_get_double_idx(result, index);
	}
	return (d);
}

/**
 * Delete a flare from the flare table
 *
 * @param flare the flare to delete
 */
void magma_flare_sql_delete(magma_flare_t *flare)
{
	gchar *query = g_strdup_printf(
		"delete from flare_%s where path = '%s'",
		magma_environment.nickname, flare->path);

	dbi_result result = magma_sql_query(query);
	dbi_result_free(result);

	g_free(query);
}

/**
 * Save a flare into the flare table
 *
 * @param flare the flare to save
 */
void magma_flare_sql_save(magma_flare_t *flare)
{
	/*
	 * the first time a flare is saved its commit path
	 * is NULL. It must be set to its natural path
	 */
	if (!flare->commit_path) flare->commit_path = flare->path;

	gchar *query = g_strdup_printf(
		"insert into flare_%s (hash, type, path, uid, gid, commit_path) "
			"values('%s', '%c', '%s', %d, %d, '%s')",
		magma_environment.nickname,
		flare->hash,
		flare->type,
		flare->path,
		flare->st.st_uid,
		flare->st.st_gid,
		flare->commit_path);

	dbi_result result = magma_sql_query(query);
	dbi_result_free(result);

	g_free(query);
}

/**
 * Load flare data from SQL
 *
 * @param flare the flare to load: must already be declared to hold
 *   its path, but it does not need to be upcasted)
 */
void magma_flare_sql_load(magma_flare_t *flare)
{
	gchar *query = g_strdup_printf(
		"select hash, type, uid, gid, commit_path, commit_time"
			"from flares_%s"
			"where path = '%s'",
		magma_environment.nickname,
		flare->path);

	dbi_result result = magma_sql_query(query);
	if (result && dbi_result_next_row(result)) {
		flare->st.st_uid   = magma_sql_fetch_integer(result, 3);
		flare->st.st_gid   = magma_sql_fetch_integer(result, 4);
		flare->commit_path = magma_sql_fetch_string (result, 5);
		flare->commit_time = magma_sql_fetch_string (result, 6);
	}

	dbi_result_free(result);
	g_free(query);
}

/**
 * Rename a flare into the flare table
 *
 * @param flare the flare to save
 */
void magma_flare_sql_rename(magma_flare_t *flare)
{
	gchar *query = g_strdup_printf(
		"update flare_%s set path = '%s', hash = '%s' "
			"where commit_path = '%s' and commit_time = '%s' and type = '%c'",
		magma_environment.nickname,
		flare->path,
		flare->hash,
		flare->commit_path,
		flare->commit_time,
		flare->type);

	dbi_result result = magma_sql_query(query);
	dbi_result_free(result);

	g_free(query);
}

/**
 * Delete a volcano
 *
 * @param v the volcano to delete
 */
void magma_sql_delete_volcano(magma_volcano *v)
{
	gchar *query = g_strdup_printf(
		"delete from lava_%s where nickname = '%s'",
		magma_environment.nickname, v->node_name);

	dbi_result result = magma_sql_query(query);
	dbi_result_free(result);

	g_free(query);
}

/**
 * Save a volcano
 *
 * @param v the volcano to save
 */
void magma_sql_save_volcano(magma_volcano *v)
{
	magma_sql_delete_volcano(v);

	gchar *query = g_strdup_printf(
		"insert into lava_%s (nickname, fqdn, ipaddr, ipport, start_key, stop_key, load, storage, bandwidth) "
			"values ('%s', '%s', '%s', %d, '%s', '%s', %lf, %u, %u)",
		magma_environment.nickname, v->node_name, v->fqdn_name,
		v->ip_addr, v->port, v->start_key, v->stop_key,
		v->load, v->storage, v->bandwidth);

	dbi_result result = magma_sql_query(query);
	dbi_result_free(result);

	g_free(query);
}
