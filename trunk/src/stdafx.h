/*  TA3D, a remake of Total Annihilation
    Copyright (C) 2006  Roland BROCHARD

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA*/

/*
**  File: stdafx.h
** Notes:  **** PLEASE SEE README.txt FOR LICENCE AGREEMENT ****
**  Cire: *stdafx.h and stdafx.cpp will generate a pch file
**         (pre compiled headers).
**        *All other cpp files MUST include this file as its first include.
**        *No .h files should include this file.
**        *The goal is to include everything that we need from system, and
**          game libiries, ie everything we need external to our project.
*/

#ifndef __TA3D_STDAFX_H__
# define __TA3D_STDAFX_H__

// Include the config options generated by the configure script
# include "../config.h"

# include "misc/string.h"
# include <vector>



# ifdef TA3D_PLATFORM_WINDOWS
#   ifndef _MSC_VER
#       define TA3D_PLATFORM_GCC
#       define TA3D_PLATFORM_MINGW
#   else
#       define TA3D_PLATFORM_MSVC
#   endif
# endif


// The rand() function on Windows platforms should be replaced by something
// that fits a 32bits integer (it would be slower of course)
# ifdef TA3D_PLATFORM_WINDOWS
#   define TA3D_RAND()	(rand() | (rand() << 16))
# else
#   define TA3D_RAND()	rand()
# endif



# if defined TA3D_PLATFORM_WINDOWS && defined TA3D_PLATFORM_MSVC
#   if defined (_INTEGRAL_MAX_BITS) && _INTEGRAL_MAX_BITS >= 64 // For Visual 6.x and later
    // 64-bit ints, guaranteed to be 4 bytes in size
	typedef unsigned __int64  uint64;
	typedef signed __int64    sint64;
#   else
#       error __int64 type not supported
#   endif
	// 32-bit ints, guaranteed to be 4 bytes in size
	typedef unsigned __int32  uint32;
	typedef signed __int32    sint32;
	// 16-bit ints, guaranteed to be 2 bytes in size
	typedef unsigned __int16  uint16;
	typedef signed __int16    sint16;
	// 8-bit ints, guaranteed to be 1 byte in size
	typedef unsigned __int8   uint8;
	typedef signed __int8     sint8;
# else
    // 64-bit ints, guaranteed to be 8 bytes in size
	typedef uint64_t  uint64;
	typedef int64_t   sint64;
	// 32-bit ints, guaranteed to be 4 bytes in size
	typedef uint32_t  uint32;
	typedef int32_t   sint32;
	// 16-bit ints, guaranteed to be 2 bytes in size
	typedef uint16_t uint16;
	typedef int16_t  sint16;
	// 8-bit ints, guaranteed to be 1 byte in size
	typedef uint8_t uint8;
	typedef int8_t  sint8;
# endif



# if defined TA3D_PLATFORM_WINDOWS //&& defined TA3D_PLATFORM_MSVC
#   ifdef TA3D_PLATFORM_MSVC
#      pragma warning(disable : 4554) 
#      pragma warning(disable : 4996)
#      pragma comment( lib, "tools/win32/libs/alleg.lib" )
#      pragma comment( lib, "tools/win32/libs/agl.lib" )
#      pragma comment( lib, "opengl32.lib" )
#      pragma comment( lib, "glu32.lib" )
#      pragma comment( lib, "glaux.lib" )
#      pragma comment( lib, "tools/win32/libs/glew32.lib" )
#      include "tools/win32/include/gl/glew.h"
    // Cire: I had to setup a pragma on c4312, 4005 warnings, because allgero include
    //  was generating alot of compiler noise.
#      pragma warning( disable : 4312 )
#      pragma warning( disable : 4005 )
#   endif
#   include <alleggl.h> // alleggl also includes allegro
    // Cire: Restore warning states
#   ifdef TA3D_PLATFORM_MSVC
#      pragma warning( default : 4005 )
#      pragma warning( default : 4312 )
#   endif
# else 
    // Cire:
    // Other platfroms may wish to adjust how allegro is included, for
    // now its this way.
