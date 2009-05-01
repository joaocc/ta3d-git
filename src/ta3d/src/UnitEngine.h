/*  TA3D, a remake of Total Annihilation
	Copyright (C) 2005   Roland BROCHARD

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

/*----------------------------------------------------------\
  |                         UnitEngine.h                      |
  |    Contains the unit engine, which simulates units and    |
  | make them interact with each other.                       |
  |                                                           |
  \----------------------------------------------------------*/

#ifndef __UNITENGINE_CLASS
# define __UNITENGINE_CLASS

# include "fbi.h"
# include "ingame/weapons/weapons.h"
# include "threads/thread.h"
# include "misc/camera.h"
# include <list>
# include <vector>
# include "misc/recttest.h"
# include "scripts/unit.script.interface.h"
# include "scripts/unit.defines.h"
# include "engine/weapondata.h"
# include "engine/mission.h"




# define SIGNAL_ORDER_NONE		    0x0
# define SIGNAL_ORDER_MOVE          0x1
# define SIGNAL_ORDER_PATROL        0x2
# define SIGNAL_ORDER_GUARD	        0x3
# define SIGNAL_ORDER_ATTACK		0x4
# define SIGNAL_ORDER_RECLAM		0x5
# define SIGNAL_ORDER_STOP		    0x6
# define SIGNAL_ORDER_ONOFF		    0x7
# define SIGNAL_ORDER_LOAD		    0x8
# define SIGNAL_ORDER_UNLOAD		0x9
# define SIGNAL_ORDER_REPAIR		0xA
# define SIGNAL_ORDER_CAPTURE	    0xB
# define SIGNAL_ORDER_DGUN		    0xC



namespace TA3D
{
	extern int MAX_UNIT_PER_PLAYER;

	void *create_unit(int type_id, int owner, Vector3D pos, MAP *map, bool sync = true, bool script = false);


	class Unit	: public ObjectSync	// Classe pour la gestion des unités	/ Class to store units's data
	{
	public:
		//! functions called from scripts (COB/BOS and Lua) (see unit.script.func module in scripts)
		void script_explode(int obj, int explosion_type);
		void script_turn_object(int obj, int axis, float angle, float speed);
		void script_move_object(int obj, int axis, float pos, float speed);
		int script_get_value_from_port(int portID, int *param = NULL);
		void script_spin_object(int obj, int axis, float target_speed, float accel);
		void script_show_object(int obj);
		void script_hide_object(int obj);
		void script_cache(int obj);
		void script_dont_cache(int obj);
		void script_shade(int obj);
		void script_dont_shade(int obj);
		void script_emit_sfx(int smoke_type, int from_piece);
		void script_stop_spin(int obj, int axis, float speed);
		void script_move_piece_now(int obj, int axis, float pos);
		void script_turn_piece_now(int obj, int axis, float angle);
		int script_get(int type, int v1, int v2);
		void script_set_value(int type, int v);
		void script_attach_unit(int unit_id, int piece_id);
		void script_drop_unit(int unit_id);
		bool script_is_turning(int obj, int axis);
		bool script_is_moving(int obj, int axis);

	public:
		float damage_modifier() const
		{return port[ARMORED] ? unit_manager.unit_type[type_id]->DamageModifier : 1.0f;}

		bool isEnemy(const int t);

		void draw_on_map();
		void clear_from_map();

		void draw_on_FOW(bool jamming = false);

		bool is_on_radar(byte p_mask);

		void start_mission_script(int mission_type);

		void next_mission();

		void clear_mission();

		void add_mission(int mission_type, const Vector3D* target = NULL, bool step = false, int dat = 0,
						 void* pointer = NULL, PATH_NODE* path = NULL, byte m_flags = 0,
						 int move_data = 0, int patrol_node = -1);

		void set_mission(int mission_type, const Vector3D* target = NULL, bool step = false, int dat = 0,
						 bool stopit = true, void* pointer = NULL, PATH_NODE* path = NULL, byte m_flags = 0,
						 int move_data = 0);

