
#include "unit.h"
#include "../UnitEngine.h"
#include "../ingame/players.h"
#include "../gfx/fx.manager.h"
#include "../sounds/manager.h"
#include "../input/mouse.h"



#define SQUARE(X)  ((X)*(X))



namespace TA3D
{



	Unit::Unit()
		:script(NULL), model(NULL), owner_id(0), type_id(0), hp(0.), Pos(),
		drawn_Pos(), V(), Angle(), drawn_Angle(), V_Angle(), sel(false),
		data(), drawing(false), port(NULL), mission(NULL), def_mission(NULL),
		flags(0), kills(0), c_time(0), compute_coord(false), idx(0), ID(0),
		h(0.), visible(false), on_radar(false), on_mini_radar(false), groupe(0),
		built(0), attacked(false), planned_weapons(0.), memory(NULL), mem_size(0),
		attached(false), attached_list(NULL), link_list(NULL), nb_attached(0), just_created(false),
		first_move(false), severity(0), cur_px(0), cur_py(0),
		metal_prod(0.), metal_cons(0.), energy_prod(0.), energy_cons(0.),
		last_time_sound(0),
		cur_metal_prod(0.), cur_metal_cons(0.), cur_energy_prod(0.), cur_energy_cons(0.),
		ripple_timer(0), weapon(),
		death_delay(0.), was_moving(false), last_path_refresh(0.), shadow_scale_dir(0.),
		hidden(false), flying(false), cloaked(false), cloaking(false), paralyzed(0.),
		//
		drawn_open(false), drawn_flying(false), drawn_x(0), drawn_y(0), drawn(false),
		//
		sight(0), radar_range(0), sonar_range(0), radar_jam_range(0), sonar_jam_range(0),
		old_px(0), old_py(0),
		move_target_computed(), was_locked(0.), self_destruct(0.), build_percent_left(0.),
		metal_extracted(0.), requesting_pathfinder(false), pad1(0), pad2(0), pad_timer(0.),
		command_locked(false), yardmap_timer(0), death_timer(0),
		//
		sync_hash(0), last_synctick(NULL), local(false), exploding(false), previous_sync(),
		nanolathe_target(0), nanolathe_reverse(false), nanolathe_feature(false),
		//
		visibility_checked(false)
	{
	}


	Unit::~Unit()
	{
		if (port)
			delete[] port;
		if (memory)
			delete[] memory;
		if (attached_list)
			delete[] attached_list;
		if (link_list)
			delete[] link_list;
		if (last_synctick)
			delete[] last_synctick;
	}



	void Unit::destroy(bool full)
	{
		while (drawing)
			rest(0);
		pMutex.lock();
		ID = 0;
		if (script)
		{
			delete script;
			script = NULL;
		}
		while (mission)
			clear_mission();
		clear_def_mission();
		init();
		flags=0;
		groupe=0;
		pMutex.unlock();
		if (full)
		{
			delete[] port;			// Ports
			port = NULL;
			delete[] memory;	// Pour se rappeler sur quelles armes on a déjà tiré
			memory = NULL;
			delete[] attached_list;
			attached_list = NULL;
			delete[] link_list;
			link_list = NULL;
			delete[] last_synctick;
			last_synctick = NULL;
		}
	}


	void Unit::start_building(const Vector3D &dir)
	{
		// Work in model coordinates
		Vector3D Dir(dir * RotateXZY(-Angle.x * DEG2RAD, -Angle.z * DEG2RAD, -Angle.y * DEG2RAD));
		Vector3D P(Dir);
		P.y = 0.0f;
		float angle = acosf(P.z / P.norm()) * RAD2DEG;
		if (P.x < 0.0f)
			angle = -angle;
		if (angle > 180)
			angle -= 360;
		else
		{
			if (angle < -180)
				angle += 360;
		}

		float angleX = asinf(Dir.y / Dir.norm()) * RAD2DEG;
		if (angleX > 180)	angleX -= 360;
		else if (angleX < -180)	angleX += 360;
		int param[] = { (int)(angle * DEG2TA), (int)(angleX * DEG2TA) };
		launchScript(SCRIPT_startbuilding, 2, param );
		playSound( "build" );
	}


	void Unit::start_mission_script(int mission_type)
	{
		if (script)
		{
			switch (mission_type)
			{
				case MISSION_ATTACK:
					break;
				case MISSION_PATROL:
				case MISSION_MOVE:
					break;
				case MISSION_BUILD_2:
					break;
				case MISSION_RECLAIM:
					break;
			}
			if (mission_type != MISSION_STOP)
				flags &= 191;
		}
	}


	void Unit::clear_mission()
	{
		if (NULL == mission)
			return;

		// Don't forget to detach the planes from air repair pads!
		if (mission->mission == MISSION_GET_REPAIRED && mission->p)
		{
			Unit *target_unit = (Unit*)(mission->p);
			target_unit->lock();
			if (target_unit->flags & 1)
			{
				int piece_id = mission->data >= 0 ? mission->data : (-mission->data - 1);
				if (target_unit->pad1 == piece_id) // tell others we've left
					target_unit->pad1 = 0xFFFF;
				else
					target_unit->pad2 = 0xFFFF;
			}
			target_unit->unlock();
		}
		pMutex.lock();
		Mission* old = mission;
		mission = mission->next;
		if (old->path)				// Détruit le chemin si nécessaire
			destroy_path(old->path);
		delete old;
		pMutex.unlock();
	}



	void Unit::compute_model_coord()
	{
		if (!compute_coord || !model)
			return;
		pMutex.lock();
		compute_coord = false;
		const float scale = unit_manager.unit_type[type_id]->Scale;

		// Matrice pour le calcul des positions des éléments du modèle de l'unité
		//    M = RotateZ(Angle.z*DEG2RAD) * RotateY(Angle.y * DEG2RAD) * RotateX(Angle.x * DEG2RAD) * Scale(scale);
		Matrix M = RotateZYX( Angle.z * DEG2RAD, Angle.y * DEG2RAD, Angle.x * DEG2RAD) * Scale(scale);
		model->compute_coord(&data, &M);
		pMutex.unlock();
	}


	void Unit::init_alloc_data()
	{
		port = new sint16[21];				// Ports
		memory = new int[TA3D_PLAYERS_HARD_LIMIT];				// Pour se rappeler sur quelles armes on a déjà tiré
		attached_list = new short[20];
		link_list = new short[20];
		last_synctick = new uint32[TA3D_PLAYERS_HARD_LIMIT];
	}


	void Unit::toggle_self_destruct()
	{
		if (self_destruct < 0.0f)
			self_destruct = unit_manager.unit_type[type_id]->selfdestructcountdown;
		else
			self_destruct = -1.0f;
	}

	bool Unit::isEnemy(const int t)
	{
		return t >= 0 && t < units.max_unit && !(players.team[units.unit[t].owner_id] & players.team[owner_id]);
	}

	int Unit::runScriptFunction(const int id, int nb_param, int *param)	// Launch and run the script, returning it's values to param if not NULL
	{
		String f_name( UnitScriptInterface::get_script_name(id) );
		MutexLocker mLocker( pMutex );
		if (script)
			return script->execute(f_name, param, nb_param);
		return -1;
	}

	void Unit::resetScript()
	{
		pMutex.lock();
		pMutex.unlock();
	}


	void Unit::stopMoving()
	{
		if (mission->flags & MISSION_FLAG_MOVE)
		{
			mission->flags &= ~MISSION_FLAG_MOVE;
			if (mission->path)
			{
				destroy_path(mission->path);
				mission->path = NULL;
				V.x = V.y = V.z = 0.0f;
			}
			if (!(unit_manager.unit_type[type_id]->canfly && nb_attached > 0)) // Once charged with units the Atlas cannot land
				launchScript(SCRIPT_StopMoving);
			else
				was_moving = false;
			if (!(mission->flags & MISSION_FLAG_DONT_STOP_MOVE))
				V.x = V.y = V.z = 0.0f;		// Stop unit's movement
		}
	}


	void Unit::lock_command()
	{
		pMutex.lock();
		command_locked = true;
		pMutex.unlock();
	}

	void Unit::unlock_command()
	{
		pMutex.lock();
		command_locked = false;
		pMutex.unlock();
	}


	void Unit::activate()
	{
		pMutex.lock();
		if (port[ACTIVATION] == 0)
		{
			playSound("activate");
			launchScript(SCRIPT_Activate);
			port[ACTIVATION] = 1;
		}
		pMutex.unlock();
	}

	void Unit::deactivate()
	{
		pMutex.lock();
		if (port[ACTIVATION] != 0)
		{
			playSound("deactivate");
			launchScript(SCRIPT_Deactivate);
			port[ACTIVATION] = 0;
		}
		pMutex.unlock();
	}



	void Unit::init(int unit_type, int owner, bool full, bool basic)
	{
		pMutex.lock();

		kills = 0;

		visibility_checked = false;

		ID = 0;
		paralyzed = 0.0f;

		yardmap_timer = 1;
		death_timer = 0;

		drawing = false;

		local = true;		// Is local by default, set to remote by create_unit when needed

		nanolathe_target = -1;		// Used for remote units only
		nanolathe_reverse = false;
		nanolathe_feature = false;

		exploding = false;

		command_locked = false;

		pad1 = 0xFFFF; pad2 = 0xFFFF;
		pad_timer = 0.0f;

		requesting_pathfinder = false;

		was_locked = 0.0f;

		metal_extracted = 0.0f;

		on_mini_radar = false;
		move_target_computed.x = move_target_computed.y = move_target_computed.z = 0.0f;

		self_destruct = -1;		// Don't auto destruct!!

		drawn_open = drawn_flying = false;
		drawn_x = drawn_y = 0;
		drawn = true;

		old_px = old_py = -10000;

		flying = false;

		cloaked = false;
		cloaking = false;

		hidden = false;
		shadow_scale_dir = -1.0f;
		last_path_refresh = 0.0f;
		metal_prod = metal_cons = energy_prod = energy_cons = cur_metal_prod = cur_metal_cons = cur_energy_prod = cur_energy_cons = 0.0f;
		last_time_sound = msec_timer;
		ripple_timer = msec_timer;
		was_moving = false;
		cur_px=0;
		cur_py=0;
		sight = 0;
		radar_range = 0;
		sonar_range = 0;
		radar_jam_range = 0;
		sonar_jam_range = 0;
		severity=0;
		if (full)
			init_alloc_data();
		just_created=true;
		first_move=true;
		attached=false;
		nb_attached=0;
		mem_size=0;
		planned_weapons=0.0f;
		attacked=false;
		groupe=0;
		weapon.clear();
		h=0.0f;
		compute_coord=true;
		c_time=0.0f;
		flags=1;
		sel=false;
		script=NULL;
		model=NULL;
		owner_id=owner;
		type_id=-1;
		hp=0.0f;
		V.x=V.y=V.z=0.0f;
		Pos=V;
		data.init();
		Angle.x=Angle.y=Angle.z=0.0f;
		V_Angle=Angle;
		int i;
		for(i=0;i<21;i++)
			port[i]=0;
		if (unit_type<0 || unit_type>=unit_manager.nb_unit)
			unit_type=-1;
		port[ACTIVATION]=0;
		mission=NULL;
		def_mission=NULL;
		port[BUILD_PERCENT_LEFT]=0;
		build_percent_left=0.0f;
		memset( last_synctick, 0, 40 );
		if (unit_type != -1)
		{
			if (!basic)
			{
				pMutex.unlock();
				set_mission(MISSION_STANDBY);
				pMutex.lock();
			}
			type_id = unit_type;
			model = unit_manager.unit_type[type_id]->model;
			weapon.resize(unit_manager.unit_type[type_id]->weapon.size());
			hp = unit_manager.unit_type[type_id]->MaxDamage;
			script = UnitScriptInterface::instanciate( unit_manager.unit_type[type_id]->script );
			port[STANDINGMOVEORDERS] = unit_manager.unit_type[type_id]->StandingMoveOrder;
			port[STANDINGFIREORDERS] = unit_manager.unit_type[type_id]->StandingFireOrder;
			if (!basic)
			{
				pMutex.unlock();
				set_mission(unit_manager.unit_type[type_id]->DefaultMissionType);
				pMutex.lock();
			}
			if (script)
			{
				script->setUnitID( idx );
				data.load( script->getNbPieces() );
				launchScript(SCRIPT_create);
			}
		}
		pMutex.unlock();
	}


	void Unit::clear_def_mission()
	{
		pMutex.lock();
		while (def_mission)
		{
			Mission* old = def_mission;
			def_mission = def_mission->next;
			if (old->path)				// Détruit le chemin si nécessaire
				destroy_path(old->path);
			delete old;
		}
		pMutex.unlock();
	}




	bool Unit::is_on_radar(byte p_mask)
	{
		int px = cur_px>>1;
		int py = cur_py>>1;
		if (px >= 0 && py >= 0 && px < units.map->radar_map->w && py < units.map->radar_map->h && type_id != -1)
			return ( (SurfaceByte(units.map->radar_map,px,py) & p_mask) && !unit_manager.unit_type[type_id]->Stealth && (unit_manager.unit_type[type_id]->fastCategory & CATEGORY_NOTSUB) )
				|| ( (SurfaceByte(units.map->sonar_map,px,py) & p_mask) && !(unit_manager.unit_type[type_id]->fastCategory & CATEGORY_NOTSUB) );
		return false;
	}

	void Unit::add_mission(int mission_type, const Vector3D* target, bool step, int dat, void* pointer,
						   PATH_NODE* path, byte m_flags, int move_data, int patrol_node)
	{
		MutexLocker locker(pMutex);

		if (command_locked && !(mission_type & MISSION_FLAG_AUTO) )
			return;

		mission_type &= ~MISSION_FLAG_AUTO;

		uint32 target_ID = 0;

		if (pointer != NULL)
		{
			switch(mission_type)
			{
				case MISSION_GET_REPAIRED:
				case MISSION_CAPTURE:
				case MISSION_LOAD:
				case MISSION_BUILD_2:
				case MISSION_RECLAIM:
				case MISSION_REPAIR:
				case MISSION_GUARD:
					target_ID = ((Unit*)pointer)->ID;
					break;
				case MISSION_ATTACK:
					if (!(m_flags&MISSION_FLAG_TARGET_WEAPON) )
						target_ID = ((Unit*)pointer)->ID;
					break;
			}
		}

		bool def_mode = false;
		if (type_id != -1 && !unit_manager.unit_type[type_id]->BMcode)
		{
			switch (mission_type)
			{
				case MISSION_MOVE:
				case MISSION_PATROL:
				case MISSION_GUARD:
					def_mode = true;
					break;
			}
		}

		if (pointer == this && !def_mode) // A unit cannot target itself
			return;

		if (mission_type == MISSION_MOVE || mission_type == MISSION_PATROL )
			m_flags |= MISSION_FLAG_MOVE;

		if (type_id != -1 && mission_type == MISSION_BUILD && unit_manager.unit_type[type_id]->BMcode && unit_manager.unit_type[type_id]->Builder && target != NULL)
		{
			bool removed = false;
			Mission *cur_mission = mission;
			Mission *prec = cur_mission;
			if (cur_mission)
				cur_mission = cur_mission->next;		// Don't read the first one ( which is being executed )

			while (cur_mission) 	// Reads the mission list
			{
				float x_space = fabsf(cur_mission->target.x - target->x);
				float z_space = fabsf(cur_mission->target.z - target->z);
				if (!cur_mission->step && cur_mission->mission == MISSION_BUILD && cur_mission->data >= 0 && cur_mission->data < unit_manager.nb_unit
					&& x_space < ((unit_manager.unit_type[ dat ]->FootprintX + unit_manager.unit_type[ cur_mission->data ]->FootprintX) << 2)
					&& z_space < ((unit_manager.unit_type[ dat ]->FootprintZ + unit_manager.unit_type[ cur_mission->data ]->FootprintZ) << 2) ) // Remove it
				{
					Mission *tmp = cur_mission;
					cur_mission = cur_mission->next;
					prec->next = cur_mission;
					if (tmp->path)				// Destroy the path if needed
						destroy_path(tmp->path);
					delete tmp;
					removed = true;
				}
				else
				{
					prec = cur_mission;
					cur_mission = cur_mission->next;
				}
			}
			if (removed)
				return;
		}

		Mission *new_mission = new Mission();
		new_mission->next = NULL;
		new_mission->mission = mission_type;
		new_mission->target_ID = target_ID;
		new_mission->step = step;
		new_mission->time = 0.0f;
		new_mission->data = dat;
		new_mission->p = pointer;
		new_mission->path = path;
		new_mission->last_d = 9999999.0f;
		new_mission->flags = m_flags;
		new_mission->move_data = move_data;
		new_mission->node = patrol_node;

		bool inserted = false;

		if (patrol_node == -1 && mission_type == MISSION_PATROL)
		{
			Mission *mission_base = def_mode ? def_mission : mission;
			if (mission_base) // Ajoute l'ordre aux autres
			{
				Mission *cur = mission_base;
				Mission *last = NULL;
				patrol_node = 0;
				while (cur != NULL)
				{
					if (cur->mission == MISSION_PATROL && patrol_node <= cur->node )
					{
						patrol_node = cur->node;
						last = cur;
					}
					cur=cur->next;
				}
				new_mission->node = patrol_node + 1;

				if (last)
				{
					new_mission->next = last->next;
					last->next = new_mission;
					inserted = true;
				}
			}
			else
				new_mission->node = 1;
		}

		if (target)
			new_mission->target = *target;

		Mission* stop = !(inserted) ? new Mission() : NULL;
		if (stop)
		{
			stop->mission = MISSION_STOP;
			stop->step = true;
			stop->time = 0.0f;
			stop->p = NULL;
			stop->data = 0;
			stop->path = NULL;
			stop->last_d = 9999999.0f;
			stop->flags = m_flags & ~MISSION_FLAG_MOVE;
			stop->move_data = move_data;
			if (step)
			{
				stop->next=NULL;
				new_mission->next=stop;
			}
			else
			{
				stop->next=new_mission;
				new_mission=stop;
			}
		}

		if (step && mission && stop)
		{
			stop->next = def_mode ? def_mission : mission;
			mission = new_mission;
			if (!def_mode )
				start_mission_script(mission->mission);
		}
		else
		{
			Mission* mission_base = def_mode ? def_mission : mission;
			if (mission_base && !inserted ) // Ajoute l'ordre aux autres
			{
				Mission* cur = mission_base;
				while (cur->next!=NULL)
					cur=cur->next;
				if (((( cur->mission == MISSION_MOVE || cur->mission == MISSION_PATROL || cur->mission == MISSION_STANDBY
						|| cur->mission == MISSION_VTOL_STANDBY || cur->mission == MISSION_STOP )		// Don't stop if it's not necessary
					  && ( mission_type == MISSION_MOVE || mission_type == MISSION_PATROL ) )
					 || ( ( cur->mission == MISSION_BUILD || cur->mission == MISSION_BUILD_2 )
						  && mission_type == MISSION_BUILD && type_id != -1 && !unit_manager.unit_type[type_id]->BMcode) )
					&& new_mission->next != NULL ) 	// Prevent factories from closing when already building a unit
				{
					stop = new_mission->next;
					delete new_mission;
					new_mission = stop;
					new_mission->next = NULL;
				}
				cur->next=new_mission;
			}
			else
			{
				if (mission_base == NULL)
				{
					if (!def_mode)
					{
						mission = new_mission;
						start_mission_script(mission->mission);
					}
					else
						def_mission = new_mission;
				}
			}
		}
	}



	void Unit::set_mission(int mission_type, const Vector3D* target, bool step, int dat, bool stopit,
						   void* pointer, PATH_NODE* path, byte m_flags, int move_data)
	{
		MutexLocker locker(pMutex);

		if (command_locked && !( mission_type & MISSION_FLAG_AUTO))
			return;
		mission_type &= ~MISSION_FLAG_AUTO;

		uint32 target_ID = 0;

		if (pointer != NULL)
		{
			switch( mission_type)
			{
				case MISSION_GET_REPAIRED:
				case MISSION_CAPTURE:
				case MISSION_LOAD:
				case MISSION_BUILD_2:
				case MISSION_RECLAIM:
				case MISSION_REPAIR:
				case MISSION_GUARD:
					target_ID = ((Unit*)pointer)->ID;
					break;
				case MISSION_ATTACK:
					if (!(m_flags&MISSION_FLAG_TARGET_WEAPON) )
						target_ID = ((Unit*)pointer)->ID;
					break;
			}
		}

		if (nanolathe_target >= 0 && network_manager.isConnected())
		{
			nanolathe_target = -1;
			g_ta3d_network->sendUnitNanolatheEvent( idx, -1, false, false );
		}

		bool def_mode = false;
		if (type_id != -1 && !unit_manager.unit_type[type_id]->BMcode)
		{
			switch (mission_type)
			{
				case MISSION_MOVE:
				case MISSION_PATROL:
				case MISSION_GUARD:
					def_mode = true;
					break;
			}
		}

		if (pointer == this && !def_mode) // A unit cannot target itself
			return;

		int old_mission=-1;
		if (!def_mode)
		{
			if (mission != NULL)
				old_mission = mission->mission;
		}
		else
			clear_def_mission();

		bool already_running = false;

		if (mission_type == MISSION_MOVE || mission_type == MISSION_PATROL )
			m_flags |= MISSION_FLAG_MOVE;

		switch(old_mission)		// Commandes de fin de mission
		{
			case MISSION_REPAIR:
			case MISSION_RECLAIM:
			case MISSION_BUILD_2: {
									  launchScript(SCRIPT_stopbuilding);
									  deactivate();
									  if (type_id != -1 && !unit_manager.unit_type[type_id]->BMcode) // Delete the unit we were building
									  {
										  sint32 prev = -1;
										  for(int i = units.nb_unit-1; i>=0 ; i--)
										  {
											  if (units.idx_list[i] == ((Unit*)(mission->p))->idx)
											  {
												  prev = i;
												  break;
											  }
										  }
										  if (prev >= 0 )
											  units.kill(((Unit*)(mission->p))->idx,units.map,prev);
									  }
									  else
										  launchScript(SCRIPT_stop);
									  break;
								  }
			case MISSION_ATTACK: {
									 if (mission_type != MISSION_ATTACK && type_id != -1 &&
										 (!unit_manager.unit_type[type_id]->canfly
										  || (unit_manager.unit_type[type_id]->canfly && mission_type != MISSION_MOVE && mission_type != MISSION_PATROL ) ) )
										 deactivate();
									 else
									 {
										 stopit = false;
										 already_running = true;
									 }
									 break;
								 }
			case MISSION_MOVE:
			case MISSION_PATROL: {
									 if (mission_type == MISSION_MOVE || mission_type == MISSION_PATROL
										 || (type_id != -1 && unit_manager.unit_type[type_id]->canfly && mission_type == MISSION_ATTACK ) )
									 {
										 stopit = false;
										 already_running = true;
									 }
									 break;
								 }
		}

		if (!def_mode)
		{
			while (mission != NULL)
				clear_mission();			// Efface les ordres précédents
			last_path_refresh = 10.0f;
		}

		if (def_mode)
		{
			def_mission = new Mission();
			def_mission->next = NULL;
			def_mission->mission = mission_type;
			def_mission->target_ID = target_ID;
			def_mission->step = step;
			def_mission->time = 0.0f;
			def_mission->data = dat;
			def_mission->p = pointer;
			def_mission->path = path;
			def_mission->last_d = 9999999.0f;
			def_mission->flags = m_flags;
			def_mission->move_data = move_data;
			def_mission->node = 1;
			if (target)
				def_mission->target=*target;

			if (stopit)
			{
				Mission *stop = new Mission();
				stop->next = def_mission;
				stop->mission = MISSION_STOP;
				stop->step = true;
				stop->time = 0.0f;
				stop->p = NULL;
				stop->data = 0;
				stop->path = NULL;
				stop->last_d = 9999999.0f;
				stop->flags = m_flags & ~MISSION_FLAG_MOVE;
				stop->move_data = move_data;
				def_mission = stop;
			}
		}
		else
		{
			mission = new Mission();
			mission->next = NULL;
			mission->mission = mission_type;
			mission->target_ID = target_ID;
			mission->step = step;
			mission->time = 0.0f;
			mission->data = dat;
			mission->p = pointer;
			mission->path = path;
			mission->last_d = 9999999.0f;
			mission->flags = m_flags;
			mission->move_data = move_data;
			mission->node = 1;
			if (target)
				mission->target=*target;

			if (stopit)
			{
				Mission *stop = new Mission();
				stop->next = mission;
				stop->mission = MISSION_STOP;
				stop->step = true;
				stop->time = 0.0f;
				stop->p = NULL;
				stop->data = 0;
				stop->path = NULL;
				stop->last_d = 9999999.0f;
				stop->flags = m_flags & ~MISSION_FLAG_MOVE;
				stop->move_data = move_data;
				mission = stop;
			}
			else
			{
				if (!already_running)
					start_mission_script(mission->mission);
			}
			c_time=0.0f;
		}
	}



