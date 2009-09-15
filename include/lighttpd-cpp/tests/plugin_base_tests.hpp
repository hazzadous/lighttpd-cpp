/**
 * For use as initialization for tests on plugin_base objects.
 */

#ifndef _PLUGIN_BASE_TESTS_HPP_
#define _PLUGIN_BASE_TESTS_HPP_

#include "../plugin.hpp"

#include <string>
#include "lighttpd_tests.hpp"

#include <gtest/gtest.h>

class plugin_base_tests : public lighttpd_tests
{
	public:
		plugin_base_tests( ){ }

		plugin_base_tests( const std::string config ) : lighttpd_tests( config )
		{}

		virtual ~plugin_base_tests( ){ }
};

TEST_F( plugin_base_tests, PluginBaseInit )
{
	plugin_base pb( "test", 1234, *srv );

	EXPECT_TRUE( srv );
	EXPECT_EQ( std::string( "test" ), pb.name );
	EXPECT_EQ( 1234, pb.version );
	EXPECT_EQ( srv, &(pb.srv) );
}

#endif // _PLUGIN_BASE_TESTS_HTTP_

