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


#ifndef LD39_SPLASH_STATE_H_
#define LD39_SPLASH_STATE_H_


#include <deque>

#include <lair/core/signal.h>

#include <lair/utils/game_state.h>
#include <lair/utils/interp_loop.h>
#include <lair/utils/input.h>

#include <lair/render_gl2/orthographic_camera.h>
#include <lair/render_gl2/render_pass.h>

#include <lair/ec/entity.h>
#include <lair/ec/entity_manager.h>
#include <lair/ec/sprite_component.h>
#include <lair/ec/bitmap_text_component.h>


using namespace lair;


class Game;

typedef std::deque<Path> PathQueue;

class SplashState : public GameState {
public:
	SplashState(Game* game);
	virtual ~SplashState();

	virtual void initialize();
	virtual void shutdown();

	virtual void run();
	virtual void quit();

	Game* game();

	void setNextState(GameState* nextState);
	void addSplash(const Path& splashImage);
	bool nextSplash();
//	void setup(GameState* nextState, const Path& splashImage, float skipTime = 1.e20);
	void updateTick();
	void updateFrame();

	void resizeEvent();

protected:
	// More or less system stuff

	EntityManager              _entities;
	RenderPass                 _renderPass;
	SpriteRenderer             _spriteRenderer;
	SpriteComponentManager     _sprites;
	BitmapTextComponentManager _texts;
	InputManager               _inputs;

	SlotTracker _slotTracker;

	OrthographicCamera _camera;

	bool        _initialized;
	bool        _running;
	InterpLoop  _loop;
	int64       _fpsTime;
	unsigned    _fpsCount;

	Input*      _skipInput;

	float       _skipTime;
	GameState*  _nextState;
	PathQueue   _splashQueue;
	EntityRef   _splash;
};


#endif