	void Unit::next_mission()
	{
		last_path_refresh = 10.0f;		// By default allow to compute a new path
		if (nanolathe_target >= 0 && network_manager.isConnected())
		{
			nanolathe_target = -1;
			g_ta3d_network->sendUnitNanolatheEvent( idx, -1, false, false );
		}

		if (mission == NULL)
		{
			command_locked = false;
			if (type_id != -1)
				set_mission( unit_manager.unit_type[type_id]->DefaultMissionType, NULL, false, 0, false);
			return;
		}
		switch (mission->mission) // Commandes de fin de mission
		{
			case MISSION_REPAIR:
			case MISSION_RECLAIM:
			case MISSION_BUILD_2:
				if (mission->next == NULL || (type_id != -1 && unit_manager.unit_type[type_id]->BMcode) || mission->next->mission != MISSION_BUILD)
				{
					launchScript(SCRIPT_stopbuilding);
					deactivate();
				}
				break;
			case MISSION_ATTACK:
				deactivate();
				break;
		}
		if (mission->mission==MISSION_STOP && mission->next==NULL)
		{
			command_locked = false;
			mission->data=0;
			return;
		}
		bool old_step = mission->step;
		Mission *old=mission;
		mission=mission->next;
		if (old->path)				// Détruit le chemin si nécessaire
			destroy_path(old->path);
		delete old;
		if (mission==NULL)
		{
			command_locked = false;
			if (type_id != -1)
				set_mission(unit_manager.unit_type[type_id]->DefaultMissionType);
		}

		// Skip a stop order before a normal order if the unit can fly (prevent planes from looking for a place to land when they don't need to land !!)
		if (type_id != -1 && unit_manager.unit_type[type_id]->canfly && mission->mission == MISSION_STOP && mission->next != NULL && mission->next->mission != MISSION_STOP)
		{
			old = mission;
			mission = mission->next;
			if (old->path)				// Détruit le chemin si nécessaire
				destroy_path(old->path);
			delete old;
		}

		if (old_step && mission && mission->next
			&& (mission->mission == MISSION_STOP || mission->mission == MISSION_VTOL_STANDBY || mission->mission == MISSION_STANDBY))
			next_mission();

		start_mission_script(mission->mission);
		c_time=0.0f;
	}


	void Unit::draw(float t, MAP* map, bool height_line)
	{
		visibility_checked = false;

		MutexLocker locker(pMutex);

		if (!(flags & 1) || type_id == -1)
			return;

		visible = false;
		on_radar = false;
		on_mini_radar = false;

		drawn_Pos = Pos;
		drawn_Angle = Angle;

		if (!model || hidden)
			return;		// S'il n'y a pas de modèle associé, on quitte la fonction

		int px = cur_px >> 1;
		int py = cur_py >> 1;
		if (px < 0 || py < 0 || px >= map->bloc_w || py >= map->bloc_h)
			return;	// Unité hors de la carte
		byte player_mask = 1 << players.local_human_id;

		on_radar = on_mini_radar = is_on_radar( player_mask );
		if (map->view[py][px] == 0 || ( map->view[py][px] > 1 && !on_radar ) || ( !on_radar && !(SurfaceByte(map->sight_map,px,py) & player_mask) ) )
			return;	// Unit is not visible

		bool radar_detected = on_radar;

		on_radar &= map->view[py][px] > 1;

		Vector3D D (Pos - Camera::inGame->pos); // Vecteur "viseur unité" partant de la caméra vers l'unité

		float dist=D.sq();
		if (dist >= 16384.0f && (D % Camera::inGame->dir) <= 0.0f)
			return;
		if ((D % Camera::inGame->dir) > Camera::inGame->zfar2)
			return;		// Si l'objet est hors champ on ne le dessine pas

		if (!cloaked || owner_id == players.local_human_id) // Don't show cloaked units
		{
			visible = true;
			on_radar |= Camera::inGame->rpos.y > gfx->low_def_limit;
		}
		else
		{
			on_radar |= radar_detected;
			visible = on_radar;
		}

		Matrix M;
		glPushMatrix();
		if (on_radar) // for mega zoom, draw only an icon
		{
			glDisable(GL_DEPTH_TEST);
			glTranslatef( Pos.x, Math::Max(Pos.y,map->sealvl+5.0f), Pos.z);
			glEnable(GL_TEXTURE_2D);
			int unit_nature = ICON_UNKNOWN;
			float size = (D % Camera::inGame->dir) * 12.0f / gfx->height;

			if (unit_manager.unit_type[type_id]->fastCategory & CATEGORY_KAMIKAZE)
				unit_nature = ICON_KAMIKAZE;
			else if (( unit_manager.unit_type[type_id]->TEDclass & CLASS_COMMANDER ) == CLASS_COMMANDER)
				unit_nature = ICON_COMMANDER;
			else if (( unit_manager.unit_type[type_id]->TEDclass & CLASS_ENERGY ) == CLASS_ENERGY)
				unit_nature = ICON_ENERGY;
			else if (( unit_manager.unit_type[type_id]->TEDclass & CLASS_METAL ) == CLASS_METAL)
				unit_nature = ICON_METAL;
			else if (( unit_manager.unit_type[type_id]->TEDclass & CLASS_TANK ) == CLASS_TANK)
				unit_nature = ICON_TANK;
			else if (unit_manager.unit_type[type_id]->Builder)
			{
				if (!unit_manager.unit_type[type_id]->BMcode)
					unit_nature = ICON_FACTORY;
				else
					unit_nature = ICON_BUILDER;
			}
			else if (( unit_manager.unit_type[type_id]->TEDclass & CLASS_SHIP ) == CLASS_SHIP )
				unit_nature = ICON_WATERUNIT;
			else if (( unit_manager.unit_type[type_id]->TEDclass & CLASS_FORT ) == CLASS_FORT )
				unit_nature = ICON_DEFENSE;
			else if (( unit_manager.unit_type[type_id]->fastCategory & CATEGORY_NOTAIR ) && ( unit_manager.unit_type[type_id]->fastCategory & CATEGORY_NOTSUB ) )
				unit_nature = ICON_LANDUNIT;
			else if (!( unit_manager.unit_type[type_id]->fastCategory & CATEGORY_NOTAIR ) )
				unit_nature = ICON_AIRUNIT;
			else if (!( unit_manager.unit_type[type_id]->fastCategory & CATEGORY_NOTSUB ) )
				unit_nature = ICON_SUBUNIT;

			glBindTexture( GL_TEXTURE_2D, units.icons[ unit_nature ] );
			glDisable( GL_CULL_FACE );
			glDisable(GL_LIGHTING);
			glDisable(GL_BLEND);
			glTranslatef( model->center.x, model->center.y, model->center.z );
			if (player_color[player_color_map[owner_id]*3] != 0.0f || player_color[player_color_map[owner_id]*3+1] != 0.0f || player_color[player_color_map[owner_id]*3+2] != 0.0f)
			{
				glColor3f(player_color[player_color_map[owner_id]*3],player_color[player_color_map[owner_id]*3+1],player_color[player_color_map[owner_id]*3+2]);
				glBegin(GL_QUADS);
				glTexCoord2f(0.0f, 0.0f);		glVertex3f( -size, 0.0f, -size);
				glTexCoord2f(1.0f, 0.0f);		glVertex3f(  size, 0.0f, -size);
				glTexCoord2f(1.0f, 1.0f);		glVertex3f(  size, 0.0f,  size);
				glTexCoord2f(0.0f, 1.0f);		glVertex3f( -size, 0.0f,  size);
				glEnd();
			}
			else
			{								// If it's black, then invert colors
				glColor3ub(0xFF,0xFF,0xFF);
				glDisable(GL_TEXTURE_2D);
				glBegin(GL_QUADS);
				glTexCoord2f(0.0f, 0.0f);		glVertex3f( -size, 0.0f, -size);
				glTexCoord2f(1.0f, 0.0f);		glVertex3f(  size, 0.0f, -size);
				glTexCoord2f(1.0f, 1.0f);		glVertex3f(  size, 0.0f,  size);
				glTexCoord2f(0.0f, 1.0f);		glVertex3f( -size, 0.0f,  size);
				glEnd();
				glEnable(GL_TEXTURE_2D);
				glBlendFunc( GL_ZERO, GL_ONE_MINUS_SRC_COLOR);
				glEnable(GL_BLEND);
				glBegin(GL_QUADS);
				glTexCoord2f(0.0f, 0.0f);		glVertex3f( -size, 0.0f, -size);
				glTexCoord2f(1.0f, 0.0f);		glVertex3f(  size, 0.0f, -size);
				glTexCoord2f(1.0f, 1.0f);		glVertex3f(  size, 0.0f,  size);
				glTexCoord2f(0.0f, 1.0f);		glVertex3f( -size, 0.0f,  size);
				glEnd();
				glDisable(GL_BLEND);
			}
			glEnable( GL_CULL_FACE );
			if (owner_id == players.local_human_id && sel)
			{
				glDisable( GL_TEXTURE_2D );
				glColor3ub(0xFF,0xFF,0);
				glBegin(GL_LINE_LOOP);
				glVertex3f( -size, 0.0f, -size);
				glVertex3f(  size, 0.0f, -size);
				glVertex3f(  size, 0.0f,  size);
				glVertex3f( -size, 0.0f,  size);
				glEnd();
			}
			glEnable(GL_DEPTH_TEST);
		}
		else
			if (visible)
			{
				glTranslatef( Pos.x, Pos.y, Pos.z );

				if (lp_CONFIG->underwater_bright && map->water && Pos.y < map->sealvl)
				{
					double eqn[4]= { 0.0f, -1.0f, 0.0f, map->sealvl - Pos.y };
					glClipPlane(GL_CLIP_PLANE2, eqn);
				}

				glRotatef(Angle.x,1.0f,0.0f,0.0f);
				glRotatef(Angle.z,0.0f,0.0f,1.0f);
				glRotatef(Angle.y,0.0f,1.0f,0.0f);
				float scale = unit_manager.unit_type[type_id]->Scale;
				glScalef(scale,scale,scale);

				//            M=RotateY(Angle.y*DEG2RAD)*RotateZ(Angle.z*DEG2RAD)*RotateX(Angle.x*DEG2RAD)*Scale(scale);			// Matrice pour le calcul des positions des éléments du modèle de l'unité
				M = RotateYZX(Angle.y*DEG2RAD, Angle.z*DEG2RAD, Angle.x*DEG2RAD)*Scale(scale);			// Matrice pour le calcul des positions des éléments du modèle de l'unité

				Vector3D *target=NULL,*center=NULL;
				Vector3D upos;
				bool c_part=false;
				bool reverse=false;
				float size=0.0f;
				OBJECT *src = NULL;
				ANIMATION_DATA *src_data = NULL;
				Vector3D v_target;				// Needed in network mode
				Unit *unit_target = NULL;
				MODEL *the_model = model;
				drawing = true;

				if (!unit_manager.unit_type[type_id]->emitting_points_computed ) // Compute model emitting points if not already done, do it here in Unit::Locked code ...
				{
					unit_manager.unit_type[type_id]->emitting_points_computed = true;
					int first = runScriptFunction( SCRIPT_QueryNanoPiece );;
					int current;
					int i = 0;
					do
					{
						current = runScriptFunction( SCRIPT_QueryNanoPiece );
						model->obj.compute_emitter_point( current );
						++i;
					} while( first != current && i < 1000 );
				}

				if (build_percent_left == 0.0f && mission != NULL && port[ INBUILDSTANCE ] != 0 && local )
				{
					if (c_time >= 0.125f)
					{
						reverse = (mission->mission==MISSION_RECLAIM);
						c_time = 0.0f;
						c_part = true;
						upos.x = upos.y = upos.z = 0.0f;
						upos = upos + Pos;
						if (mission->p != NULL && (mission->mission == MISSION_REPAIR || mission->mission == MISSION_BUILD
												   || mission->mission == MISSION_BUILD_2 || mission->mission == MISSION_CAPTURE))
						{
							unit_target = ((Unit*)mission->p);
							pMutex.unlock();
							unit_target->lock();
							if ((unit_target->flags & 1) && unit_target->model!=NULL)
							{
								size=unit_target->model->size2;
								center=&unit_target->model->center;
								src = &unit_target->model->obj;
								src_data = &unit_target->data;
								unit_target->compute_model_coord();
							}
							else
							{
								unit_target->unlock();
								pMutex.lock();
								unit_target = NULL;
								c_part = false;
							}
						}
						else
						{
							if (mission->mission == MISSION_RECLAIM || mission->mission == MISSION_REVIVE ) // Reclaiming features
							{
								int feature_type = features.feature[ mission->data ].type;
								Feature *feature = feature_manager.getFeaturePointer(feature_type);
								if (mission->data >= 0 && feature && feature->model )
								{
									size = feature->model->size2;
									center = &feature->model->center;
									src = &feature->model->obj;
									src_data = NULL;
								}
								else
								{
									D.x = D.y = D.z = 0.f;
									center = &D;
									size = 32.0f;
								}
							}
							else
								c_part=false;
						}
						target = &(mission->target);
					}
				}
				else
				{
					if (!local && nanolathe_target >= 0 && port[ INBUILDSTANCE ] != 0 )
					{
						if (c_time>=0.125f)
						{
							reverse = nanolathe_reverse;
							c_time=0.0f;
							c_part=true;
							upos.x=upos.y=upos.z=0.0f;
							upos=upos+Pos;
							if (!nanolathe_feature)
							{
								unit_target = &(units.unit[ nanolathe_target ]);
								pMutex.unlock();
								unit_target->lock();
								if ((unit_target->flags & 1) && unit_target->model )
								{
									size = unit_target->model->size2;
									center = &unit_target->model->center;
									src = &unit_target->model->obj;
									src_data = &unit_target->data;
									unit_target->compute_model_coord();
									v_target = unit_target->Pos;
								}
								else
								{
									unit_target->unlock();
									pMutex.lock();
									unit_target = NULL;
									c_part = false;
								}
							}
							else // Reclaiming features
							{
								int feature_type = features.feature[ nanolathe_target ].type;
								v_target = features.feature[ nanolathe_target ].Pos;
								Feature *feature = feature_manager.getFeaturePointer(feature_type);
								if (feature && feature->model )
								{
									size = feature->model->size2;
									center = &feature->model->center;
									src = &feature->model->obj;
									src_data = NULL;
								}
								else
								{
									D.x = D.y = D.z = 0.f;
									center = &D;
									size = 32.0f;
								}
							}
							target = &v_target;
						}
					}
				}

				bool old_mode = gfx->getShadowMapMode();

				if (build_percent_left == 0.0f)
				{
					if (cloaked || ( cloaking && owner_id != players.local_human_id ) )
						glColor4ub( 0xFF, 0xFF, 0xFF, 0x7F );
					the_model->draw(t,&data,owner_id==players.local_human_id && sel,false,c_part,build_part,target,&upos,&M,size,center,reverse,owner_id,!cloaked,src,src_data);
					if (cloaked || ( cloaking && owner_id != players.local_human_id ) )
						gfx->set_color( 0xFFFFFFFF );
					if (height_line && h>1.0f && unit_manager.unit_type[type_id]->canfly) // For flying units, draw a line that shows how high is the unit
					{
						glPopMatrix();
						glPushMatrix();
						glDisable(GL_TEXTURE_2D);
						glDisable(GL_LIGHTING);
						glColor3ub(0xFF,0xFF,0);
						glBegin(GL_LINES);
						for (float y=Pos.y;y>Pos.y-h;y-=10.0f)
						{
							glVertex3f(Pos.x,y,Pos.z);
							glVertex3f(Pos.x,y-5.0f,Pos.z);
						}
						glEnd();
					}
				}
				else
				{
					gfx->setShadowMapMode(true);
					if (build_percent_left<=33.0f)
					{
						float h = model->top - model->bottom;
						double eqn[4]= { 0.0f, 1.0f, 0.0f, -model->bottom - h*(33.0f-build_percent_left)*0.033333f};

						glClipPlane(GL_CLIP_PLANE0, eqn);
						glEnable(GL_CLIP_PLANE0);
						the_model->draw(0.0f,&data,owner_id==players.local_human_id && sel,true,c_part,build_part,target,&upos,&M,size,center,reverse,owner_id,true,src,src_data);

						eqn[1]=-eqn[1];	eqn[3]=-eqn[3];
						glClipPlane(GL_CLIP_PLANE0, eqn);
						the_model->draw(0.0f,&data,owner_id==players.local_human_id && sel,false,false,build_part,target,&upos,&M,size,center,reverse,owner_id);
						glDisable(GL_CLIP_PLANE0);

						glPolygonMode (GL_FRONT_AND_BACK, GL_LINE);
						the_model->draw(0.0f,&data,owner_id==players.local_human_id && sel,true,false,build_part,target,&upos,&M,size,center,reverse,owner_id);
						glPolygonMode (GL_FRONT_AND_BACK, GL_FILL);
					}
					else
					{
						if (build_percent_left<=66.0f)
						{
							float h = model->top - model->bottom;
							double eqn[4]= { 0.0f, 1.0f, 0.0f, -model->bottom - h*(66.0f-build_percent_left)*0.033333f};

							glPolygonMode (GL_FRONT_AND_BACK, GL_FILL);
							glClipPlane(GL_CLIP_PLANE0, eqn);
							glEnable(GL_CLIP_PLANE0);
							glPolygonMode (GL_FRONT_AND_BACK, GL_LINE);
							the_model->draw(0.0f,&data,owner_id==players.local_human_id && sel,true,c_part,build_part,target,&upos,&M,size,center,reverse,owner_id,true,src,src_data);
							glPolygonMode (GL_FRONT_AND_BACK, GL_FILL);

							eqn[1]=-eqn[1];	eqn[3]=-eqn[3];
							glClipPlane(GL_CLIP_PLANE0, eqn);
							the_model->draw(0.0f,&data,owner_id==players.local_human_id && sel,true,false,build_part,target,&upos,&M,size,center,reverse,owner_id);
							glDisable(GL_CLIP_PLANE0);
						}
						else
						{
							glPolygonMode (GL_FRONT_AND_BACK, GL_LINE);
							the_model->draw(0.0f,&data,owner_id==players.local_human_id && sel,true,false,build_part,target,&upos,&M,size,center,reverse,owner_id);
							glPolygonMode (GL_FRONT_AND_BACK, GL_FILL);
						}
					}
				}

				if (lp_CONFIG->underwater_bright && map->water && Pos.y < map->sealvl)
				{
					gfx->setShadowMapMode(true);
					glEnable(GL_CLIP_PLANE2);

					glEnable( GL_BLEND );
					glBlendFunc( GL_ONE, GL_ONE );
					glDepthFunc( GL_EQUAL );
					glColor4ub( 0x7F, 0x7F, 0x7F, 0x7F );
					the_model->draw(t,&data,false,true,false,0,NULL,NULL,NULL,0.0f,NULL,false,owner_id,false);
					glColor4ub( 0xFF, 0xFF, 0xFF, 0xFF );
					glDepthFunc( GL_LESS );
					glDisable( GL_BLEND );

					glDisable(GL_CLIP_PLANE2);
				}
				gfx->setShadowMapMode(old_mode);

				if (unit_target)
				{
					unit_target->unlock();
					pMutex.lock();
				}
			}
		drawing = false;
		glPopMatrix();
	}



	void Unit::draw_shadow(const Vector3D& Dir, MAP* map)
	{
		pMutex.lock();
		if (!(flags & 1))
		{
			pMutex.unlock();
			return;
		}

		if (on_radar || hidden)
		{
			pMutex.unlock();
			return;
		}

		if (!model)
			LOG_WARNING("Model is NULL ! (" << __FILE__ << ":" << __LINE__ << ")");

		if (cloaked && owner_id != players.local_human_id ) // Unit is cloaked
		{
			pMutex.unlock();
			return;
		}

		if (!visible)
		{
			Vector3D S_Pos = drawn_Pos-(h/Dir.y)*Dir;//map->hit(Pos,Dir);
			int px=((int)(S_Pos.x)+map->map_w_d)>>4;
			int py=((int)(S_Pos.z)+map->map_h_d)>>4;
			if (px<0 || py<0 || px>=map->bloc_w || py>=map->bloc_h)
			{
				pMutex.unlock();
				return;	// Shadow out of the map
			}
			if (map->view[py][px]!=1)
			{
				pMutex.unlock();
				return;	// Unvisible shadow
			}
		}

		drawing = true;			// Prevent the model to be set to NULL and the data structure from being reset
		pMutex.unlock();

		glPushMatrix();
		glTranslatef(drawn_Pos.x,drawn_Pos.y,drawn_Pos.z);
		glRotatef(drawn_Angle.x,1.0f,0.0f,0.0f);
		glRotatef(drawn_Angle.z,0.0f,0.0f,1.0f);
		glRotatef(drawn_Angle.y,0.0f,1.0f,0.0f);
		float scale = unit_manager.unit_type[type_id]->Scale;
		glScalef(scale,scale,scale);
		glPolygonMode (GL_FRONT_AND_BACK, GL_FILL);

		if ((type_id != -1 && unit_manager.unit_type[type_id]->canmove) || shadow_scale_dir < 0.0f)
		{
			Vector3D H = drawn_Pos;
			H.y += 2.0f * model->size2 + 1.0f;
			Vector3D D = map->hit( H, Dir, true, 2000.0f);
			shadow_scale_dir = (D - H).norm();
		}
		//    model->draw_shadow(((shadow_scale_dir*Dir*RotateX(-drawn_Angle.x*DEG2RAD))*RotateZ(-drawn_Angle.z*DEG2RAD))*RotateY(-drawn_Angle.y*DEG2RAD),0.0f,&data);
		model->draw_shadow(shadow_scale_dir*Dir*RotateXZY(-drawn_Angle.x*DEG2RAD, -drawn_Angle.z*DEG2RAD, -drawn_Angle.y*DEG2RAD),0.0f, &data);

		glPopMatrix();

		drawing = false;
	}


	void Unit::drawShadowBasic(const Vector3D& Dir,MAP *map)
	{
		pMutex.lock();
		if (!(flags & 1))
		{
			pMutex.unlock();
			return;
		}
		if (on_radar || hidden)
		{
			pMutex.unlock();
			return;
		}

		if (cloaked && owner_id != players.local_human_id ) // Unit is cloaked
		{
			pMutex.unlock();
			return;
		}

		if (!visible)
		{
			Vector3D S_Pos (drawn_Pos - (h / Dir.y) * Dir);//map->hit(Pos,Dir);
			int px = ((int)(S_Pos.x + map->map_w_d)) >> 4;
			int py = ((int)(S_Pos.z + map->map_h_d)) >> 4;
			if (px < 0 || py < 0 || px >= map->bloc_w || py >= map->bloc_h)
			{
				pMutex.unlock();
				return;	// Shadow out of the map
			}
			if (map->view[py][px]!=1)
			{
				pMutex.unlock();
				return;	// Unvisible shadow
			}
		}
		drawing = true;			// Prevent the model to be set to NULL and the data structure from being reset
		pMutex.unlock();

		glPushMatrix();
		glTranslatef(drawn_Pos.x,drawn_Pos.y,drawn_Pos.z);
		glRotatef(drawn_Angle.x,1.0f,0.0f,0.0f);
		glRotatef(drawn_Angle.z,0.0f,0.0f,1.0f);
		glRotatef(drawn_Angle.y,0.0f,1.0f,0.0f);
		float scale = unit_manager.unit_type[type_id]->Scale;
		glScalef(scale,scale,scale);
		glPolygonMode (GL_FRONT_AND_BACK, GL_FILL);
		if (unit_manager.unit_type[type_id]->canmove || shadow_scale_dir < 0.0f )
		{
			Vector3D H = drawn_Pos;
			H.y += 2.0f * model->size2 + 1.0f;
			Vector3D D = map->hit( H, Dir, true, 2000.0f);
			shadow_scale_dir = (D - H).norm();
		}
		//    model->drawShadowBasic(((shadow_scale_dir*Dir*RotateX(-drawn_Angle.x*DEG2RAD))*RotateZ(-drawn_Angle.z*DEG2RAD))*RotateY(-drawn_Angle.y*DEG2RAD),0.0f,&data);
		model->draw_shadow_basic(shadow_scale_dir*Dir*RotateXZY(-drawn_Angle.x*DEG2RAD, -drawn_Angle.z*DEG2RAD, -drawn_Angle.y*DEG2RAD),0.0f,&data);

		glPopMatrix();

		drawing = false;
	}