		void compute_model_coord();

		void init_alloc_data();

		void toggle_self_destruct();

		void lock_command();

		void unlock_command();

		void init(int unit_type= - 1, int owner = -1, bool full = false, bool basic = false);

		void clear_def_mission();

		void destroy(bool full = false);

		void draw(float t, MAP *map, bool height_line = true);

		void draw_shadow(const Vector3D& Dir, MAP *map);

		void draw_shadow_basic(const Vector3D& Dir, MAP *map);

		int launch_script(const int id, int nb_param = 0, int *param = NULL);			        // Start a script as a separate "thread" of the unit

		int run_script_function(const int id, int nb_param = 0, int *param = NULL); // Launch and run the script, returning it's values to param if not NULL

		void reset_script();

		const void play_sound(const String& key);

		const int move( const float dt,MAP *map, int *path_exec, const int key_frame = 0 );

		void show_orders( bool only_build_commands=false, bool def_orders=false );				// Dessine les ordres reçus

		void activate();

		void deactivate();

		int shoot(int target,Vector3D startpos,Vector3D Dir,int w_id,const Vector3D& target_pos);

		bool hit(Vector3D P,Vector3D Dir,Vector3D *hit_vec, float length = 100.0f);

		bool hit_fast(Vector3D P,Vector3D Dir,Vector3D* hit_vec, float length = 100.0f);

		void stop_moving();

		void explode();

		inline bool do_nothing()
		{
			return (!mission || ((mission->mission == MISSION_STOP || mission->mission == MISSION_STANDBY || mission->mission == MISSION_VTOL_STANDBY) && mission->next == NULL)) && !port[INBUILDSTANCE];
		}

		inline bool do_nothing_ai()
		{
			return (!mission || ((mission->mission == MISSION_STOP || mission->mission == MISSION_STANDBY || mission->mission == MISSION_VTOL_STANDBY || mission->mission == MISSION_MOVE) && !mission->next)) && !port[INBUILDSTANCE];
		}

	private:
		void start_building(const Vector3D &dir);

	public:
		UnitScriptInterface     *script;		// Scripts concernant l'unité
		MODEL					*model;			// Modèle représentant l'objet
		byte					owner_id;		// Numéro du propriétaire de l'unité
		short					type_id;		// Type d'unité
		float					hp;				// Points de vide restant à l'unité
		Vector3D				Pos;			// Vecteur position
		Vector3D				drawn_Pos;		// To prevent the shadow to be drawn where the unit will be on next frame
		Vector3D				V;				// Vitesse de l'unité
		Vector3D				Angle;			// Orientation dans l'espace
		Vector3D				drawn_Angle;	// Idem drawn_Pos
		Vector3D				V_Angle;		// Variation de l'orientation dans l'espace
		bool					sel;			// Unité sélectionnée?
		ANIMATION_DATA          data;			// Données pour l'animation de l'unité par le script
		bool					drawing;
		sint16					*port;			// Ports
		Mission					*mission;		// Orders given to the unit
		Mission					*def_mission;	// Orders given to units built by this plant
		byte					flags;			// Pour indiquer entre autres au gestionnaire d'unités si l'unité existe
		int                     kills;          // How many kills did this unit
		// 0	-> nothing
		// 1	-> the unit exists
		// 4	-> being killed
		// 64	-> landed (for planes)
		float					c_time;			// Compteur de temps entre 2 émissions de particules par une unité de construction
		bool					compute_coord;	// Indique s'il est nécessaire de recalculer les coordonnées du modèle 3d
		uint16					idx;			// Indice dans le tableau d'unité
		uint32					ID;				// the number of the unit (in total creation order) so we can identify it even if we move it :)
		float					h;				// Altitude (par rapport au sol)
		bool					visible;		// Indique si l'unité est visible / Tell if the unit is currently visible
		bool					on_radar;		// Radar drawing mode (icons)
		bool					on_mini_radar;	// On minimap radar
		short					groupe;			// Indique si l'unité fait partie d'un groupe
		bool					built;			// Indique si l'unité est en cours de construction (par une autre unité)
		bool					attacked;		// Indique si l'unité est attaquée
		float					planned_weapons;	// Armes en construction / all is in the name
		int						*memory;		// Pour se rappeler sur quelles armes on a déjà tiré
		byte					mem_size;
		bool					attached;
		short					*attached_list;
		short					*link_list;
		byte					nb_attached;
		bool					just_created;
		bool					first_move;
		int						severity;
		sint16					cur_px;
		sint16					cur_py;
		float					metal_prod;
		float					metal_cons;
		float					energy_prod;
		float					energy_cons;
		uint32					last_time_sound;	// Remember last time it played a sound, so we don't get a unit SHOUTING for a simple move
		float					cur_metal_prod;
		float					cur_metal_cons;
		float					cur_energy_prod;
		float					cur_energy_cons;
		uint32					ripple_timer;
		WeaponData::Vector 		weapon;
		float                   death_delay;
		bool					was_moving;
		float					last_path_refresh;
		float					shadow_scale_dir;
		bool					hidden;				// Used when unit is attached to another one but is hidden (i.e. transport ship)
		bool					flying;
		bool					cloaked;			// Is the unit cloaked
		bool					cloaking;			// Cloaking the unit if enough energy
		float                   paralyzed;

