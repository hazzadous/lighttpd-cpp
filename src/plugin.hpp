/**
 * Base class for deriving C++ plugins from.  Possibly useful if you 
 * like this sort of thing.  Otherwise just make use you extern the
 * appropriate functions (i.e. the plugin init function) to have C symbols.
 */
#ifndef _LIGHTTPD_PLUGIN_HPP_
#define _LIGHTTPD_PLUGIN_HPP_

#include <cstddef>

#include "c++-compat/plugin.h"

#include "handler_helpers.hpp"
#include "datatype_helpers.hpp"

/**
 * Abstract base class for C++ lighty plugins.  Not sure if its worth it 
 * yet, lets see how useful it is.  Really there shouldn't be any
 * significant performance hit with these wrappers, only a small increase
 * in memory usage per plugin (irrelivant).  Other hit due to function
 * calling convensions for which I do not know the details.
 *
 * Things that need encapsulating:
 * 
 * 	Plugin house keeping:
 *   - set which part of the request flow we handle.  i.e. _plugin_init
 *   - populate plugin config.  i.e. set_defaults
 *   - clean up allocated memory we used.  i.e. cleanup
 *
 *  The request flow:
 *   - HTTP request recieved, i.e. 	protocol, method, uri_raw, 
 *   								headers (see http_req.h)
 *   - "Clean" uri_raw -> uri_clean  i.e. mod_rewrite
 *   - Map the clean request to an appropriate docroot.
 *   - Get the physical path to the requested file.
 *   - Do untold horrors to the requested file.
 *     - More things here.
 *   - Send a response back maybe.
 *
 * Lighty allows plugins to hook in to multiple points of this flow,
 * i.e. one type of plugin, but their effects at certain levels may be NULL.
 * Serves as an easy method of sharing data between points of flow.  Perhaps
 * theres something in trying to abstract this shared data out, and removing
 * this hardcoded sharing within a plugin.  Don't know, just a gut feeling.
 *
 */

/**
 * Absolute base for plugins.
 */
class plugin_base
{
protected:
	// We always have a reference to the C plugin structure
	// lighty handles the freeing of p
	plugin_base( const char* name, const std::size_t version, plugin& p )
	 : p( p ), srv( 0 ) // don't know srv yet, need initialize
	{
		p.name = buffer_init_string( name );
		p.version = version;
		p.data = reinterpret_cast< void* >( this );

		// We don't have enough infomation to call init.
		// Instead, call it from set_defaults_wrapper

		// p.init = &plugin_base::initialize_wrapper;
		p.set_defaults = &plugin_base::set_defaults_wrapper;
	}

	plugin& p;
	const server* srv;

public:

	// Here we have functions that are required by (almost) all plugins

	// For set defaults, we just call set defaults on all config_options
	// in the current translation unit.
	handler_t set_defaults( )
	{
		return config_option_base::set_all_defaults( );
	}

	static handler_t set_defaults_wrapper( server* s, void* p_d )
	{
		plugin_base& p = reinterpret_cast< plugin_base& >( p_d );
		initialize_wrapper( s, p_d );
		return p.set_defaults( );
	}

	// Set srv and return this.  There are quite a few inconsistencies here I know,
	// this should really be called directly and return a newly allocated plugin.
	// Its a tug of war between C and C++ ways.  I choose to init the plugin in
	// create_plugin and set p->data in the plugin_base constructor.
	void* initialize( const server& s )
	{
		srv = &s;
		return reinterpret_cast< void* >( this );
	}

	static void* initialize_wrapper( server* s, void* p_d )
	{
		plugin_base& p = reinterpret_cast< plugin_base& >( p_d );
		return p.initialize( *s );
	}
};

/**
 * Abstract plugin base-class for plugins, non-constructable, hidden from C 
 * compilers. An alternative would be to have a plugin type derived from 
 * this for each handler type.  
 */
template< typename MostDerived >
class Plugin : public plugin_base
{
	typedef plugin_base super_type;

protected:
	// Private constructor for setting plugin name.
	// Could say this is a part of the traditional lighty init function
	Plugin( const char* name, const std::size_t version, plugin& p )
	 : plugin_base( name, version, p )
	{
		// The handler setter to use to configure hooks in plugin p below.
		typedef typename handlers_setter< MostDerived >::type setter;

		// use for_each to set the appropriate function pointers
		// in the plugin structure, dependend on the type "handlers"
		// defined in a derived class.
		setter::set( p );
	}

public:
	// This is to be called after entry, i.e. from _plugin_init.
	// We can't really eliminate the need to implement _plugin_init in
	// derived modules as we have to have the correct function name.
	// use the DEFINE_MODULE macro to create the appropriate entry function.
	static int create_plugin( plugin& p )
	{
		// Make sure exceptions do not propergate to C code
		try
		{
			new MostDerived( p );
			return 0;
		}
		catch( ... )
		{
			return 1;
		}
	}
};

// A list of defined interfaces.
// The first argument specifies a typedef handle to the handler types,
// the second specifies the handle name as seen in the plugin structure.
// This macro is only for handlers that take just the connection.
MAKE_HANDLER( UriRawHandler,       handle_uri_raw       );
MAKE_HANDLER( UriCleanHandler,     handle_uri_clean     );
MAKE_HANDLER( DocRootHandler,      handle_docroot       );
MAKE_HANDLER( PhysicalHandler,     handle_physical      );
MAKE_HANDLER( StartBackendHandler, handle_start_backend );

// This is defined in handler_helpers.hpp .  It should not be used by derived plugins.
#undef MAKE_HANDLER

// Given your plugin name, this will create the entry point for lighttpd to use.
// To be used in your plugins cpp file.
#define MAKE_PLUGIN( name ) \
	extern "C" \
	{ \
		static int name##_plugin_init( plugin* p ) \
		{ \
			return Plugin< name >::create_plugin( *p ); \
		} \
	}

#endif // _LIGHTTPD_PLUGIN_HPP_

