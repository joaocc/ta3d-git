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

#include "fx.manager.h"
#include "../misc/math.h"
#include "../mesh/instancing.h"         // Because we need the RenderQueue object
#include "../ingame/players.h"


namespace TA3D
{

	FXManager fx_manager;
	MODEL* FXManager::currentParticleModel = NULL;



	int FXManager::add(const String& filename, const String& entryName, const Vector3D& pos, const float size)
	{
		if(Camera::inGame != NULL && (pos - Camera::inGame->pos).sq() >= Camera::inGame->zfar2)
			return -1;

		MutexLocker locker(pMutex);

		if (nb_fx + 1 > max_fx)
		{
			max_fx++;
			fx.resize( max_fx );
		}
		++nb_fx;
		int idx = -1;

		for (int i = 0; i < max_fx; ++i)
		{
			if (!fx[i].playing)
			{
				idx = i;
				break;
			}
		}

		String tmp("anims\\");
		tmp << filename << ".gaf";

		String fullname(tmp);
		fullname << "-" << entryName;

		int anm_idx = findInCache(fullname);
		if (anm_idx == -1)
		{
			byte *data;
			if (strcasecmp(filename.c_str(),"fx"))
				data = VFS::Instance()->readFile(tmp);
			else
				data = fx_data;
			if (data)
			{
				Gaf::Animation* anm = new Gaf::Animation();
				anm->loadGAFFromRawData(data, Gaf::RawDataGetEntryIndex(data, entryName));
				// Next line has been removed in order to remain thread safe, conversion is done in main thread
				//			anm->convert(false,true);
				pCacheIsDirty = true;				// Set cache as dirty so we will do conversion at draw time

				anm_idx = putInCache(fullname, anm);

				if(data != fx_data)
					DELETE_ARRAY(data);
			}
		}
		else
			++use[anm_idx];
		fx[idx].load(anm_idx, pos, size);

		return idx;
	}


	void FXManager::loadData()
	{
		pMutex.lock();
		currentParticleModel = model_manager.get_model("fxpart");
		// Reload the texture for flashes
		if (flash_tex == 0)
			flash_tex = gfx->load_texture("gfx/flash.tga");
		// Reload the texture for ripples
		if (ripple_tex == 0)
			ripple_tex = gfx->load_texture("gfx/ripple.tga");
		// Reload textures for waves
		if (wave_tex[0] == 0)
			wave_tex[0] = gfx->load_texture("gfx/wave0.tga");
		if (wave_tex[1] == 0)
			wave_tex[1] = gfx->load_texture("gfx/wave1.tga");
		if (wave_tex[2] == 0)
			wave_tex[2] = gfx->load_texture("gfx/wave2.tga");
		pMutex.unlock();
	}


	int FXManager::addFlash(const Vector3D& pos, const float size)
	{
		if(Camera::inGame != NULL && (pos - Camera::inGame->pos).sq() >= Camera::inGame->zfar2)
			return -1;

		MutexLocker locker(pMutex);
		if(nb_fx + 1 > max_fx)
		{
			max_fx++;
			fx.resize( max_fx );
		}
		++nb_fx;
		int idx=-1;
		for (int i=0;i<max_fx; ++i)
		{
			if(!fx[i].playing)
			{
				idx=i;
				break;
			}
		}
		fx[idx].load(-1, pos, size);

		return idx;
	}



	int FXManager::addWave(const Vector3D& pos,float size)
	{
		if (Camera::inGame != NULL && (pos-Camera::inGame->pos).sq() >= Camera::inGame->zfar2)
			return -1;

		MutexLocker locker(pMutex);

		if(nb_fx+1>max_fx)
		{
			max_fx++;
			fx.resize( max_fx );
		}
		++nb_fx;
		int idx = -1;
		for (int i = 0; i < max_fx; ++i)
		{
			if(!fx[i].playing)
			{
				idx=i;
				break;
			}
		}
		fx[idx].load(-2 - (Math::RandomTable() % 3), pos, size * 4.0f);

		return idx;
	}

	int FXManager::addRipple(const Vector3D& pos,float size)
	{
		if (Camera::inGame != NULL && (pos - Camera::inGame->pos).sq() >= Camera::inGame->zfar2)
			return -1;

		MutexLocker locker(pMutex);

		if(nb_fx + 1 > max_fx)
		{
			max_fx ++;
			fx.resize( max_fx );
		}
		++nb_fx;
		int idx = -1;
		for (int i = 0; i < max_fx; ++i)
		{
			if(!fx[i].playing)
			{
				idx=i;
				break;
			}
		}
		fx[idx].load(-5, pos, size);

		return idx;
	}




	void FXManager::doClearAllParticles()
	{
		pElectrics.clear();
		if (!pParticles.empty())
		{
			for (ListOfParticles::iterator i = pParticles.begin(); i != pParticles.end(); ++i)
				delete *i;
			pParticles.clear();
		}
	}

