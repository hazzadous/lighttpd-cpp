"""
Author: 	Harry Waye
Desc: 		SConstruct build framework for building a lighttpd library
			and the mod_blank example plugin based on the lighttpd-cpp
			library.
"""

##
# General environment config.
##
#Append( LIBPATH=[ 'lib/' ] )
env = Environment( )

##
# Building the lighttpd static library
# I'm sure there are more neat ways of doing these things,
# but lets just run lemon every time for now.
##

# Used to join defines later on.
import string

defines = \
[
	'HAVE_SOCKLEN_T', 
	'HAVE_STDINT_H', 
	'HAVE_SYS_UN_H',
	'HAVE_INET_ATON',
	'HAVE_MMAP',
	# For some reason the lighty version is 0x10500 converted to decimal.
	'LIGHTTPD_VERSION_ID=0x10500',
	'PACKAGE_NAME=\\"lighttpd\\"', 
	'PACKAGE_VERSION=\\"1.5.0\\"',
	'LIBRARY_DIR=\\"./lib/\\"'
]


##
# Compile our empty module.
##
mod_blank_list = SharedLibrary \
( 
	'src/mod_blank', 
	'src/mod_blank.cpp',  
	SHLIBPREFIX='', CCFLAGS="-I./include/ -D'LIGHTTPD_VERSION_ID=0x10500'"
)

##
# Compile our empty module tests.
##
Program \
(
	'src/tests/datatype_helper_tests',
	'src/tests/datatype_helper_tests.cpp',
	CCFLAGS="-I./include/ " + string.join( [ '' ] + defines, " -D" ), LIBPATH=[ "./lib/" ], 
	LIBS=[ "gtest_main", mod_blank_list, "dl"  ]
)

Program \
(
	'src/tests/handler_setter_tests',
	'src/tests/handler_setter_tests.cpp',
	CCFLAGS="-I./include/ " + string.join( [ '' ] + defines, " -D" ), LIBPATH=[ "./lib/" ], 
	LIBS=[ "gtest_main", mod_blank_list, "dl"  ]
)

Program \
(
	'src/tests/mod_blank_tests',
	'src/tests/mod_blank_tests.cpp',
	CCFLAGS="-g -I./include/ " + string.join( [ '' ] + defines, " -D" ), LIBPATH=[ "./lib/" ], 
	LIBS=[ "gtest_main", mod_blank_list, "dl"  ]
)

