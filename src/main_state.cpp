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


#include <functional>

#include <lair/core/json.h>

#include "game.h"
#include "level.h"

#include "main_state.h"


#define ONE_SEC (1000000000)


const float TICK_LENGTH_IN_SEC = 1.f / float(TICKS_PER_SEC);


void dumpEntityTree(Logger& log, EntityRef e, unsigned indent = 0) {
	log.info(std::string(indent * 2u, ' '), e.name(), ": ", e.isEnabled(), ", ", e.position3().transpose());
	EntityRef c = e.firstChild();
	while(c.isValid()) {
		dumpEntityTree(log, c, indent + 1);
		c = c.nextSibling();
	}
}


MainState::MainState(Game* game)
	: GameState(game),

      _mainPass(renderer()),

      _entities(log(), _game->serializer()),
      _spriteRenderer(renderer()),
      _sprites(assets(), loader(), &_mainPass, &_spriteRenderer),
      _collisions(),
      _characters(&_collisions),
      _texts(loader(), &_mainPass, &_spriteRenderer),
      _tileLayers(loader(), &_mainPass, &_spriteRenderer),

      _inputs(sys(), &log()),

      _camera(),

      _initialized(false),
      _running(false),
      _loop(sys()),
      _fpsTime(0),
      _fpsCount(0),

      _quitInput(nullptr),
      _leftInput(nullptr),
      _rightInput(nullptr),
      _downInput(nullptr),
      _upInput(nullptr),
      _jumpInput(nullptr),
      _dashInput(nullptr)
{
	_entities.registerComponentManager(&_sprites);
	_entities.registerComponentManager(&_collisions);
	_entities.registerComponentManager(&_characters);
	_entities.registerComponentManager(&_texts);
	_entities.registerComponentManager(&_tileLayers);
}


MainState::~MainState() {
}


void MainState::initialize() {
	_loop.reset();
	_loop.setTickDuration(    ONE_SEC /  TICKS_PER_SEC);
	_loop.setFrameDuration(   ONE_SEC /  FRAMES_PER_SEC);
	_loop.setMaxFrameDuration(_loop.frameDuration() * 3);
	_loop.setFrameMargin(     _loop.frameDuration() / 2);

	window()->onResize.connect(std::bind(&MainState::resizeEvent, this))
	        .track(_slotTracker);

	_quitInput  = _inputs.addInput("quit");
	_leftInput  = _inputs.addInput("left");
	_rightInput = _inputs.addInput("right");
	_downInput = _inputs.addInput("down");
	_upInput = _inputs.addInput("up");
	_jumpInput  = _inputs.addInput("jump");
	_dashInput  = _inputs.addInput("dash");

	_inputs.mapScanCode(_quitInput,  SDL_SCANCODE_ESCAPE);
	_inputs.mapScanCode(_leftInput,  SDL_SCANCODE_LEFT);
	_inputs.mapScanCode(_rightInput, SDL_SCANCODE_RIGHT);
	_inputs.mapScanCode(_downInput, SDL_SCANCODE_DOWN);
	_inputs.mapScanCode(_upInput, SDL_SCANCODE_UP);
	_inputs.mapScanCode(_jumpInput,  SDL_SCANCODE_SPACE);
	_inputs.mapScanCode(_jumpInput,  SDL_SCANCODE_X);
	_inputs.mapScanCode(_dashInput,  SDL_SCANCODE_Z);

	// TODO: load stuff.
	loadEntities("entities.ldl", _entities.root());

	_models      = _entities.findByName("__models__");
	_playerModel = _entities.findByName("player_model", _models);

	_scene       = _entities.findByName("scene");
	_gui         = _entities.findByName("gui");

	_level.reset(new Level(this, "test_map.json"));
	_level->preload();

//	loader()->load<SoundLoader>("sound.ogg");
//	//loader()->load<MusicLoader>("music.ogg");

	loader()->waitAll();

	// Set to true to debug OpenGL calls
	renderer()->context()->setLogCalls(false);

	// Physics !
	_playerPhysics.reset(new CharPhysicsParams);

	float tileSize = TILE_SIZE * 2;

	_playerPhysics->accelTime    =  0.1 * TICKS_PER_SEC;
	_playerPhysics->maxSpeed     =  8   * tileSize * TICK_LENGTH_IN_SEC;
	_playerPhysics->playerAccel  = _playerPhysics->maxSpeed / _playerPhysics->accelTime;
	_playerPhysics->airControl   =  0.5 * _playerPhysics->playerAccel;

	_playerPhysics->numJumps     = 2;
	_playerPhysics->jumpTicks    = 10;
	_playerPhysics->gravity      = 24 * tileSize * TICK_LENGTH_IN_SEC * TICK_LENGTH_IN_SEC;
	_playerPhysics->jumpSpeed    = 16 * tileSize * TICK_LENGTH_IN_SEC;
	_playerPhysics->jumpAccel    = _playerPhysics->jumpSpeed / _playerPhysics->jumpTicks;
	_playerPhysics->maxFallSpeed = _playerPhysics->jumpSpeed;

	_playerPhysics->wallJumpAccel = 0.5 * _playerPhysics->jumpAccel;
	_playerPhysics->maxWallFallSpeed = 0.25 * _playerPhysics->maxFallSpeed;

	_playerPhysics->numDashes = 1;
	_playerPhysics->dashTicks = 15;
	_playerPhysics->dashSpeed = 32   * tileSize * TICK_LENGTH_IN_SEC;

	_initialized = true;
}