	void Unit::explode()
	{
		exploding = true;
		if (local && network_manager.isConnected() ) // Sync unit destruction (and corpse creation ;) )
		{
			struct event explode_event;
			explode_event.type = EVENT_UNIT_EXPLODE;
			explode_event.opt1 = idx;
			explode_event.opt2 = severity;
			explode_event.x = Pos.x;
			explode_event.y = Pos.y;
			explode_event.z = Pos.z;
			network_manager.sendEvent( &explode_event );
		}

		int power = Math::Max(unit_manager.unit_type[type_id]->FootprintX, unit_manager.unit_type[type_id]->FootprintZ);
		fx_manager.addFlash( Pos, power * 32 );
		fx_manager.addExplosion( Pos, V, power * 3, power * 10 );

		int param[] = { severity * 100 / unit_manager.unit_type[type_id]->MaxDamage, 0 };
		int corpse_type = runScriptFunction(SCRIPT_killed, 2, param);
		if (attached)
			corpse_type = 3;			// When we were flying we just disappear
		bool sinking = the_map->get_unit_h( Pos.x, Pos.z ) <= the_map->sealvl;

		switch( corpse_type )
		{
			case 1:			// Some good looking corpse
				{
					pMutex.unlock();
					flags = 1;				// Set it to 1 otherwise it won't remove it from map
					clear_from_map();
					flags = 4;
					pMutex.lock();
					if (cur_px > 0 && cur_py > 0 && cur_px < (the_map->bloc_w<<1) && cur_py < (the_map->bloc_h<<1))
						if (the_map->map_data[ cur_py ][ cur_px ].stuff == -1)
						{
							int type=feature_manager.get_feature_index(unit_manager.unit_type[type_id]->Corpse);
							if (type >= 0)
							{
								the_map->map_data[ cur_py ][ cur_px ].stuff = features.add_feature(Pos,type);
								if (the_map->map_data[ cur_py ][ cur_px ].stuff >= 0) 	// Keep unit orientation
								{
									features.feature[ the_map->map_data[ cur_py ][ cur_px ].stuff ].angle = Angle.y;
									if (sinking)
										features.sink_feature( the_map->map_data[ cur_py ][ cur_px ].stuff );
									features.drawFeatureOnMap( the_map->map_data[ cur_py ][ cur_px ].stuff );
								}
							}
						}
				}
				break;
			case 2:			// Some exploded corpse
				{
					pMutex.unlock();
					flags = 1;				// Set it to 1 otherwise it won't remove it from map
					clear_from_map();
					flags = 4;
					pMutex.lock();
					if (cur_px > 0 && cur_py > 0 && cur_px < (the_map->bloc_w<<1) && cur_py < (the_map->bloc_h<<1))
						if (the_map->map_data[ cur_py ][ cur_px ].stuff == -1)
						{
							int type=feature_manager.get_feature_index( (String( unit_manager.unit_type[type_id]->name) + "_heap").c_str() );
							if (type >= 0)
							{
								the_map->map_data[ cur_py ][ cur_px ].stuff = features.add_feature(Pos,type);
								if (the_map->map_data[ cur_py ][ cur_px ].stuff >= 0 ) {			// Keep unit orientation
									features.feature[ the_map->map_data[ cur_py ][ cur_px ].stuff ].angle = Angle.y;
									if (sinking )
										features.sink_feature( the_map->map_data[ cur_py ][ cur_px ].stuff );
									features.drawFeatureOnMap( the_map->map_data[ cur_py ][ cur_px ].stuff );
								}
							}
						}
				}
				break;
			default:
				flags = 1;		// Nothing replaced just remove the unit from position map
				pMutex.unlock();
				clear_from_map();
				pMutex.lock();
		}
		pMutex.unlock();
		int w_id = weapons.add_weapon(weapon_manager.get_weapon_index( self_destruct == 0.0f ? unit_manager.unit_type[type_id]->SelfDestructAs : unit_manager.unit_type[type_id]->ExplodeAs ),idx);
		pMutex.lock();
		if (w_id >= 0)
		{
			weapons.weapon[w_id].Pos = Pos;
			weapons.weapon[w_id].target_pos = Pos;
			weapons.weapon[w_id].target = -1;
			weapons.weapon[w_id].just_explode = true;
		}
		for (int i = 0; i < data.nb_piece; ++i)
		{
			if (!(data.flag[i]&FLAG_EXPLODE))// || (data.flag[i]&FLAG_EXPLODE && (data.explosion_flag[i]&EXPLODE_BITMAPONLY)))
				data.flag[i]|=FLAG_HIDE;
		}
	}

	float ballistic_angle(float v, float g, float d, float y_s, float y_e) // Calculs de ballistique pour l'angle de tir
	{
		float v2 = v*v;
		float gd = g*d;
		float v2gd = v2/gd;
		float a = v2gd*(4.0f*v2gd-8.0f*(y_e-y_s)/d)-4.0f;
		if (a<0.0f)				// Pas de solution
			return 360.0f;
		return RAD2DEG*atanf(v2gd-0.5f*sqrtf(a));
	}

