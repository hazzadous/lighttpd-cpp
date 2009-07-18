/**
 * The following are some structures used to decide at compile-time which handlers in
 * the lighttpd plugin structure to be set, based on a list of handler interface types
 * i.e. calling a handlers_setter< list< UriRawHandler, UriCleanHandler > >( p ) object
 * will set p->handle_uri_raw and p->handle_uri_clean hooks.
 * A list of handler types supported:
 *  - UriRawHandler
 *  - UriCleanHandler
 *  - DocRootHandler
 *  - PhysicalHandler
 *  - StartBackendHandler
 */

#ifndef _LIGHTTPD_HANDLER_HELPERS_HPP_
#define _LIGHTTPD_HANDLER_HELPERS_HPP_

#include <boost/mpl/empty.hpp>
#include <boost/mpl/front.hpp>
#include <boost/mpl/pop_front.hpp>
#include <boost/mpl/eval_if.hpp>
#include <boost/mpl/identity.hpp>

// Forward declarations for handlers_setter to use
template < typename PluginType, typename Handler, typename HandlerList >
struct handlers_setter_impl;
struct handlers_setter_end;

template < typename PluginType, typename HandlerList >
struct handlers_setter_choose
{
	typedef handlers_setter_impl< 	PluginType,
									typename boost::mpl::front< HandlerList >::type,
									typename boost::mpl::pop_front< HandlerList >::type > type;
};

// calling handlers_setters type will set the hooks in p to
// the appropriate functions according to HandlerList
template < typename PluginType, typename HandlerList = typename PluginType::handlers >
struct handlers_setter
{
	typedef typename boost::mpl::empty< HandlerList >::type is_empty;
	typedef boost::mpl::identity< handlers_setter_end > handlers_setter_choose_end;
	typedef typename boost::mpl::eval_if< 	is_empty,
											handlers_setter_choose_end,
											handlers_setter_choose< PluginType, HandlerList > >::type type;
};

/**
 * handlers_setter_base will decide which our next setter will be
 */
template < typename PluginType, typename HandlerList >
struct handlers_setter_base
{
	typedef typename handlers_setter< PluginType, HandlerList >::type next;
};

/**
 * When we reach handlers_setter_end, we are at the end of the HandlerList
 */
struct handlers_setter_end
{
	static void set( plugin& p ){}
};

/**
 * Here we can partial specialize for Handler such that the right
 * hook will be set.
 */
template < typename PluginType, typename Handler, typename HandlerList >
struct handlers_setter_impl : handlers_setter_base< PluginType, HandlerList > {};

// Macro for defining interfaces for handler classes, wrapper functions for 
// the calls to the appropriate handlers in class( p_d ) and metafunction
// specializations of the handlers_setter_impl type.
#define MAKE_HANDLER( TypedefHandle, handler_name ) \
	struct TypedefHandle { handler_t handler_name( connection& ); }; \
	template < typename PluginType, typename HandlerList > \
	struct handlers_setter_impl< PluginType, TypedefHandle, HandlerList > \
	 : handlers_setter_base< PluginType, HandlerList > \
	{ \
		typedef handlers_setter_base< PluginType, HandlerList > super_type; \
		static handler_t handler_wrapper( server* srv, connection* con, void* p ) \
		{ \
			PluginType* P = reinterpret_cast< PluginType* >( p ); \
			return P->handler_name( *con ); \
		} \
		static void set( plugin& p ) \
		{ \
			p.handler_name = &handler_wrapper; \
			super_type::next::set( p ); \
		} \
	};

#endif // _LIGHTTPD_HANDLER_HELPERS_HPP_

