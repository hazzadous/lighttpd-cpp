"""
Author: 	Harry Waye
Desc: 		SConstruct build framework for building a lighttpd library
			and the mod_blank example plugin based on the lighttpd-cpp
			library.
"""

##
# Building the lighttpd.a
# I'm sure there are more neat ways of doing these things,
# but lets just run lemon every time for now.
##

# Used to join defines later on.
import string

# First we need to build lemon, some parser that generates
# .c and .h files from .y files.  Never seen it before.
lemon_list = Program( 'include/lighttpd/lemon', 'include/lighttpd/lemon.c' )

# Now we have this lemon thingy lets do stuff with the .y files.
lemon_files = Glob( 'include/lighttpd/*.y' )
lemon_list = [ ]
for lemon_file in lemon_files:
	lemon = Command( '', '', 'cd include/lighttpd/; ./lemon ' + lemon_file.name )
	lemon_list += lemon
	Requires( lemon, 'include/lighttpd/lemon' )

# Select all the source files from lighttpd and remove select ones.
# I'm too lazy to pick out all the ones that are needed.
# There are probably far too many things in lighttpd.a after this.
source_files = Glob( 'include/lighttpd/*.c' )
Requires( source_files, lemon_list )
exclude_files = [	File( 'include/lighttpd/lemon.c' ), 
					File( 'include/lighttpd/lempar.c' ), 
					File( 'include/lighttpd/network.c' ), 
					File( 'include/lighttpd/splaytree.c' ), 
					File( 'include/lighttpd/stream.c' ), 
					File( 'include/lighttpd/xgetopt.c' ),
					File( 'include/lighttpd/sys-socket.c' ), 
					File( 'include/lighttpd/fcgi-stat-accel.c' ),
					File( 'include/lighttpd/server.c' ) ] + Glob( 'include/lighttpd/*_test.c' )	+ Glob( 'include/lighttpd/mod_*.c' )
library_sources = list( set( source_files ) - set( exclude_files ) )

defines = [	'HAVE_SOCKLEN_T', 
			'HAVE_STDINT_H', 
			'LIGHTTPD_VERSION_ID=1.5', 
			'PACKAGE_NAME=\\"lighttpd\\"', 
			'PACKAGE_VERSION=\\"1.5.0\\"' ]

# Make our lighttpd library
lighttpd_list = StaticLibrary( 'lib/lighttpd.a', library_sources, CCFLAGS=string.join( [ '' ] + defines, " -D" ) )

# Compile our empty module.
SharedLibrary( 'src/mod_blank.so', 'src/mod_blank.cpp', LIBPATH=[ 'lib/' ], LIBS=lighttpd_list, CCFLAGS="-I./include/" )

