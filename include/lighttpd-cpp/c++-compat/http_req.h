/**
 * A C++ compatible version of lighttpd/http_req.h.  Certain things are
 * not compatible in the original, such as types and variables sharing names.
 *
 * This file contains the following changes:
 *
 *  * type http_req -> type http_req_t
 *
 */

#ifndef _HTTP_REQ_H_
#define _HTTP_REQ_H_

#include <stdio.h>

#include <lighttpd/array.h>
#include <lighttpd/chunk.h>
#include <lighttpd/http_parser.h>

typedef struct {
	int protocol;   /* http/1.0, http/1.1 */
	int method;     /* e.g. GET */
	buffer *uri_raw; /* e.g. /foobar/ */
	array *headers;
} http_req_t;

typedef struct {
	int     ok;
	buffer *errmsg;

	http_req_t *req;
	buffer_pool *unused_buffers;
} http_req_ctx_t;

extern "C"
{
	LI_API http_req_t * http_request_init(void);
	LI_API void http_request_free(http_req_t *req);
	LI_API void http_request_reset(http_req_t *req);

	LI_API parse_status_t http_request_parse_cq(chunkqueue *cq, http_req_t *http_request);

	/* declare prototypes for the parser */
	void *http_req_parserAlloc(void *(*mallocProc)(size_t));
	void http_req_parserFree(void *p,  void (*freeProc)(void*));
	void http_req_parserTrace(FILE *TraceFILE, char *zTracePrompt);
	void http_req_parser(void *, int, buffer *, http_req_ctx_t *);
}

#endif