void MainState::shutdown() {
	_slotTracker.disconnectAll();

	_initialized = false;
}


void MainState::run() {
	lairAssert(_initialized);

	log().log("Starting main state...");
	_running = true;
	_loop.start();
	_fpsTime  = int64(sys()->getTimeNs());
	_fpsCount = 0;

	startGame();

	do {
		switch(_loop.nextEvent()) {
		case InterpLoop::Tick:
			updateTick();
			break;
		case InterpLoop::Frame:
			updateFrame();
			break;
		}
	} while (_running);
	_loop.stop();
}


void MainState::quit() {
	_running = false;
}


Game* MainState::game() {
	return static_cast<Game*>(_game);
}


void MainState::startGame() {
	// TODO: Setup game

	while(_scene.firstChild().isValid())
		_scene.firstChild().destroy();

	_level->initialize();
	_characters.setLevel(_level.get());

	_player = _entities.cloneEntity(_playerModel, _scene, "player");
	CharacterComponent* pChar = _characters.addComponent(_player);
	pChar->physics = _playerPhysics;

	_level->start("spawn");

	dumpEntityTree(log(), _entities.root());

	//audio()->playMusic(assets()->getAsset("music.ogg"));
//	audio()->playSound(assets()->getAsset("sound.ogg"), 2);
}


void MainState::updateTick() {
	loader()->finalizePending();

	_inputs.sync();
	_entities.setPrevWorldTransforms();

	if(_quitInput->justPressed()) {
		quit();
	}

	// Player input
	CharacterComponent* pChar = _characters.get(_player);
	if(_leftInput->isPressed())
		pChar->pressMove(DIR_LEFT);
	if(_rightInput->isPressed())
		pChar->pressMove(DIR_RIGHT);
	if(_downInput->isPressed())
		pChar->pressMove(DIR_DOWN);
	if(_upInput->isPressed())
		pChar->pressMove(DIR_UP);
	pChar->pressJump(_jumpInput->isPressed());
	pChar->pressDash(_dashInput->justPressed());

	// Update components
	_characters.updatePhysics();

	_entities.updateWorldTransforms();
	_collisions.findCollisions();
//	for(const HitEvent& hit: _collisions.hitEvents()) {
//		log().info(_loop.tickCount(), ": hit ", hit.entities[0].name(),
//		        ", ", hit.entities[1].name(), ", ", hit.penetration.transpose());
//	}

	_characters.processCollisions();

	_entities.updateWorldTransforms();
}


void MainState::updateFrame() {
	// Update camera

	Vector3 c;
	c << _player.interpPosition2(_loop.frameInterp()), .5;
	Vector3 h(960, 540, .5);
	Box3 viewBox(c - h, c + h);
	_camera.setViewBox(viewBox);

	// Rendering
	Context* glc = renderer()->context();

	_texts.createTextures();
	_tileLayers.createTextures();
	renderer()->uploadPendingTextures();

	glc->clear(gl::COLOR_BUFFER_BIT | gl::DEPTH_BUFFER_BIT);

	_mainPass.clear();
	_spriteRenderer.clear();

	_sprites.render(_entities.root(), _loop.frameInterp(), _camera);
	_texts.render(_entities.root(), _loop.frameInterp(), _camera);
	_tileLayers.render(_entities.root(), _loop.frameInterp(), _camera);

	_mainPass.render();

	window()->swapBuffers();
	glc->setLogCalls(false);

	int64 now = int64(sys()->getTimeNs());
	++_fpsCount;
	if(_fpsCount == 60) {
		log().info("Fps: ", _fpsCount * float(ONE_SEC) / (now - _fpsTime));
		_fpsTime  = now;
		_fpsCount = 0;
	}
}


void MainState::resizeEvent() {
	Box3 viewBox(Vector3::Zero(),
	             Vector3(window()->width(),
	                     window()->height(), 1));
	_camera.setViewBox(viewBox);
}


bool MainState::loadEntities(const Path& path, EntityRef parent, const Path& cd) {
	Path localPath = makeAbsolute(cd, path);
	log().info("Load entity \"", localPath, "\"");

	Path realPath = game()->dataPath() / localPath;
	Path::IStream in(realPath.native().c_str());
	if(!in.good()) {
		log().error("Unable to read \"", localPath, "\".");
		return false;
	}
	ErrorList errors;
	LdlParser parser(&in, localPath.utf8String(), &errors, LdlParser::CTX_MAP);

	bool success = _entities.loadEntitiesFromLdl(parser, parent);

	errors.log(log());

	return success;
}
