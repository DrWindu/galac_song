/*
 *  Copyright (C) 2016-2017 the authors (see AUTHORS)
 *
 *  This file is part of ld36.
 *
 *  lair is free software: you can redistribute it and/or modify it
 *  under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  lair is distributed in the hope that it will be useful, but
 *  WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with lair.  If not, see <http://www.gnu.org/licenses/>.
 *
 */


#ifndef COMPONENTS_H
#define COMPONENTS_H


#include <map>

#include <lair/core/lair.h>
#include <lair/core/path.h>

#include <lair/ec/entity.h>
#include <lair/ec/component.h>
#include <lair/ec/dense_component_manager.h>
#include <lair/ec/collision_component.h>


using namespace lair;

class Level;

enum DirFlags {
	DIR_NONE  = 0x00,
	DIR_UP    = 1 << UP,
	DIR_LEFT  = 1 << LEFT,
	DIR_DOWN  = 1 << DOWN,
	DIR_RIGHT = 1 << RIGHT,
};

class TriggerComponentManager;

class TriggerComponent : public Component {
public:
	typedef TriggerComponentManager Manager;

public:
	TriggerComponent(Manager* manager, _Entity* entity);
	TriggerComponent(const TriggerComponent&)  = delete;
	TriggerComponent(      TriggerComponent&&) = default;
	~TriggerComponent() = default;

	TriggerComponent& operator=(const TriggerComponent&)  = delete;
	TriggerComponent& operator=(      TriggerComponent&&) = default;

	static const PropertyList& properties();

public:
	bool        prevInside;
	bool        inside;
	std::string onEnter;
	std::string onExit;
	std::string onUse;
};

class TriggerComponentManager : public DenseComponentManager<TriggerComponent> {
public:
	TriggerComponentManager();
	virtual ~TriggerComponentManager() = default;
};


class CharacterComponentManager;

struct CharPhysicsParams {
	float accelTime;
	float maxSpeed;
	float playerAccel;
	float airControl;

	int   numJumps;
	int   jumpTicks;
	float gravity;
	float jumpSpeed;
	float jumpAccel;
	float maxFallSpeed;

	float wallJumpAccel;
	float maxWallFallSpeed;

	int   numDashes;
	int   dashTicks;
	float dashSpeed;
};
typedef std::shared_ptr<CharPhysicsParams> CharPhysicsParamsSP;


class CharacterComponent : public Component {
public:
	typedef CharacterComponentManager Manager;

public:
	CharacterComponent(Manager* manager, _Entity* entity);
	CharacterComponent(const CharacterComponent&)  = delete;
	CharacterComponent(      CharacterComponent&&) = default;
	~CharacterComponent() = default;

	CharacterComponent& operator=(const CharacterComponent&)  = delete;
	CharacterComponent& operator=(      CharacterComponent&&) = default;

	void pressMove(unsigned directions);
	void pressJump(bool press = true);
	void pressDash(bool press = true);

	void reset();

	static const PropertyList& properties();

public:
	CharPhysicsParamsSP physics;

	unsigned dirPressed;
	unsigned prevDirPressed;
	bool     jumpPressed;
	bool     prevJumpPressed;
	bool     dashPressed;

	float    penetration[4];
	unsigned touchDir;
	unsigned prevTouchDir;

	Vector2  velocity;

	DirFlags moveDir;
	DirFlags lookDir;

	int      jumpDuration;
	int      jumpCount;
	DirFlags wallJumpDir;

	int      dashDuration;
	int      dashCount;

	std::vector<HitEvent> _hits;
};

class CharacterComponentManager : public DenseComponentManager<CharacterComponent> {
public:
	CharacterComponentManager(CollisionComponentManager* collisions);
	virtual ~CharacterComponentManager() = default;

	inline void setLevel(Level* level) { _level = level; }

	void updatePhysics();
	void processCollisions();

public:
	CollisionComponentManager* _collisions;
	Level* _level;
};


#endif
