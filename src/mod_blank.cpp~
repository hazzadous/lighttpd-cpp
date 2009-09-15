/**
 * An empty plugin for test purposes.
 */

#include <lighttpd-cpp/plugin.hpp>

#include <boost/mpl/list.hpp>
#include <string>

class mod_blank : public Plugin< mod_blank >
{
public:
	mod_blank( server& srv )
	 :	Plugin< mod_blank >( srv ),
		some_string		( "some_string" ),
		some_int		( "some_int" ),
		some_bool		( "some_bool" ),
		some_short		( "some_short" )
	{}

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

MAKE_PLUGIN( mod_blank, "blank", LIGHTTPD_VERSION_ID );

