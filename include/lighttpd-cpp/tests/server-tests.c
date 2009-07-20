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

#include "settings.h"
#include "server.h"
#include "buffer.h"
#include "network.h"
#include "network_backends.h"
#include "log.h"
#include "keyvalue.h"
#include "response.h"
#include "request.h"
#include "chunk.h"
#include "fdevent.h"
#include "connections.h"
#include "stat_cache.h"
#include "plugin.h"
#include "joblist.h"
#include "status_counter.h"

/**
 * stack-size of the aio-threads
 *
 * the default is 8Mbyte which is a bit to much. Reducing it to 64k seems to be fine
 * If you experience random segfaults, increase it.
 */

#define LI_THREAD_STACK_SIZE (64 * 1024)


#ifdef HAVE_GETOPT_H
#include <getopt.h>
#else
#ifdef _WIN32
#include "xgetopt.h"
#endif
#endif

#include "valgrind/valgrind.h"

#ifdef HAVE_SYS_WAIT_H
#include <sys/wait.h>
#endif

#ifdef HAVE_PWD_H
#include <grp.h>
#include <pwd.h>
#endif

#ifdef HAVE_SYS_RESOURCE_H
#include <sys/resource.h>
#endif

#ifdef HAVE_SYS_PRCTL_H
#include <sys/prctl.h>
#endif

#ifdef USE_OPENSSL
#include <openssl/err.h>
#endif

#ifndef __sgi
/* IRIX doesn't like the alarm based time() optimization */
/* #define USE_ALARM */
#endif

#ifdef _WIN32
#undef HAVE_SIGNAL
#endif

#include "sys-files.h"
#include "sys-process.h"
#include "sys-socket.h"

#ifdef HAVE_GETUID
# ifndef HAVE_ISSETUGID

static int l_issetugid() {
	return (geteuid() != getuid() || getegid() != getgid());
}

#  define issetugid l_issetugid
# endif
#endif

static volatile sig_atomic_t srv_shutdown = 0;
static volatile sig_atomic_t graceful_shutdown = 0;
static volatile sig_atomic_t graceful_restart = 0;
static volatile sig_atomic_t handle_sig_alarm = 1;
static volatile sig_atomic_t handle_sig_hup = 0;
static volatile siginfo_t last_sigterm_info;
static volatile siginfo_t last_sighup_info;

#if defined(HAVE_SIGACTION) && defined(SA_SIGINFO)
static void sigaction_handler(int sig, siginfo_t *si, void *context) {
	static siginfo_t empty_siginfo;
	UNUSED(context);

	if (!si) si = &empty_siginfo;

	switch (sig) {
	case SIGTERM: 
		srv_shutdown = 1; 
		memcpy((siginfo_t*) &last_sigterm_info, si, sizeof(*si));
		break;
	case SIGINT:
		if (graceful_shutdown) {
			srv_shutdown = 1;
		} else {
			graceful_shutdown = 1;
		}

		memcpy((siginfo_t*) &last_sigterm_info, si, sizeof(*si));

		break;
	case SIGALRM: 
		handle_sig_alarm = 1; 
		break;
	case SIGHUP: 
		handle_sig_hup = 1; 
		memcpy((siginfo_t*) &last_sighup_info, si, sizeof(*si));
		break;
	case SIGCHLD: 
		break;
	}
}
#elif defined(HAVE_SIGNAL) || defined(HAVE_SIGACTION)
static void signal_handler(int sig) {
	switch (sig) {
	case SIGTERM: srv_shutdown = 1; break;
	case SIGINT:
	     if (graceful_shutdown) srv_shutdown = 1;
	     else graceful_shutdown = 1;

	     break;
	case SIGALRM: handle_sig_alarm = 1; break;
	case SIGHUP:  handle_sig_hup = 1; break;
	case SIGCHLD:  break;
	}
}
#endif

#ifdef HAVE_FORK
static void daemonize(void) {
#ifdef SIGTTOU
	signal(SIGTTOU, SIG_IGN);
#endif
#ifdef SIGTTIN
	signal(SIGTTIN, SIG_IGN);
#endif
#ifdef SIGTSTP
	signal(SIGTSTP, SIG_IGN);
#endif
	if (0 != fork()) exit(0);

	if (-1 == setsid()) exit(0);

	signal(SIGHUP, SIG_IGN);

	if (0 != fork()) exit(0);

	if (0 != chdir("/")) exit(0);
}
#endif

