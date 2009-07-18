/**
 * These are classes to automate the filling of what would be
 * the plugin_config and plugin_data structures if we were using
 * C, making the following valid (or close to valid):
 *  struct mod_blank
 *  {
 *  	mod_blank( ... ) : hostname( "some-config-tag" [, &validator [, &default_setter] ] ) { ... }
 *		std::string& default_setter( std::string& s ){ ... return std::string( "asdf" ); }
 *  	config_option< std::string > hostname;
 *  };
 *
 * So a config options type is specified at runtime, and we can add new types 
 * sit on top of the lighttpd ones.  i.e. we could have a network mask type,
 * getting options from T_CONFIG_STRING and that checks for the correct formatting.
 * If incorrect we could throw an exception to be caught by the set_defaults 
 * handler, or we could set it to the value returned by default_setter.
 */

#ifndef _LIGHTTPD_DATATYPE_HELPERS_HPP_
#define _LIGHTTPD_DATATYPE_HELPERS_HPP_

#include <vector>
#include <algorithm>
#include <string>

#include "c++-compat/base.h"
#include "c++-compat/plugin.h"

// Something for all config_options to have in common.
// From here we can call set_defaults for all config options
// using the set_defaults static function.
struct config_option_base
{
	typedef std::vector< config_option_base* > registry_type;
	typedef registry_type::iterator registry_iterator;
	typedef registry_type::const_iterator registry_const_iterator;

	config_option_base( const char* key ) : key( key )
	{
		registry.push_back( this );
	}

	~config_option_base( )
	{
		registry.erase( std::find( registry.begin(), registry.end(), this ) );
	}

	virtual handler_t set_defaults( const server& srv ) = 0;
	virtual handler_t set_defaults( ) = 0;

	static handler_t set_all_defaults( )
	{
		for( registry_iterator i = registry.begin( ); i != registry.end( ); ++i )
		{
			if( (*i)->set_defaults( ) != HANDLER_ERROR ) continue;
			return HANDLER_ERROR;
		}
		return HANDLER_GO_ON;
	}

	const char* key;
	const server* srv;

	static registry_type registry;
};

// Careful that we only get one of these per module.
// i.e. only one translation unit.
config_option_base::registry_type config_option_base::registry;

// Traits for the config_values_type_t enum values from lighttpd base.h
// These traits classes specify how the types should be initialized.
template < std::size_t ConfigValuesType >
struct config_values_traits
{};

// Keep a record of the T_CONFIG_* value in this base type
template < std::size_t ConfigValuesType >
struct config_values_traits_base
{
	static const config_values_type_t config_values_type;
};

template < std::size_t ConfigValuesType >
const config_values_type_t 
config_values_traits_base< ConfigValuesType >::config_values_type( ConfigValuesType );

// derive from this if we just use the default constructor
// Its annoying that we have to re-typedef things in derived
// classes but it seems that we do in gcc.  I haven't checked the 
// standard on this.  If it is a standard, then it enables overloading of
// types to an extent.
template < typename Type, std::size_t ConfigValuesType >
struct config_values_traits_default
 : config_values_traits_base< ConfigValuesType >
{
	typedef Type value_type;

	struct initializer
	{
		typedef value_type result;
		static result* act( )
		{
			return new result( );
		}
	};

	struct cleanup
	{
		typedef bool result;
		static result act( value_type* pvalue )
		{
			delete pvalue;
			return true;
		}
	};
};

// We need a traits class for each of these types:
// typedef enum { T_CONFIG_UNSET,
//		T_CONFIG_STRING,
//		T_CONFIG_SHORT,
//		T_CONFIG_INT,
//		T_CONFIG_BOOLEAN,
//		T_CONFIG_ARRAY,
//		T_CONFIG_LOCAL,
//		T_CONFIG_DEPRECATED,
//		T_CONFIG_UNSUPPORTED
//} config_values_type_t;

template <>
struct config_values_traits< T_CONFIG_STRING >
 : config_values_traits_base< T_CONFIG_STRING >
{
	typedef buffer value_type;

	struct initializer
	{
		typedef value_type result;
		static result* act( )
		{
			return buffer_init( );
		}
	};

	struct cleanup
	{
		typedef bool result;
		static result act( value_type* pvalue )
		{
			buffer_free( pvalue );
			return true;
		}
	};
};

