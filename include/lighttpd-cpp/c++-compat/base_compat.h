/**
 * A C++ compatible version of lighttpd/base.h.  Certain things are
 * not compatible in the original, such as types and variables sharing names.
 *
 * This file contains the following changes:
 *
 *  * type http_req -> type http_req_t
 *  * type request -> type request_t
 *  * type response -> type response_t
 *  * type physical -> type physical_t
 *  * type stat_cache_t-> type stat_cache_t
 *
 */

#ifndef _LIGHTTPD_CPP_BASE_COMPAT_H_
#define _LIGHTTPD_CPP_BASE_COMPAT_H_

#include <sys/types.h>
#include <sys/stat.h>

#ifdef HAVE_CONFIG_H
#include <lighttpd/config.h>
#endif

#include <limits.h>
#ifdef HAVE_STDINT_H
# include <stdint.h>
#endif
#ifdef HAVE_INTTYPES_H
# include <inttypes.h>
#endif

#include <lighttpd/settings.h>

#ifdef HAVE_GLIB_H
/* include glib.h before our buffer.h and array.h to make sure their parameter-names
 * don't clash with our type-names */
#include <glib.h>
#endif

#include <lighttpd/buffer.h>
#include <lighttpd/array.h>
#include <lighttpd/chunk.h>
#include <lighttpd/keyvalue.h>
#include <lighttpd/settings.h>
#include <lighttpd/fdevent.h>
#include <lighttpd/sys-socket.h>
//#include "http_req.h"
#include <lighttpd/etag.h>

#if defined HAVE_LIBSSL && defined HAVE_OPENSSL_SSL_H
# define USE_OPENSSL
# include <openssl/ssl.h>
#endif

#ifdef HAVE_SYS_INOTIFY_H
# include <sys/inotify.h>
#endif

#ifdef USE_LINUX_AIO_SENDFILE
# include <libaio.h>
#endif

#ifdef USE_POSIX_AIO
# include <aio.h>
#endif

/** some compat */
#ifndef O_BINARY
# define O_BINARY 0
#endif

#ifndef SIZE_MAX
# ifdef SIZE_T_MAX
#  define SIZE_MAX SIZE_T_MAX
# else
#  define SIZE_MAX ((size_t)~0)
# endif
#endif

#ifndef SSIZE_MAX
# define SSIZE_MAX ((size_t)~0 >> 1)
#endif

#ifdef __APPLE__
#include <crt_externs.h>
#define environ (* _NSGetEnviron())
#elif !defined(_WIN32)
extern char **environ;
#endif

/* for solaris 2.5 and NetBSD 1.3.x */
#ifndef HAVE_SOCKLEN_T
typedef int socklen_t;
#endif

/* solaris and NetBSD 1.3.x again */
#if (!defined(HAVE_STDINT_H)) && (!defined(HAVE_INTTYPES_H)) && (!defined(uint32_t))
/* # define uint32_t u_int32_t */
typedef unsigned __int32 uint32_t;
#endif


#ifndef SHUT_WR
# define SHUT_WR 1
#endif

#include <lighttpd/settings.h>

