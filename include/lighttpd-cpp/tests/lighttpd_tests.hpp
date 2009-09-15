/**
 * Test base that sets up a srv object and passes specified config file.
 */

#ifndef _LIGHTTPD_TESTS_HPP_
#define _LIGHTTPD_TESTS_HPP_

#include <gtest/gtest.h>
#include <string>

#include <lighttpd-cpp/c++-compat/server.h>

extern server* server_init( );
extern void server_free( server* srv );

class lighttpd_tests : public testing::Test
{
	public:
		lighttpd_tests( const std::string config="lighttpd.conf" ) : config( config )
		{
		}

		virtual ~lighttpd_tests( ){ }

		/**
		 * Init and configure a server instance, srv.
		 */
		void SetUp( )
		{
			// Make allocations for the srv and parse the config file.
			srv = server_init( );

			srv->srvconf.port = 0;
			srv->srvconf.dont_daemonize = 0;
			srv->srvconf.daemonize_on_shutdown = 0;
			srv->srvconf.max_stat_threads = 4;
			srv->srvconf.max_read_threads = 8;

			//config_read( srv, "" );
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
		}

	protected:
		const std::string config;
		server* srv;
};

TEST_F( lighttpd_tests, ServerInitialized )
{
	ASSERT_TRUE( srv );
	ASSERT_TRUE( srv->conns );
	ASSERT_TRUE( srv->joblist );
	ASSERT_TRUE( srv->joblist_prev );
	ASSERT_TRUE( srv->fdwaitqueue );
}


#endif // _LIGHTTPD_TESTS_HPP_