	const int Unit::move(const float dt, MAP* map, int* path_exec, const int key_frame)
	{
		pMutex.lock();

		bool was_open = port[YARD_OPEN] != 0;
		bool was_flying = flying;
		sint32	o_px = cur_px;
		sint32	o_py = cur_py;
		compute_coord = true;
		Vector3D	old_V = V;			// Store the speed, so we can do some calculations
		bool	b_TargetAngle = false;		// Do we aim, move, ... ?? Need to change unit angle
		float	f_TargetAngle = 0.0f;

		Vector3D NPos = Pos;
		int n_px = cur_px;
		int n_py = cur_py;
		bool precomputed_position = false;

		if (type_id < 0 || type_id >= unit_manager.nb_unit || flags == 0 ) // A unit which cannot exist
		{
			pMutex.unlock();
			LOG_ERROR("Unit::move : A unit which doesn't exist was found");
			return	-1;		// Should NEVER happen
		}

		float resource_min_factor = TA3D::Math::Min(TA3D::players.energy_factor[owner_id], TA3D::players.metal_factor[owner_id]);

		if (build_percent_left == 0.0f && unit_manager.unit_type[type_id]->isfeature) // Turn this unit into a feature
		{
			if (cur_px > 0 && cur_py > 0 && cur_px < (map->bloc_w<<1) && cur_py < (map->bloc_h<<1) )
			{
				if (map->map_data[ cur_py ][ cur_px ].stuff == -1)
				{
					int type = feature_manager.get_feature_index(unit_manager.unit_type[type_id]->Corpse);
					if (type >= 0)
					{
						features.lock();
						map->map_data[ cur_py ][ cur_px ].stuff=features.add_feature(Pos,type);
						if (map->map_data[ cur_py ][ cur_px ].stuff == -1)
							LOG_ERROR("Could not turn `" << unit_manager.unit_type[type_id]->Unitname << "` into a feature ! Cannot create the feature");
						else
							features.feature[map->map_data[ cur_py ][ cur_px ].stuff].angle = Angle.y;
						pMutex.unlock();
						clear_from_map();
						pMutex.lock();
						features.drawFeatureOnMap( map->map_data[ cur_py ][ cur_px ].stuff );
						features.unlock();
						flags = 4;
					}
					else
						LOG_ERROR("Could not turn `" << unit_manager.unit_type[type_id]->Unitname << "` into a feature ! Feature not found");
				}
			}
			pMutex.unlock();
			return -1;
		}

		if (map->ota_data.waterdoesdamage && Pos.y < map->sealvl)		// The unit is damaged by the "acid" water
			hp -= dt*map->ota_data.waterdamage;

		bool jump_commands = (((idx+key_frame)&0xF) == 0);		// Saute certaines commandes / Jump some commands so it runs faster with lots of units

		if (build_percent_left == 0.0f && self_destruct >= 0.0f) // Self-destruction code
		{
			int old = (int)self_destruct;
			self_destruct -= dt;
			if (old != (int)self_destruct) // Play a sound :-)
				playSound( String::Format( "count%d", old));
			if (self_destruct <= 0.0f)
			{
				self_destruct = 0.0f;
				hp = 0.0f;
				severity = unit_manager.unit_type[type_id]->MaxDamage;
			}
		}

		if (hp<=0.0f && (local || exploding)) // L'unité est détruite
		{
			if (mission
				&& !unit_manager.unit_type[type_id]->BMcode
				&& ( mission->mission == MISSION_BUILD_2 || mission->mission == MISSION_BUILD )		// It was building something that we must destroy too
				&& mission->p != NULL )
			{
				((Unit*)(mission->p))->lock();
				((Unit*)(mission->p))->hp = 0.0f;
				((Unit*)(mission->p))->built = false;
				((Unit*)(mission->p))->unlock();
			}
			death_timer++;
			if (death_timer == 255 ) // Ok we've been dead for a long time now ...
			{
				pMutex.unlock();
				return -1;
			}
			switch(flags&0x17)
			{
				case 1:				// Début de la mort de l'unité	(Lance le script)
					flags = 4;		// Don't remove the data on the position map because they will be replaced
					if (build_percent_left == 0.0f && local)
						explode();
					else
						flags = 1;
					death_delay=1.0f;
					if (flags == 1 )
					{
						pMutex.unlock();
						return -1;
					}
					break;
				case 4:				// Vérifie si le script est terminé
					if (death_delay<=0.0f || !data.explode)
					{
						flags = 1;
						pMutex.unlock();
						clear_from_map();
						return -1;
					}
					death_delay -= dt;
					for(int i=0;i<data.nb_piece;i++)
						if (!(data.flag[i]&FLAG_EXPLODE))// || (data.flag[i]&FLAG_EXPLODE && (data.explosion_flag[i]&EXPLODE_BITMAPONLY)))
							data.flag[i]|=FLAG_HIDE;
					break;
				case 0x14:				// Unit has been captured, this is a FAKE unit, just here to be removed
					flags=4;
					pMutex.unlock();
					return -1;
				default:		// It doesn't explode (it has been reclaimed for example)
					flags=1;
					pMutex.unlock();
					clear_from_map();
					return -1;
			}
			if (data.nb_piece>0 && build_percent_left == 0.0f)
			{
				data.move(dt,map->ota_data.gravity);
				if (c_time>=0.1f)
				{
					c_time=0.0f;
					for(int i=0;i<data.nb_piece;i++)
						if (data.flag[i]&FLAG_EXPLODE && (data.explosion_flag[i]&EXPLODE_BITMAPONLY)!=EXPLODE_BITMAPONLY)
						{
							if (data.explosion_flag[i]&EXPLODE_FIRE)
							{
								compute_model_coord();
								particle_engine.make_smoke(Pos+data.pos[i],fire,1,0.0f,0.0f);
							}
							if (data.explosion_flag[i]&EXPLODE_SMOKE)
							{
								compute_model_coord();
								particle_engine.make_smoke(Pos+data.pos[i],0,1,0.0f,0.0f);
							}
						}
				}
			}
			goto script_exec;
		}
		else if (!jump_commands && do_nothing() && local)
			if (Pos.x<-map->map_w_d || Pos.x>map->map_w_d || Pos.z<-map->map_h_d || Pos.z>map->map_h_d)
			{
				Vector3D target = Pos;
				if (target.x < -map->map_w_d+256)
					target.x = -map->map_w_d+256;
				else if (target.x > map->map_w_d-256)
					target.x = map->map_w_d-256;
				if (target.z < -map->map_h_d+256)
					target.z = -map->map_h_d+256;
				else if (target.z > map->map_h_d-256)
					target.z = map->map_h_d-256;
				add_mission(MISSION_MOVE | MISSION_FLAG_AUTO,&target,true,0,NULL,NULL,0,1);		// Stay on map
			}

		flags &= 0xEF;		// To fix a bug

		if (build_percent_left > 0.0f) // Unit isn't finished
		{
			if (!built && local)
			{
				float frac = 1000.0f / ( 6 * unit_manager.unit_type[type_id]->BuildTime );
				metal_prod = frac * unit_manager.unit_type[type_id]->BuildCostMetal;
				frac *= dt;
				hp -= frac * unit_manager.unit_type[type_id]->MaxDamage;
				build_percent_left += frac * 100.0f;
			}
			else
				metal_prod = 0.0f;
			goto script_exec;
		}
		else
		{
			if (hp < unit_manager.unit_type[type_id]->MaxDamage && unit_manager.unit_type[type_id]->HealTime > 0)
			{
				hp += unit_manager.unit_type[type_id]->MaxDamage * dt / unit_manager.unit_type[type_id]->HealTime;
				if (hp > unit_manager.unit_type[type_id]->MaxDamage )
					hp = unit_manager.unit_type[type_id]->MaxDamage;
			}
		}

		if (data.nb_piece>0)
			data.move(dt,units.g_dt);

		if (cloaking && paralyzed <= 0.0f)
		{
			int conso_energy = (mission == NULL || !(mission->flags & MISSION_FLAG_MOVE) ) ? unit_manager.unit_type[type_id]->CloakCost : unit_manager.unit_type[type_id]->CloakCostMoving;
			TA3D::players.requested_energy[owner_id] += conso_energy;
			if (players.energy[ owner_id ] >= (energy_cons + conso_energy) * dt)
			{
				energy_cons += conso_energy;
				int dx = unit_manager.unit_type[type_id]->mincloakdistance >> 3;
				int distance = SQUARE(unit_manager.unit_type[type_id]->mincloakdistance);
				// byte mask = 1 << owner_id;
				bool found = false;
				for(int y = cur_py - dx ; y <= cur_py + dx && !found ; y++)
					if (y >= 0 && y < map->bloc_h_db - 1)
						for(int x = cur_px - dx ; x <= cur_px + dx ; x++)
							if (x >= 0 && x < map->bloc_w_db - 1)
							{
								int cur_idx = map->map_data[y][x].unit_idx;

								if (cur_idx>=0 && cur_idx < units.max_unit && (units.unit[cur_idx].flags & 1) && units.unit[cur_idx].owner_id != owner_id
									&& distance >= (Pos - units.unit[ cur_idx ].Pos).sq())
								{
									found = true;
									break;
								}
							}
				cloaked = !found;
			}
			else
				cloaked = false;
		}
		else
			cloaked = false;

		if (paralyzed > 0.0f)       // This unit is paralyzed
		{
			paralyzed -= dt;
			if (unit_manager.unit_type[type_id]->model)
			{
				Vector3D randVec;
				bool random_vector=false;
				int n = 0;
				for ( int base_n = Math::RandFromTable() ; !random_vector && n < unit_manager.unit_type[type_id]->model->obj.nb_sub_obj ; n++ )
					random_vector = unit_manager.unit_type[type_id]->model->obj.random_pos( &data, (base_n + n) % unit_manager.unit_type[type_id]->model->obj.nb_sub_obj, &randVec );
				if (random_vector)
					fx_manager.addElectric( Pos + randVec );
			}
			if (build_percent_left <= 0.0f)
				metal_prod = 0.0f;
		}

		if (attached || paralyzed > 0.0f)
			goto script_exec;

		if (unit_manager.unit_type[type_id]->canload && nb_attached > 0)
		{
			int e = 0;
			compute_model_coord();
			for (int i = 0; i + e < nb_attached; ++i)
			{
				if (units.unit[attached_list[i]].flags)
				{
					units.unit[attached_list[i]].Pos = Pos+data.pos[link_list[i]];
					units.unit[attached_list[i]].cur_px = ((int)(units.unit[attached_list[i]].Pos.x)+map->map_w_d)>>3;
					units.unit[attached_list[i]].cur_py = ((int)(units.unit[attached_list[i]].Pos.z)+map->map_h_d)>>3;
					units.unit[attached_list[i]].Angle = Angle;
				}
				else
				{
					++e;
					--i;
					continue;
				}
				attached_list[i]=attached_list[i+e];
			}
			nb_attached-=e;
		}

		if (planned_weapons > 0.0f)	// Construit des armes / build weapons
		{
			const float old = planned_weapons - (int)planned_weapons;
			int idx = -1;
			for (unsigned int i = 0; i < unit_manager.unit_type[type_id]->weapon.size(); ++i)
			{
				if (unit_manager.unit_type[type_id]->weapon[i] && unit_manager.unit_type[type_id]->weapon[i]->stockpile)
				{
					idx=i;
					break;
				}
			}
			if (idx != -1 && unit_manager.unit_type[type_id]->weapon[idx]->reloadtime != 0.0f)
			{
				float dn=dt/unit_manager.unit_type[type_id]->weapon[idx]->reloadtime;
				float conso_metal=((float)unit_manager.unit_type[type_id]->weapon[idx]->metalpershot)/unit_manager.unit_type[type_id]->weapon[idx]->reloadtime;
				float conso_energy=((float)unit_manager.unit_type[type_id]->weapon[idx]->energypershot)/unit_manager.unit_type[type_id]->weapon[idx]->reloadtime;

				TA3D::players.requested_energy[owner_id] += conso_energy;
				TA3D::players.requested_metal[owner_id] += conso_metal;

				if (players.metal[owner_id] >= (metal_cons + conso_metal * resource_min_factor) * dt
					&& players.energy[owner_id] >= (energy_cons + conso_energy * resource_min_factor) * dt)
				{
					metal_cons += conso_metal * resource_min_factor;
					energy_cons += conso_energy * resource_min_factor;
					planned_weapons -= dn * resource_min_factor;
					float last=planned_weapons-(int)planned_weapons;
					if ((last==0.0f && last!=old) || (last>old && old>0.0f) || planned_weapons<=0.0f)		// On en a fini une / one is finished
						weapon[idx].stock++;
					if (planned_weapons<0.0f)
						planned_weapons=0.0f;
				}
			}
		}

		V_Angle.reset();
		c_time += dt;

		//------------------------------ Beginning of weapon related code ---------------------------------------
		for (unsigned int i = 0; i < weapon.size(); ++i)
		{
			if (unit_manager.unit_type[type_id]->weapon[i] == NULL)
				continue;		// Skip that weapon if not present on the unit
			weapon[i].delay += dt;
			weapon[i].time += dt;

			int Query_script;
			int Aim_script;
			int AimFrom_script;
			int Fire_script;
			switch(i)
			{
				case 0:
					Query_script = SCRIPT_QueryPrimary;
					Aim_script = SCRIPT_AimPrimary;
					AimFrom_script = SCRIPT_AimFromPrimary;
					Fire_script = SCRIPT_FirePrimary;
					break;
				case 1:
					Query_script = SCRIPT_QuerySecondary;
					Aim_script = SCRIPT_AimSecondary;
					AimFrom_script = SCRIPT_AimFromSecondary;
					Fire_script = SCRIPT_FireSecondary;
					break;
				case 2:
					Query_script = SCRIPT_QueryTertiary;
					Aim_script = SCRIPT_AimTertiary;
					AimFrom_script = SCRIPT_AimFromTertiary;
					Fire_script = SCRIPT_FireTertiary;
					break;
				default:
					Query_script = SCRIPT_QueryWeapon + (i - 3) * 4;
					Aim_script = SCRIPT_AimWeapon + (i - 3) * 4;
					AimFrom_script = SCRIPT_AimFromWeapon + (i - 3) * 4;
					Fire_script = SCRIPT_FireWeapon + (i - 3) * 4;
			}

			switch ((weapon[i].state & 3))
			{
				case WEAPON_FLAG_IDLE:										// Doing nothing, waiting for orders
					script->setReturnValue( UnitScriptInterface::get_script_name(Aim_script), 0);
					if (jump_commands)	break;
					weapon[i].data = -1;
					break;
				case WEAPON_FLAG_AIM:											// Vise une unité / aiming code
                    if (jump_commands)	break;
                    if (weapon[i].target == NULL || ((weapon[i].state&WEAPON_FLAG_WEAPON)==WEAPON_FLAG_WEAPON && ((WEAPON*)(weapon[i].target))->weapon_id!=-1)
						|| ((weapon[i].state&WEAPON_FLAG_WEAPON)!=WEAPON_FLAG_WEAPON && (((Unit*)(weapon[i].target))->flags&1)))
					{
						if ((weapon[i].state&WEAPON_FLAG_WEAPON)!=WEAPON_FLAG_WEAPON && weapon[i].target != NULL && ((Unit*)(weapon[i].target))->cloaked
							&& ((Unit*)(weapon[i].target))->owner_id != owner_id && !((Unit*)(weapon[i].target))->is_on_radar( 1 << owner_id))
						{
							weapon[i].data = -1;
							weapon[i].state = WEAPON_FLAG_IDLE;
							break;
						}

						if (!(weapon[i].state & WEAPON_FLAG_COMMAND_FIRE) && unit_manager.unit_type[type_id]->weapon[i]->commandfire) // Not allowed to fire
						{
							weapon[i].data = -1;
							weapon[i].state = WEAPON_FLAG_IDLE;
							break;
						}

						if (weapon[i].delay >= unit_manager.unit_type[type_id]->weapon[ i ]->reloadtime || unit_manager.unit_type[type_id]->weapon[ i ]->stockpile)
						{
							bool readyToFire = false;

							Unit *target_unit = (weapon[i].state & WEAPON_FLAG_WEAPON ) == WEAPON_FLAG_WEAPON ? NULL : (Unit*) weapon[i].target;
							WEAPON *target_weapon = (weapon[i].state & WEAPON_FLAG_WEAPON ) == WEAPON_FLAG_WEAPON ? (WEAPON*) weapon[i].target : NULL;

							Vector3D target = target_unit==NULL ? (target_weapon==NULL ? weapon[i].target_pos-Pos : target_weapon->Pos-Pos) : target_unit->Pos-Pos;
							float dist = target.sq();
							int maxdist = 0;
							int mindist = 0;

							if (unit_manager.unit_type[type_id]->attackrunlength>0)
							{
								if (target % V < 0.0f)
								{
									weapon[i].state = WEAPON_FLAG_IDLE;
									weapon[i].data = -1;
									break;	// We're not shooting at the target
								}
								float t = 2.0f / map->ota_data.gravity * fabsf(target.y);
								mindist = (int)sqrtf(t*V.sq())-((unit_manager.unit_type[type_id]->attackrunlength+1)>>1);
								maxdist = mindist+(unit_manager.unit_type[type_id]->attackrunlength);
							}
							else
							{
								if (unit_manager.unit_type[type_id]->weapon[ i ]->waterweapon && Pos.y > units.map->sealvl)
								{
									if (target % V < 0.0f)
									{
										weapon[i].state = WEAPON_FLAG_IDLE;
										weapon[i].data = -1;
										break;	// We're not shooting at the target
									}
									float t = 2.0f/map->ota_data.gravity*fabsf(target.y);
									mindist = (int)sqrtf(t*V.sq());
									maxdist = mindist + (unit_manager.unit_type[type_id]->weapon[ i ]->range>>1);
								}
								else
									maxdist = unit_manager.unit_type[type_id]->weapon[ i ]->range>>1;
							}

							if (dist > maxdist * maxdist || dist < mindist * mindist)
							{
								weapon[i].state = WEAPON_FLAG_IDLE;
								weapon[i].data = -1;
								break;	// We're too far from the target
							}

							Vector3D target_translation;
							if (target_unit != NULL)
								target_translation = ( target.norm() / unit_manager.unit_type[type_id]->weapon[ i ]->weaponvelocity) * (target_unit->V - V);

                            if (unit_manager.unit_type[type_id]->weapon[ i ]->turret) 	// Si l'unité doit viser, on la fait viser / if it must aim, we make it aim
							{
                                readyToFire = script->getReturnValue( UnitScriptInterface::get_script_name(Aim_script) );

                                int start_piece = weapon[i].aim_piece;
                                if (weapon[i].aim_piece < 0)
                                    weapon[i].aim_piece = start_piece = runScriptFunction(AimFrom_script);
								if (start_piece < 0 || start_piece >= data.nb_piece)
									start_piece = 0;
								compute_model_coord();

								Vector3D target_pos_on_unit;						// Read the target piece on the target unit so we better know where to aim
								target_pos_on_unit.x = target_pos_on_unit.y = target_pos_on_unit.z = 0.0f;
								if (target_unit != NULL)
								{
                                    if (weapon[i].data == -1)
                                        weapon[i].data = target_unit->get_sweet_spot();
                                    if (weapon[i].data >= 0)
										target_pos_on_unit = target_unit->data.pos[ weapon[i].data ];
								}

								target = target + target_translation - data.pos[start_piece];
                                if (target_unit != NULL)
									target = target + target_pos_on_unit;

								if (unit_manager.unit_type[type_id]->aim_data[i].check)     // Check angle limitations (not in OTA)
								{
									// Go back in model coordinates so we can compare to the weapon main direction
									Vector3D dir = target * RotateXZY(-Angle.x * DEG2RAD, -Angle.z * DEG2RAD, -Angle.y * DEG2RAD);
									// Check weapon
									if (VAngle(dir, unit_manager.unit_type[type_id]->aim_data[i].dir) > unit_manager.unit_type[type_id]->aim_data[i].Maxangledif)
									{
										weapon[i].state = WEAPON_FLAG_IDLE;
										weapon[i].data = -1;
										break;
									}
								}

								dist = target.norm();
								target = (1.0f / dist) * target;
								Vector3D I, J, IJ, RT;
								I.x = 0.0f;     I.z = 1.0f;     I.y = 0.0f;
								J.x = 1.0f;     J.z = 0.0f;     J.y = 0.0f;
								IJ.x = 0.0f;    IJ.z = 0.0f;    IJ.y = 1.0f;
								RT = target;
								RT.y = 0.0f;
								RT.unit();
								float angle = acosf(I % RT) * RAD2DEG;
								if (J % RT < 0.0f) angle =- angle;
								angle -= Angle.y;
								if (angle < -180.0f)        angle += 360.0f;
								else if (angle > 180.0f)    angle -= 360.0f;

								int aiming[] = { (int)(angle*DEG2TA), -4096 };
								if (unit_manager.unit_type[type_id]->weapon[ i ]->ballistic) // Calculs de ballistique / ballistic calculations
								{
									Vector3D D = target_unit == NULL ? ( target_weapon == NULL ? Pos + data.pos[start_piece] - weapon[i].target_pos : (Pos+data.pos[start_piece]-target_weapon->Pos) ) : (Pos+data.pos[start_piece]-target_unit->Pos-target_pos_on_unit);
									D.y = 0.0f;
									float v;
									if (unit_manager.unit_type[type_id]->weapon[ i ]->startvelocity == 0.0f)
										v = unit_manager.unit_type[type_id]->weapon[ i ]->weaponvelocity;
									else
										v = unit_manager.unit_type[type_id]->weapon[ i ]->startvelocity;
									if (target_unit == NULL)
									{
										if (target_weapon == NULL)
											aiming[1] = (int)(ballistic_angle(v,map->ota_data.gravity,D.norm(),(Pos+data.pos[start_piece]).y,weapon[i].target_pos.y)*DEG2TA);
										else
											aiming[1] = (int)(ballistic_angle(v,map->ota_data.gravity,D.norm(),(Pos+data.pos[start_piece]).y,target_weapon->Pos.y)*DEG2TA);
									}
									else
										aiming[1] = (int)(ballistic_angle(v,map->ota_data.gravity,D.norm(),(Pos+data.pos[start_piece]).y,target_unit->Pos.y+target_unit->model->center.y*0.5f)*DEG2TA);
								}
								else
								{
									Vector3D K = target;
									K.y = 0.0f;
									K.unit();
									angle = acosf(K % target) * RAD2DEG;
									if (target.y < 0.0f)
										angle = -angle;
									angle -= Angle.x;
									if (angle > 180.0f)     angle -= 360.0f;
									if (angle < -180.0f)    angle += 360.0f;
									if (fabsf(angle) > 180.0f)
									{
										weapon[i].state = WEAPON_FLAG_IDLE;
										weapon[i].data = -1;
										break;
									}
									aiming[1] = (int)(angle*DEG2TA);
								}
                                if (readyToFire)
                                {
                                    if (unit_manager.unit_type[type_id]->weapon[i]->lineofsight)
                                    {
                                        if (!target_unit)
                                        {
                                            if (target_weapon == NULL )
                                                weapon[i].aim_dir = weapon[i].target_pos - (Pos + data.pos[start_piece]);
                                            else
                                                weapon[i].aim_dir = ((WEAPON*)(weapon[i].target))->Pos - (Pos + data.pos[start_piece]);
                                        }
                                        else
                                            weapon[i].aim_dir = ((Unit*)(weapon[i].target))->Pos + target_pos_on_unit - (Pos + data.pos[start_piece]);
                                        weapon[i].aim_dir = weapon[i].aim_dir + target_translation;
                                        weapon[i].aim_dir.unit();
                                    }
                                    else
                                        weapon[i].aim_dir = cosf(aiming[1] * TA2RAD) * (cosf(aiming[0] * TA2RAD + Angle.y * DEG2RAD) * I
                                                                                        + sinf(aiming[0] * TA2RAD + Angle.y * DEG2RAD) * J)
                                            + sinf(aiming[1] * TA2RAD) * IJ;
                                }
                                if (!readyToFire)
                                    launchScript(Aim_script, 2, aiming);
                            }
							else
								readyToFire = true;
							if (readyToFire)
							{
								weapon[i].time = 0.0f;
								weapon[i].state = WEAPON_FLAG_SHOOT;									// (puis) on lui demande de tirer / tell it to fire
								weapon[i].burst = 0;
							}
						}
					}
					else
					{
						launchScript(SCRIPT_TargetCleared);
						weapon[i].state = WEAPON_FLAG_IDLE;
						weapon[i].data = -1;
					}
					break;
				case WEAPON_FLAG_SHOOT:											// Tire sur une unité / fire!
                    if (weapon[i].target == NULL || (( weapon[i].state & WEAPON_FLAG_WEAPON ) == WEAPON_FLAG_WEAPON && ((WEAPON*)(weapon[i].target))->weapon_id!=-1)
						|| (( weapon[i].state & WEAPON_FLAG_WEAPON ) != WEAPON_FLAG_WEAPON && (((Unit*)(weapon[i].target))->flags&1)))
					{
						if (weapon[i].burst > 0 && weapon[i].delay < unit_manager.unit_type[type_id]->weapon[ i ]->burstrate)
							break;
						if ((players.metal[owner_id]<unit_manager.unit_type[type_id]->weapon[ i ]->metalpershot
							 || players.energy[owner_id]<unit_manager.unit_type[type_id]->weapon[ i ]->energypershot)
							&& !unit_manager.unit_type[type_id]->weapon[ i ]->stockpile)
						{
							weapon[i].state = WEAPON_FLAG_AIM;		// Pas assez d'énergie pour tirer / not enough energy to fire
							weapon[i].data = -1;
							script->setReturnValue(UnitScriptInterface::get_script_name(Aim_script), 0);
							break;
						}
						if (unit_manager.unit_type[type_id]->weapon[ i ]->stockpile && weapon[i].stock<=0)
						{
							weapon[i].state = WEAPON_FLAG_AIM;		// Plus rien pour tirer / nothing to fire
							weapon[i].data = -1;
							script->setReturnValue(UnitScriptInterface::get_script_name(Aim_script), 0);
							break;
						}
						int start_piece = runScriptFunction(Query_script);
						if (start_piece >= 0 && start_piece < data.nb_piece)
						{
							compute_model_coord();
							if (!unit_manager.unit_type[type_id]->weapon[ i ]->waterweapon && Pos.y + data.pos[start_piece].y <= map->sealvl)     // Can't shoot from water !!
								break;
							Vector3D Dir = data.dir[start_piece];
							if (unit_manager.unit_type[type_id]->weapon[ i ]->vlaunch)
							{
								Dir.x = 0.0f;
								Dir.y = 1.0f;
								Dir.z = 0.0f;
							}
							else if (Dir.x==0.0f && Dir.y==0.0f && Dir.z==0.0f)
								Dir = weapon[i].aim_dir;
							if (i == 3)
							{
								LOG_DEBUG("firing from " << (Pos+data.pos[start_piece]).y << " (" << units.map->get_unit_h((Pos+data.pos[start_piece]).x, (Pos+data.pos[start_piece]).z) << ")");
								LOG_DEBUG("from piece " << start_piece << " (" << Query_script << "," << Aim_script << "," << Fire_script << ")" );
							}

							// SHOOT NOW !!
							if (unit_manager.unit_type[type_id]->weapon[ i ]->stockpile )
								weapon[i].stock--;
							else
							{													// We use energy and metal only for weapons with no prebuilt ammo
								players.c_metal[owner_id] -= unit_manager.unit_type[type_id]->weapon[ i ]->metalpershot;
								players.c_energy[owner_id] -= unit_manager.unit_type[type_id]->weapon[ i ]->energypershot;
							}
							launchScript( Fire_script );			// Run the fire animation script
							if (!unit_manager.unit_type[type_id]->weapon[ i ]->soundstart.empty())	sound_manager->playSound(unit_manager.unit_type[type_id]->weapon[i]->soundstart, &Pos);

							if (weapon[i].target == NULL)
								shoot(-1,Pos+data.pos[start_piece],Dir,i, weapon[i].target_pos );
							else
							{
								if (weapon[i].state & WEAPON_FLAG_WEAPON)
									shoot(((WEAPON*)(weapon[i].target))->idx,Pos+data.pos[start_piece],Dir,i, weapon[i].target_pos);
								else
									shoot(((Unit*)(weapon[i].target))->idx,Pos+data.pos[start_piece],Dir,i, weapon[i].target_pos);
							}
							weapon[i].burst++;
                            if (weapon[i].burst >= unit_manager.unit_type[type_id]->weapon[i]->burst)
                                weapon[i].burst = 0;
                            weapon[i].delay = 0.0f;
                            weapon[i].aim_piece = -1;
						}
						if (weapon[i].burst == 0 && unit_manager.unit_type[type_id]->weapon[ i ]->commandfire && !unit_manager.unit_type[type_id]->weapon[ i ]->dropped)    // Shoot only once
						{
							weapon[i].state = WEAPON_FLAG_IDLE;
							weapon[i].data = -1;
							script->setReturnValue(UnitScriptInterface::get_script_name(Aim_script), 0);
							if (mission != NULL )
								mission->flags |= MISSION_FLAG_COMMAND_FIRED;
							break;
						}
						if (weapon[i].target != NULL && (weapon[i].state & WEAPON_FLAG_WEAPON)!=WEAPON_FLAG_WEAPON && ((Unit*)(weapon[i].target))->hp > 0)  // La cible est-elle détruite ?? / is target destroyed ??
						{
							if (weapon[i].burst == 0)
							{
								weapon[i].state = WEAPON_FLAG_AIM;
								weapon[i].data = -1;
								weapon[i].time = 0.0f;
								script->setReturnValue(UnitScriptInterface::get_script_name(Aim_script), 0);
							}
						}
						else if (weapon[i].target != NULL || weapon[i].burst == 0)
						{
							launchScript(SCRIPT_TargetCleared);
							weapon[i].state = WEAPON_FLAG_IDLE;
							weapon[i].data = -1;
							script->setReturnValue(UnitScriptInterface::get_script_name(Aim_script), 0);
						}
					}
					else
					{
						launchScript(SCRIPT_TargetCleared);
						weapon[i].state = WEAPON_FLAG_IDLE;
						weapon[i].data = -1;
					}
					break;
			}
		}

		//---------------------------- Beginning of mission execution code --------------------------------------

		if (mission == NULL)
			was_moving = false;

		if (mission)
		{
			mission->time+=dt;
			last_path_refresh += dt;

			//----------------------------------- Beginning of moving code ------------------------------------

			if ((mission->flags & MISSION_FLAG_MOVE) && unit_manager.unit_type[type_id]->canmove && unit_manager.unit_type[type_id]->BMcode)
			{
				if (!was_moving)
				{
					if (unit_manager.unit_type[type_id]->canfly)
						activate();
					launchScript(SCRIPT_startmoving);
					if (nb_attached==0)
						launchScript(SCRIPT_MoveRate1);		// For the armatlas
					else
						launchScript(SCRIPT_MoveRate2);
					was_moving = true;
				}
				Vector3D J,I,K;
				K.x = 0.0f;
				K.y = 1.0f;
				K.z = 0.0f;
				Vector3D Target(mission->target);
				if (mission->path && ( !(mission->flags & MISSION_FLAG_REFRESH_PATH) || last_path_refresh < 5.0f ) )
					Target = mission->path->Pos;
				else
				{// Look for a path to the target
					if (mission->path)// If we want to refresh the path
					{
						Target = mission->target;//mission->path->Pos;
						destroy_path( mission->path );
						mission->path = NULL;
					}
					mission->flags &= ~MISSION_FLAG_REFRESH_PATH;
					float dist = (Target-Pos).sq();
					if (( mission->move_data <= 0 && dist > 100.0f ) || ( ( mission->move_data * mission->move_data << 6 ) < dist))
					{
						if (!requesting_pathfinder && last_path_refresh >= 5.0f)
						{
							requesting_pathfinder = true;
							units.requests[ owner_id ].push_back( idx );
						}
						if (path_exec[ owner_id ] < MAX_PATH_EXEC && last_path_refresh >= 5.0f && !units.requests[ owner_id ].empty() && idx == units.requests[ owner_id ].front())
						{
							units.requests[ owner_id ].pop_front();
							requesting_pathfinder = false;

							path_exec[ owner_id ]++;
							move_target_computed = mission->target;
							last_path_refresh = 0.0f;
							if (unit_manager.unit_type[type_id]->canfly)
							{
								if (mission->move_data<=0)
									mission->path = direct_path(mission->target);
								else
								{
									Vector3D Dir = mission->target-Pos;
									Dir.unit();
									mission->path = direct_path(mission->target-(mission->move_data<<3)*Dir);
								}
							}
							else
							{
								float dh_max = unit_manager.unit_type[type_id]->MaxSlope * H_DIV;
								float h_min = unit_manager.unit_type[type_id]->canhover ? -100.0f : map->sealvl - unit_manager.unit_type[type_id]->MaxWaterDepth * H_DIV;
								float h_max = map->sealvl - unit_manager.unit_type[type_id]->MinWaterDepth * H_DIV;
								float hover_h = unit_manager.unit_type[type_id]->canhover ? map->sealvl : -100.0f;
								if (mission->move_data <= 0)
									mission->path = find_path(map->map_data,map->h_map,map->path,map->map_w,map->map_h,map->bloc_w<<1,map->bloc_h<<1,
															  dh_max, h_min, h_max, Pos, mission->target, unit_manager.unit_type[type_id]->FootprintX, unit_manager.unit_type[type_id]->FootprintZ, idx, 0, hover_h );
								else
									mission->path = find_path(map->map_data,map->h_map,map->path,map->map_w,map->map_h,map->bloc_w<<1,map->bloc_h<<1,
															  dh_max, h_min, h_max, Pos, mission->target, unit_manager.unit_type[type_id]->FootprintX, unit_manager.unit_type[type_id]->FootprintZ, idx, mission->move_data, hover_h );

								if (mission->path == NULL)
								{
									bool place_is_empty = map->check_rect( cur_px-(unit_manager.unit_type[type_id]->FootprintX>>1), cur_py-(unit_manager.unit_type[type_id]->FootprintZ>>1), unit_manager.unit_type[type_id]->FootprintX, unit_manager.unit_type[type_id]->FootprintZ, idx);
									if (!place_is_empty)
									{
										LOG_WARNING("A Unit is blocked !" << __FILE__ << ":" << __LINE__);
										mission->flags &= ~MISSION_FLAG_MOVE;
									}
									else
										mission->flags |= MISSION_FLAG_REFRESH_PATH;			// Retry later
									launchScript(SCRIPT_StopMoving);
									was_moving = false;
								}

								if (mission->path == NULL)					// Can't find a path to get where it has been ordered to go
									playSound("cant1");
							}
							if (mission->path)// Update required data
								Target = mission->path->Pos;
						}
					}
					else
						stopMoving();
				}

				if (mission->path) // If we have a path, follow it
				{
					if ((mission->target - move_target_computed).sq() >= 10000.0f )			// Follow the target above all...
						mission->flags |= MISSION_FLAG_REFRESH_PATH;
					J = Target - Pos;
					J.y = 0.0f;
					float dist = J.norm();
					if ((dist > mission->last_d && dist < 15.0f) || mission->path == NULL)
					{
						if (mission->path)
						{
							float dh_max = unit_manager.unit_type[type_id]->MaxSlope * H_DIV;
							float h_min = unit_manager.unit_type[type_id]->canhover ? -100.0f : map->sealvl - unit_manager.unit_type[type_id]->MaxWaterDepth * H_DIV;
							float h_max = map->sealvl - unit_manager.unit_type[type_id]->MinWaterDepth * H_DIV;
							float hover_h = unit_manager.unit_type[type_id]->canhover ? map->sealvl : -100.0f;
							mission->path = next_node(mission->path, map->map_data, map->h_map, map->bloc_w_db, map->bloc_h_db, dh_max, h_min, h_max, unit_manager.unit_type[type_id]->FootprintX, unit_manager.unit_type[type_id]->FootprintZ, idx, hover_h );
						}
						mission->last_d = 9999999.0f;
						if (mission->path == NULL) // End of path reached
						{
							J = move_target_computed - Pos;
							J.y = 0.0f;
							if (J.sq() <= 256.0f || flying)
							{
								if (!(mission->flags & MISSION_FLAG_DONT_STOP_MOVE) && (mission == NULL || mission->mission != MISSION_PATROL ) )
									playSound( "arrived1" );
								mission->flags &= ~MISSION_FLAG_MOVE;
							}
							else										// We are not where we are supposed to be !!
								mission->flags |= MISSION_FLAG_REFRESH_PATH;
							if (!( unit_manager.unit_type[type_id]->canfly && nb_attached > 0 ) ) {		// Once charged with units the Atlas cannot land
								launchScript(SCRIPT_StopMoving);
								was_moving = false;
							}
							if (!(mission->flags & MISSION_FLAG_DONT_STOP_MOVE) )
								V.x = V.y = V.z = 0.0f;		// Stop unit's movement
							if (mission->step)      // It's meaningless to try to finish this mission like an ordinary order
								next_mission();
						}
					}
					else
						mission->last_d = dist;
					if (mission->flags & MISSION_FLAG_MOVE)	// Are we still moving ??
					{
						if (dist > 0.0f)
							J = 1.0f / dist * J;

						b_TargetAngle = true;
						f_TargetAngle = acosf( J.z ) * RAD2DEG;
						if (J.x < 0.0f ) f_TargetAngle = -f_TargetAngle;

						if (Angle.y - f_TargetAngle >= 360.0f )	f_TargetAngle += 360.0f;
						else if (Angle.y - f_TargetAngle <= -360.0f )	f_TargetAngle -= 360.0f;

						J.z = cosf(Angle.y*DEG2RAD);
						J.x = sinf(Angle.y*DEG2RAD);
						J.y = 0.0f;
						I.z = -J.x;
						I.x = J.z;
						I.y = 0.0f;
						V = (V%K)*K + (V%J)*J;
						if (!(dist < 15.0f && fabsf( Angle.y - f_TargetAngle ) >= 1.0f))
						{
							if (fabsf( Angle.y - f_TargetAngle ) >= 45.0f )
							{
								if (J % V > 0.0f && V.norm() > unit_manager.unit_type[type_id]->BrakeRate * dt )
									V = V - ((( fabsf( Angle.y - f_TargetAngle ) - 35.0f ) / 135.0f + 1.0f) * 0.5f * unit_manager.unit_type[type_id]->BrakeRate * dt) * J;
							}
							else
							{
								float speed = V.norm();
								float time_to_stop = speed / unit_manager.unit_type[type_id]->BrakeRate;
								float min_dist = time_to_stop * (speed-unit_manager.unit_type[type_id]->BrakeRate*0.5f*time_to_stop);
								if (min_dist>=dist && !(mission->flags & MISSION_FLAG_DONT_STOP_MOVE)
									&& ( mission->next == NULL || ( mission->next->mission != MISSION_MOVE && mission->next->mission != MISSION_PATROL ) ) )	// Brake if needed
									V = V-unit_manager.unit_type[type_id]->BrakeRate*dt*J;
								else if (speed < unit_manager.unit_type[type_id]->MaxVelocity )
									V = V + unit_manager.unit_type[type_id]->Acceleration*dt*J;
								else
									V = unit_manager.unit_type[type_id]->MaxVelocity / speed * V;
							}
						}
						else
						{
							float speed = V.norm();
							if (speed > unit_manager.unit_type[type_id]->MaxVelocity )
								V = unit_manager.unit_type[type_id]->MaxVelocity / speed * V;
						}
					}
				}

				NPos = Pos + dt*V;			// Check if the unit can go where V brings it
				if (was_locked ) // Random move to solve the unit lock problem
				{
					if (V.x > 0.0f)
						NPos.x += (Math::RandFromTable() % 101) * 0.01f;
					else
						NPos.x -= (Math::RandFromTable() % 101) * 0.01f;
					if (V.z > 0.0f)
						NPos.z += (Math::RandFromTable() % 101) * 0.01f;
					else
						NPos.z -= (Math::RandFromTable() % 101) * 0.01f;

					if (was_locked >= 5.0f)
					{
						was_locked = 5.0f;
						mission->flags |= MISSION_FLAG_REFRESH_PATH;			// Refresh path because this shouldn't happen unless
						// obstacles have moved
					}
				}
				n_px = ((int)(NPos.x)+map->map_w_d+4)>>3;
				n_py = ((int)(NPos.z)+map->map_h_d+4)>>3;
				precomputed_position = true;
				bool locked = false;
				if (!flying )
				{
					if (n_px != cur_px || n_py != cur_py) // has something changed ??
					{
						bool place_is_empty = can_be_there( n_px, n_py, map, type_id, owner_id, idx );
						if (!(flags & 64) && !place_is_empty)
						{
							if (!unit_manager.unit_type[type_id]->canfly)
							{
								locked = true;
								// Check some basic solutions first
								if (cur_px != n_px
									&& can_be_there( cur_px, n_py, map, type_id, owner_id, idx ))
								{
									V.z = V.z != 0.0f ? (V.z < 0.0f ? -sqrtf( SQUARE(V.z) + SQUARE(V.x) ) : sqrtf( SQUARE(V.z) + SQUARE(V.x) ) ) : 0.0f;
									V.x = 0.0f;
									NPos.x = Pos.x;
									n_px = cur_px;
								}
								else if (cur_py != n_py && can_be_there( n_px, cur_py, map, type_id, owner_id, idx ))
								{
									V.x = (V.x != 0.0)
										? ((V.x < 0.0f)
										   ? -sqrtf(SQUARE(V.z) + SQUARE(V.x))
										   : sqrtf(SQUARE(V.z) + SQUARE(V.x)))
										: 0.0f;
									V.z = 0.0f;
									NPos.z = Pos.z;
									n_py = cur_py;
								}
								else if (can_be_there( cur_px, cur_py, map, type_id, owner_id, idx )) {
									V.x = V.y = V.z = 0.0f;		// Don't move since we can't
									NPos = Pos;
									n_px = cur_px;
									n_py = cur_py;
									mission->flags |= MISSION_FLAG_MOVE;
									if (fabsf( Angle.y - f_TargetAngle ) <= 0.1f || !b_TargetAngle) // Don't prevent unit from rotating!!
									{
										if (mission->path)
											destroy_path(mission->path);
										mission->path = NULL;
									}
								}
								else
									LOG_WARNING("A Unit is blocked !" << __FILE__ << ":" << __LINE__);
							}
							else if (!flying && local )
							{
								if (Pos.x<-map->map_w_d || Pos.x>map->map_w_d || Pos.z<-map->map_h_d || Pos.z>map->map_h_d)
								{
									Vector3D target = Pos;
									if (target.x < -map->map_w_d+256)
										target.x = -map->map_w_d+256;
									else if (target.x > map->map_w_d-256)
										target.x = map->map_w_d-256;
									if (target.z < -map->map_h_d+256)
										target.z = -map->map_h_d+256;
									else if (target.z > map->map_h_d-256)
										target.z = map->map_h_d-256;
									next_mission();
									add_mission(MISSION_MOVE | MISSION_FLAG_AUTO,&target,true,0,NULL,NULL,0,1);		// Stay on map
								}
								else
									if (!can_be_there( cur_px, cur_py, map, type_id, owner_id, idx ))
									{
										NPos = Pos;
										n_px = cur_px;
										n_py = cur_py;
										Vector3D target = Pos;
										target.x += ((sint32)(Math::RandFromTable()&0x1F))-16;		// Look for a place to land
										target.z += ((sint32)(Math::RandFromTable()&0x1F))-16;
										mission->flags |= MISSION_FLAG_MOVE;
										if (mission->path )
											destroy_path(mission->path);
										mission->path = direct_path( target );
									}
							}
						}
						else if (!(flags & 64) && unit_manager.unit_type[type_id]->canfly && ( mission == NULL ||
																							   ( mission->mission != MISSION_MOVE && mission->mission != MISSION_ATTACK ) ) )
							flags |= 64;
					}
					else
					{
						bool place_is_empty = map->check_rect(n_px-(unit_manager.unit_type[type_id]->FootprintX>>1),n_py-(unit_manager.unit_type[type_id]->FootprintZ>>1),unit_manager.unit_type[type_id]->FootprintX,unit_manager.unit_type[type_id]->FootprintZ,idx);
						if (!place_is_empty)
						{
							pMutex.unlock();
							clear_from_map();
							pMutex.lock();
							LOG_WARNING("A Unit is blocked ! (probably spawned on something)" << __FILE__ << ":" << __LINE__);
						}
					}
				}
				if (locked)
					was_locked += dt;
				else
					was_locked = 0.0f;
			}
			else
			{
				was_moving = false;
				requesting_pathfinder = false;
			}

			if (flying && local) // Force planes to stay on map
			{
				if (Pos.x<-map->map_w_d || Pos.x>map->map_w_d || Pos.z<-map->map_h_d || Pos.z>map->map_h_d) {
					if (Pos.x < -map->map_w_d )
						V.x += dt * ( -map->map_w_d - Pos.x ) * 0.1f;
					else if (Pos.x > map->map_w_d )
						V.x -= dt * ( Pos.x - map->map_w_d ) * 0.1f;
					if (Pos.z < -map->map_h_d )
						V.z += dt * ( -map->map_h_d - Pos.z ) * 0.1f;
					else if (Pos.z > map->map_h_d )
						V.z -= dt * ( Pos.z - map->map_h_d ) * 0.1f;
					float speed = V.norm();
					if (speed > unit_manager.unit_type[type_id]->MaxVelocity && speed > 0.0f ) {
						V = unit_manager.unit_type[type_id]->MaxVelocity / speed * V;
						speed = unit_manager.unit_type[type_id]->MaxVelocity;
					}
					if (speed > 0.0f)
					{
						Angle.y = acosf( V.z / speed ) * RAD2DEG;
						if (V.x < 0.0f)
							Angle.y = -Angle.y;
					}
				}
			}

			//----------------------------------- End of moving code ------------------------------------

			switch(mission->mission)						// Commandes générales / General orders
			{
				case MISSION_WAIT:					// Wait for a specified time (campaign)
					mission->flags = 0;			// Don't move, do not shoot !! just wait
					if (mission->time >= mission->data * 0.001f )	// Done :)
						next_mission();
					break;
				case MISSION_WAIT_ATTACKED:			// Wait until a specified unit is attacked (campaign)
					if (mission->data < 0 || mission->data >= units.max_unit || !(units.unit[ mission->data ].flags & 1) )
						next_mission();
					else if (units.unit[ mission->data ].attacked )
						next_mission();
					break;
				case MISSION_GET_REPAIRED:
					if (mission->p && (((Unit*)mission->p)->flags & 1) && ((Unit*)mission->p)->ID == mission->target_ID ) {
						Unit *target_unit = (Unit*) mission->p;

						if (!(mission->flags & MISSION_FLAG_PAD_CHECKED) ) {
							mission->flags |= MISSION_FLAG_PAD_CHECKED;
							int param[] = { 0, 1 };
							target_unit->runScriptFunction( SCRIPT_QueryLandingPad, 2, param );
							mission->data = param[ 0 ];
						}

						target_unit->compute_model_coord();
						int piece_id = mission->data >= 0 ? mission->data : (-mission->data - 1);
						mission->target = target_unit->Pos + target_unit->data.pos[ piece_id ];

						Vector3D Dir = mission->target - Pos;
						Dir.y = 0.0f;
						float dist = Dir.sq();
						int maxdist = 6;
						if (dist > maxdist * maxdist && unit_manager.unit_type[type_id]->BMcode ) {	// Si l'unité est trop loin du chantier
							mission->flags &= ~MISSION_FLAG_BEING_REPAIRED;
							c_time = 0.0f;
							mission->flags |= MISSION_FLAG_MOVE;
							mission->move_data = maxdist*8/80;
							if (mission->path )
								mission->path->Pos = mission->target;			// Update path in real time!
						}
						else if (!(mission->flags & MISSION_FLAG_MOVE) ) {
							b_TargetAngle = true;
							f_TargetAngle = target_unit->Angle.y;
							if (mission->data >= 0 ) {
								mission->flags |= MISSION_FLAG_BEING_REPAIRED;
								Dir = mission->target - Pos;
								Pos = Pos + 3.0f * dt * Dir;
								Pos.x = mission->target.x;
								Pos.z = mission->target.z;
								if (Dir.sq() < 3.0f ) {
									target_unit->lock();
									if (target_unit->pad1 != 0xFFFF && target_unit->pad2 != 0xFFFF ) {		// We can't land here
										target_unit->unlock();
										next_mission();
										if (mission && mission->mission == MISSION_STOP )		// Don't stop we were patroling
											next_mission();
										break;
									}
									if (target_unit->pad1 == 0xFFFF )			// tell others we're here
										target_unit->pad1 = piece_id;
									else target_unit->pad2 = piece_id;
									target_unit->unlock();
									mission->data = -mission->data - 1;
								}
							}
							else {						// being repaired
								Pos = mission->target;
								V.x = V.y = V.z = 0.0f;

								if (target_unit->port[ ACTIVATION ])
								{
									float conso_energy=((float)(unit_manager.unit_type[target_unit->type_id]->WorkerTime * unit_manager.unit_type[type_id]->BuildCostEnergy)) / unit_manager.unit_type[type_id]->BuildTime;
									TA3D::players.requested_energy[owner_id] += conso_energy;
									if (players.energy[owner_id] >= (energy_cons + conso_energy * TA3D::players.energy_factor[owner_id]) * dt)
									{
										target_unit->lock();
										target_unit->energy_cons += conso_energy * TA3D::players.energy_factor[owner_id];
										target_unit->unlock();
										hp += dt * TA3D::players.energy_factor[owner_id] * unit_manager.unit_type[target_unit->type_id]->WorkerTime * unit_manager.unit_type[type_id]->MaxDamage / unit_manager.unit_type[type_id]->BuildTime;
									}
									if (hp >= unit_manager.unit_type[type_id]->MaxDamage) // Unit has been repaired
									{
										hp = unit_manager.unit_type[type_id]->MaxDamage;
										target_unit->lock();
										if (target_unit->pad1 == piece_id )			// tell others we've left
											target_unit->pad1 = 0xFFFF;
										else target_unit->pad2 = 0xFFFF;
										target_unit->unlock();
										next_mission();
										if (mission && mission->mission == MISSION_STOP )		// Don't stop we were patroling
											next_mission();
										break;
									}
									built=true;
								}
							}
						}
					}
					else
						next_mission();
					break;
				case MISSION_STANDBY_MINE:		// Don't even try to do something else, the unit must die !!
					if (self_destruct < 0.0f ) {
						int dx = ((unit_manager.unit_type[type_id]->SightDistance+(int)(h+0.5f))>>3) + 1;
						int enemy_idx=-1;
						int sx=Math::RandFromTable()&1;
						int sy=Math::RandFromTable()&1;
						// byte mask=1<<owner_id;
						for(int y=cur_py-dx+sy;y<=cur_py+dx;y+=2) {
							if (y>=0 && y<map->bloc_h_db-1)
								for(int x=cur_px-dx+sx;x<=cur_px+dx;x+=2)
									if (x>=0 && x<map->bloc_w_db-1 ) {
										int cur_idx = map->map_data[y][x].unit_idx;
										if (cur_idx>=0 && cur_idx<units.max_unit && (units.unit[cur_idx].flags & 1) && units.unit[cur_idx].owner_id != owner_id
											&& unit_manager.unit_type[units.unit[cur_idx].type_id]->ShootMe ) {		// This unit is on the sight_map since dx = sightdistance !!
											enemy_idx = cur_idx;
											break;
										}
									}
							if (enemy_idx>=0)	break;
							sx ^= 1;
						}
						if (enemy_idx >= 0 )					// Annihilate it !!!
							toggle_self_destruct();
					}
					break;
				case MISSION_UNLOAD:
					if (nb_attached > 0 )
					{
						Vector3D Dir = mission->target-Pos;
						Dir.y=0.0f;
						float dist = Dir.sq();
						int maxdist=0;
						if (unit_manager.unit_type[type_id]->TransMaxUnits==1)		// Code for units like the arm atlas
							maxdist=3;
						else
							maxdist=unit_manager.unit_type[type_id]->SightDistance;
						if (dist>maxdist*maxdist && unit_manager.unit_type[type_id]->BMcode ) {	// Si l'unité est trop loin du chantier
							c_time = 0.0f;
							mission->flags |= MISSION_FLAG_MOVE;
							mission->move_data = maxdist*8/80;
						}
						else if (!(mission->flags & MISSION_FLAG_MOVE) )
						{
							if (mission->last_d>=0.0f)
							{
								if (unit_manager.unit_type[type_id]->TransMaxUnits==1)// Code for units like the arm atlas
								{
									if (attached_list[0] >= 0 && attached_list[0] < units.max_unit				// Check we can do that
										&& units.unit[ attached_list[0] ].flags && can_be_built( Pos, map, units.unit[ attached_list[0] ].type_id, owner_id ) ) {
										launchScript(SCRIPT_EndTransport);

										Unit *target_unit = &(units.unit[ attached_list[0] ]);
										target_unit->attached = false;
										target_unit->hidden = false;
										nb_attached = 0;
										pMutex.unlock();
										target_unit->draw_on_map();
										pMutex.lock();
									}
									else if (attached_list[0] < 0 || attached_list[0] >= units.max_unit
											 || units.unit[ attached_list[0] ].flags == 0 )
										nb_attached = 0;

									next_mission();
								}
								else
								{
									if (attached_list[ nb_attached - 1 ] >= 0 && attached_list[ nb_attached - 1 ] < units.max_unit				// Check we can do that
										&& units.unit[ attached_list[ nb_attached - 1 ] ].flags && can_be_built( mission->target, map, units.unit[ attached_list[ nb_attached - 1 ] ].type_id, owner_id ) ) {
										int idx = attached_list[ nb_attached - 1 ];
										int param[]= { idx, PACKXZ( mission->target.x * 2.0f + map->map_w, mission->target.z * 2.0f + map->map_h ) };
										launchScript(SCRIPT_TransportDrop, 2, param);
									}
									else if (attached_list[ nb_attached - 1 ] < 0 || attached_list[ nb_attached - 1 ] >= units.max_unit
											 || units.unit[ attached_list[ nb_attached - 1 ] ].flags == 0 )
										nb_attached--;
								}
								mission->last_d=-1.0f;
							}
							else
							{
								//                                if (!is_running(get_script_index(SCRIPT_TransportDrop)) && port[ BUSY ] == 0.0f )
								if (port[ BUSY ] == 0)
									next_mission();
							}
						}
					}
					else
						next_mission();
					break;
				case MISSION_LOAD:
					if (mission->p!=NULL) {
						Unit *target_unit=(Unit*) mission->p;
						if (!(target_unit->flags & 1) || target_unit->ID != mission->target_ID ) {
							next_mission();
							break;
						}
						Vector3D Dir=target_unit->Pos-Pos;
						Dir.y=0.0f;
						mission->target=target_unit->Pos;
						float dist = Dir.sq();
						int maxdist=0;
						if (unit_manager.unit_type[type_id]->TransMaxUnits==1)		// Code for units like the arm atlas
							maxdist=3;
						else
							maxdist=unit_manager.unit_type[type_id]->SightDistance;
						if (dist>maxdist*maxdist && unit_manager.unit_type[type_id]->BMcode) {	// Si l'unité est trop loin du chantier
							c_time = 0.0f;
							mission->flags |= MISSION_FLAG_MOVE;
							mission->move_data = maxdist*8/80;
						}
						else if (!(mission->flags & MISSION_FLAG_MOVE))
						{
							if (mission->last_d>=0.0f)
							{
								if (unit_manager.unit_type[type_id]->TransMaxUnits==1)  		// Code for units like the arm atlas
								{
									if (nb_attached == 0)
									{
										//										int param[] = { (int)((Pos.y - target_unit->Pos.y - target_unit->model->top)*2.0f) << 16 };
										int param[] = { (int)((Pos.y - target_unit->Pos.y)*2.0f) << 16 };
										launchScript(SCRIPT_BeginTransport, 1, param);
										runScriptFunction( SCRIPT_QueryTransport, 1, param);
										target_unit->attached = true;
										link_list[nb_attached] = param[0];
										target_unit->hidden = param[0] < 0;
										attached_list[nb_attached++] = target_unit->idx;
										target_unit->clear_from_map();
									}
									next_mission();
								}
								else
								{
									if (nb_attached>=unit_manager.unit_type[type_id]->TransportCapacity)
									{
										next_mission();
										break;
									}
									int param[]= { target_unit->idx };
									launchScript(SCRIPT_TransportPickup, 1, param);
								}
								mission->last_d = -1.0f;
							}
							else
							{
								if (port[ BUSY ] == 0)
									next_mission();
							}
						}
					}
					else
						next_mission();
					break;
				case MISSION_CAPTURE:
				case MISSION_REVIVE:
				case MISSION_RECLAIM:
					if (mission->p != NULL)		// Récupère une unité / It's a unit
					{
						Unit *target_unit=(Unit*) mission->p;
						if ((target_unit->flags & 1) && target_unit->ID == mission->target_ID)
						{
							if (mission->mission == MISSION_CAPTURE)
							{
								if (unit_manager.unit_type[target_unit->type_id]->commander || target_unit->owner_id == owner_id)
								{
									playSound( "cant1" );
									next_mission();
									break;
								}
								if (!(mission->flags & MISSION_FLAG_TARGET_CHECKED))
								{
									mission->flags |= MISSION_FLAG_TARGET_CHECKED;
									mission->data = Math::Min(unit_manager.unit_type[target_unit->type_id]->BuildCostMetal * 100, 10000);
								}
							}
							Vector3D Dir=target_unit->Pos-Pos;
							Dir.y=0.0f;
							mission->target=target_unit->Pos;
							float dist=Dir.sq();
							int maxdist = mission->mission == MISSION_CAPTURE ? (int)(unit_manager.unit_type[type_id]->SightDistance) : (int)(unit_manager.unit_type[type_id]->BuildDistance);
							if (dist > maxdist * maxdist && unit_manager.unit_type[type_id]->BMcode)	// Si l'unité est trop loin du chantier
							{
								c_time=0.0f;
								mission->flags |= MISSION_FLAG_MOVE;// | MISSION_FLAG_REFRESH_PATH;
								mission->move_data = maxdist*7/80;
								mission->last_d = 0.0f;
							}
							else if (!(mission->flags & MISSION_FLAG_MOVE))
							{
								if (mission->last_d>=0.0f)
								{
									start_building(target_unit->Pos - Pos);
									mission->last_d=-1.0f;
								}

								if (unit_manager.unit_type[type_id]->BMcode && port[ INBUILDSTANCE ] != 0.0f)
								{
									if (local && network_manager.isConnected() && nanolathe_target < 0 ) {		// Synchronize nanolathe emission
										nanolathe_target = target_unit->idx;
										g_ta3d_network->sendUnitNanolatheEvent( idx, target_unit->idx, false, mission->mission == MISSION_RECLAIM );
									}

									playSound( "working" );
									// Récupère l'unité
									float recup = dt * 4.5f * unit_manager.unit_type[target_unit->type_id]->MaxDamage / unit_manager.unit_type[type_id]->WorkerTime;
									if (mission->mission == MISSION_CAPTURE)
									{
										mission->data -= (int)(dt * 1000.0f + 0.5f);
										if (mission->data <= 0 )			// Unit has been captured
										{
											pMutex.unlock();

											target_unit->clear_from_map();
											target_unit->lock();

											Unit *new_unit = (Unit*) create_unit( target_unit->type_id, owner_id, target_unit->Pos, map);
											if (new_unit ) {
												new_unit->lock();

												new_unit->Angle = target_unit->Angle;
												new_unit->hp = target_unit->hp;
												new_unit->build_percent_left = target_unit->build_percent_left;

												new_unit->unlock();
											}

											target_unit->flags = 0x14;
											target_unit->hp = 0.0f;
											target_unit->local = true;		// Force synchronization in networking mode

											target_unit->unlock();

											pMutex.lock();
											launchScript(SCRIPT_stopbuilding);
											launchScript(SCRIPT_stop);
											next_mission();
										}
									}
									else
									{
										if (recup > target_unit->hp)	recup = target_unit->hp;
										target_unit->hp -= recup;
										if (dt > 0.0f)
											metal_prod += recup * unit_manager.unit_type[target_unit->type_id]->BuildCostMetal / (dt * unit_manager.unit_type[target_unit->type_id]->MaxDamage);
										if (target_unit->hp <= 0.0f)		// Work done
										{
											launchScript(SCRIPT_stopbuilding);
											launchScript(SCRIPT_stop);
											target_unit->flags |= 0x10;			// This unit is being reclaimed it doesn't explode!
											next_mission();
										}
									}
								}
							}
						}
						else
							next_mission();
					}
					else if (mission->data>=0 && mission->data<features.max_features )	{	// Reclaim a feature/wreckage
						features.lock();
						if (features.feature[mission->data].type <= 0 )	{
							features.unlock();
							next_mission();
							break;
						}
						bool feature_locked = true;

						Vector3D Dir = features.feature[mission->data].Pos - Pos;
						Dir.y=0.0f;
						mission->target=features.feature[mission->data].Pos;
						float dist=Dir.sq();
						int maxdist = mission->mission == MISSION_REVIVE ? (int)(unit_manager.unit_type[type_id]->SightDistance) : (int)(unit_manager.unit_type[type_id]->BuildDistance);
						if (dist>maxdist*maxdist && unit_manager.unit_type[type_id]->BMcode) {	// If the unit is too far from its target
							c_time = 0.0f;
							mission->flags |= MISSION_FLAG_MOVE;// | MISSION_FLAG_REFRESH_PATH;
							mission->move_data = maxdist*7/80;
							mission->last_d=0.0f;
						}
						else if (!(mission->flags & MISSION_FLAG_MOVE))
						{
							if (mission->last_d>=0.0f)
							{
								start_building(features.feature[mission->data].Pos - Pos);
								mission->last_d = -1.0f;
							}
							if (unit_manager.unit_type[type_id]->BMcode && port[ INBUILDSTANCE ] != 0)
							{
								if (local && network_manager.isConnected() && nanolathe_target < 0)		// Synchronize nanolathe emission
								{
									nanolathe_target = mission->data;
									g_ta3d_network->sendUnitNanolatheEvent( idx, mission->data, true, true );
								}

								playSound( "working" );
								// Reclaim the object
								float recup=dt*unit_manager.unit_type[type_id]->WorkerTime;
								if (recup>features.feature[mission->data].hp)	recup=features.feature[mission->data].hp;
								features.feature[mission->data].hp -= recup;
								Feature *feature = feature_manager.getFeaturePointer(features.feature[mission->data].type);
								if (dt > 0.0f && mission->mission == MISSION_RECLAIM)
								{
									metal_prod += recup * feature->metal / (dt * feature->damage);
									energy_prod += recup * feature->energy / (dt * feature->damage);
								}
								if (features.feature[mission->data].hp <= 0.0f ) {		// Job done
									features.removeFeatureFromMap( mission->data );		// Remove the object from map

									if (mission->mission == MISSION_REVIVE
										&& !feature->name.empty() ) {			// Creates the corresponding unit
										bool success = false;
										String wreckage_name = feature->name;
										wreckage_name = wreckage_name.substr( 0, wreckage_name.length() - 5 );		// Remove the _dead/_heap suffix

										int wreckage_type_id = unit_manager.get_unit_index( wreckage_name.c_str() );
										Vector3D obj_pos = features.feature[mission->data].Pos;
										float obj_angle = features.feature[mission->data].angle;
										features.unlock();
										feature_locked = false;
										if (network_manager.isConnected() )
											g_ta3d_network->sendFeatureDeathEvent( mission->data );
										features.delete_feature(mission->data);			// Delete the object

										if (wreckage_type_id >= 0)
										{
											pMutex.unlock();
											Unit *unit_p = (Unit*) create_unit( wreckage_type_id, owner_id, obj_pos, map );

											if (unit_p)
											{
												unit_p->lock();

												unit_p->Angle.y = obj_angle;
												unit_p->hp = 0.01f;					// Need to be repaired :P
												unit_p->build_percent_left = 0.0f;	// It's finished ...
												unit_p->unlock();
												unit_p->draw_on_map();
											}
											pMutex.lock();

											if (unit_p)
											{
												mission->mission = MISSION_REPAIR;		// Now let's repair what we've resurrected
												mission->p = unit_p;
												mission->data = 1;
												success = true;
											}
										}
										if (!success)
										{
											playSound("cant1");
											launchScript(SCRIPT_stopbuilding);
											launchScript(SCRIPT_stop);
											next_mission();
										}
									}
									else
									{
										features.unlock();
										feature_locked = false;
										if (network_manager.isConnected())
											g_ta3d_network->sendFeatureDeathEvent( mission->data );
										features.delete_feature(mission->data);			// Delete the object
										launchScript(SCRIPT_stopbuilding);
										launchScript(SCRIPT_stop);
										next_mission();
									}
								}
							}
						}
						if (feature_locked )
							features.unlock();
					}
					else
						next_mission();
					break;
				case MISSION_GUARD:
					if (jump_commands)	break;
					if (mission->p!=NULL && (((Unit*)mission->p)->flags & 1) && ((Unit*)mission->p)->owner_id==owner_id && ((Unit*)mission->p)->ID == mission->target_ID) {		// On ne défend pas n'importe quoi
						if (unit_manager.unit_type[type_id]->Builder)
						{
							if (((Unit*)mission->p)->build_percent_left > 0.0f || ((Unit*)mission->p)->hp<unit_manager.unit_type[((Unit*)mission->p)->type_id]->MaxDamage) // Répare l'unité
							{
								add_mission(MISSION_REPAIR | MISSION_FLAG_AUTO,&((Unit*)mission->p)->Pos,true,0,((Unit*)mission->p),NULL);
								break;
							}
							else
								if (((Unit*)mission->p)->mission!=NULL && (((Unit*)mission->p)->mission->mission==MISSION_BUILD_2 || ((Unit*)mission->p)->mission->mission==MISSION_REPAIR)) // L'aide à construire
								{
									add_mission(MISSION_REPAIR | MISSION_FLAG_AUTO,&((Unit*)mission->p)->mission->target,true,0,((Unit*)mission->p)->mission->p,NULL);
									break;
								}
						}
						if (unit_manager.unit_type[type_id]->canattack)
						{
							if (((Unit*)mission->p)->mission!=NULL && ((Unit*)mission->p)->mission->mission==MISSION_ATTACK) // L'aide à attaquer
							{
								add_mission(MISSION_ATTACK | MISSION_FLAG_AUTO,&((Unit*)mission->p)->mission->target,true,0,((Unit*)mission->p)->mission->p,NULL);
								break;
							}
						}
						if (((Vector3D)(Pos-((Unit*)mission->p)->Pos)).sq()>=25600.0f) // On reste assez près
						{
							mission->flags |= MISSION_FLAG_MOVE;// | MISSION_FLAG_REFRESH_PATH;
							mission->move_data = 10;
							mission->target = ((Unit*)mission->p)->Pos;
							c_time=0.0f;
							break;
						}
					}
					else
						next_mission();
					break;
				case MISSION_PATROL:					// Mode patrouille
					{
						pad_timer += dt;

						if (mission->next == NULL )
							add_mission(MISSION_PATROL | MISSION_FLAG_AUTO,&Pos,false,0,NULL,NULL,MISSION_FLAG_CAN_ATTACK,0,0);	// Retour à la case départ après l'éxécution de tous les ordres / back to beginning

						mission->flags |= MISSION_FLAG_CAN_ATTACK;
						if (unit_manager.unit_type[type_id]->canfly ) // Don't stop moving and check if it can be repaired
						{
							mission->flags |= MISSION_FLAG_DONT_STOP_MOVE;

							if (hp < unit_manager.unit_type[type_id]->MaxDamage && !attacked && pad_timer >= 5.0f ) // Check if a repair pad is free
							{
								bool attacking = false;
								for (int i = 0 ; i < 3; ++i)
								{
									if (weapon[i].state != WEAPON_FLAG_IDLE )
									{
										attacking = true;
										break;
									}
								}
								if (!attacking )
								{
									pad_timer = 0.0f;
									bool going_to_repair_pad = false;
									pMutex.unlock();
									units.lock();
									for (std::list<uint16>::iterator i = units.repair_pads[owner_id].begin(); i != units.repair_pads[owner_id].end(); ++i)
									{
										units.unit[ *i ].lock();
										Vector3D Dir = units.unit[ *i ].Pos - Pos;
										Dir.y = 0.0f;
										if ((units.unit[ *i ].pad1 == 0xFFFF || units.unit[ *i ].pad2 == 0xFFFF) && units.unit[ *i ].build_percent_left == 0.0f
											&& Dir.sq() <= SQUARE(unit_manager.unit_type[type_id]->ManeuverLeashLength)) // He can repair us :)
											{
												int target_idx = *i;
												units.unit[ target_idx ].unlock();
												pMutex.lock();
												add_mission( MISSION_GET_REPAIRED | MISSION_FLAG_AUTO, &units.unit[ *i ].Pos, true, 0, &(units.unit[ *i ]),NULL);
												pMutex.unlock();
												units.repair_pads[ owner_id ].erase(i);
												units.repair_pads[ owner_id ].push_back( target_idx );		// So we don't try it before others :)
												going_to_repair_pad = true;
												break;
											}
										units.unit[ *i ].unlock();
									}
									units.unlock();
									pMutex.lock();
									if (going_to_repair_pad )
										break;
								}
							}
						}

						if ((mission->flags & MISSION_FLAG_MOVE) == 0 ) // Monitor the moving process
						{
							if (!unit_manager.unit_type[type_id]->canfly || ( mission->next == NULL || ( mission->next != NULL && mission->mission != MISSION_PATROL ) ) )
							{
								V.x = V.y = V.z = 0.0f;			// Stop the unit
								if (precomputed_position )
								{
									NPos = Pos;
									n_px = cur_px;
									n_py = cur_py;
								}
							}

							Mission* cur = mission;					// Make a copy of current list to make it loop 8)
							while (cur->next)
								cur = cur->next;
							cur->next = new Mission();
							*(cur->next) = *mission;
							cur->next->path = NULL;
							cur->next->next = NULL;
							cur->next->flags |= MISSION_FLAG_MOVE;

							Mission *cur_start = mission->next;
							while( cur_start != NULL && cur_start->mission != MISSION_PATROL )
							{
								cur = cur_start;
								while (cur->next)
									cur = cur->next;
								cur->next = new Mission();
								*(cur->next) = *cur_start;
								cur->next->path = NULL;
								cur->next->next = NULL;
								cur_start = cur_start->next;
							}

							next_mission();
						}
					}
					break;
				case MISSION_STANDBY:
				case MISSION_VTOL_STANDBY:
					if (jump_commands)	break;
					if (mission->data>5)
					{
						if (mission->next)		// If there is a mission after this one
						{
							next_mission();
							if (mission && (mission->mission == MISSION_STANDBY || mission->mission == MISSION_VTOL_STANDBY))
								mission->data = 0;
						}
					}
					else
						mission->data++;
					break;
				case MISSION_ATTACK:										// Attaque une unité / attack a unit
					{
						Unit *target_unit = (mission->flags & MISSION_FLAG_TARGET_WEAPON) == MISSION_FLAG_TARGET_WEAPON ? NULL : (Unit*) mission->p;
						WEAPON *target_weapon = (mission->flags & MISSION_FLAG_TARGET_WEAPON) == MISSION_FLAG_TARGET_WEAPON ? (WEAPON*) mission->p : NULL;
						if ((target_unit!=NULL && (target_unit->flags&1) && target_unit->ID == mission->target_ID)
							|| (target_weapon!=NULL && target_weapon->weapon_id!=-1)
							|| (target_weapon==NULL && target_unit==NULL))
						{
							if (target_unit)				// Check if we can target the unit
							{
								byte mask = 1 << owner_id;
								if (target_unit->cloaked && !target_unit->is_on_radar( mask ))
								{
									for( int i = 0 ; i < weapon.size() ; i++ )
										if (weapon[ i ].target == mission->p)		// Stop shooting
											weapon[ i ].state = WEAPON_FLAG_IDLE;
									next_mission();
									break;
								}
							}

							if (jump_commands && mission->data != 0
								&& unit_manager.unit_type[type_id]->attackrunlength == 0 )	break;					// Just do basic checks every tick, and advanced ones when needed

							if (weapon.size() == 0
								&& !unit_manager.unit_type[type_id]->kamikaze)		// Check if this units has weapons
							{
								next_mission();
								break;
							}

							Vector3D Dir = target_unit==NULL ? (target_weapon == NULL ? mission->target-Pos : target_weapon->Pos-Pos) : target_unit->Pos-Pos;
							Dir.y = 0.0f;
							if (target_weapon || target_unit)
								mission->target = target_unit==NULL ? target_weapon->Pos : target_unit->Pos;
							float dist = Dir.sq();
							int maxdist = 0;
							int mindist = 0xFFFFF;

							//                            if (target_unit != NULL && unit_manager.unit_type[target_unit->type_id]->checkCategory( unit_manager.unit_type[type_id]->BadTargetCategory ))
							if (target_unit != NULL && unit_manager.unit_type[target_unit->type_id]->checkCategory( unit_manager.unit_type[type_id]->NoChaseCategory ))
							{
								next_mission();
								break;
							}

							for (int i = 0 ; i < weapon.size() ; i++)
							{
								if (unit_manager.unit_type[type_id]->weapon[ i ]==NULL || unit_manager.unit_type[type_id]->weapon[ i ]->interceptor)	continue;
								int cur_mindist;
								int cur_maxdist;
								bool allowed_to_fire = true;
								if (unit_manager.unit_type[type_id]->attackrunlength>0)
								{
									if (Dir % V < 0.0f )	allowed_to_fire = false;
									float t = 2.0f/map->ota_data.gravity*fabsf(Pos.y-mission->target.y);
									cur_mindist = (int)sqrtf(t*V.sq())-((unit_manager.unit_type[type_id]->attackrunlength+1)>>1);
									cur_maxdist = cur_mindist+(unit_manager.unit_type[type_id]->attackrunlength);
								}
								else if (unit_manager.unit_type[type_id]->weapon[ i ]->waterweapon && Pos.y > units.map->sealvl)
								{
									if (Dir % V < 0.0f )	allowed_to_fire = false;
									float t = 2.0f/map->ota_data.gravity*fabsf(Pos.y-mission->target.y);
									cur_maxdist = (int)sqrtf(t*V.sq()) + (unit_manager.unit_type[type_id]->weapon[ i ]->range>>1);
									cur_mindist = 0;
								}
								else
								{
									cur_maxdist = unit_manager.unit_type[type_id]->weapon[ i ]->range>>1;
									cur_mindist = 0;
								}
								if (maxdist < cur_maxdist)	maxdist = cur_maxdist;
								if (mindist > cur_mindist)	mindist = cur_mindist;
								if (allowed_to_fire && dist >= cur_mindist * cur_mindist && dist <= cur_maxdist * cur_maxdist && !unit_manager.unit_type[type_id]->weapon[ i ]->interceptor)
								{
									//                                    if (( (weapon[i].state & 3) == WEAPON_FLAG_IDLE || ( (weapon[i].state & 3) != WEAPON_FLAG_IDLE && weapon[i].target != mission->p ) )
									//                                        && ( target_unit == NULL || ( (!unit_manager.unit_type[type_id]->weapon[ i ]->toairweapon
									//                                                                       || ( unit_manager.unit_type[type_id]->weapon[ i ]->toairweapon && target_unit->flying ) )
									//                                                                      && !unit_manager.unit_type[target_unit->type_id]->checkCategory( unit_manager.unit_type[type_id]->w_badTargetCategory[i] ) ) )
									//                                        && ( ((mission->flags & MISSION_FLAG_COMMAND_FIRE) && (unit_manager.unit_type[type_id]->weapon[ i ]->commandfire || !unit_manager.unit_type[type_id]->candgun) )
									//                                             || (!(mission->flags & MISSION_FLAG_COMMAND_FIRE) && !unit_manager.unit_type[type_id]->weapon[ i ]->commandfire)
									//                                             || unit_manager.unit_type[type_id]->weapon[ i ]->dropped ) )
									if (( (weapon[i].state & 3) == WEAPON_FLAG_IDLE || ( (weapon[i].state & 3) != WEAPON_FLAG_IDLE && weapon[i].target != mission->p ) )
										&& ( target_unit == NULL || ( (!unit_manager.unit_type[type_id]->weapon[ i ]->toairweapon
																	   || ( unit_manager.unit_type[type_id]->weapon[ i ]->toairweapon && target_unit->flying ) )
																	  && !unit_manager.unit_type[target_unit->type_id]->checkCategory( unit_manager.unit_type[type_id]->NoChaseCategory ) ) )
										&& ( ((mission->flags & MISSION_FLAG_COMMAND_FIRE) && (unit_manager.unit_type[type_id]->weapon[ i ]->commandfire || !unit_manager.unit_type[type_id]->candgun) )
											 || (!(mission->flags & MISSION_FLAG_COMMAND_FIRE) && !unit_manager.unit_type[type_id]->weapon[ i ]->commandfire)
											 || unit_manager.unit_type[type_id]->weapon[ i ]->dropped ) )
									{
										weapon[i].state = WEAPON_FLAG_AIM;
										weapon[i].target = mission->p;
										weapon[i].target_pos = mission->target;
										weapon[i].data = -1;
										if (mission->flags & MISSION_FLAG_TARGET_WEAPON)
											weapon[i].state |= WEAPON_FLAG_WEAPON;
										if (unit_manager.unit_type[type_id]->weapon[ i ]->commandfire)
											weapon[i].state |= WEAPON_FLAG_COMMAND_FIRE;
									}
								}
							}

							if (unit_manager.unit_type[type_id]->kamikaze && unit_manager.unit_type[type_id]->kamikazedistance > maxdist)
								maxdist = unit_manager.unit_type[type_id]->kamikazedistance;

							if (mindist > maxdist)	mindist = maxdist;

							mission->flags |= MISSION_FLAG_CAN_ATTACK;

							if (unit_manager.unit_type[type_id]->kamikaze				// Kamikaze attack !!
								&& dist <= unit_manager.unit_type[type_id]->kamikazedistance * unit_manager.unit_type[type_id]->kamikazedistance
								&& self_destruct < 0.0f)
								self_destruct = 0.01f;

							if (dist>maxdist*maxdist || dist<mindist*mindist)	// Si l'unité est trop loin de sa cible / if unit isn't where it should be
							{
								if (!unit_manager.unit_type[type_id]->canmove)		// Bah là si on peut pas bouger faut changer de cible!! / need to change target
								{
									next_mission();
									break;
								}
								else if (!unit_manager.unit_type[type_id]->canfly || unit_manager.unit_type[type_id]->hoverattack)
								{
									c_time=0.0f;
									mission->flags |= MISSION_FLAG_MOVE;
									mission->move_data = maxdist*7/80;
								}
							}
							else if (mission->data == 0)
							{
								mission->data = 2;
								int param[] = { 0 };
								for( int i = 0 ; i < weapon.size() ; i++ )
									if (unit_manager.unit_type[type_id]->weapon[ i ])
										param[ 0 ] = Math::Max(param[0], (int)( unit_manager.unit_type[type_id]->weapon[i]->reloadtime * 1000.0f) * Math::Max(1, (int)unit_manager.unit_type[type_id]->weapon[i]->burst));
								launchScript(SCRIPT_SetMaxReloadTime, 1, param);
							}

							if (mission->flags & MISSION_FLAG_COMMAND_FIRED)
								next_mission();
						}
						else
							next_mission();
					}
					break;
				case MISSION_GUARD_NOMOVE:
					mission->flags |= MISSION_FLAG_CAN_ATTACK;
					mission->flags &= ~MISSION_FLAG_MOVE;
					if (mission->next)
						next_mission();
					break;
				case MISSION_STOP:											// Arrête tout ce qui était en cours / stop everything running
					while (mission->next
						   && (mission->mission == MISSION_STOP
							   || mission->mission == MISSION_STANDBY
							   || mission->mission == MISSION_VTOL_STANDBY)
						   && (mission->next->mission == MISSION_STOP
							   || mission->next->mission == MISSION_STANDBY
							   || mission->next->mission == MISSION_VTOL_STANDBY))     // Don't make a big stop stack :P
						next_mission();
					if (mission->mission != MISSION_STOP
						&& mission->mission != MISSION_STANDBY
						&& mission->mission != MISSION_VTOL_STANDBY)
						break;
					mission->mission = MISSION_STOP;
					if (jump_commands && mission->data != 0 )	break;
					if (mission->data>5)
					{
						if (mission->next)
						{
							next_mission();
							if (mission!=NULL && mission->mission==MISSION_STOP)		// Mode attente / wait mode
								mission->data=1;
						}
					}
					else
					{
						if (mission->data==0)
						{
							launchScript(SCRIPT_StopMoving);		// Arrête tout / stop everything
							launchScript(SCRIPT_stopbuilding);
							for( int i = 0 ; i < weapon.size() ; i++ )
								if (weapon[i].state)
								{
									launchScript(SCRIPT_TargetCleared);
									break;
								}
							for( int i = 0 ; i < weapon.size() ; i++ )			// Stop weapons
							{
								weapon[i].state = WEAPON_FLAG_IDLE;
								weapon[i].data = -1;
							}
						}
						mission->data++;
					}
					break;
				case MISSION_REPAIR:
					{
						Unit *target_unit=(Unit*) mission->p;
						if (target_unit!=NULL && (target_unit->flags & 1) && target_unit->build_percent_left == 0.0f && target_unit->ID == mission->target_ID)
						{
							if (target_unit->hp>=unit_manager.unit_type[target_unit->type_id]->MaxDamage || !unit_manager.unit_type[type_id]->BMcode)
							{
								if (unit_manager.unit_type[type_id]->BMcode)
									target_unit->hp=unit_manager.unit_type[target_unit->type_id]->MaxDamage;
								next_mission();
							}
							else
							{
								Vector3D Dir = target_unit->Pos - Pos;
								Dir.y=0.0f;
								mission->target=target_unit->Pos;
								float dist=Dir.sq();
								int maxdist=(int)(unit_manager.unit_type[type_id]->BuildDistance
												  + ( (unit_manager.unit_type[target_unit->type_id]->FootprintX + unit_manager.unit_type[target_unit->type_id]->FootprintZ) << 1 ) );
								if (dist>maxdist*maxdist && unit_manager.unit_type[type_id]->BMcode)	// Si l'unité est trop loin du chantier
								{
									mission->flags |= MISSION_FLAG_MOVE;
									mission->move_data = maxdist * 7 / 80;
									mission->data = 0;
									c_time = 0.0f;
								}
								else if (!(mission->flags & MISSION_FLAG_MOVE))
								{
									if (mission->data==0)
									{
										mission->data = 1;
										start_building(target_unit->Pos - Pos);
									}

									if (port[ INBUILDSTANCE ] != 0.0f)
									{
										if (local && network_manager.isConnected() && nanolathe_target < 0 )		// Synchronize nanolathe emission
										{
											nanolathe_target = target_unit->idx;
											g_ta3d_network->sendUnitNanolatheEvent( idx, target_unit->idx, false, false );
										}

										float conso_energy=((float)(unit_manager.unit_type[type_id]->WorkerTime*unit_manager.unit_type[target_unit->type_id]->BuildCostEnergy))/unit_manager.unit_type[target_unit->type_id]->BuildTime;
										TA3D::players.requested_energy[owner_id] += conso_energy;
										if (players.energy[owner_id] >= (energy_cons + conso_energy * TA3D::players.energy_factor[owner_id]) * dt)
										{
											energy_cons += conso_energy * TA3D::players.energy_factor[owner_id];
											target_unit->hp += dt * TA3D::players.energy_factor[owner_id] * unit_manager.unit_type[type_id]->WorkerTime*unit_manager.unit_type[target_unit->type_id]->MaxDamage/unit_manager.unit_type[target_unit->type_id]->BuildTime;
										}
										target_unit->built=true;
									}
								}
							}
						}
						else if (target_unit!=NULL && target_unit->flags)
						{
							Vector3D Dir = target_unit->Pos - Pos;
							Dir.y=0.0f;
							mission->target=target_unit->Pos;
							float dist=Dir.sq();
							int maxdist=(int)(unit_manager.unit_type[type_id]->BuildDistance
											  + ( (unit_manager.unit_type[target_unit->type_id]->FootprintX + unit_manager.unit_type[target_unit->type_id]->FootprintZ) << 1 ));
							if (dist>maxdist*maxdist && unit_manager.unit_type[type_id]->BMcode)	// Si l'unité est trop loin du chantier
							{
								c_time=0.0f;
								mission->flags |= MISSION_FLAG_MOVE;
								mission->move_data = maxdist*7/80;
							}
							else if (!(mission->flags & MISSION_FLAG_MOVE))
							{
								if (unit_manager.unit_type[type_id]->BMcode)
								{
									start_building(target_unit->Pos - Pos);
									mission->mission = MISSION_BUILD_2;		// Change de type de mission
								}
							}
						}
					}
					break;
				case MISSION_BUILD_2:
					{
						Unit *target_unit=(Unit*) mission->p;
						if (target_unit->flags && target_unit->ID == mission->target_ID)
						{
							target_unit->lock();
							if (target_unit->build_percent_left <= 0.0f)
							{
								target_unit->build_percent_left = 0.0f;
								if (unit_manager.unit_type[target_unit->type_id]->ActivateWhenBuilt)		// Start activated
								{
									target_unit->port[ ACTIVATION ] = 0;
									target_unit->activate();
								}
								if (unit_manager.unit_type[target_unit->type_id]->init_cloaked )				// Start cloaked
									target_unit->cloaking = true;
								if (!unit_manager.unit_type[type_id]->BMcode) // Ordre de se déplacer
								{
									Vector3D target=Pos;
									target.z+=128.0f;
									if (def_mission == NULL)
										target_unit->set_mission(MISSION_MOVE | MISSION_FLAG_AUTO,&target,false,5,true,NULL,NULL,0,5);		// Fait sortir l'unité du bâtiment
									else
									{
										target_unit->mission = new Mission();
										Mission *target_mission = target_unit->mission;
										*target_mission = *def_mission;
										target_mission->next = NULL;
										target_mission->path = NULL;
										while (target_mission->next != NULL)
											target_mission = target_mission->next;
										Mission *cur = def_mission->next;
										while (cur)// Copy mission list
										{
											target_mission->next = new Mission();
											target_mission = target_mission->next;
											*target_mission = *cur;
											target_mission->next = NULL;
											target_mission->path = NULL;
											cur = cur->next;
										}
									}
								}
								mission->p=NULL;
								next_mission();
							}
							else if (port[ INBUILDSTANCE ] != 0)
							{
								if (local && network_manager.isConnected() && nanolathe_target < 0) // Synchronize nanolathe emission
								{
									nanolathe_target = target_unit->idx;
									g_ta3d_network->sendUnitNanolatheEvent( idx, target_unit->idx, false, false );
								}

								float conso_metal=((float)(unit_manager.unit_type[type_id]->WorkerTime*unit_manager.unit_type[target_unit->type_id]->BuildCostMetal))/unit_manager.unit_type[target_unit->type_id]->BuildTime;
								float conso_energy=((float)(unit_manager.unit_type[type_id]->WorkerTime*unit_manager.unit_type[target_unit->type_id]->BuildCostEnergy))/unit_manager.unit_type[target_unit->type_id]->BuildTime;

								TA3D::players.requested_energy[owner_id] += conso_energy;
								TA3D::players.requested_metal[owner_id] += conso_metal;

								if (players.metal[owner_id]>= (metal_cons + conso_metal * resource_min_factor) * dt
									&& players.energy[owner_id]>= (energy_cons + conso_energy * resource_min_factor) * dt)
								{
									metal_cons+=conso_metal * resource_min_factor;
									energy_cons+=conso_energy * resource_min_factor;
									target_unit->build_percent_left-=dt*resource_min_factor*unit_manager.unit_type[type_id]->WorkerTime*100.0f/unit_manager.unit_type[target_unit->type_id]->BuildTime;
									target_unit->hp+=dt*resource_min_factor*unit_manager.unit_type[type_id]->WorkerTime*unit_manager.unit_type[target_unit->type_id]->MaxDamage/unit_manager.unit_type[target_unit->type_id]->BuildTime;
								}
								if (!unit_manager.unit_type[type_id]->BMcode)
								{
									int buildinfo = runScriptFunction(SCRIPT_QueryBuildInfo);
									if (buildinfo >= 0)
									{
										compute_model_coord();
										Vector3D old_pos = target_unit->Pos;
										target_unit->Pos = Pos + data.pos[buildinfo];
										if (unit_manager.unit_type[target_unit->type_id]->Floater || ( unit_manager.unit_type[target_unit->type_id]->canhover && old_pos.y <= map->sealvl ) )
											target_unit->Pos.y = old_pos.y;
										if (((Vector3D)(old_pos-target_unit->Pos)).sq() > 1000000.0f) // It must be continuous
										{
											target_unit->Pos.x = old_pos.x;
											target_unit->Pos.z = old_pos.z;
										}
										else
										{
											target_unit->cur_px = ((int)(target_unit->Pos.x)+map->map_w_d+4)>>3;
											target_unit->cur_py = ((int)(target_unit->Pos.z)+map->map_h_d+4)>>3;
										}
										target_unit->Angle = Angle;
										target_unit->Angle.y += data.axe[1][buildinfo].angle;
										pMutex.unlock();
										target_unit->draw_on_map();
										pMutex.lock();
									}
								}
								mission->target = target_unit->Pos;
								target_unit->built=true;
							}
							else
							{
								activate();
								target_unit->built=true;
							}
							target_unit->unlock();
						}
						else
							next_mission();
					}
					break;
				case MISSION_BUILD:
					if (mission->p)
					{
						start_building( mission->target - Pos );
						mission->mission = MISSION_BUILD_2;		// Change mission type
						((Unit*)(mission->p))->built = true;
					}
					else
					{
						Vector3D Dir = mission->target - Pos;
						Dir.y = 0.0f;
						float dist = Dir.sq();
						int maxdist = (int)(unit_manager.unit_type[type_id]->BuildDistance
											+ ( (unit_manager.unit_type[mission->data]->FootprintX + unit_manager.unit_type[mission->data]->FootprintZ) << 1 ) );
						if (dist>maxdist*maxdist && unit_manager.unit_type[type_id]->BMcode)	// Si l'unité est trop loin du chantier
						{
							mission->flags |= MISSION_FLAG_MOVE;
							mission->move_data = maxdist * 7 / 80;
						}
						else
						{
							if (mission->flags & MISSION_FLAG_MOVE) // Stop moving if needed
								stopMoving();
							if (unit_manager.unit_type[type_id]->BMcode || (!unit_manager.unit_type[type_id]->BMcode && port[ INBUILDSTANCE ] && port[YARD_OPEN] && !port[BUGGER_OFF]))
							{
								/*								pMutex.unlock();
																draw_on_map();
																pMutex.lock();*/
								V.x = 0.0f;
								V.y = 0.0f;
								V.z = 0.0f;
								if (!unit_manager.unit_type[type_id]->BMcode)
								{
									int buildinfo = runScriptFunction(SCRIPT_QueryBuildInfo);
									if (buildinfo >= 0)
									{
										compute_model_coord();
										mission->target = Pos + data.pos[ buildinfo ];
									}
								}
								if (map->check_rect((((int)(mission->target.x)+map->map_w_d+4)>>3)-(unit_manager.unit_type[mission->data]->FootprintX>>1),(((int)(mission->target.z)+map->map_h_d+4)>>3)-(unit_manager.unit_type[mission->data]->FootprintZ>>1),unit_manager.unit_type[mission->data]->FootprintX,unit_manager.unit_type[mission->data]->FootprintZ,-1)) // Check it we have an empty place to build our unit
								{
									pMutex.unlock();
									mission->p = create_unit(mission->data,owner_id,mission->target,map);
									pMutex.lock();
									if (mission->p)
									{
										mission->target_ID = ((Unit*)mission->p)->ID;
										((Unit*)(mission->p))->hp = 0.000001f;
										((Unit*)(mission->p))->built = true;
									}
									//                                    else
									//                                        LOG_WARNING(idx << " can't create unit! (`" << __FILE__ << "`:" << __LINE__ << ")");
								}
								else if (unit_manager.unit_type[type_id]->BMcode)
									next_mission();
							}
							else
								activate();
						}
					}
					break;
			};

			switch(unit_manager.unit_type[type_id]->TEDclass)			// Commandes particulières
			{
				case CLASS_PLANT:
					switch(mission->mission)
					{
						case MISSION_STANDBY:
						case MISSION_BUILD:
						case MISSION_BUILD_2:
						case MISSION_REPAIR:
							break;
						default:
							next_mission();
					};
					break;
				case CLASS_WATER:
				case CLASS_VTOL:
				case CLASS_KBOT:
				case CLASS_COMMANDER:
				case CLASS_TANK:
				case CLASS_CNSTR:
				case CLASS_SHIP:
					{
						if (!(mission->flags & MISSION_FLAG_MOVE) && !(mission->flags & MISSION_FLAG_DONT_STOP_MOVE)
							&& ((mission->mission!=MISSION_ATTACK && unit_manager.unit_type[type_id]->canfly) || !unit_manager.unit_type[type_id]->canfly))
						{
							if (!flying )
								V.x = V.z = 0.0f;
							if (precomputed_position)
							{
								NPos = Pos;
								n_px = cur_px;
								n_py = cur_py;
							}
						}
						switch(mission->mission)
						{
							case MISSION_ATTACK:
							case MISSION_PATROL:
							case MISSION_REPAIR:
							case MISSION_BUILD:
							case MISSION_BUILD_2:
							case MISSION_GET_REPAIRED:
								if (unit_manager.unit_type[type_id]->canfly)
									activate();
								break;
							case MISSION_STANDBY:
								if (mission->next)
									next_mission();
								V.x = V.y = V.z = 0.0f;			// Frottements
								break;
							case MISSION_MOVE:
								mission->flags |= MISSION_FLAG_CAN_ATTACK;
								if (!(mission->flags & MISSION_FLAG_MOVE) )			// Monitor the moving process
								{
									if (mission->next
										&& (mission->next->mission == MISSION_MOVE
											|| (mission->next->mission == MISSION_STOP && mission->next->next && mission->next->next->mission == MISSION_MOVE) ) )
										mission->flags |= MISSION_FLAG_DONT_STOP_MOVE;

									if (!(mission->flags & MISSION_FLAG_DONT_STOP_MOVE) )			// If needed
										V.x = V.y = V.z = 0.0f;			// Stop the unit
									if (precomputed_position ) {
										NPos = Pos;
										n_px = cur_px;
										n_py = cur_py;
									}
									if ((mission->flags & MISSION_FLAG_DONT_STOP_MOVE) && mission->next && mission->next->mission == MISSION_STOP )			// If needed
										next_mission();
									next_mission();
								}
								break;
							default:
								if (unit_manager.unit_type[type_id]->canfly)
									deactivate();
						};
					}
					break;
				case CLASS_UNDEF:
				case CLASS_METAL:
				case CLASS_ENERGY:
				case CLASS_SPECIAL:
				case CLASS_FORT:
					break;
				default:
					LOG_WARNING("Unknown type :" << unit_manager.unit_type[type_id]->TEDclass);
			};

			switch(mission->mission)		// Pour le code post déplacement
			{
				case MISSION_ATTACK:
					//                    if (unit_manager.unit_type[type_id]->canfly && !unit_manager.unit_type[type_id]->hoverattack ) 			// Un avion??
					if (unit_manager.unit_type[type_id]->canfly)			// Un avion??
					{
						activate();
						mission->flags &= ~MISSION_FLAG_MOVE;			// We're doing it here, so no need to do it twice
						Vector3D J,I,K;
						K.x=K.z=0.0f;
						K.y=1.0f;
						Vector3D Target = mission->target;
						J = Target-Pos;
						J.y = 0.0f;
						float dist = J.norm();
						mission->last_d = dist;
						if (dist > 0.0f)
							J = 1.0f / dist * J;
						if (dist > unit_manager.unit_type[type_id]->ManeuverLeashLength * 0.5f)
						{
							b_TargetAngle = true;
							f_TargetAngle = acosf(J.z) * RAD2DEG;
							if (J.x < 0.0f) f_TargetAngle = -f_TargetAngle;
						}

						J.z = cosf(Angle.y * DEG2RAD);
						J.x = sinf(Angle.y * DEG2RAD);
						J.y = 0.0f;
						I.z = -J.x;
						I.x = J.z;
						I.y = 0.0f;
						V = (V%K)*K+(V%J)*J+units.exp_dt_4*(V%I)*I;
						float speed = V.sq();
						if (speed < unit_manager.unit_type[type_id]->MaxVelocity * unit_manager.unit_type[type_id]->MaxVelocity)
							V=V+unit_manager.unit_type[type_id]->Acceleration*dt*J;
					}
					break;
			}

			if (( (mission->flags & MISSION_FLAG_MOVE) || !local ) && !jump_commands )// Set unit orientation if it's on the ground
			{
				if (!unit_manager.unit_type[type_id]->canfly && !unit_manager.unit_type[type_id]->Upright
					&& !unit_manager.unit_type[type_id]->floatting()
					&& !( unit_manager.unit_type[type_id]->canhover && Pos.y <= map->sealvl ))
				{
					Vector3D I,J,K,A,B,C;
					Matrix M = RotateY((Angle.y+90.0f)*DEG2RAD);
					I.x = 4.0f;
					J.z = 4.0f;
					K.y = 1.0f;
					A = Pos - unit_manager.unit_type[type_id]->FootprintZ*I*M;
					B = Pos + (unit_manager.unit_type[type_id]->FootprintX*I-unit_manager.unit_type[type_id]->FootprintZ*J)*M;
					C = Pos + (unit_manager.unit_type[type_id]->FootprintX*I+unit_manager.unit_type[type_id]->FootprintZ*J)*M;
					A.y = map->get_unit_h(A.x,A.z);	// Projete le triangle
					B.y = map->get_unit_h(B.x,B.z);
					C.y = map->get_unit_h(C.x,C.z);
					Vector3D D=(B-A)*(B-C);
					if (D.y>=0.0f) // On ne met pas une unité à l'envers!!
					{
						D.unit();
						float dist_sq = sqrtf( D.y*D.y+D.z*D.z );
						float angle_1= dist_sq != 0.0f ? acosf( D.y / dist_sq )*RAD2DEG : 0.0f;
						if (D.z<0.0f)	angle_1=-angle_1;
						D=D*RotateX(-angle_1*DEG2RAD);
						float angle_2=VAngle(D,K)*RAD2DEG;
						if (D.x>0.0f)	angle_2=-angle_2;
						if (fabsf(angle_1-Angle.x)<=10.0f && fabsf(angle_2-Angle.z)<=10.0f)
						{
							Angle.x=angle_1;
							Angle.z=angle_2;
						}
					}
				}
				else if (!unit_manager.unit_type[type_id]->canfly)
					Angle.x = Angle.z = 0.0f;
			}

			bool returning_fire = ( port[ STANDINGFIREORDERS ] == SFORDER_RETURN_FIRE && attacked );
			if (( ((mission->flags & MISSION_FLAG_CAN_ATTACK) == MISSION_FLAG_CAN_ATTACK) || do_nothing() )
				&& ( port[ STANDINGFIREORDERS ] == SFORDER_FIRE_AT_WILL || returning_fire )
				&& !jump_commands && local)
			{
				// Si l'unité peut attaquer d'elle même les unités enemies proches, elle le fait / Attack nearby enemies

				bool can_fire = unit_manager.unit_type[type_id]->AutoFire && unit_manager.unit_type[type_id]->canattack;

				if (!can_fire)
				{
					for( int i = 0 ; i < weapon.size() && !can_fire ; i++ )
						can_fire =  unit_manager.unit_type[type_id]->weapon[i] != NULL && !unit_manager.unit_type[type_id]->weapon[i]->commandfire
							&& !unit_manager.unit_type[type_id]->weapon[i]->interceptor && weapon[i].state == WEAPON_FLAG_IDLE;
				}
				else
				{
					can_fire = false;
					for( int i = 0 ; i < weapon.size() && !can_fire ; i++ )
						can_fire =  unit_manager.unit_type[type_id]->weapon[i] != NULL && weapon[i].state == WEAPON_FLAG_IDLE;
				}

				if (can_fire)
				{
					int dx=(unit_manager.unit_type[type_id]->SightDistance+(int)(h+0.5f))>>3;
					int enemy_idx=-1;
					for( int i = 0 ; i < weapon.size() ; i++ )
						if (unit_manager.unit_type[type_id]->weapon[i] != NULL && (unit_manager.unit_type[type_id]->weapon[i]->range>>4)>dx
							&& !unit_manager.unit_type[type_id]->weapon[i]->interceptor && !unit_manager.unit_type[type_id]->weapon[i]->commandfire)
							dx=unit_manager.unit_type[type_id]->weapon[i]->range>>4;
					if (unit_manager.unit_type[type_id]->kamikaze && (unit_manager.unit_type[type_id]->kamikazedistance>>3) > dx )
						dx=unit_manager.unit_type[type_id]->kamikazedistance;

					int sx=Math::RandFromTable()&0xF;
					int sy=Math::RandFromTable()&0xF;
					byte mask=1<<owner_id;
					for(int y=cur_py-dx+sy;y<=cur_py+dx;y+=0x8)
					{
						if (y>=0 && y<map->bloc_h_db-1)
							for(int x=cur_px-dx+sx;x<=cur_px+dx;x+=0x8)
								if (x>=0 && x<map->bloc_w_db-1 )
								{
									bool land_test = true;
									IDX_LIST_NODE *cur = map->map_data[y][x].air_idx.head;
									for( ; land_test || cur != NULL ; )
									{
										int cur_idx;
										if (land_test)
										{
											cur_idx = map->map_data[y][x].unit_idx;
											land_test = false;
										}
										else
										{
											cur_idx = cur->idx;
											cur = cur->next;
										}
										if (isEnemy( cur_idx ) && units.unit[cur_idx].flags
											&& unit_manager.unit_type[units.unit[cur_idx].type_id]->ShootMe
											&& ( units.unit[cur_idx].is_on_radar( mask ) ||
												 ( (SurfaceByte(units.map->sight_map,x>>1,y>>1) & mask)
												   && !units.unit[cur_idx].cloaked ) )
											&& !unit_manager.unit_type[ units.unit[cur_idx].type_id ]->checkCategory( unit_manager.unit_type[type_id]->NoChaseCategory ) )
											//                                             && !unit_manager.unit_type[ units.unit[cur_idx].type_id ]->checkCategory( unit_manager.unit_type[type_id]->BadTargetCategory ) )
										{
											if (returning_fire)
											{
												bool breakBool = false;
												for(int i = 0 ; i < units.unit[cur_idx].weapon.size() ; i++)
													if( units.unit[cur_idx].weapon[i].state != WEAPON_FLAG_IDLE && units.unit[cur_idx].weapon[i].target == this )
													{
														enemy_idx = cur_idx;
														x = cur_px + dx;
														y = cur_py + dx;
														breakBool = true;
														break;
													}
												if (breakBool)  break;
											}
											else
											{
												enemy_idx = cur_idx;
												x = cur_px + dx;
												y = cur_py + dx;
												break;
											}
										}
									}
								}
						if (enemy_idx>=0)	break;
					}
					if (enemy_idx>=0)			// Si on a trouvé une unité, on l'attaque
					{
						if (do_nothing() )
							set_mission(MISSION_ATTACK | MISSION_FLAG_AUTO,&(units.unit[enemy_idx].Pos),false,0,true,&(units.unit[enemy_idx]),NULL);
						else
							for( int i = 0 ; i < weapon.size() ; i++ )
								if (weapon[i].state == WEAPON_FLAG_IDLE && unit_manager.unit_type[type_id]->weapon[ i ] != NULL
									&& !unit_manager.unit_type[type_id]->weapon[ i ]->commandfire
									&& !unit_manager.unit_type[type_id]->weapon[ i ]->interceptor
									&& (!unit_manager.unit_type[type_id]->weapon[ i ]->toairweapon
										|| ( unit_manager.unit_type[type_id]->weapon[ i ]->toairweapon && units.unit[enemy_idx].flying ) )
									&& !unit_manager.unit_type[ units.unit[enemy_idx].type_id ]->checkCategory( unit_manager.unit_type[type_id]->NoChaseCategory ) )
									//                                        && !unit_manager.unit_type[ units.unit[enemy_idx].type_id ]->checkCategory( unit_manager.unit_type[type_id]->w_badTargetCategory[i] ) ) )
								{
									weapon[i].state = WEAPON_FLAG_AIM;
									weapon[i].target = &(units.unit[enemy_idx]);
									weapon[i].data = -1;
								}
					}
				}
				if (weapon.size() > 0 && unit_manager.unit_type[type_id]->antiweapons && unit_manager.unit_type[type_id]->weapon[0])
				{
					float coverage=unit_manager.unit_type[type_id]->weapon[0]->coverage*unit_manager.unit_type[type_id]->weapon[0]->coverage;
					float range=unit_manager.unit_type[type_id]->weapon[0]->range*unit_manager.unit_type[type_id]->weapon[0]->range>>2;
					int enemy_idx=-1;
					byte e=0;
					for(byte i=0;i+e<mem_size;i++)
					{
						if (memory[i+e]<0 || memory[i+e]>=weapons.nb_weapon || weapons.weapon[memory[i+e]].weapon_id==-1)
						{
							e++;
							i--;
							continue;
						}
						memory[i] = memory[i+e];
					}
					mem_size -= e;
					unlock();
					weapons.lock();
					for(std::vector<uint32>::iterator f = weapons.idx_list.begin() ; f != weapons.idx_list.end() ; ++f)
					{
						uint32 i = *f;
						// Yes we don't defend against allies :D, can lead to funny situations :P
						if (weapons.weapon[i].weapon_id!=-1 && !(players.team[ units.unit[weapons.weapon[i].shooter_idx].owner_id ] & players.team[ owner_id ])
							&& weapon_manager.weapon[weapons.weapon[i].weapon_id].targetable)
						{
							if (((Vector3D)(weapons.weapon[i].target_pos-Pos)).sq()<=coverage
								&& ((Vector3D)(weapons.weapon[i].Pos-Pos)).sq()<=range)
							{
								int idx=-1;
								for (e = 0; e < mem_size; ++e)
								{
									if (memory[e] == i)
									{
										idx=i;
										break;
									}
								}
								if (idx == -1)
								{
									enemy_idx=i;
									if (mem_size < TA3D_PLAYERS_HARD_LIMIT)
									{
										memory[mem_size]=i;
										mem_size++;
									}
									break;
								}
							}
						}
					}
					weapons.unlock();
					lock();
					if (enemy_idx>=0)			// If we found a target, then attack it, here  we use attack because we need the mission list to act properly
						add_mission(MISSION_ATTACK | MISSION_FLAG_AUTO,&(weapons.weapon[enemy_idx].Pos),false,0,&(weapons.weapon[enemy_idx]),NULL,12);	// 12 = 4 | 8, targets a weapon and automatic fire
				}
			}
		}

		if (unit_manager.unit_type[type_id]->canfly) // Set plane orientation
		{
			Vector3D J,K;
			K.x=K.z=0.0f;
			K.y=1.0f;
			J = V * K;

			Vector3D virtual_G;						// Compute the apparent gravity force ( seen from the plane )
			virtual_G.x = virtual_G.z = 0.0f;		// Standard gravity vector
			virtual_G.y = -4.0f * units.g_dt;
			float d = J.sq();
			if (d)
				virtual_G = virtual_G + (((old_V - V) % J) / d) * J;		// Add the opposite of the speed derivative projected on the side of the unit

			d = virtual_G.norm();
			if (d)
			{
				virtual_G = -1.0f / d * virtual_G;

				d = sqrtf(virtual_G.y*virtual_G.y+virtual_G.z*virtual_G.z);
				float angle_1 = (d != 0.0f) ? acosf(virtual_G.y/d)*RAD2DEG : 0.0f;
				if (virtual_G.z<0.0f)	angle_1 = -angle_1;
				virtual_G = virtual_G * RotateX(-angle_1*DEG2RAD);
				float angle_2 = acosf( virtual_G % K )*RAD2DEG;
				if (virtual_G.x > 0.0f)	angle_2 = -angle_2;

				if (fabsf( angle_1 - Angle.x ) < 360.0f )
					Angle.x += dt*( angle_1 - Angle.x );				// We need something continuous
				if (fabsf( angle_2 - Angle.z ) < 360.0f )
					Angle.z += dt*( angle_2 - Angle.z );

				if (Angle.x < -360.0f || Angle.x > 360.0f )		Angle.x = 0.0f;
				if (Angle.z < -360.0f || Angle.z > 360.0f )		Angle.z = 0.0f;
			}
		}

		if (build_percent_left==0.0f)
		{

			// Change the unit's angle the way we need it to be changed

			if (b_TargetAngle && !isNaN(f_TargetAngle) && unit_manager.unit_type[type_id]->BMcode)	// Don't remove the class check otherwise factories can spin
			{
				while (!isNaN(f_TargetAngle) && fabsf( f_TargetAngle - Angle.y ) > 180.0f)
				{
					if (f_TargetAngle < Angle.y)
						Angle.y -= 360.0f;
					else
						Angle.y += 360.0f;
				}
				if (!isNaN(f_TargetAngle) && fabsf( f_TargetAngle - Angle.y ) >= 1.0f)
				{
					float aspeed = unit_manager.unit_type[type_id]->TurnRate;
					if (f_TargetAngle < Angle.y )
						aspeed =- aspeed;
					float a = f_TargetAngle - Angle.y;
					V_Angle.y = aspeed;
					float b = f_TargetAngle - (Angle.y + dt*V_Angle.y);
					if (((a < 0.0f && b > 0.0f) || (a > 0.0f && b < 0.0f)) && !isNaN(f_TargetAngle))
					{
						V_Angle.y = 0.0f;
						Angle.y = f_TargetAngle;
					}
				}
			}

			Angle = Angle + dt * V_Angle;
			Vector3D OPos = Pos;
			if (precomputed_position)
			{
				if (unit_manager.unit_type[type_id]->canmove && unit_manager.unit_type[type_id]->BMcode && !flying )
					V.y-=units.g_dt;			// L'unité subit la force de gravitation
				Pos = NPos;
				Pos.y = OPos.y + V.y * dt;
				cur_px = n_px;
				cur_py = n_py;
			}
			else
			{
				if (unit_manager.unit_type[type_id]->canmove && unit_manager.unit_type[type_id]->BMcode )
					V.y-=units.g_dt;			// L'unité subit la force de gravitation
				Pos = Pos+dt*V;			// Déplace l'unité
				cur_px = ((int)(Pos.x)+map->map_w_d+4)>>3;
				cur_py = ((int)(Pos.z)+map->map_h_d+4)>>3;
			}
			if (units.current_tick - ripple_timer >= 7 && Pos.y <= map->sealvl && Pos.y + model->top >= map->sealvl && (unit_manager.unit_type[type_id]->fastCategory & CATEGORY_NOTSUB)
				&& cur_px >= 0 && cur_py >= 0 && cur_px < map->bloc_w_db && cur_py < map->bloc_h_db && !map->map_data[ cur_py ][ cur_px ].lava && map->water )
			{
				Vector3D Diff = OPos - Pos;
				Diff.y = 0.0f;
				if (Diff.sq() > 0.1f && lp_CONFIG->waves)
				{
					ripple_timer = units.current_tick;
					Vector3D ripple_pos = Pos;
					ripple_pos.y = map->sealvl + 1.0f;
					fx_manager.addRipple( ripple_pos, ( ((sint32)(Math::RandFromTable() % 201)) - 100 ) * 0.0001f );
				}
			}
		}
script_exec:
		if (map && !attached && ( (!jump_commands && unit_manager.unit_type[type_id]->canmove) || first_move ))
		{
			bool hover_on_water = false;
			float min_h = map->get_unit_h(Pos.x,Pos.z);
			h = Pos.y - min_h;
			if (!unit_manager.unit_type[type_id]->Floater && !unit_manager.unit_type[type_id]->canfly && !unit_manager.unit_type[type_id]->canhover && h > 0.0f && unit_manager.unit_type[type_id]->WaterLine == 0.0f )
				Pos.y = min_h;
			else if (unit_manager.unit_type[type_id]->canhover && Pos.y <= map->sealvl)
			{
				hover_on_water = true;
				Pos.y = map->sealvl;
				if (V.y<0.0f)
					V.y=0.0f;
			}
			else if (unit_manager.unit_type[type_id]->Floater)
			{
				Pos.y = map->sealvl+unit_manager.unit_type[type_id]->AltFromSeaLevel*H_DIV;
				V.y=0.0f;
			}
			else if (unit_manager.unit_type[type_id]->WaterLine)
			{
				Pos.y=map->sealvl-unit_manager.unit_type[type_id]->WaterLine*H_DIV;
				V.y=0.0f;
			}
			else if (!unit_manager.unit_type[type_id]->canfly && Pos.y > Math::Max( min_h, map->sealvl ) && unit_manager.unit_type[type_id]->BMcode)	// Prevent non flying units from "jumping"
			{
				Pos.y = Math::Max(min_h, map->sealvl);
				if (V.y<0.0f)
					V.y=0.0f;
			}
			if (unit_manager.unit_type[type_id]->canhover)
			{
				int param[1] = { hover_on_water ? ( map->sealvl - min_h >= 8.0f ? 2 : 1) : 4 };
				runScriptFunction(SCRIPT_setSFXoccupy, 1, param);
			}
			if (min_h>Pos.y)
			{
				Pos.y=min_h;
				if (V.y<0.0f)
					V.y=0.0f;
			}
			if (unit_manager.unit_type[type_id]->canfly && build_percent_left==0.0f && local)
			{
				if (mission && ( (mission->flags & MISSION_FLAG_MOVE) || mission->mission == MISSION_BUILD || mission->mission == MISSION_BUILD_2 || mission->mission == MISSION_REPAIR
								 || mission->mission == MISSION_ATTACK || mission->mission == MISSION_MOVE || mission->mission == MISSION_GET_REPAIRED || mission->mission == MISSION_PATROL
								 || mission->mission == MISSION_RECLAIM || nb_attached > 0 || Pos.x < -map->map_w_d || Pos.x > map->map_w_d || Pos.z < -map->map_h_d || Pos.z > map->map_h_d ))
				{
					if (!(mission->mission == MISSION_GET_REPAIRED && (mission->flags & MISSION_FLAG_BEING_REPAIRED) ) )
					{
						float ideal_h=Math::Max(min_h,map->sealvl)+unit_manager.unit_type[type_id]->CruiseAlt*H_DIV;
						V.y=(ideal_h-Pos.y)*2.0f;
					}
					flying = true;
				}
				else
				{
					if (can_be_there( cur_px, cur_py, units.map, type_id, owner_id, idx ))		// Check it can be there
					{
						float ideal_h = min_h;
						V.y=(ideal_h-Pos.y)*1.5f;
						flying = false;
					}
					else				// There is someone there, find an other place to land
					{
						flying = true;
						if (mission == NULL
							|| (mission->mission != MISSION_STOP && mission->mission != MISSION_VTOL_STANDBY && mission->mission != MISSION_STANDBY)
							|| mission->data > 5)   // Wait for MISSION_STOP to check if we have some work to do
						{                                                                               // This prevents planes from keeping looking for a place to land
							Vector3D next_target = Pos;                                                 // instead of going back to work :/
							float find_angle = (Math::RandFromTable() % 360) * DEG2RAD;
							next_target.x += cosf( find_angle ) * (32.0f + unit_manager.unit_type[type_id]->FootprintX * 8.0f);
							next_target.z += sinf( find_angle ) * (32.0f + unit_manager.unit_type[type_id]->FootprintZ * 8.0f);
							add_mission( MISSION_MOVE | MISSION_FLAG_AUTO, &next_target, true );
						}
					}
				}
			}
			port[GROUND_HEIGHT] = (int)(Pos.y-min_h+0.5f);
		}
		port[HEALTH] = (int)hp*100 / unit_manager.unit_type[type_id]->MaxDamage;
		if (script)
			script->run(dt);
		yardmap_timer--;
		if (hp > 0.0f &&
			(((o_px != cur_px || o_py != cur_py || first_move || (was_flying ^ flying) || ((port[YARD_OPEN] != 0.0f) ^ was_open) || yardmap_timer == 0) && build_percent_left <= 0.0f) || !drawn))
		{
			first_move = build_percent_left > 0.0f;
			pMutex.unlock();
			draw_on_map();
			pMutex.lock();
			yardmap_timer = TICKS_PER_SEC + (Math::RandFromTable() & 15);
		}

		built=false;
		attacked=false;
		pMutex.unlock();
		return 0;
	}