typedef struct {
	connection_state_t state;

	/* timestamps */
	time_t read_idle_ts;
	time_t close_timeout_ts;
	time_t write_request_ts;

	time_t connection_start;
	time_t request_start;

	struct timeval start_tv;

	size_t request_count;        /* number of requests handled in this connection */
	size_t loops_per_request;    /* to catch endless loops in a single request
				      *
				      * used by mod_rewrite, mod_fastcgi, ... and others
				      * this is self-protection
				      */

	int fd;                      /* the FD for this connection */
	int fde_ndx;                 /* index for the fdevent-handler */
	int ndx;                     /* reverse mapping to server->connection[ndx] */

	/* fd states */
	int is_readable;
	int is_writable;

	int keep_alive;              /* only request.c can enable it, all other just disable */
	int keep_alive_idle;         /* remember max_keep_alive_idle from config */

	int file_started;
	int file_finished;

	chunkqueue *write_queue;      /* a large queue for low-level write ( HTTP response ) [ file, mem ] */
	chunkqueue *read_queue;       /* a small queue for low-level read ( HTTP request ) [ mem ] */
	chunkqueue *request_content_queue; /* takes request-content into tempfile if necessary [ tempfile, mem ]*/

	int traffic_limit_reached;

	off_t bytes_written;          /* used by mod_accesslog, mod_rrd */
	off_t bytes_written_cur_second; /* used by mod_accesslog, mod_rrd */
	off_t bytes_read;             /* used by mod_accesslog, mod_rrd */
	off_t bytes_header;

	int http_status;

	sock_addr dst_addr;
	buffer *dst_addr_buf;

	/* request */
	buffer *parse_request;
	unsigned int parsed_response; /* bitfield which contains the important header-fields of the parsed response header */

	request  request;
	request_uri uri;
	physical physical;
	response response;

	size_t header_len;

	buffer *authed_user;
	array  *environment; /* used to pass lighttpd internal stuff to the FastCGI/CGI apps, setenv does that */

	/* response */
	int    got_response;

	int    in_joblist;

	connection_type mode;

	void **plugin_ctx;           /* plugin connection specific config */

	specific_config conf;        /* global connection specific config */
	cond_cache_t *cond_cache;

	buffer *server_name;

	/* error-handler */
	buffer *error_handler;
	int error_handler_saved_status;
	int in_error_handler;

	void *srv_socket;   /* reference to the server-socket (typecast to server_socket) */

#ifdef USE_OPENSSL
	SSL *ssl;
# ifndef OPENSSL_NO_TLSEXT
	buffer *tlsext_server_name;
# endif
#endif
	/* etag handling */
	etag_flags_t etag_flags;

	int conditional_is_valid[COMP_LAST_ELEMENT]; 
} connection_t;

typedef struct {
  /** HEADER */
  /* the request-line */
  buffer *request;
  buffer *uri;

  buffer *orig_uri;

  http_method_t  http_method;
  http_version_t http_version;

  buffer *request_line;

  /* strings to the header */
  buffer *http_host; /* not alloced */
  const char   *http_range;
  const char   *http_content_type;
  const char   *http_if_modified_since;
  const char   *http_if_none_match;

  array  *headers;

  /* CONTENT */
  size_t content_length; /* returned by strtoul() */

  /* internal representation */
  int     accept_encoding;

  /* internal */
  buffer *pathinfo;
} request_t;

typedef struct {
	off_t   content_length;
	int     keep_alive;               /* used by the subrequests in proxy, cgi and fcgi to say whether the subrequest_t was keep-alive or not */

	array  *headers;

	enum {
		HTTP_TRANSFER_ENCODING_IDENTITY, HTTP_TRANSFER_ENCODING_CHUNKED
	} transfer_encoding;
} response_t;

typedef struct {
	buffer *path;
	buffer *basedir; /* path = "(basedir)(.*)" */

	buffer *doc_root; /* path = doc_root + rel_path */
	buffer *rel_path;

	buffer *etag;
} physical_t;

typedef struct {
#ifdef HAVE_GLIB_H
	GHashTable *files; /* a HashTable of stat_cache_entries for the files */
	GHashTable *dirs;  /* a HashTable of stat_cache_entries for the dirs */
#endif

	buffer *dir_name;  /* for building the dirname from the filename */
	buffer *hash_key;  /* tmp-buf for building the hash-key */

#if defined(HAVE_SYS_INOTIFY_H)
	iosocket *sock;    /* socket to the inotify fd (this should be in a backend struct */
#endif
} stat_cache_t;

LI_EXPORT unsigned short sock_addr_get_port(sock_addr *addr); /* configfile-glue.c */

#endif
