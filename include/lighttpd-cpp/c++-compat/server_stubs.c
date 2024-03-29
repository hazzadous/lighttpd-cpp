/**
 * Definitions of things in server.c but with external linkage.
 */

#include "server_stubs.h"
#include <sys/types.h>
#include <sys/stat.h>

#ifdef HAVE_SYS_TIME_H
#include <sys/time.h>
#else
#ifdef HAVE_TIME_H
#include <time.h>
#endif
#endif

#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <time.h>
#include <signal.h>
#include <assert.h>
#include <locale.h>

#include <stdio.h>

server *server_init(void) {
	int i;
	FILE *frandom = NULL;

	server *srv = calloc(1, sizeof(*srv));
	assert(srv);

	srv->max_fds = 1024;
#define CLEAN(x) \
	srv->x = buffer_init();

	CLEAN(response_header);
	CLEAN(parse_full_path);
	CLEAN(ts_debug_str);
	CLEAN(ts_date_str);
	CLEAN(response_range);
	CLEAN(tmp_buf);
	srv->empty_string = buffer_init_string("");
	CLEAN(cond_check_buf);

	CLEAN(srvconf.errorlog_file);
	CLEAN(srvconf.breakagelog_file);
	CLEAN(srvconf.groupname);
	CLEAN(srvconf.username);
	CLEAN(srvconf.changeroot);
	CLEAN(srvconf.bindhost);
	CLEAN(srvconf.event_handler);
	CLEAN(srvconf.pid_file);

	CLEAN(tmp_chunk_len);
#undef CLEAN

#define CLEAN(x) \
	srv->x = array_init();

	CLEAN(config_context);
	CLEAN(config_touched);
#undef CLEAN

	for (i = 0; i < FILE_CACHE_MAX; i++) {
		srv->mtime_cache[i].mtime = (time_t)-1;
		srv->mtime_cache[i].str = buffer_init();
	}

	if ((NULL != (frandom = fopen("/dev/urandom", "rb")) || NULL != (frandom = fopen("/dev/random", "rb")))
	            && 1 == fread(srv->entropy, sizeof(srv->entropy), 1, frandom)) {
		srand(*(unsigned int*)srv->entropy);
		srv->is_real_entropy = 1;
	} else {
		unsigned int j;
		srand(time(NULL) ^ getpid());
		srv->is_real_entropy = 0;
		for (j = 0; j < sizeof(srv->entropy); j++)
			srv->entropy[j] = rand();
	}
	if (frandom) fclose(frandom);

	srv->cur_ts = time(NULL);
	srv->startup_ts = srv->cur_ts;

	srv->conns = calloc(1, sizeof(*srv->conns));
	srv->joblist = calloc(1, sizeof(*srv->joblist));
	srv->joblist_prev = calloc(1, sizeof(*srv->joblist));
	srv->fdwaitqueue = calloc(1, sizeof(*srv->fdwaitqueue));

	srv->srvconf.modules = array_init();
	srv->srvconf.modules_dir = buffer_init_string(LIBRARY_DIR);
	srv->srvconf.network_backend = buffer_init();
	srv->srvconf.upload_tempdirs = array_init();

	srv->split_vals = array_init();

#ifdef USE_LINUX_AIO_SENDFILE
	srv->linux_io_ctx = NULL;
	/**
	 * we can't call io_setup before the fork() in daemonize()
	 */
#endif

	return srv;
}

void server_free(server *srv) {
	size_t i;

	for (i = 0; i < FILE_CACHE_MAX; i++) {
		buffer_free(srv->mtime_cache[i].str);
	}


#ifdef USE_LINUX_AIO_SENDFILE
	if (srv->linux_io_ctx) {
		io_destroy(srv->linux_io_ctx);
	}
#endif

#define CLEAN(x) \
	buffer_free(srv->x);

	CLEAN(response_header);
	CLEAN(parse_full_path);
	CLEAN(ts_debug_str);
	CLEAN(ts_date_str);
	CLEAN(response_range);
	CLEAN(tmp_buf);
	CLEAN(empty_string);
	CLEAN(cond_check_buf);

	CLEAN(srvconf.errorlog_file);
	CLEAN(srvconf.breakagelog_file);
	CLEAN(srvconf.groupname);
	CLEAN(srvconf.username);
	CLEAN(srvconf.changeroot);
	CLEAN(srvconf.bindhost);
	CLEAN(srvconf.event_handler);
	CLEAN(srvconf.pid_file);
	CLEAN(srvconf.modules_dir);
	CLEAN(srvconf.network_backend);

	CLEAN(tmp_chunk_len);
#undef CLEAN

#ifdef USE_GTHREAD
	fdevent_unregister(srv->ev, srv->wakeup_iosocket);
	iosocket_free(srv->wakeup_iosocket);
#endif

#if 0
	fdevent_unregister(srv->ev, srv->fd);
#endif
	fdevent_free(srv->ev);

	free(srv->conns);

	if (srv->config_storage) {
		for (i = 0; i < srv->config_context->used; i++) {
			specific_config *s = srv->config_storage[i];

			if (!s) continue;

			buffer_free(s->document_root);
			buffer_free(s->server_name);
			buffer_free(s->server_tag);
			buffer_free(s->ssl_pemfile);
			buffer_free(s->ssl_ca_file);
			buffer_free(s->error_handler);
			buffer_free(s->errorfile_prefix);
			array_free(s->mimetypes);
			buffer_free(s->ssl_cipher_list);

			free(s);
		}
		free(srv->config_storage);
		srv->config_storage = NULL;
	}

#define CLEAN(x) \
	array_free(srv->x);

	CLEAN(config_context);

	/* TODO: Seems to not like this inside tests */
	// CLEAN(config_touched);

	CLEAN(srvconf.upload_tempdirs);
#undef CLEAN

	joblist_free(srv, srv->joblist);
	joblist_free(srv, srv->joblist_prev);
	fdwaitqueue_free(srv, srv->fdwaitqueue);

	if (srv->stat_cache) {
		stat_cache_free(srv->stat_cache);
	}

	array_free(srv->srvconf.modules);
	array_free(srv->split_vals);
#ifdef USE_OPENSSL
	if (srv->ssl_is_init) {
		CRYPTO_cleanup_all_ex_data();
		ERR_free_strings();
		ERR_remove_state(0);
		EVP_cleanup();
	}
#endif
	free(srv);
}