	bool Unit::hit(Vector3D P,Vector3D Dir,Vector3D* hit_vec, float length)
	{
		pMutex.lock();
		if (!(flags&1))
		{
			pMutex.unlock();
			return false;
		}
		if (model)
		{
			Vector3D c_dir=model->center+Pos-P;
			if (c_dir.norm()-length <=model->size2 )
			{
				float scale=unit_manager.unit_type[type_id]->Scale;
				//            Matrix M=RotateX(-Angle.x*DEG2RAD)*RotateZ(-Angle.z*DEG2RAD)*RotateY(-Angle.y*DEG2RAD)*Scale(1.0f/scale);
				Matrix M = RotateXZY(-Angle.x*DEG2RAD, -Angle.z*DEG2RAD, -Angle.y*DEG2RAD)*Scale(1.0f/scale);
				Vector3D RP=(P-Pos) * M;
				bool is_hit = model->hit(RP,Dir,&data,hit_vec,M) >= -1;
				if (is_hit)
				{
					//                *hit_vec=(*hit_vec)*(RotateY(Angle.y*DEG2RAD)*RotateZ(Angle.z*DEG2RAD)*RotateX(Angle.x*DEG2RAD)*Scale(scale))+Pos;
					*hit_vec = ((*hit_vec) * RotateYZX(Angle.y*DEG2RAD, Angle.z*DEG2RAD, Angle.x*DEG2RAD))*Scale(scale)+Pos;
					*hit_vec=((*hit_vec-P)%Dir)*Dir+P;
				}

				pMutex.unlock();
				return is_hit;
			}
		}
		pMutex.unlock();
		return false;
	}

