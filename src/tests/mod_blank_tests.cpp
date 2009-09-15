/**
 * Test module initialization and destruction, including everything 
 * that entails, ie config, handler helpers.
 */

#include <string>
#include <gtest/gtest.h>

#include <lighttpd-cpp/tests/plugin_tests.hpp>
#include "../mod_blank.hpp"

MAKE_PLUGIN( mod_blank, "blank", LIGHTTPD_VERSION_ID );

class mod_blank_tests : public plugin_tests< mod_blank >
{
	public:
		mod_blank_tests( ) : plugin_tests< mod_blank >( "src/tests/mod_blank_stub.conf" )
		{
			p.init
			= p.cleanup
			= p.set_defaults
			= NULL;

			p.handle_trigger
			= p.handle_sighup
			= p.handle_uri_raw
			= p.handle_uri_clean
			= p.handle_docroot
			= p.handle_physical
			= p.handle_start_backend
			= p.handle_send_request_content
			= p.handle_response_header
			= p.handle_read_response_content
			= p.handle_filter_response_content
			= p.handle_response_done
			= p.connection_reset
			= p.handle_connection_close
			= p.handle_joblist
			= NULL;
		}

		virtual ~mod_blank_tests( ){ };

		plugin p;
};

TEST_F( mod_blank_tests, PluginConstruction )
{
	ASSERT_TRUE( srv );

	mod_blank p( *srv );

	EXPECT_EQ( std::string( "blank" ), p.name );
	EXPECT_EQ( LIGHTTPD_VERSION_ID, p.version );
}

TEST_F( mod_blank_tests, PluginInit )
{
	ASSERT_FALSE( mod_blank_plugin_init( &p ) );

	EXPECT_TRUE( p.name );
	EXPECT_TRUE( buffer_is_equal_string( p.name, mod_blank::name.c_str( ), mod_blank::name.size( ) ) );
	EXPECT_TRUE( p.init );
	EXPECT_TRUE( p.cleanup );
	EXPECT_TRUE( p.set_defaults );

	// Make sure all the handlers were set correctly
	EXPECT_FALSE( p.handle_trigger );
	EXPECT_FALSE( p.handle_sighup );
	EXPECT_TRUE( p.handle_uri_raw );
	EXPECT_TRUE( p.handle_uri_clean );
	EXPECT_TRUE( p.handle_docroot );
	EXPECT_TRUE( p.handle_physical );
	EXPECT_TRUE( p.handle_start_backend );
	EXPECT_FALSE( p.handle_send_request_content );
	EXPECT_FALSE( p.handle_response_header );
	EXPECT_FALSE( p.handle_read_response_content );
	EXPECT_FALSE( p.handle_filter_response_content );
	EXPECT_FALSE( p.handle_response_done );
	EXPECT_FALSE( p.connection_reset );
	EXPECT_FALSE( p.handle_connection_close );
	EXPECT_FALSE( p.handle_joblist );
}