		// Following variables are used to control the drawing of the unit on the presence maps
		bool			drawn_open;			// Used to store the last state the unit was drawn on the presence map (opened or closed)
		bool			drawn_flying;
		sint32			drawn_x, drawn_y;
		bool			drawn;

		// Following variables are used to control the drawing of FOW data
		uint16			sight;
		uint16			radar_range;
		uint16			sonar_range;
		uint16			radar_jam_range;
		uint16			sonar_jam_range;
		sint16			old_px;
		sint16			old_py;

		Vector3D		move_target_computed;
		float			was_locked;

		float			self_destruct;		// Count down for self-destruction
		float			build_percent_left;
		float			metal_extracted;

		bool			requesting_pathfinder;
		uint16			pad1, pad2;			// Repair pads currently used
		float			pad_timer;			// Store last try to find a free landing pad

		bool			command_locked;		// Used for missions

		uint8			yardmap_timer;		// To redraw the unit on yardmap periodically
		uint8			death_timer;		// To remove dead units

		// Following variables are used to control the synchronization of data between game clients
	public:
		uint32			sync_hash;
		uint32			*last_synctick;
		bool			local;
		bool			exploding;
		struct	sync	previous_sync;		// previous sync data
		sint32			nanolathe_target;
		bool			nanolathe_reverse;
		bool			nanolathe_feature;

		// Following variables are used by the renderer
	public:
		bool            visibility_checked;
	}; // class Unit

#define	ICON_UNKNOWN		0x0
#define	ICON_BUILDER		0x1
#define	ICON_TANK			0x2
#define	ICON_LANDUNIT		0x3
#define	ICON_DEFENSE		0x4
#define	ICON_ENERGY			0x5
#define	ICON_METAL			0x6
#define	ICON_WATERUNIT		0x7
#define	ICON_COMMANDER		0x8
#define ICON_SUBUNIT		0x9
#define ICON_AIRUNIT		0xA
#define ICON_FACTORY		0xB
#define ICON_KAMIKAZE		0xC