	bool Unit::hit_fast(Vector3D P,Vector3D Dir,Vector3D* hit_vec, float length)
	{
		pMutex.lock();
		if (!(flags&1))	{
			pMutex.unlock();
			return false;
		}
		if (model)
		{
			Vector3D c_dir = model->center+Pos-P;
			if (c_dir.sq() <= ( model->size2 + length ) * ( model->size2 + length ) ) {
				float scale=unit_manager.unit_type[type_id]->Scale;
				//            Matrix M = RotateX(-Angle.x*DEG2RAD)*RotateZ(-Angle.z*DEG2RAD)*RotateY(-Angle.y*DEG2RAD)*Scale(1.0f/scale);
				Matrix M = RotateXZY(-Angle.x*DEG2RAD, -Angle.z*DEG2RAD, -Angle.y*DEG2RAD)*Scale(1.0f/scale);
				Vector3D RP = (P - Pos) * M;
				bool is_hit = model->hit_fast(RP,Dir,&data,hit_vec,M);
				if (is_hit) {
					//                *hit_vec=(*hit_vec)*(RotateY(Angle.y*DEG2RAD)*RotateZ(Angle.z*DEG2RAD)*RotateX(Angle.x*DEG2RAD)*Scale(scale))+Pos;
					*hit_vec = ((*hit_vec)*RotateYZX(Angle.y*DEG2RAD, Angle.z*DEG2RAD, Angle.x*DEG2RAD))*Scale(scale)+Pos;
					*hit_vec=((*hit_vec-P)%Dir)*Dir+P;
				}

				pMutex.unlock();
				return is_hit;
			}
		}
		pMutex.unlock();
		return false;
	}