static server *server_init(void) {
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
	assert(srv->conns);

	srv->joblist = calloc(1, sizeof(*srv->joblist));
	assert(srv->joblist);

	srv->joblist_prev = calloc(1, sizeof(*srv->joblist));
	assert(srv->joblist_prev);

	srv->fdwaitqueue = calloc(1, sizeof(*srv->fdwaitqueue));
	assert(srv->fdwaitqueue);

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

static void server_free(server *srv) {
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
	CLEAN(config_touched);
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

static void show_version (void) {
#ifdef USE_OPENSSL
# define TEXT_SSL " (ssl)"
#else
# define TEXT_SSL
#endif
	char *b = PACKAGE_NAME "-" PACKAGE_VERSION TEXT_SSL \
" - a light and fast webserver\n" \
"Build-Date: " __DATE__ " " __TIME__ "\n";
;
#undef TEXT_SSL
	if (-1 == write(STDOUT_FILENO, b, strlen(b))) {
		/* what to do if this happens ? */

		exit(-1);
	}
}

static void show_features (void) {
	const fdevent_handler_info_t *handler;
	const network_backend_info_t *backend;

	const char *features = 
#ifdef HAVE_IPV6
      "\t+ IPv6 support\n"
#else
      "\t- IPv6 support\n"
#endif
#if defined HAVE_ZLIB_H && defined HAVE_LIBZ
      "\t+ zlib support\n"
#else
      "\t- zlib support\n"
#endif
#if defined HAVE_BZLIB_H && defined HAVE_LIBBZ2
      "\t+ bzip2 support\n"
#else
      "\t- bzip2 support\n"
#endif
#ifdef HAVE_LIBCRYPT
      "\t+ crypt support\n"
#else
      "\t- crypt support\n"
#endif
#ifdef USE_OPENSSL
      "\t+ SSL Support\n"
#else
      "\t- SSL Support\n"
#endif
#ifdef HAVE_LIBPCRE
      "\t+ PCRE support\n"
#else
      "\t- PCRE support\n"
#endif
#ifdef HAVE_MYSQL
      "\t+ MySQL support\n"
#else
      "\t- MySQL support\n"
#endif
#if defined(HAVE_LDAP_H) && defined(HAVE_LBER_H) && defined(HAVE_LIBLDAP) && defined(HAVE_LIBLBER)
      "\t+ LDAP support\n"
#else
      "\t- LDAP support\n"
#endif
#ifdef HAVE_MEMCACHE_H
      "\t+ memcached support\n"
#else
      "\t- memcached support\n"
#endif
#ifdef HAVE_FAM_H
      "\t+ FAM support\n"
#else
      "\t- FAM support\n"
#endif
#ifdef HAVE_LUA_H
      "\t+ LUA support\n"
#else
      "\t- LUA support\n"
#endif
#ifdef HAVE_LIBXML_H
      "\t+ xml support\n"
#else
      "\t- xml support\n"
#endif
#ifdef HAVE_SQLITE3_H
      "\t+ SQLite support\n"
#else
      "\t- SQLite support\n"
#endif
#ifdef HAVE_GDBM_H
      "\t+ GDBM support\n"
#else
      "\t- GDBM support\n"
#endif
      ;

	show_version();

	printf("\nEvent Handlers:\n\n");
	for (handler = fdevent_get_handlers(); handler->name; handler++) {
		printf("\t%c %s", handler->init ? '+' : '-', handler->name);
		if (handler->description) {
			printf(": %s\n", handler->description);
		}
		else {
			printf("\n");
		}
	}

	printf("\nNetwork Backends:\n\n");
	for (backend = network_get_backends(); backend->name; backend++) {
		printf("\t%c %s", backend->write_handler ? '+' : '-', backend->name);
		if (backend->description) {
			printf(": %s\n", backend->description);
		}
		else {
			printf("\n");
		}
	}

#ifdef USE_MMAP
	printf("\t+ (mmap) support\n");
#else
	printf("\t- (mmap) support\n");
#endif
	printf("\nFeatures:\n\n%s", features);
}

static void show_help (void) {
#ifdef USE_OPENSSL
# define TEXT_SSL " (ssl)"
#else
# define TEXT_SSL
#endif
	char *b = PACKAGE_NAME "-" PACKAGE_VERSION TEXT_SSL " ("__DATE__ " " __TIME__ ")" \
" - a light and fast webserver\n" \
"usage:\n" \
" -f <name>  filename of the config-file\n" \
" -m <name>  module directory (default: "LIBRARY_DIR")\n" \
" -p         print the parsed config-file in internal form, and exit\n" \
" -t         test the config-file, and exit\n" \
" -D         don't go to background (default: go to background)\n" \
" -I         go to background on SIGINT (useful with -D)\n" \
"            has no effect when using kqueue or /dev/poll\n" \
" -v         show version\n" \
" -V         show compile-time features\n" \
" -h         show this help\n" \
"\n"
;
#undef TEXT_SSL
#undef TEXT_IPV6
	if (-1 == write(STDOUT_FILENO, b, strlen(b))) {
		exit(-1);
	}
}
#ifdef USE_GTHREAD
static GMutex *joblist_queue_mutex = NULL;
static GCond  *joblist_queue_cond = NULL;
#endif

/**
 * a async handler
 *
 * The problem:
 * - poll(..., 1000) is blocking
 * - g_async_queue_timed_pop(..., 1000) is blocking too
 *
 * I havn't found a way to monitor g_async_queue_timed_pop with poll or friends
 *
 * So ... we run g_async_queue_timed_pop() and fdevents_poll() at the same time. As long
 * as the poll() is running g_async_queue_timed_pop() running too.
 *
 * The one who finishes earlier writes to wakeup_pipe[1] to interrupt
 * the other system call. We use mutexes to synchronize the two functions.
 *
 */
#ifdef USE_GTHREAD
static void *joblist_queue_thread(void *_data) {
	server *srv = _data;

	g_mutex_lock(joblist_queue_mutex);

	while (!srv_shutdown) {
		GTimeVal ts;
		connection *con;

		g_cond_wait(joblist_queue_cond, joblist_queue_mutex);
		/* wait for getting signaled */

		if (srv_shutdown)
			break;

		/* wait one second as the poll() */
		g_get_current_time(&ts);
		g_time_val_add(&ts, 1000 * 1000);

		/* we can't get interrupted :(
		 * if we don't get something into the queue we leave */
		if (NULL != (con = g_async_queue_timed_pop(srv->joblist_queue, &ts))) {
			int killme = 0;
			do {
				if (con == (void *)1) {
					/* ignore the wakeup-packet, it is only used to break out of the
					 * blocking nature of g_async_queue_timed_pop()  */
				} else {
					killme++;
					joblist_append(srv, con);
				}
			} while ((con = g_async_queue_try_pop(srv->joblist_queue)));

			/* interrupt the poll() */
			if (killme) write(srv->wakeup_pipe[1], " ", 1);
		}
	}

	g_mutex_unlock(joblist_queue_mutex);

	return NULL;
}
#endif

/**
 * call this function whenever you get a EMFILE or ENFILE as return-value
 *
 * after each socket(), accept(), connect() or open() call
 *
 */
int server_out_of_fds(server *srv, connection *con) {
	/* we get NULL of accept() ran out of FDs */

	if (con) {
		fdwaitqueue_append(srv, con);
	}

	return 0;
}

static int lighty_mainloop(server *srv) {
	fdevent_revents *revents = fdevent_revents_init();
	int poll_errno;
	size_t conns_user_at_sockets_disabled = 0;

	/* the getevents and the poll() have to run in parallel
	 * as soon as one has data, it has to interrupt the otherone */

	/* main-loop */
	while (!srv_shutdown) {
		int n;
		size_t ndx;
		time_t min_ts;

		if (handle_sig_hup) {
			handler_t r;

			/* reset notification */
			handle_sig_hup = 0;

#if 0
      			pid_t pid;

			/* send the old process into a graceful-shutdown and start a
			 * new process right away
			 *
			 * BUGS:
			 * - if webserver is running on port < 1024 (e.g. 80, 433)
			 *   we don't have the permissions to bind to that port anymore
			 *
			 *
			 *  */
			if (0 == (pid = fork())) {
				execve(argv[0], argv, envp);

				exit(-1);
			} else if (pid == -1) {

			} else {
				/* parent */

				graceful_shutdown = 1; /* shutdown without killing running connections */
				graceful_restart = 1;  /* don't delete pid file */
			}
#else
			/* cycle logfiles */

			switch(r = plugins_call_handle_sighup(srv)) {
			case HANDLER_GO_ON:
				break;
			default:
				log_error_write(srv, __FILE__, __LINE__, "sd", "sighup-handler return with an error", r);
				break;
			}

			if (-1 == log_error_cycle()) {
				log_error_write(srv, __FILE__, __LINE__, "s", "cycling errorlog failed, dying");

				return -1;
			} else {
				TRACE("logfiles cycled by UID=%d, PID=%d", 
						last_sighup_info.si_uid, 
						last_sighup_info.si_pid);
			}
#endif
		}

		if (handle_sig_alarm) {
			/* a new second */

#ifdef USE_ALARM
			/* reset notification */
			handle_sig_alarm = 0;
#endif

			/* get current time */
			min_ts = time(NULL);

			if (min_ts != srv->cur_ts) {
				int cs = 0;
				connections *conns = srv->conns;
				handler_t r;

				switch(r = plugins_call_handle_trigger(srv)) {
				case HANDLER_GO_ON:
					break;
				case HANDLER_ERROR:
					log_error_write(srv, __FILE__, __LINE__, "s", "one of the triggers failed");
					break;
				default:
					log_error_write(srv, __FILE__, __LINE__, "d", r);
					break;
				}

				/* trigger waitpid */
				srv->cur_ts = min_ts;

				/* cleanup stat-cache */
				stat_cache_trigger_cleanup(srv);
				/**
				 * check all connections for timeouts
				 *
				 */
				for (ndx = 0; ndx < conns->used; ndx++) {
					int changed = 0;
					connection *con;
					int t_diff;

					con = conns->ptr[ndx];

					switch (con->state) {
					case CON_STATE_READ_REQUEST_HEADER:
					case CON_STATE_READ_REQUEST_CONTENT:
						if (con->recv->is_closed) {
							if (srv->cur_ts - con->read_idle_ts > con->conf.max_connection_idle) {
								/* time - out */
#if 0
								TRACE("(connection process timeout) [%s]", SAFE_BUF_STR(con->dst_addr_buf));
#endif
								connection_set_state(srv, con, CON_STATE_ERROR);
								changed = 1;
							}
						}

						if (con->request_count == 1) {
							if (srv->cur_ts - con->read_idle_ts > con->conf.max_read_idle) {
								/* time - out */
#if 0
								TRACE("(initial read timeout) [%s]", SAFE_BUF_STR(con->dst_addr_buf));
#endif
								connection_set_state(srv, con, CON_STATE_ERROR);
								changed = 1;
							}
						} else {
							if (srv->cur_ts - con->read_idle_ts > con->conf.max_keep_alive_idle) {
								/* time - out */
#if 0
								TRACE("(keep-alive read timeout) [%s]", SAFE_BUF_STR(con->dst_addr_buf));
#endif
								connection_set_state(srv, con, CON_STATE_ERROR);
								changed = 1;
							}
						}
						break;
					case CON_STATE_WRITE_RESPONSE_HEADER:
					case CON_STATE_WRITE_RESPONSE_CONTENT:


						if (con->write_request_ts != 0 &&
						    srv->cur_ts - con->write_request_ts > con->conf.max_write_idle) {
							/* time - out */
							if (con->conf.log_timeouts) {
								log_error_write(srv, __FILE__, __LINE__, "sbsosds",
									"NOTE: a request for",
									con->request.uri,
									"timed out after writing",
									con->bytes_written,
									"bytes. We waited",
									(int)con->conf.max_write_idle,
									"seconds. If this a problem increase server.max-write-idle");
							}
							connection_set_state(srv, con, CON_STATE_ERROR);
							changed = 1;
						}
						break;
					default:
						/* the other ones are uninteresting */
						break;
					}
					/* we don't like div by zero */
					if (0 == (t_diff = srv->cur_ts - con->connection_start)) t_diff = 1;

					if (con->traffic_limit_reached &&
					    (con->conf.kbytes_per_second == 0 ||
					     ((con->bytes_written / t_diff) < con->conf.kbytes_per_second * 1024))) {
						/* enable connection again */
						con->traffic_limit_reached = 0;

						changed = 1;
					}

					if (changed) {
						connection_state_machine(srv, con);
					}
					con->bytes_written_cur_second = 0;
					*(con->conf.global_bytes_per_second_cnt_ptr) = 0;

#if 0
					if (cs == 0) {
						fprintf(stderr, "connection-state: ");
						cs = 1;
					}

					fprintf(stderr, "c[%d,%d]: %s ",
						con->fd,
						con->fcgi.fd,
						connection_get_state(con->state));
#endif
				}

				if (cs == 1) fprintf(stderr, "\n");
			}
		}

		if (srv->sockets_disabled) {
			/* our server sockets are disabled, why ? */

			if ((srv->fdwaitqueue->used == 0) &&
			    (srv->conns->used <= srv->max_conns * 9 / 10) &&
			    (0 == graceful_shutdown)) {
				size_t i;

				for (i = 0; i < srv->srv_sockets.used; i++) {
					server_socket *srv_socket = srv->srv_sockets.ptr[i];
					fdevent_event_add(srv->ev, srv_socket->sock, FDEVENT_IN);
				}

				TRACE("[note] sockets enabled again%s", "");

				srv->sockets_disabled = 0;
			}
		} else {
			if ((srv->fdwaitqueue->used) || /* looks like some cons are waiting for FDs*/
			    (srv->conns->used >= srv->max_conns) || /* out of connections */
			    (graceful_shutdown)) { /* graceful_shutdown */
				size_t i;

				/* disable server-fds */

				for (i = 0; i < srv->srv_sockets.used; i++) {
					server_socket *srv_socket = srv->srv_sockets.ptr[i];

					fdevent_event_del(srv->ev, srv_socket->sock);

					if (graceful_shutdown) {
						/* we don't want this socket anymore,
						 *
						 * closing it right away will make it possible for
						 * the next lighttpd to take over (graceful restart)
						 *  */

						fdevent_unregister(srv->ev, srv_socket->sock);
						closesocket(srv_socket->sock->fd);
						srv_socket->sock->fd = -1;

#ifdef HAVE_FORK
						/* FreeBSD kqueue could possibly work with rfork(RFFDG)
						* while Solaris /dev/poll would require re-registering
						* all fd */
						if (srv->srvconf.daemonize_on_shutdown &&
							srv->event_handler != FDEVENT_HANDLER_FREEBSD_KQUEUE &&
							srv->event_handler != FDEVENT_HANDLER_SOLARIS_DEVPOLL) {
							daemonize();
						}
#endif

						/* network_close() will cleanup after us */
					}
				}

				if (graceful_shutdown) {
					TRACE("[note] graceful shutdown started by UID=%d, PID=%d", last_sigterm_info.si_uid, last_sigterm_info.si_pid);
				} else if (srv->fdwaitqueue->used) {
					TRACE("[note] out of FDs, server-socket get disabled for a while, we have %zu connections open and they are waiting for %zu FDs",
					    srv->conns->used, srv->fdwaitqueue->used);
				} else if (srv->conns->used >= srv->max_conns) {
					TRACE("[note] we reached our connection limit of %zu connections. Disabling server-sockets for a while", srv->max_conns);
				}

				srv->sockets_disabled = 1;

				/* we count the number of free fds indirectly.
				 *
				 * instead of checking the fds we only check the connection handles we free'd since
				 * the server-sockets got disabled
				 * */
				conns_user_at_sockets_disabled = srv->conns->used;
			}
		}

		if (graceful_shutdown && srv->conns->used == 0) {
			/* we are in graceful shutdown phase and all connections are closed
			 * we are ready to terminate without harming anyone */
			srv_shutdown = 1;
			continue;
		}

		/* we still have some fds to share */
		if (!srv_shutdown && srv->fdwaitqueue->used) {
			/* ok, we are back to the problem of 'how many fds do we have available ?' */
			connection *con;
			int fd;
			int avail_fds = conns_user_at_sockets_disabled - srv->conns->used;

			if (-1 == (fd = open("/dev/null", O_RDONLY))) {
				switch (errno) {
				case EMFILE:
					avail_fds = 0;

					break;
				default:
					break;
				}
			} else {
				close(fd);

				/* we have at least one FD as we just checked */
				if (!avail_fds) avail_fds++;
			}


			TRACE("conns used: %zu, fd-waitqueue has %zu entries, fds to share: %d", srv->conns->used, srv->fdwaitqueue->used, avail_fds);

			while (avail_fds-- && NULL != (con = fdwaitqueue_unshift(srv, srv->fdwaitqueue))) {
				connection_state_machine(srv, con);
			}
		}
#ifdef USE_GTHREAD
		/* open the joblist-queue handling */
		g_mutex_unlock(joblist_queue_mutex);
		g_cond_signal(joblist_queue_cond);
#endif
		n = fdevent_poll(srv->ev, 1000);
		poll_errno = errno;

#ifdef USE_GTHREAD
		if (FALSE == g_mutex_trylock(joblist_queue_mutex)) {
			/**
			 * we couldn't get the lock, looks like the joblist-thread
			 * is still blocking on g_async_queue_timed_pop()
			 *
			 * let's send it a bogus job to jump out of the blocking mode
			 */
			g_async_queue_push(srv->joblist_queue, (void *)1); /* HACK to wakeup the g_async_queue_timed_pop() */

			g_mutex_lock(joblist_queue_mutex);
		}
#endif
		if (n > 0) {
			/* n is the number of events */
			size_t i;
			fdevent_get_revents(srv->ev, n, revents);

			/* handle client connections first
			 *
			 * this is a bit of a hack, but we have to make sure than we handle
			 * close-events before the connection is reused for a keep-alive
			 * request
			 *
			 * this is mostly an issue for mod_proxy_core, but you never know
			 *
			 */

			for (i = 0; i < revents->used; i++) {
				fdevent_revent *revent = revents->ptr[i];
				handler_t r;

				/* skip server-fds */
				if (revent->handler == network_server_handle_fdevent) continue;

				switch (r = (*(revent->handler))(srv, revent->context, revent->revents)) {
				case HANDLER_WAIT_FOR_FD:
					server_out_of_fds(srv, NULL);
				case HANDLER_FINISHED:
				case HANDLER_GO_ON:
				case HANDLER_WAIT_FOR_EVENT:
					break;
				case HANDLER_ERROR:
					/* should never happen */
					SEGFAULT("got HANDLER_ERROR from a plugin: %s", "dieing");
					break;
				default:
					ERROR("got handler_t(%d) from a plugin: ignored", r);
					break;
				}
			}

			for (i = 0; i < revents->used; i++) {
				fdevent_revent *revent = revents->ptr[i];
				handler_t r;

				/* server fds only */
				if (revent->handler != network_server_handle_fdevent) continue;

				switch (r = (*(revent->handler))(srv, revent->context, revent->revents)) {
				case HANDLER_WAIT_FOR_FD:
					server_out_of_fds(srv, NULL);
				case HANDLER_FINISHED:
				case HANDLER_GO_ON:
				case HANDLER_WAIT_FOR_EVENT:
					break;
				case HANDLER_ERROR:
					/* should never happen */
					SEGFAULT("got HANDLER_ERROR from a plugin: %s", "dieing");
					break;
				default:
					ERROR("got handler_t(%d) from a plugin: ignored", r);
					break;
				}
			}

		} else if (n < 0 && poll_errno != EINTR) {
			ERROR("fdevent_poll failed: %s", strerror(poll_errno));
		}

		/*
		 * Note: Two joblist's are needed so a connection can be added back into the joblist
		 * without getting stuck inside the for loop.
		 */
		if(srv->joblist->used > 0) {
			connections *joblist = srv->joblist;
			/* switch joblist queues. */
			srv->joblist = srv->joblist_prev;
			srv->joblist_prev = joblist;
		}
		for (ndx = 0; ndx < srv->joblist_prev->used; ndx++) {
			connection *con = srv->joblist_prev->ptr[ndx];
			handler_t r;

			con->in_joblist = 0;
			connection_state_machine(srv, con);

			switch(r = plugins_call_handle_joblist(srv, con)) {
			case HANDLER_FINISHED:
			case HANDLER_GO_ON:
				break;
			default:
				ERROR("got handler_t(%d) from a plugin: ignored", r);
				break;
			}
		}

		srv->joblist_prev->used = 0;
	}

	fdevent_revents_free(revents);

	return 0;
}

#ifdef USE_GTHREAD
static handler_t wakeup_handle_fdevent(void *s, void *context, int revent) {
	server     *srv = (server *)s;
	connection *con = context;
	char buf[16];
	UNUSED(con);
	UNUSED(revent);

	(void) read(srv->wakeup_iosocket->fd, buf, sizeof(buf));
	return HANDLER_GO_ON; 
}
#endif

