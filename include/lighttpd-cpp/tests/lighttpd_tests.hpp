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
			srv = server_init( );
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
	ASSERT_TRUE( srv != NULL );
}


#endif // _LIGHTTPD_TESTS_HPP_
