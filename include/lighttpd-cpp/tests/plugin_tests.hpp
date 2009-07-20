/**
 * For use as initialization for running tests on your mod_ objects.
 */

#ifndef _PLUGIN_TESTS_HPP_
#define _PLUGIN_TESTS_HPP_

#include <gtest/gtest.h>

template< typename Plugin >
class plugin_tests : public ::testing::Test
{
	plugin_tests( ){ }
	virtual ~plugin_tests( ){ }

	virtual void SetUp( )
	{
		
	}

	virtual void TearDown( )
	{

	}
}

#endif _PLUGIN_TESTS_HPP_
