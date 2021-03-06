-- Arm Commander KBOT Script

createUnitScript("armcom")
__this:piece("torso","ruparm","luparm","rbigflash","nanospray","pelvis","lfirept","rthigh","rleg","lthigh","lleg","head")

-- State variables
__this.bMoving = false
__this.bAiming = false
__this.bCanAim = false
__this.aimtype = 0

#include "exptype.lh"
#include "walk.lh"
#include "walklegs.lh"

-- Aiming definitions

__this.AIM_NONE = 0
__this.AIM_DGUN = 1
__this.AIM_LASER = 2
__this.AIM_NANO = 3
__this.AIM_RESTORE = 4

__this.MotionControl = function(this)
	-- So the stand will get reset
	local just_moved = true

	while true do
		local moving = this.bMoving
		local aiming = this.bAiming

		if moving then
			if aiming then
				this.bCanAim = true
				this:walklegs()
			else

				this.bCanAim = false
				this:walk()
			end

			just_moved = true

		else
			this.bCanAim = true

			if just_moved then
				this:move( this.pelvis, y_axis, 0, 1)

				this:turn( this.rthigh, x_axis, 0, 200)
				this:turn( this.rleg, x_axis, 0, 200)

				this:turn( this.lthigh, x_axis, 0, 200)
				this:turn( this.lleg, x_axis, 0, 200)

				if not aiming then
					this:turn( this.torso, x_axis, 0, 90)
				end

				just_moved = false
			end
			this:sleep( 0.1 )
		end
	end
end

__this.Create = function(this)
	this:hide( this.rbigflash )
	this:hide( this.lfirept )
	this:hide( this.nanospray )

	-- Initial State
	this.bMoving = false
	this.bAiming = false
	this.bCanAim = true

	this.aimtype = this.AIM_NONE

	-- Motion control system
	this:start_script( this.MotionControl, this )
end

__this.StartMoving = function(this)
	this.bMoving = true
end

__this.StopMoving = function(this)
	this.bMoving = false
end

__this.SweetSpot = function(this)
	return this.torso
end

__this.QueryNanoPiece = function(this)
	return this.nanospray
end

__this.RestorePosition = function(this)
    if this.aimtype == this.AIM_RESTORE then
        return
    end
	this.aimtype = this.AIM_RESTORE;

	this:turn( this.torso, y_axis, 0, 90 )
	this:turn( this.ruparm, x_axis, 0, 45 )
	this:turn( this.luparm, x_axis, 0, 45 )

	while (this:is_turning( this.torso, y_axis ) or this:is_turning( this.ruparm, x_axis ) or this:is_turning( this.luparm, x_axis )) and this.aimtype == this.AIM_RESTORE do
        this:yield()
    end

    if this.aimtype == this.AIM_RESTORE then
        this.aimtype = this.AIM_NONE;
        this.bAiming = false
    end
end

__this.AimFromPrimary = function(this)
	return this.lfirept
end

__this.QueryPrimary = function(this)
	return this.lfirept
end

__this.AimPrimary = function(this, heading, pitch)
	this:set_script_value("AimPrimary", false)

	-- Don't override the big gun
	if this.aimtype == this.AIM_DGUN then
	  return
	end

    if this.bAiming and not this.bCanAim then
        return
    end

	heading = heading * TA2DEG
	pitch = pitch * TA2DEG

	-- Announce that we would like to aim, and wait until we can
	this.bAiming = true
	while not this.bCanAim do
        this.bAiming = true
		this:yield()
	end

	-- Aim
	this:turn( this.torso, y_axis, heading, 300 )
	this:turn( this.luparm, x_axis, -pitch - 30, 45 )

    if this.aimtype == this.AIM_LASER then
        return
    end

    this.aimtype = this.AIM_LASER

	while (this:is_turning( this.torso, y_axis ) or this:is_turning( this.luparm, x_axis )) and this.aimtype == this.AIM_LASER do
        this:yield()
    end

    if this.aimtype == this.AIM_LASER then
        this:set_script_value("AimPrimary", true)
        this.aimtype = this.AIM_NONE
        this.bAiming = false
    end
end

__this.FirePrimary = function(this)
	-- Muzzle flare
	this:show( this.lfirept )
	this:sleep( 0.1 )
	this:hide( this.lfirept )
end

__this.AimFromTertiary = function(this)
	return this.torso
end

__this.QueryTertiary = function(this)
	return this.rbigflash
end

__this.AimTertiary = function(this, heading, pitch)
	this:set_script_value("AimTertiary", false)

    if this.bAiming and not this.bCanAim then
        return
    end

	-- Announce that we would like to aim, and wait until we can
	this.bAiming = true
	while not this.bCanAim do
        this.bAiming = true
		this:yield()
	end

	heading = heading * TA2DEG
	pitch = pitch * TA2DEG

	-- Aim
	this:turn( this.torso, y_axis, heading, 300 )
	this:turn( this.ruparm, x_axis, - pitch - 24, 45 )

    if this.aimtype == this.AIM_DGUN then
        return
    end

    this.aimtype = this.AIM_DGUN

	while (this:is_turning( this.torso, y_axis ) or this:is_turning( this.ruparm, x_axis )) and this.aimtype == this.AIM_DGUN do
        this:yield()
    end

    if this.aimtype == this.AIM_DGUN then
        this:set_script_value("AimTertiary", true)
        this.aimtype = this.AIM_NONE
        this.bAiming = false
    end
end

__this.FireTertiary = function(this)
	-- Muzzle flare
	this:show( this.rbigflash )
	this:sleep( 0.1 )
	this:hide( this.rbigflash )
end

__this.StartBuilding = function(this, heading, pitch)
    if this.bAiming and not this.bCanAim then
        return
    end

	-- Announce that we would like to aim, and wait until we can
	this.bAiming = true
	while not this.bCanAim do
        this.bAiming = true
		this:yield()
	end

	heading = heading * TA2DEG
	pitch = pitch * TA2DEG

	-- Aim
	this:turn( this.torso, y_axis, heading, 300 )
	this:turn( this.luparm, x_axis, -pitch - 30, 45 )
    if this.aimtype == this.AIM_NANO then
        return
    end

    this.aimtype = this.AIM_NANO
	while (this:is_turning( this.torso, y_axis ) or this:is_turning( this.luparm, x_axis )) and this.aimtype == this.AIM_NANO do
        this:yield()
    end

	-- Announce that we are ready
    if this.aimtype == this.AIM_NANO then
        this.aimtype = this.AIM_NONE
        this:set( INBUILDSTANCE, true )
        this.bAiming = false
    end
end

__this.TargetCleared = function(this, which)
	this:RestorePosition()
end

__this.StopBuilding = function(this)
	-- We are no longer in a position to build
	this:set( INBUILDSTANCE, false )

	this:RestorePosition()
end