template <>
struct config_values_traits< T_CONFIG_SHORT >
 : config_values_traits_default< short, T_CONFIG_SHORT > {};

template <>
struct config_values_traits< T_CONFIG_INT >
 : config_values_traits_default< int, T_CONFIG_INT >
{
	typedef config_values_traits_default< int, T_CONFIG_INT > super_type;
	typedef super_type::initializer initializer;
	typedef super_type::cleanup cleanup;
};

template <>
struct config_values_traits< T_CONFIG_BOOLEAN >
 : config_values_traits_default< unsigned short, T_CONFIG_BOOLEAN > {};

template <>
struct config_values_traits< T_CONFIG_ARRAY >
 : config_values_traits_base< T_CONFIG_ARRAY >
{
	typedef array value_type;

	struct initializer
	{
		typedef value_type result;

		static result* act( )
		{
			return array_init( );
		}
	};

	struct cleanup
	{
		typedef bool result;

		static result act( value_type* pvalue )
		{
			array_free( pvalue );
			return true;
		}
	};
};

template <>
struct config_values_traits< T_CONFIG_LOCAL >
{
};

template <>
struct config_values_traits< T_CONFIG_DEPRECATED >
{
};

template <>
struct config_values_traits< T_CONFIG_UNSUPPORTED >
{
};

// The traits class gives us a mapping from OptionType to the associated
// lighttpd enum T_CONFIG_* values.  Using this we can select a method of
// conversion from buffer/array/etc to our OptionType.
// OptionType must have a constructor that takes i.e. buf->ptr or array->data etc
template < typename OptionType, std::size_t ConfigValueType >
struct config_option_traits_base
{
	static const config_values_type_t value_enum;
	typedef config_values_traits< ConfigValueType > values_type_traits;
	typedef typename values_type_traits::value_type value_type;
	typedef OptionType option_type;

	// A default initializer.  We can do this in gcc, not sure if it
	// is standard.
	struct initializer
	{
		typedef option_type result;
		static result* act( const value_type* pvalue )
		{
			return new option_type;
		}
	};
};

template < typename OptionType, std::size_t ConfigValueType >
const config_values_type_t config_option_traits_base< OptionType, ConfigValueType >
::value_enum( static_cast< config_values_type_t >( ConfigValueType ) );

template < typename OptionType >
struct config_option_traits
{};

// Configure some default types
template <>
struct config_option_traits< std::string > : config_option_traits_base< std::string, T_CONFIG_STRING >
{
	// templating seems to mean we have to pull typedefs down
	typedef config_option_traits_base< std::string, T_CONFIG_STRING > super_type;
	typedef super_type::value_type value_type;
	typedef super_type::values_type_traits values_type_traits;
	typedef std::string option_type;

	struct initializer
	{
		typedef option_type result;
		static result* act( const value_type* buf )
		{
			return new option_type( buf->ptr );
		}
	};		
};

template <>
struct config_option_traits< int > : config_option_traits_base< int, T_CONFIG_INT >
{
	typedef config_option_traits_base< int, T_CONFIG_INT > super_type;
	typedef super_type::initializer initializer;
};

template <>
struct config_option_traits< short > : config_option_traits_base< short, T_CONFIG_SHORT >
{
	typedef config_option_traits_base< short, T_CONFIG_SHORT > super_type;
	typedef super_type::initializer initializer;
};

template <>
struct config_option_traits< bool > : config_option_traits_base< bool, T_CONFIG_BOOLEAN >
{
	typedef config_option_traits_base< bool, T_CONFIG_BOOLEAN > super_type;
	typedef super_type::initializer initializer;
};

// For now I'll just allow vectors of string.  A boost::any would be nice here maybe.
// I'd like to try and stay in with the people are thinking I'm nuts including boost
// in here, but I think I'm already through the looking glass so I'll probably end up
// going all out with the templating and boost inclusion.
template <>
struct config_option_traits< std::vector< std::string > >
 : config_option_traits_base< std::vector< std::string >, T_CONFIG_ARRAY >
{};