	void FXManager::init()
	{
		doClearAllParticles();
		currentParticleModel = NULL;
		pCacheIsDirty = false;
		fx_data = NULL;
		max_fx = 0;
		nb_fx = 0;
		fx.clear();

		max_cache_size = 0;
		cache_size = 0;
		cache_name.clear();
		cache_anm = NULL;
		use = NULL;

		flash_tex = 0;
		wave_tex[0] = 0;
		wave_tex[1] = 0;
		wave_tex[2] = 0;
		ripple_tex = 0;
	}



	void FXManager::destroy()
	{
		doClearAllParticles();

		gfx->destroy_texture(flash_tex);
		gfx->destroy_texture(ripple_tex);
		gfx->destroy_texture(wave_tex[0]);
		gfx->destroy_texture(wave_tex[1]);
		gfx->destroy_texture(wave_tex[2]);

		DELETE_ARRAY(fx_data);
		fx.clear();
		if (cache_size > 0)
		{
			cache_name.clear();
			if (cache_anm)
			{
				for (int i = 0;i < max_cache_size; ++i)
					delete cache_anm[i];
				DELETE_ARRAY(cache_anm);
			}
		}
		DELETE_ARRAY(use);
		init();
	}

	void FXManager::draw(Camera& cam, MAP *map, float w_lvl, bool UW)
	{
		pMutex.lock();

		if (pCacheIsDirty)// We have work to do
		{
			for (int i = 0 ; i < max_cache_size ; ++i)
			{
				if (cache_anm[i])
					cache_anm[i]->convert(false,true);
			}
			pCacheIsDirty = false;
		}

		glColor4ub(0xFF, 0xFF, 0xFF, 0xFF);

		glEnable(GL_TEXTURE_2D);
		glDisable(GL_LIGHTING);
		glDisable(GL_CULL_FACE);
		glDepthMask(GL_FALSE);
		gfx->set_alpha_blending();
		cam.setView(true);
		glEnable(GL_POLYGON_OFFSET_FILL);
		glPolygonOffset(0.0f,-1600.0f);
		if (UW)
		{
			for(int i = 0 ; i < max_fx ; ++i)
			{
				if (fx[i].playing && fx[i].Pos.y < w_lvl)
					fx[i].draw(cam, map, cache_anm);
			}
		}
		else
		{
			for(int i = 0; i < max_fx; ++i)
			{
				if (fx[i].playing && fx[i].Pos.y >= w_lvl)
					fx[i].draw(cam, map, cache_anm);
			}
		}
		glDisable(GL_POLYGON_OFFSET_FILL);
		glPolygonOffset(0.0f,0.0f);
		gfx->unset_alpha_blending();
		glDepthMask(GL_TRUE);

		if (!UW && lp_CONFIG->explosion_particles && FXManager::currentParticleModel != NULL)
		{
			RenderQueue renderQueue(FXManager::currentParticleModel->id);
			for (ListOfParticles::iterator i = pParticles.begin(); i != pParticles.end(); ++i)
				(*i)->draw( renderQueue );
			renderQueue.draw_queue();
		}

		glDisable(GL_TEXTURE_2D);
		if (!UW)
			for (ListOfElectrics::iterator i = pElectrics.begin(); i != pElectrics.end(); ++i)
				(*i)->draw();

		pMutex.unlock();
	}

	void FXManager::addParticle(const Vector3D& p, const Vector3D& s, const float l)
	{
		if (lp_CONFIG->explosion_particles)
		{
			pMutex.lock();
			pParticles.push_back(new FXParticle(p, s, l));
			pMutex.unlock();
		}
	}

	void FXManager::addExplosion(const Vector3D& p, const int n, const float power)
	{
		if (!lp_CONFIG->explosion_particles)
			return;

		if (the_map)            // Visibility test
		{
			int px=((int)(p.x+0.5f) + the_map->map_w_d)>>4;
			int py=((int)(p.z+0.5f) + the_map->map_h_d)>>4;
			if (px<0 || py<0 || px >= the_map->bloc_w || py >= the_map->bloc_h)	return;
			byte player_mask = 1 << players.local_human_id;
			if (the_map->view[py][px]!=1
				|| !(SurfaceByte(the_map->sight_map, px, py ) & player_mask))	return;
		}

		pMutex.lock();
		float rev = 5.0f / (the_map->ota_data.gravity + 0.1f);
		for (int i = 0 ; i < n ; ++i)
		{
			float a = (Math::RandomTable() % 36000) * 0.01f * DEG2RAD;
			float b = (Math::RandomTable() % 18000) * 0.01f * DEG2RAD;
			float s = power * ((Math::RandomTable() % 9001) * 0.0001f + 0.1f);
			float scosb = s * cosf(b);
			Vector3D vs(cosf(a) * scosb,
						s * sinf(b),
						sinf(a) * scosb);
			float l = (Math::RandomTable() % 1001) * 0.001f - 0.5f + Math::Min(rev * vs.y, 10.0f);

			pParticles.push_back(new FXParticle(p, vs, l));
		}
		pMutex.unlock();
	}


