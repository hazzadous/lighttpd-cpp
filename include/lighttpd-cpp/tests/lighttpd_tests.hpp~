/**
 * Test base that sets up a srv object and passes specified config file.
 */

#ifndef _LIGHTTPD_TESTS_HPP_
#define _LIGHTTPD_TESTS_HPP_

#include <gtest/gtest.h>
#include <string>

#include <lighttpd-cpp/c++-compat/server.h>

// We need to have logging otherwise lighttp explodes.
extern "C"
{
	void log_init( );
	void status_counter_init( );

	void log_free( );
	void status_counter_free( );
}

// We use our own init/free for server as the lighttpd ones are internal to server.c.
extern server* server_init( );
extern void server_free( server* srv );

class lighttpd_tests : public testing::Test
{
	public:
		lighttpd_tests( const std::string config="./src/tests/lighttpd.conf" ) : config( config )
		{
		}

		virtual ~lighttpd_tests( ){ }

		/**
		 * Init and configure a server instance, srv.
		 */
		void SetUp( )
		{
			// If we do not have log and status_counter then we die.
			log_init( );
			status_counter_init( );

			// Make allocations for the srv and parse the config file.
			srv = server_init( );

			srv->srvconf.port = 0;
			srv->srvconf.dont_daemonize = 0;
			srv->srvconf.daemonize_on_shutdown = 0;
			srv->srvconf.max_stat_threads = 4;
			srv->srvconf.max_read_threads = 8;

			config_read( srv, config.c_str( ) );
		}

		/**
		 * Clean up the srv instance.
		 */
		void TearDown( )
		{
			if( srv != NULL )
			{
				server_free( srv );
			}

			log_free( );
			status_counter_free( );
		}

	protected:
		const std::string config;
		server* srv;
};

TEST_F( lighttpd_tests, ServerInitialized )
{
	ASSERT_TRUE( srv );
	EXPECT_TRUE( srv->conns );
	EXPECT_TRUE( srv->joblist );
	EXPECT_TRUE( srv->joblist_prev );
	EXPECT_TRUE( srv->fdwaitqueue );

	EXPECT_TRUE( srv->config_storage );
}

TEST_F( lighttpd_tests, ConfigParsed )
{
	data_unset *dc = srv->config_context->data[0];

	// Check global config is there, and (random test) that the port is 8080.
	ASSERT_TRUE( dc );
	// EXPECT_EQ( 8080, 
}


#endif // _LIGHTTPD_TESTS_HPP_

