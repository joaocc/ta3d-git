/*  TA3D, a remake of Total Annihilation
    Copyright (C) 2005  Roland BROCHARD

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

#ifndef __LUA_ENV_H__
#define __LUA_ENV_H__

# include "../lua/lua.hpp"
# include "../threads/thread.h"

namespace TA3D
{
    /*!
    ** This class manages the Lua global environment and ensures Lua threads
    ** can access it safely
    */
    class LUA_ENV : public ObjectSync
    {
    protected:
        lua_State   *L;             // The global Lua state
    public:

        LUA_ENV();
        ~LUA_ENV();

        void set_global_string( const String &name, const String &value );
        void set_global_number( const String &name, const double value );
        void set_global_boolean( const String &name, const bool value );
        String get_global_string( const String &name );
        double get_global_number( const String &name );
        bool get_global_boolean( const String &name );
        bool is_global_string( const String &name );
        bool is_global_number( const String &name );
        bool is_global_boolean( const String &name );

    public:
        static LUA_ENV *global;
        static LUA_ENV *instance();
        static void destroy();
    };

}

#endif
