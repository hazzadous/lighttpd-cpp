
#include "mod_blank.hpp"

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

mod_blank::mod_blank( server& srv )
 :	Plugin< mod_blank >( "blank", 1, srv ),
	some_string		( "some_string" ),
	some_int		( "some_int" ),
	some_bool		( "some_bool" ),
	some_short		( "some_short" )
{}

MAKE_PLUGIN( mod_blank );

