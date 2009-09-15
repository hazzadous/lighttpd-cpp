/**
 * For use as initialization for tests on plugin_base objects.
 */

#ifndef _PLUGIN_BASE_TESTS_HPP_
#define _PLUGIN_BASE_TESTS_HPP_

#include <string>
#include <lighttpd-cpp/tests/lighttpd_tests.hpp>

class plugin_base_tests : public lighttpd_tests
{
	public:
		plugin_base_tests( const std::string config ) : lighttpd_tests( config )
		{
		}

		virtual ~plugin_base_tests( ){ }
};

#endif // _PLUGIN_BASE_TESTS_HTTP_
