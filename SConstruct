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

lighttpd_include = 'include/lighttpd/'

# First we need to build lemon, some parser that generates
# .c and .h files from .y files.  Never seen it before.
lemon_list = Program( '%s/lemon' % lighttpd_include, '%s/lemon.c' % lighttpd_include )

# Now we have this lemon thingy lets do stuff with the .y files.
lemon_files = Glob( '%s/*.y' % lighttpd_include )
lemon_list = [ ]
for lemon_file in lemon_files:
	lemon = Command( '', '', 'cd %s; ./lemon %s' % ( lighttpd_include, lemon_file.name ) )
	lemon_list += lemon
	Requires( lemon, '%s/lemon' % lighttpd_include )

# Select all the source files from lighttpd and remove select ones.
# I'm too lazy to pick out all the ones that are needed.
# There are probably far too many things in lighttpd.a after this.
source_files = Glob( '%s/*.c' % lighttpd_include )
Requires( source_files, lemon_list )

# filenames to exclude in 
exclude_filenames = \
[ 	
	'lemon.c', 
	'lempar.c', 
	'network.c', 
	'splaytree.c', 
	'stream.c', 
	'xgetopt.c',
	'sys-socket.c', 
	'fcgi-stat-accel.c',
	'server.c' 
]

exclude_files = [ File( '%s/%s' % ( lighttpd_include, filename ) ) for filename in exclude_filenames ] + \
				Glob( '%s/*_test.c' % lighttpd_include ) + \
				Glob( '%s/mod_*.c' % lighttpd_include )

library_sources = list( set( source_files ) - set( exclude_files ) )

defines = \
[
	'HAVE_SOCKLEN_T', 
	'HAVE_STDINT_H', 
	# For some reason the lighty version is 0x10500 converted to decimal.
	'LIGHTTPD_VERSION_ID=0x10500',
	'PACKAGE_NAME=\\"lighttpd\\"', 
	'PACKAGE_VERSION=\\"1.5.0\\"'
]

# Make our lighttpd library
lighttpd_list = SharedLibrary \
( 
	'lib/lighttpd', 
	library_sources, 
	CCFLAGS=string.join( [ '' ] + defines, " -D" ) 
)

##
# Compile our empty module.
##
SharedLibrary \
( 
	'src/mod_blank', 
	'src/mod_blank.cpp',  
	SHLIBPREFIX='', CCFLAGS="-I./include/ -D'LIGHTTPD_VERSION_ID=0x10500'"
)