	void FXManager::addExplosion(const Vector3D& p, const Vector3D& s, const int n, const float power)
	{
		if (!lp_CONFIG->explosion_particles)
			return;

		if (the_map)            // Visibility test
		{
			int px = ((int)(p.x + 0.5f) + the_map->map_w_d) >> 4;
			int py = ((int)(p.z + 0.5f) + the_map->map_h_d) >> 4;
			if (px<0 || py<0 || px >= the_map->bloc_w || py >= the_map->bloc_h)
				return;
			byte player_mask = 1 << players.local_human_id;
			if (the_map->view[py][px] != 1 || !(SurfaceByte(the_map->sight_map, px, py) & player_mask))
				return;
		}

		// Temporary variables
		float a;
		float b;
		float speed;
		float l;
		Vector3D vs;

		// Locking...
		pMutex.lock();

		// Foreach particle
		for (int i = 0; i < n; ++i)
		{
			a     = (Math::RandomTable() % 36000) * 0.01f * DEG2RAD;
			b     = (Math::RandomTable() % 18000) * 0.01f * DEG2RAD;
			speed = power * ((Math::RandomTable() % 9001) * 0.0001f + 0.1f);
			vs.x  = speed * cosf(a) * cosf(b);
			vs.y  = speed * sinf(b);
			vs.z  = speed * sinf(a) * cosf(b);
			l     = Math::Min(5.0f * vs.y / (the_map->ota_data.gravity + 0.1f), 10.0f);

			// Adding the new particle into the list
			pParticles.push_back(new FXParticle(p, s + vs, l));
		}
		// Unlokcing
		pMutex.unlock();
	}


	void FXManager::addElectric(const Vector3D& p)
	{
		pMutex.lock();
		pElectrics.push_back(new FXElectric(p));
		pMutex.unlock();
	}

	int FXManager::putInCache(const String& filename, Gaf::Animation* anm)
	{
		// Already available in the cache ?
		const int is_in = findInCache(filename);
		if (is_in >= 0)
			return is_in;

		int idx = -1;
		if (cache_size + 1 > max_cache_size)
		{
			max_cache_size += 100;
			cache_name.resize(max_cache_size);
			Gaf::Animation** n_anm = new Gaf::Animation*[max_cache_size];
			int *n_use = new int[max_cache_size];
			for (int i = max_cache_size - 100; i < max_cache_size; ++i)
			{
				n_use[i] = 0;
				n_anm[i] = NULL;
			}
			if (cache_size > 0)
			{
				for(int i = 0; i < max_cache_size - 100; ++i)
				{
					n_anm[i] = cache_anm[i];
					n_use[i] = use[i];
				}
				DELETE_ARRAY(cache_anm);
				DELETE_ARRAY(use);
			}
			use=n_use;
			cache_anm=n_anm;
			idx=cache_size;
		}
		else
		{
			idx = 0;
			for(int i = 0; i < max_cache_size; ++i)
			{
				if(cache_anm[i]==NULL)
					idx=i;
			}
		}
		use[idx] = 1;
		cache_anm[idx] = anm;
		cache_name[idx] = filename;
		++cache_size;

		return idx;
	}


	int FXManager::findInCache(const String& filename) const
	{
		if (cache_size <= 0)
			return -1;
		for(int i = 0; i < max_cache_size; ++i)
		{
			if (cache_anm[i] != NULL && !cache_name[i].empty())
			{
				if (filename == cache_name[i])
					return i;
			}
		}
		return -1;
	}


	void FXManager::doMoveAllParticles(const float& dt)
	{
		for (ListOfParticles::iterator i = pParticles.begin(); i != pParticles.end(); )
		{
			if ((*i)->move(dt))
			{
				delete *i;
				pParticles.erase(i++);
			}
			else
				++i;
		}
		for (ListOfElectrics::iterator i = pElectrics.begin(); i != pElectrics.end(); )
		{
			if ((*i)->move(dt))
			{
				delete *i;
				pElectrics.erase(i++);
			}
			else
				++i;
		}
	}


	void FXManager::doMoveAllFX(const float& dt)
	{
		for (int i = 0; i < max_fx; ++i)
		{
			if(fx[i].move(dt, cache_anm))
			{
				if (fx[i].anm == -1 || fx[i].anm == -2 || fx[i].anm == -3 || fx[i].anm == -4 || fx[i].anm == -5)
				{
					--nb_fx;
					continue;		// Flash, ripple or Wave
				}
				use[fx[i].anm]--;
				--nb_fx;
				if (use[fx[i].anm] <= 0) // Animation used nowhere
				{
					cache_name[fx[i].anm].clear();
					if (cache_anm[fx[i].anm])
						delete cache_anm[fx[i].anm];
					cache_anm[fx[i].anm] = NULL;
					--cache_size;
				}
			}
		}

	}


	void FXManager::move(const float dt)
	{
		pMutex.lock();
		doMoveAllFX(dt);
		doMoveAllParticles(dt);
		pMutex.unlock();
	}


	FXManager::FXManager()
	{
		init();
	}

	FXManager::~FXManager()
	{
		destroy();
	}


} // namespace TA3D
