#ifndef _PLUGIN_COMPAT_H_
#define _PLUGIN_COMPAT_H_

#define HAVE_SOCKLEN_T
#define HAVE_STDINT_H
extern "C"
{
	// include our compatible base first
#	include "base.h"

	// this will not include the original base.h
#	include <lighttpd/plugin.h>
}

#endif