	void Unit::show_orders(bool only_build_commands, bool def_orders)				// Dessine les ordres reçus
	{
		if (!def_orders)
			show_orders( only_build_commands, true );

		pMutex.lock();

		if (!(flags&1))
		{
			pMutex.unlock();
			return;
		}

		bool low_def = (Camera::inGame->rpos.y > gfx->low_def_limit);

		Mission *cur = def_orders ? def_mission : mission;
		if (low_def )
		{
			glEnable(GL_BLEND);
			glDisable(GL_TEXTURE_2D);
			glDisable(GL_LIGHTING);
			glDisable(GL_CULL_FACE);
			glBlendFunc(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA);
			glColor4ub(0xFF,0xFF,0xFF,0xFF);
		}
		else
		{
			glEnable(GL_BLEND);
			glEnable(GL_TEXTURE_2D);
			glDisable(GL_LIGHTING);
			glDisable(GL_CULL_FACE);
			glBlendFunc(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA);
			glColor4ub(0xFF,0xFF,0xFF,0xFF);
		}
		Vector3D p_target=Pos;
		Vector3D n_target=Pos;
		float rab=(msec_timer%1000)*0.001f;
		uint32	remaining_build_commands = !(unit_manager.unit_type[type_id]->BMcode) ? 0 : 0xFFFFFFF;

		std::list<Vector3D>	points;

		while(cur)
		{
			if (cur->step) 	// S'il s'agit d'une étape on ne la montre pas
			{
				cur=cur->next;
				continue;
			}
			if (!only_build_commands)
			{
				int curseur=anim_cursor(CURSOR_CROSS_LINK);
				float dx = 0.5f * cursor[CURSOR_CROSS_LINK].ofs_x[curseur];
				float dz = 0.5f * cursor[CURSOR_CROSS_LINK].ofs_y[curseur];
				float x,y,z;
				float dist = ((Vector3D)(cur->target-p_target)).norm();
				int rec = (int)(dist / 30.0f);
				switch (cur->mission)
				{
					case MISSION_LOAD:
					case MISSION_UNLOAD:
					case MISSION_GUARD:
					case MISSION_PATROL:
					case MISSION_MOVE:
					case MISSION_BUILD:
					case MISSION_BUILD_2:
					case MISSION_REPAIR:
					case MISSION_ATTACK:
					case MISSION_RECLAIM:
					case MISSION_REVIVE:
					case MISSION_CAPTURE:
						if ((cur->p && ((Unit*)(cur->p))->ID != cur->target_ID) || (cur->flags & MISSION_FLAG_TARGET_WEAPON) )
						{
							cur = cur->next;
							continue;	// Don't show this, it'll be removed
						}
						n_target=cur->target;
						n_target.y = Math::Max(units.map->get_unit_h( n_target.x, n_target.z ), units.map->sealvl);
						if (rec > 0)
						{
							if (low_def)
							{
								glDisable(GL_DEPTH_TEST);
								glColor4ub( 0xFF, 0xFF, 0xFF, 0x7F );
								glBegin( GL_QUADS );
								Vector3D D = n_target - p_target;
								D.y = D.x;
								D.x = D.z;
								D.z = -D.y;
								D.y = 0.0f;
								D.unit();
								D = 5.0f * D;
								Vector3D P;
								P = p_target - D;	glVertex3fv( (GLfloat*)&P );
								P = p_target + D;	glVertex3fv( (GLfloat*)&P );
								P = n_target + D;	glVertex3fv( (GLfloat*)&P );
								P = n_target - D;	glVertex3fv( (GLfloat*)&P );
								glEnd();
								glColor4ub( 0xFF, 0xFF, 0xFF, 0xFF );
								glEnable(GL_DEPTH_TEST);
							}
							else
							{
								for (int i = 0; i < rec; ++i)
								{
									x = p_target.x+(n_target.x-p_target.x)*(i+rab)/rec;
									z = p_target.z+(n_target.z-p_target.z)*(i+rab)/rec;
									y = Math::Max(units.map->get_unit_h( x, z ), units.map->sealvl);
									y += 0.75f;
									x -= dx;
									z -= dz;
									points.push_back(Vector3D(x, y, z));
								}
							}
						}
						p_target = n_target;
				}
			}
			glDisable(GL_DEPTH_TEST);
			switch(cur->mission)
			{
				case MISSION_BUILD:
					if (cur->p!=NULL)
						cur->target=((Unit*)(cur->p))->Pos;
					if (cur->data>=0 && cur->data<unit_manager.nb_unit && remaining_build_commands > 0 )
					{
						remaining_build_commands--;
						float DX = (unit_manager.unit_type[cur->data]->FootprintX<<2);
						float DZ = (unit_manager.unit_type[cur->data]->FootprintZ<<2);
						float blue = 0.0f, green = 1.0f;
						if (only_build_commands)
						{
							blue = 1.0f;
							green = 0.0f;
						}
						glPushMatrix();
						glTranslatef(cur->target.x,Math::Max( cur->target.y, units.map->sealvl ),cur->target.z);
						glDisable(GL_CULL_FACE);
						glDisable(GL_TEXTURE_2D);
						glEnable(GL_BLEND);
						glBlendFunc(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA);
						glBegin(GL_QUADS);
						glColor4f(0.0f,green,blue,1.0f);
						glVertex3f(-DX,0.0f,-DZ);			// First quad
						glVertex3f(DX,0.0f,-DZ);
						glColor4f(0.0f,green,blue,0.0f);
						glVertex3f(DX+2.0f,5.0f,-DZ-2.0f);
						glVertex3f(-DX-2.0f,5.0f,-DZ-2.0f);

						glColor4f(0.0f,green,blue,1.0f);
						glVertex3f(-DX,0.0f,-DZ);			// Second quad
						glVertex3f(-DX,0.0f,DZ);
						glColor4f(0.0f,green,blue,0.0f);
						glVertex3f(-DX-2.0f,5.0f,DZ+2.0f);
						glVertex3f(-DX-2.0f,5.0f,-DZ-2.0f);

						glColor4f(0.0f,green,blue,1.0f);
						glVertex3f(DX,0.0f,-DZ);			// Third quad
						glVertex3f(DX,0.0f,DZ);
						glColor4f(0.0f,green,blue,0.0f);
						glVertex3f(DX+2.0f,5.0f,DZ+2.0f);
						glVertex3f(DX+2.0f,5.0f,-DZ-2.0f);

						glEnd();
						glDisable(GL_BLEND);
						glEnable(GL_TEXTURE_2D);
						glEnable(GL_CULL_FACE);
						glPopMatrix();
						if (unit_manager.unit_type[cur->data]->model!=NULL)
						{
							glEnable(GL_LIGHTING);
							glEnable(GL_CULL_FACE);
							glEnable(GL_DEPTH_TEST);
							glPushMatrix();
							glTranslatef(cur->target.x,cur->target.y,cur->target.z);
							glColor4f(0.0f,green,blue,0.5f);
							glDepthFunc( GL_GREATER );
							unit_manager.unit_type[cur->data]->model->obj.draw(0.0f,NULL,false,false,false);
							glDepthFunc( GL_LESS );
							unit_manager.unit_type[cur->data]->model->obj.draw(0.0f,NULL,false,false,false);
							glPopMatrix();
							glEnable(GL_BLEND);
							glEnable(GL_TEXTURE_2D);
							glDisable(GL_LIGHTING);
							glDisable(GL_CULL_FACE);
							glDisable(GL_DEPTH_TEST);
							glBlendFunc(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA);
						}
						glPushMatrix();
						glTranslatef(cur->target.x,Math::Max( cur->target.y, units.map->sealvl ),cur->target.z);
						glDisable(GL_CULL_FACE);
						glDisable(GL_TEXTURE_2D);
						glEnable(GL_BLEND);
						glBlendFunc(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA);
						glBegin(GL_QUADS);
						glColor4f(0.0f,green,blue,1.0f);
						glVertex3f(-DX,0.0f,DZ);			// Fourth quad
						glVertex3f(DX,0.0f,DZ);
						glColor4f(0.0f,green,blue,0.0f);
						glVertex3f(DX+2.0f,5.0f,DZ+2.0f);
						glVertex3f(-DX-2.0f,5.0f,DZ+2.0f);
						glEnd();
						glPopMatrix();
						glEnable(GL_BLEND);
						if (low_def )
							glDisable(GL_TEXTURE_2D);
						else
							glEnable(GL_TEXTURE_2D);
						glDisable(GL_CULL_FACE);
						glColor4f(1.0f,1.0f,1.0f,1.0f);
					}
					break;
				case MISSION_UNLOAD:
				case MISSION_LOAD:
				case MISSION_MOVE:
				case MISSION_BUILD_2:
				case MISSION_REPAIR:
				case MISSION_RECLAIM:
				case MISSION_REVIVE:
				case MISSION_PATROL:
				case MISSION_GUARD:
				case MISSION_ATTACK:
				case MISSION_CAPTURE:
					if (!only_build_commands)
					{
						if (cur->p!=NULL)
							cur->target=((Unit*)(cur->p))->Pos;
						int cursor_type = CURSOR_ATTACK;
						switch( cur->mission )
						{
							case MISSION_GUARD:		cursor_type = CURSOR_GUARD;		break;
							case MISSION_ATTACK:	cursor_type = CURSOR_ATTACK;	break;
							case MISSION_PATROL:	cursor_type = CURSOR_PATROL;	break;
							case MISSION_RECLAIM:	cursor_type = CURSOR_RECLAIM;	break;
							case MISSION_BUILD_2:
							case MISSION_REPAIR:	cursor_type = CURSOR_REPAIR;	break;
							case MISSION_MOVE:		cursor_type = CURSOR_MOVE;		break;
							case MISSION_LOAD:		cursor_type = CURSOR_LOAD;		break;
							case MISSION_UNLOAD:	cursor_type = CURSOR_UNLOAD;	break;
							case MISSION_REVIVE:	cursor_type = CURSOR_REVIVE;	break;
							case MISSION_CAPTURE:	cursor_type = CURSOR_CAPTURE;	break;
						}
						int curseur=anim_cursor( cursor_type );
						float x=cur->target.x - 0.5f * cursor[cursor_type].ofs_x[curseur];
						float y=cur->target.y + 1.0f;
						float z=cur->target.z - 0.5f * cursor[cursor_type].ofs_y[curseur];
						float sx = 0.5f * (cursor[cursor_type].bmp[curseur]->w - 1);
						float sy = 0.5f * (cursor[cursor_type].bmp[curseur]->h - 1);
						if (low_def)
							glEnable(GL_TEXTURE_2D);
						glBindTexture(GL_TEXTURE_2D, cursor[cursor_type].glbmp[curseur]);
						glBegin(GL_QUADS);
						glTexCoord2f(0.0f,0.0f);  glVertex3f(x,y,z);
						glTexCoord2f(1.0f,0.0f);  glVertex3f(x+sx,y,z);
						glTexCoord2f(1.0f,1.0f);  glVertex3f(x+sx,y,z+sy);
						glTexCoord2f(0.0f,1.0f);  glVertex3f(x,y,z+sy);
						glEnd();
						if (low_def)
							glDisable(GL_TEXTURE_2D);
					}
					break;
			}
			glEnable(GL_DEPTH_TEST);
			cur = cur->next;
		}

		if (!points.empty())
		{
			int curseur=anim_cursor(CURSOR_CROSS_LINK);
			float sx = 0.5f * (cursor[CURSOR_CROSS_LINK].bmp[curseur]->w - 1);
			float sy = 0.5f * (cursor[CURSOR_CROSS_LINK].bmp[curseur]->h - 1);

			Vector3D* P = new Vector3D[points.size() << 2];
			float* T = new float[points.size() << 3];

			int n = 0;
			for (std::list<Vector3D>::const_iterator i = points.begin(); i != points.end(); ++i)
			{
				P[n] = *i;
				T[n<<1] = 0.0f;		T[(n<<1)+1] = 0.0f;
				++n;

				P[n] = *i;	P[n].x += sx;
				T[n<<1] = 1.0f;		T[(n<<1)+1] = 0.0f;
				++n;

				P[n] = *i;	P[n].x += sx;	P[n].z += sy;
				T[n<<1] = 1.0f;		T[(n<<1)+1] = 1.0f;
				++n;

				P[n] = *i;	P[n].z += sy;
				T[n<<1] = 0.0f;		T[(n<<1)+1] = 1.0f;
				++n;
			}

			glDisableClientState( GL_NORMAL_ARRAY );
			glDisableClientState( GL_COLOR_ARRAY );
			glEnableClientState( GL_VERTEX_ARRAY );
			glEnableClientState( GL_TEXTURE_COORD_ARRAY );

			glVertexPointer( 3, GL_FLOAT, 0, P);
			glClientActiveTextureARB(GL_TEXTURE0_ARB );
			glTexCoordPointer(2, GL_FLOAT, 0, T);
			glBindTexture(GL_TEXTURE_2D, cursor[CURSOR_CROSS_LINK].glbmp[curseur]);

			glDrawArrays(GL_QUADS, 0, n);

			delete[] P;
			delete[] T;
		}
		glDisable(GL_BLEND);
		pMutex.unlock();
	}


