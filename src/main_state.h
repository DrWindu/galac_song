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
#include <lair/ec/sprite_component.h>
#include <lair/ec/bitmap_text_component.h>
#include <lair/ec/tile_layer_component.h>


using namespace lair;


class Game;

enum {
	TILE_SIZE      = 48,
	TICKS_PER_SEC  = 60,
	FRAMES_PER_SEC = 60,
};

extern const float TICK_LENGTH_IN_SEC;

enum HitFlags {
	HIT_SOLID     = 0x01,
	HIT_PLAYER    = 0x02,
	HIT_CHARACTER = 0x04,
	HIT_TRIGGER   = 0x08,
};

enum Direction {
	DIR_NONE  = 0x00,
	DIR_RIGHT = 0x01,
	DIR_UP    = 0x02,
	DIR_LEFT  = 0x04,
	DIR_DOWN  = 0x08,
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

	void startGame();
	void updateTick();
	void updateFrame();

	void resizeEvent();

	bool loadEntities(const Path& path, EntityRef parent = EntityRef(),
	                  const Path& cd = Path());

protected:
	// More or less system stuff

	RenderPass                 _mainPass;

	EntityManager              _entities;
	SpriteRenderer             _spriteRenderer;
	SpriteComponentManager     _sprites;
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

	Input*      _quitInput;
	Input*      _leftInput;
	Input*      _rightInput;
	Input*      _jumpInput;
	Input*      _dashInput;

	TileMapAspectSP _tileMap;

	EntityRef   _models;

	EntityRef   _playerModel;
	Vector2     _playerVelocity;
	Direction   _playerMoveDir;
	int         _jumpDuration;
	int         _jumpCount;
	Direction   _wallJumpDir;
	int         _dashDuration;
	int         _dashCount;
	Direction   _playerDir;
	unsigned    _playerAnim;
	float       _playerAnimTime;

	EntityRef   _tileLayer;

	EntityRef   _scene;
	EntityRef   _player;

	EntityRef   _gui;
};


#endif
