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

// Tests to which we are friends.
class lighttpd_tests;
class plugin_base_tests;

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
	// Sets up plugin generic settings
	plugin_base( const std::string& name, const std::size_t& version, server& srv )
	 : srv( srv ), name( name ), version( version )
	{}

	const server& srv;

public:
	virtual ~plugin_base( ){ }

	// Allow access to the constructor from the following tests.
	friend class plugin_base_tests_PluginBaseInit_Test;

	// Here we have functions and data that are required by all plugins
	const std::string& name;
	const std::size_t& version;

	// For set defaults, we just call set defaults on all config_options
	// in the current translation unit.
	handler_t set_defaults( )
	{
		return config_option_base::set_all_defaults( srv );
	}

	static handler_t set_defaults_wrapper( server* s, void* p_d )
	{
		plugin_base& p = reinterpret_cast< plugin_base& >( p_d );
		return p.set_defaults( );
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
	Plugin( server& srv )
	 : plugin_base( name, version, srv )
	{}

public:
	// Make sure the base destructor gets called.
	virtual ~Plugin( ){ }

	// The name and version of the plugin implemented by MostDerived.
	// Needs to be static so that we have it in time for plugin_init.
	static std::string name;
	static std::size_t version;

	// This is to be called after entry, i.e. from _plugin_init.
	// We can't really eliminate the need to implement _plugin_init in
	// derived modules as we have to have the correct function name.
	// use the MAKE_PLUGIN macro to create the appropriate entry function.
	static int plugin_init( plugin& p )
	{
		// Make sure exceptions do not propergate to C code
#ifndef TESTING
		try
		{
#endif
			p.name = buffer_init_string( name.c_str( ) );
			p.version = version;

			// Here we are just setting members of plugin p,
			// we create an instance of MostDerived later when this init function
			// is called, and destroying on p.cleanup
			p.init = &init;
			p.cleanup = &cleanup;

			// This set_defaults function will set defaults of all config_options
			// that have been specified in this translation unit.
			p.set_defaults = &plugin_base::set_defaults_wrapper;

			// The handler setter to use to configure hooks in plugin p below.
			typedef typename handlers_setter< MostDerived >::type setter;

			// use for_each to set the appropriate function pointers
			// in the plugin structure, dependent on the type "handlers"
			// defined in a derived class.
			setter::set( p );

			return 0;
#ifndef TESTING
		}
		catch( ... )
		{
			return 1;
		}
#endif
	}

	// To be called from lighttpd in its _init phase of plugin initialization.
	// Creates a plugin instance of type MostDerived and returns it to be used
	// as the lighttpd's plugin.data.
	static void* init( server* srv )
	{
		// Stop propagation to C code.
#ifndef TESTING
		try
		{
#endif
			// Here the actual plugin instance is created.
			return reinterpret_cast< void* >( new MostDerived( *srv ) );
#ifndef TESTING
		}
		catch( ... )
		{
			return NULL;
		}
#endif
	}

	// Calls plugin base destructor that works its way down to MostDerived.
	static handler_t cleanup( server* srv, void* p_d )
	{
		try
		{
			delete reinterpret_cast< plugin_base* >( p_d );
			return HANDLER_GO_ON;
		}
		catch( ... )
		{
		}

		return HANDLER_ERROR;
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
#define MAKE_PLUGIN( class_name, plugin_name, plugin_version ) \
	extern "C" \
	{ \
		LI_EXPORT int class_name##_plugin_init( plugin* p ) \
		{ \
			return class_name::plugin_init( *p ); \
		} \
	} \
	template <>	std::string Plugin< class_name >::name( plugin_name ); \
	template <> std::size_t Plugin< class_name >::version( plugin_version );

#endif // _LIGHTTPD_PLUGIN_HPP_

