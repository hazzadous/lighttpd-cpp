/**
 * An empty plugin for test purposes.
 */

#ifndef _LIGHTTPD_MOD_BLANK_HPP_
#define _LIGHTTPD_MOD_BLANK_HPP_

#include <lighttpd-cpp/plugin.hpp>

#include <boost/mpl/list.hpp>
#include <string>

class mod_blank : public Plugin< mod_blank >
{
public:
	mod_blank( plugin& p );
	typedef boost::mpl::list< 	UriRawHandler,
								UriCleanHandler,
								DocRootHandler,
								PhysicalHandler,
								StartBackendHandler > handlers;

	handler_t handle_uri_raw( connection& con ){ return HANDLER_GO_ON; }
	handler_t handle_uri_clean( connection& con ){ return HANDLER_GO_ON; }
	handler_t handle_docroot( connection& con ){ return HANDLER_GO_ON; }
	handler_t handle_physical( connection& con ){ return HANDLER_GO_ON; }
	handler_t handle_start_backend( connection& con ){ return HANDLER_GO_ON; }

	config_option< std::string > 	some_string;
	config_option< int > 			some_int;
	config_option< bool > 			some_bool;
	config_option< short > 			some_short;
};

#endif // _LIGHTTPD_MOD_BLANK_HPP_