	class INGAME_UNITS :	public ObjectSync,			// Class to manage huge number of units during the game
	protected IInterface,				// It inherits from what we need to use threads
	public Thread
	{
		virtual const char *className() { return "INGAME_UNITS"; }
	public:
		typedef std::vector< std::list< uint16 > >  RepairPodsList;
	public:
		/*----------------------- Variables générales ----------------------------------------------*/
		uint16	nb_unit;		// Nombre d'unités
		uint16	max_unit;		// Nombre maximum d'unités stockables dans le tableau
		Unit	*unit;			// Tableau contenant les références aux unités
		uint16	index_list_size;
		uint16	*idx_list;
		uint16	*free_idx;
		uint16	free_index_size[10];
		RepairPodsList repair_pads;		// Lists of built repair pads

		/*----------------------- Variables réservées au joueur courant ----------------------------*/

		float	nb_attacked;
		float	nb_built;

		uint8	page;

		/*----------------------- Variables reserved to texture data -------------------------------*/

		GLuint	icons[13];

		/*----------------------- Variables reserved to precalculations ----------------------------*/

		float	exp_dt_1;
		float	exp_dt_2;
		float	exp_dt_4;
		float	g_dt;
		int		sound_min_ticks;

		/*----------------------- Variables reserved to thread management --------------------------*/

		bool	thread_running;
		bool	thread_ask_to_stop;
		bool	wind_change;
		MAP		*map;
		uint32	next_unit_ID;			// Used to make it unique
		uint32	current_tick;
		uint32	client_tick[10];
		uint32	client_speed[10];
		float	apparent_timefactor;
		uint32	last_tick[5];
		sint32	last_on;				// Indicate the unit index which was under the cursor (mini map orders)

		std::vector< uint16 >               visible_unit;   // A list to store visible units
		std::vector< std::list< uint16 > >	requests;		// Store all the request for pathfinder calls

	public:

		float		*mini_pos;			// Position on mini map
		uint32		*mini_col;			// Colors of units

	private:
		uint32		InterfaceMsg(const lpcImsg msg);	// Manage signals sent through the interface to unit manager

	protected:
		void	proc(void*);
		void	signalExitThread();

	public:

		void set_wind_change();

		void init(bool register_interface = false);

		INGAME_UNITS();

		void destroy(bool delete_interface = true);

		~INGAME_UNITS() {destroy(false);}

		void kill(int index,MAP *map,int prev,bool sync = true);			// Détruit une unité

		void draw(MAP *map, bool underwater = false, bool limit = false, bool cullface = true, bool height_line = true); // Dessine les unités visibles

		void draw_shadow(const Vector3D& Dir, MAP* map, float alpha = 0.5f); // Dessine les ombres des unités visibles

		void draw_mini(float map_w, float map_h, int mini_w, int mini_h, SECTOR** map_data); // Repère les unités sur la mini-carte

		void move(float dt, MAP* map = NULL, int key_frame = 0, bool wind_change = false);

		int create(int type_id,int owner);

		/*!
		** \brief Select all units from a user selection
		**
		** \param cam The camera
		** \param pos The user selection, from the mouse coordinates
		** /return True if at least one unit has been selected
		*/
		bool selectUnits(const RectTest &reigon);

		int pick(Camera& cam,int sensibility=1);

		int pick_minimap();

		void give_order_move(int player_id, const Vector3D& target, bool set = true, byte flags = 0);

		void give_order_patrol(int player_id, const Vector3D& target, bool set = true);

		void give_order_guard(int player_id, int target, bool set = true);

		void give_order_unload(int player_id, const Vector3D& target,bool set = true);

		void give_order_load(int player_id,int target,bool set = true);

		void give_order_build(int player_id, int unit_type_id, const Vector3D& target, bool set = true);

		bool remove_order(int player_id, const Vector3D& target);

		void complete_menu(int index, bool hide_info = false, bool hide_bpic = false);

	}; // class INGAME_UNITS





	extern INGAME_UNITS units;

	bool can_be_built(const Vector3D& Pos, MAP *map, const int unit_type_id, const int player_id );

	bool can_be_there( const int px, const int py, MAP *map, const int unit_type_id, const int player_id, const int unit_id = -1 );

	bool can_be_there_ai( const int px, const int py, MAP *map, const int unit_type_id, const int player_id, const int unit_id = -1, const bool leave_space = false );


} // namespace TA3D


#endif