// The config_option structures deal with condition decisions so we can write
// option[ con ] where option is a L(config_option< SomeType >), con is a L(connection) 
// and get back the appropriate L(OptionType) option from the "defaults" memeber data.
template < 	typename OptionType, 
			std::size_t ConfigScopeType = T_CONFIG_SCOPE_CONNECTION,
			typename OptionTraits = config_option_traits< OptionType > >
struct config_option : public config_option_base
{
	typedef std::vector< const OptionType* > defaults_list_type;
	typedef typename defaults_list_type::const_iterator defaults_iterator;
	typedef OptionTraits option_traits;
	typedef typename option_traits::values_type_traits values_type_traits;

	typedef bool (*validator_type)( const OptionType& );
	typedef bool (*defaults_setter_type)( OptionType& );

	config_option(	const char* key,
					validator_type val = 0,
					defaults_setter_type def = 0 )
	 : config_option_base( key )
	{ }

	virtual handler_t set_defaults( )
	{
		return set_defaults( *srv );
	}

	virtual handler_t set_defaults( const server& s )
	{
		// initializer defines the method that the lighttpd
		// config type should be created with, before it is
		// passed to config_insert_values_global.
		typedef typename values_type_traits::initializer initializer;
		typedef typename initializer::result initializer_result_type;

		typedef typename values_type_traits::cleanup cleanup;
		typedef typename cleanup::result cleanup_result_type;

		// next be have information how the data structure created
		// with the above should be converted to our chosen option type.
		typedef typename option_traits::initializer option_initializer;

		srv = &s;
		config_values_t cv[] = {
			{ key,	NULL, option_traits::value_enum, static_cast< config_scope_type_t >( ConfigScopeType ) },
			{ NULL,	NULL, T_CONFIG_UNSET, T_CONFIG_SCOPE_UNSET }
		};

		for( std::size_t i = 0; i < s.config_context->used; i++ )
		{
			initializer_result_type* res = initializer::act();
			cv[0].destination = reinterpret_cast< void* >( res );

			if ( 0 != config_insert_values_global( const_cast< server* >( srv ), 
							((data_config *)srv->config_context->data[i])->value, cv) )
			{
				return HANDLER_ERROR;
			}

			// Initialize option, but don't take control of the 
			// res memory allocation ( i.e. just make a copy and leave as is )
			OptionType* option = option_initializer::act( res );
			if( ( validator && defaults_setter ) && !validator( *option ) )
				defaults_setter( *option );
			defaults.push_back( option );

			// Although we don't do anything with the result,
			// I'll keep the option available
			cleanup_result_type cleanup_result = cleanup::act( res );
		}

		return HANDLER_GO_ON;
	}

	// Return the appropriate value for the options, depending on the 
	// server and connection.
	const OptionType& operator[]( const connection& con ) const
	{
		// Lets start with the global context.  Front must exist.
		// Plus I'd hope that config_context->used and the size of
		// defaults are the same.
		defaults_iterator di = defaults.begin();
		const OptionType* option = *di;
		const data_config** dc = ++((data_config **)srv->config_context->data);

		// skip the first, the global context
		while( di != defaults.end() )
		{
			// condition match
			if( config_check_cond( srv, &con, *dc ) )	
			{
				// merge config
				data_unset **du = (*dc)->value->data;
				data_unset **du_end = du + (*dc)->value->used;
				while( du++ != du_end )
				{
					if ( buffer_is_equal_string( (*du)->key, CONST_STR_LEN(key) ) )
					{
						option = *di;
					}
				}
			}

			++dc; ++di;
		}

		return *option;
	}

	validator_type validator;
	defaults_setter_type defaults_setter;
	defaults_list_type defaults;

	static const config_scope_type_t config_scope;
};

// Record what type of config we are
template < typename OptionType, std::size_t ConfigScopeType, typename OptionTraits >
const config_scope_type_t config_option< OptionType, ConfigScopeType, OptionTraits >
::config_scope( static_cast< config_scope_type_t >( ConfigScopeType ) );

#endif // _LIGHTTPD_DATATYPE_HELPERS_HPP_


