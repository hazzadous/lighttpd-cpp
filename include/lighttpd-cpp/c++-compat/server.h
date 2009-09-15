/**
 * For the tests, we need certain thing in server.c that
 * don't have external linkage.  It seems amazing to me that
 * they keep this stuff locked away with main, which is making
 * writing unittests more annoying that it should be. 
 */

#ifndef _CPP_COMPAT_SERVER_H_
#define _CPP_COMPAT_SERVER_H_

#include "base.h"

// Include server.h, there are some things we can get from it,
// like the config parsers.
extern "C"
{
#	include "server_stubs.h"
}

#endif // _CPP_COMPAT_SERVER_H_