	int Unit::shoot(int target,Vector3D startpos,Vector3D Dir,int w_id,const Vector3D &target_pos)
	{
        WEAPON_DEF *pW = unit_manager.unit_type[type_id]->weapon[ w_id ];        // Critical information, we can't lose it so we save it before unlocking this unit
		int owner = owner_id;
		Vector3D D = Dir * RotateY( -Angle.y * DEG2RAD );
		int param[] = { (int)(-10.0f*DEG2TA*D.z), (int)(-10.0f*DEG2TA*D.x) };
        launchScript( SCRIPT_RockUnit, 2, param );

        if (pW->startsmoke && visible)
			particle_engine.make_smoke(startpos, 0, 1, 0.0f, -1.0f, 0.0f, 0.3f);

        pMutex.unlock();

		weapons.lock();

        int w_idx = weapons.add_weapon(pW->nb_id,idx);

		if (network_manager.isConnected() && local) // Send synchronization packet
		{
			struct event event;
			event.type = EVENT_WEAPON_CREATION;
			event.opt1 = idx;
			event.opt2 = target;
			event.opt3 = units.current_tick; // Will be used to extrapolate those data on client side
			event.opt4 = pW->damage;
			event.opt5 = owner_id;
			event.x = target_pos.x;
			event.y = target_pos.y;
			event.z = target_pos.z;
			event.vx = startpos.x;
			event.vy = startpos.y;
			event.vz = startpos.z;
			event.dx = (sint16)(Dir.x * 16384.0f);
			event.dy = (sint16)(Dir.y * 16384.0f);
			event.dz = (sint16)(Dir.z * 16384.0f);
			memcpy( event.str, pW->internal_name.c_str(), pW->internal_name.size() + 1 );

			network_manager.sendEvent( &event );
		}

		weapons.weapon[w_idx].damage = pW->damage;
		weapons.weapon[w_idx].Pos = startpos;
		weapons.weapon[w_idx].local = local;
		if (pW->startvelocity==0.0f && !pW->selfprop)
			weapons.weapon[w_idx].V = pW->weaponvelocity*Dir;
		else
			weapons.weapon[w_idx].V = pW->startvelocity*Dir;
		//        if (pW->dropped || !pW->lineofsight)
		weapons.weapon[w_idx].V = weapons.weapon[w_idx].V+V;
		weapons.weapon[w_idx].owner = owner;
		weapons.weapon[w_idx].target=target;
		if (target >= 0 )
		{
			if (pW->interceptor)
				weapons.weapon[w_idx].target_pos = weapons.weapon[target].Pos;
			else
				weapons.weapon[w_idx].target_pos = target_pos;
		}
		else
			weapons.weapon[w_idx].target_pos = target_pos;

		weapons.weapon[w_idx].stime = 0.0f;
		weapons.weapon[w_idx].visible = visible;        // Not critical so we don't duplicate this
		weapons.unlock();
		pMutex.lock();
		return w_idx;
	}


	void Unit::draw_on_map()
	{
		if (type_id == -1 || !(flags & 1) )
			return;

		if (drawn )	clear_from_map();
		if (attached )	return;

		drawn_flying = flying;
		if (flying )
			units.map->air_rect( cur_px-(unit_manager.unit_type[type_id]->FootprintX>>1), cur_py-(unit_manager.unit_type[type_id]->FootprintZ>>1), unit_manager.unit_type[type_id]->FootprintX, unit_manager.unit_type[type_id]->FootprintZ, idx );
		else
		{
			// First check we're on a "legal" place if it can move
			pMutex.lock();
			if (unit_manager.unit_type[type_id]->canmove && unit_manager.unit_type[type_id]->BMcode
				&& !can_be_there( cur_px, cur_py, units.map, type_id, owner_id ) )
			{
				// Try to find a suitable place

				bool found = false;
				for( int r = 1 ; r < 20 && !found ; r++ ) // Circular check
				{
					int r2 = r * r;
					for( int y = 0 ; y <= r ; y++ )
					{
						int x = (int)(sqrtf( r2 - y * y ) + 0.5f);
						if (can_be_there( cur_px+x, cur_py+y, units.map, type_id, owner_id ) )
						{
							cur_px += x;
							cur_py += y;
							found = true;
							break;
						}
						if (can_be_there( cur_px+x, cur_py+y, units.map, type_id, owner_id ) )
						{
							cur_px -= x;
							cur_py += y;
							found = true;
							break;
						}
						if (can_be_there( cur_px+x, cur_py+y, units.map, type_id, owner_id ) )
						{
							cur_px += x;
							cur_py -= y;
							found = true;
							break;
						}
						if (can_be_there( cur_px+x, cur_py+y, units.map, type_id, owner_id ) )
						{
							cur_px -= x;
							cur_py -= y;
							found = true;
							break;
						}
					}
				}
				if (found)
				{
					Pos.x = (cur_px<<3) + 4 - units.map->map_w_d;
					Pos.z = (cur_py<<3) + 4 - units.map->map_h_d;
					if (mission && (mission->flags & MISSION_FLAG_MOVE) )
						mission->flags |= MISSION_FLAG_REFRESH_PATH;
				}
				else
					printf("error: units overlaps on yardmap !!\n");

			}
			pMutex.unlock();

			units.map->rect( cur_px-(unit_manager.unit_type[type_id]->FootprintX>>1), cur_py-(unit_manager.unit_type[type_id]->FootprintZ>>1), unit_manager.unit_type[type_id]->FootprintX, unit_manager.unit_type[type_id]->FootprintZ, idx, unit_manager.unit_type[type_id]->yardmap, port[YARD_OPEN]!=0.0f );
			drawn_open = port[YARD_OPEN]!=0.0f;
		}
		drawn_x = cur_px;
		drawn_y = cur_py;
		drawn = true;
	}

	void Unit::clear_from_map()
	{
		if (!drawn)
			return;

		int type = type_id;

		if (type == -1 || !(flags & 1) )
			return;

		drawn = false;
		if (drawn_flying )
			units.map->air_rect( drawn_x-(unit_manager.unit_type[type]->FootprintX>>1), drawn_y-(unit_manager.unit_type[type]->FootprintZ>>1), unit_manager.unit_type[type]->FootprintX, unit_manager.unit_type[type]->FootprintZ, idx, true );
		else
			units.map->rect( drawn_x-(unit_manager.unit_type[type]->FootprintX>>1), drawn_y-(unit_manager.unit_type[type]->FootprintZ>>1), unit_manager.unit_type[type]->FootprintX, unit_manager.unit_type[type]->FootprintZ, -1, unit_manager.unit_type[type]->yardmap, drawn_open );
	}

	void Unit::draw_on_FOW( bool jamming )
	{
		if (hidden || build_percent_left != 0.0f )
			return;

		int unit_type = type_id;

		if (flags == 0 || unit_type == -1)  return;

		bool system_activated = (port[ACTIVATION] && unit_manager.unit_type[unit_type]->onoffable) || !unit_manager.unit_type[unit_type]->onoffable;

		if (jamming )
		{
			radar_jam_range = system_activated ? (unit_manager.unit_type[unit_type]->RadarDistanceJam >> 4) : 0;
			sonar_jam_range = system_activated ? (unit_manager.unit_type[unit_type]->SonarDistanceJam >> 4) : 0;

			units.map->update_player_visibility( owner_id, cur_px, cur_py, 0, 0, 0, radar_jam_range, sonar_jam_range, true );
		}
		else
		{
			sint16 cur_sight = ((int)h + unit_manager.unit_type[unit_type]->SightDistance) >> 4;
			radar_range = system_activated ? (unit_manager.unit_type[unit_type]->RadarDistance >> 4) : 0;
			sonar_range = system_activated ? (unit_manager.unit_type[unit_type]->SonarDistance >> 4) : 0;

			units.map->update_player_visibility( owner_id, cur_px, cur_py, cur_sight, radar_range, sonar_range, 0, 0, false, old_px != cur_px || old_py != cur_py || cur_sight != sight );

			sight = cur_sight;
			old_px = cur_px;
			old_py = cur_py;
		}
	}



	void Unit::playSound(const String &key)
	{
		pMutex.lock();
		if (owner_id == players.local_human_id && msec_timer - last_time_sound >= units.sound_min_ticks )
		{
			last_time_sound = msec_timer;
			sound_manager->playTDFSound(unit_manager.unit_type[type_id]->soundcategory, key , &Pos);
		}
		pMutex.unlock();
	}



	int Unit::launchScript(const int id, int nb_param, int *param)			// Start a script as a separate "thread" of the unit
	{
		const String& f_name = UnitScriptInterface::get_script_name(id);
		if (f_name.empty())
			return -2;

		MutexLocker locker(pMutex);

		if (!script)
			return -2;

		if (local && network_manager.isConnected()) // Send synchronization event
		{
			struct event event;
			event.type = EVENT_UNIT_SCRIPT;
			event.opt1 = idx;
			event.opt2 = id;
			event.opt3 = nb_param;
			memcpy(event.str, param, sizeof(int) * nb_param);
			network_manager.sendEvent(&event);
		}

		ScriptInterface *newThread = script->fork( f_name, param, nb_param );

		if (newThread == NULL || !newThread->is_running())
			return -2;

		return 0;
	}

    int Unit::get_sweet_spot()
    {
        if (type_id < 0)
            return -1;
        if (unit_manager.unit_type[type_id]->sweetspot_cached == -1)
        {
            lock();
            unit_manager.unit_type[type_id]->sweetspot_cached = runScriptFunction(SCRIPT_SweetSpot);
            unlock();
        }
        return unit_manager.unit_type[type_id]->sweetspot_cached;
    }


} // namespace TA3D
