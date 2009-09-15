/**
 * For use as initialization for running tests on your mod_ objects.
 */

#ifndef _PLUGIN_TESTS_HPP_
#define _PLUGIN_TESTS_HPP_

#include <string>
#include <lighttpd-cpp/tests/plugin_base_tests.hpp>

template< typename Plugin >
class plugin_tests : public plugin_base_tests
{
	public:
		plugin_tests( const std::string config ) : plugin_base_tests( config ){ }
		virtual ~plugin_tests( ){ }
};

#endif // _PLUGIN_TESTS_HPP_
