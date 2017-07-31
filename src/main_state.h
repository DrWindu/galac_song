/*
 *  Copyright (C) 2015, 2017 the authors (see AUTHORS)
 *
 *  This file is part of Draklia's ld39.
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


#ifndef LD39_MAIN_STATE_H_
#define LD39_MAIN_STATE_H_


#include <lair/core/signal.h>

#include <lair/utils/game_state.h>
#include <lair/utils/interp_loop.h>
#include <lair/utils/input.h>
#include <lair/utils/tile_map.h>

#include <lair/render_gl2/orthographic_camera.h>
#include <lair/render_gl2/render_pass.h>

#include <lair/ec/entity.h>
#include <lair/ec/entity_manager.h>
#include <lair/ec/collision_component.h>
#include <lair/ec/sprite_component.h>
#include <lair/ec/bitmap_text_component.h>
#include <lair/ec/tile_layer_component.h>

#include "components.h"


using namespace lair;


class Game;
class Level;
class MainState;

typedef std::shared_ptr<Level> LevelSP;
typedef std::unordered_map<Path, LevelSP, boost::hash<Path>> LevelMap;

enum {
	TICKS_PER_SEC  = 60,
	FRAMES_PER_SEC = 60,
};

extern const float TICK_LENGTH_IN_SEC;

typedef int (*Command)(MainState* state, EntityRef self, int argc, const char** argv);
typedef std::unordered_map<std::string, Command> CommandMap;

struct CommandExpr {
	String    command;
	EntityRef self;
};
typedef std::deque<CommandExpr> CommandList;


enum State {
	STATE_PLAY,
	STATE_DEATH,
};


class MainState : public GameState {
public:
	MainState(Game* game);
	virtual ~MainState();

	virtual void initialize();
	virtual void shutdown();

	virtual void run();
	virtual void quit();

	Game* game();

	void exec(const std::string& cmd, EntityRef self = EntityRef());
	void exec(const CommandList& commands);
	void execNext();
	int execSingle(const std::string& cmd, EntityRef self = EntityRef());
	int exec(int argc, const char** argv, EntityRef self = EntityRef());

	LevelSP registerLevel(const Path& level);
	void loadLevel(const Path& level, const String& spawn = "spawn");
	void startLevel(const Path& level, const String& spawn = "spawn");

	EntityRef getEntity(const String& name, const EntityRef& ancestor = EntityRef());
	EntityRef createTrigger(EntityRef parent, const char* name, const Box2& box);

	void updateTriggers(bool disableCmds = false);

	void killPlayer();

	void startGame();
	void updateTick();
	void updateFrame();

	void resizeEvent();

	bool loadEntities(const Path& path, EntityRef parent = EntityRef(),
	                  const Path& cd = Path());

public:
	// More or less system stuff

	RenderPass                 _mainPass;

	EntityManager              _entities;
	SpriteRenderer             _spriteRenderer;
	SpriteComponentManager     _sprites;
	CollisionComponentManager  _collisions;
	TriggerComponentManager    _triggers;
	CharacterComponentManager  _characters;
	BitmapTextComponentManager _texts;
	TileLayerComponentManager  _tileLayers;
//	AnimationComponentManager  _anims;
	InputManager               _inputs;

	SlotTracker _slotTracker;

	OrthographicCamera _camera;

	bool        _initialized;
	bool        _running;
	InterpLoop  _loop;
	int64       _fpsTime;
	unsigned    _fpsCount;

	CommandMap  _commands;
	CommandList _commandList;

	Input*      _quitInput;
	Input*      _leftInput;
	Input*      _rightInput;
	Input*      _downInput;
	Input*      _upInput;
	Input*      _jumpInput;
	Input*      _dashInput;

	State    _state;
	float    _transitionTime;

	LevelMap _levelMap;
	LevelSP  _level;
	String   _spawnName;
	Path     _nextLevel;
	String   _nextLevelSpawn;

	EntityRef   _models;

	EntityRef   _playerModel;
	CharPhysicsParamsSP _playerPhysics;
	EntityRef   _playerDeathModel;

	EntityRef   _tileLayer;

	EntityRef   _background;
	EntityRef   _scene;
	EntityRef   _player;
	EntityRef   _playerDeath;

	EntityRef   _gui;
};


#endif