#   include <alleggl.h>
#endif



// OpenGL
# ifdef TA3D_PLATFORM_DARWIN
#   include <Headers/glu.h> // see `/System/Library/Frameworks/OpenGL.framework`
# else
#   include <GL/glu.h>
# endif

#include "jpeg/ta3d_jpg.h"
#include <math.h>


// Cire:
//   Since byte seems to be common throughout the project we'll typedef here.
typedef uint8         byte;
typedef unsigned char uchar;
typedef signed char   schar;

// Floating point types, defined for consistencies sakes.
typedef float  real32;
typedef double real64;


#ifdef max
	#undef max
#endif

#ifdef min
	#undef min
#endif

template<class T> inline T max(T a, T b)	{	return (a > b) ? a : b;	}
template<class T> inline T min(T a, T b)	{	return (a > b) ? b : a;	}

#define isNaN(x) isnan(x)

#if defined TA3D_PLATFORM_WINDOWS && defined TA3D_PLATFORM_MSVC
// Cire:
//  The below functions don't exists within windows math routines.
	#define strcasecmp(x,xx) _stricmp( x, xx )

	static inline const real32 asinh( const real32 f )
	{
		return (real32)log( (real32)(f + sqrt( f * f + 1)) );
	}

	static inline const real32 acosh( const real32 f )
	{
		return (real32)log( (real32)( f + sqrt( f * f- 1)) );
	}

	static inline const real32 atanh( const real32 f)
	{
		return (real32)log( (real32)( (real32)(1.0f / f + 1.0f) /
			(real32)(1.0f / f - 1.0f))  ) / 2.0f;
	}
#endif

#if ((MAKE_VERSION(4, 2, 1) == MAKE_VERSION(ALLEGRO_VERSION, ALLEGRO_SUB_VERSION, ALLEGRO_WIP_VERSION)) && ALLEGRO_WIP_VERSION>0) \
	|| (MAKE_VERSION(4, 2, 1) < MAKE_VERSION(ALLEGRO_VERSION, ALLEGRO_SUB_VERSION, ALLEGRO_WIP_VERSION))
	#define FILE_SIZE	file_size_ex
#else
	#define FILE_SIZE	file_size
#endif



namespace TA3D
{

    //! \name String manipulations
    //@{


    /*!
    ** \brief Copy then Convert the case (lower case) of characters in the string
    ** \param s The string to convert
    ** \return The new string
    */
	String Lowercase(const String& szString);
	
    /*!
    ** \brief Copy then Convert the case (upper case) of characters in the string
    ** \param s The string to convert
    ** \return The new string
    */
    String Uppercase(const String& szString);
	
    String format(const char* fmt, ...);
	
    /*!
    ** \brief Copy then Remove trailing and leading spaces
    ** \param s The string to convert
    ** \param trimChars The chars to remove
    ** \return The new string
    */
    String TrimString(const String& s, String trimChars = String(" \t\n\r"));
	
    sint32 SearchString(const String& s, const String& StringToSearch, const bool ignoreCase);
	
    String ReplaceString(const String& s, const String& toSearch, const String& replaceWith, const bool ignoreCase);
	
    String ReplaceChar(const String& s, const char toSearch, const char replaceWith);
	
    bool StartsWith(const String& a, const String& b);
	
    uint32 hash_string(const String& s);
	
    int find(const String::Vector& v, const String& s);
    
    //@} String manipulations

    #if defined TA3D_PLATFORM_WINDOWS && defined TA3D_PLATFORM_MSVC
	void ExtractPathFile(const String& szFullFileName, String& szFile, String& szDir);
    #endif


	String GetClientPath(void);

	bool IsPowerOfTwo(int a);

	String get_path(const String& s);


} // namespace TA3D



// zuzuf: to prevent some warnings
# undef PACKAGE_BUGREPORT
# undef PACKAGE_NAME
# undef PACKAGE_TARNAME
# undef PACKAGE_STRING
# undef PACKAGE_VERSION


#endif // __TA3D_STDAFX_H__
