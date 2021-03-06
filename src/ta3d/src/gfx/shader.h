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
#ifndef __TA3D_GFX_Shader_H__
# define __TA3D_GFX_Shader_H__

# include <stdafx.h>
# include <misc/string.h>


namespace TA3D
{

	/*!
	** \brief Shader
	*/
	class Shader
	{
	public:
		//! \name Constructor & Destructor
		//@{
		//! Default constructor
		Shader()
			:pLoaded(false)
		{}
		//! Destructor
		~Shader() {destroy();}
		//@}

		void destroy();


		/*!
		** \brief Load a shader from files
		*/
		void load(const String& pShaderFragmentFilename, const String& vertexFilename);

		/*!
		** \brief Load a shader from a memory buffer
		*/
		void load_memory(const char* pShaderFragment_data, const int frag_len, const char *vertex_data, const int vert_len);


		/*!
		** \brief Enable the shader
		*/
		void on();

		/*!
		** \brief Disable the shader
		*/
		void off();


		//! \name Variable for the ARB extension
		//@{
		void setvar1f(const String &var, const float v0);
		void setvar2f(const String &var, const float v0, const float v1);
		void setvar3f(const String &var, const float v0, const float v1, const float v2);
		void setvar4f(const String &var, const float v0, const float v1, const float v2, const float v3);
		void setvar1i(const String &var, const int v0);
		void setvar2i(const String &var, const int v0, const int v1);
		void setvar3i(const String &var, const int v0, const int v1, const int v2);
		void setvar4i(const String &var, const int v0, const int v1, const int v2, const int v3);

		void setmat4f(const String &var, const GLfloat *mat);
		//@}

		/*!
		** \brief Get if the shader has been loaded by the previous call to `load` or `load_memory`
		**
		** \see load()
		** \see load_memory()
		*/
		bool isLoaded() const {return pLoaded;}


	private:
		//! Is the shader loaded ?
		bool pLoaded;
		//!
		GLhandleARB		pShaderProgram;
		//!
		GLhandleARB		pShaderFragment;
		//!
		GLhandleARB		pShaderVertex;

	}; // class Shader




} // namespace TA3D

#endif // __TA3D_GFX_Shader_H__
