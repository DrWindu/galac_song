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

#include "main_state.h"


#define ONE_SEC (1000000000)


const float TICK_LENGTH_IN_SEC = 1.f / float(TICKS_PER_SEC);


void dumpEntityTree(Logger& log, EntityRef e, unsigned indent = 0) {
	log.info(std::string(indent * 2u, ' '), e.name(), ": ", e.isEnabled());
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
      _jumpInput(nullptr),
      _dashInput(nullptr)
{

	_entities.registerComponentManager(&_sprites);
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
	_jumpInput  = _inputs.addInput("jump");
	_dashInput  = _inputs.addInput("dash");

	_inputs.mapScanCode(_quitInput,  SDL_SCANCODE_ESCAPE);
	_inputs.mapScanCode(_leftInput,  SDL_SCANCODE_LEFT);
	_inputs.mapScanCode(_rightInput, SDL_SCANCODE_RIGHT);
	_inputs.mapScanCode(_jumpInput,  SDL_SCANCODE_SPACE);
	_inputs.mapScanCode(_jumpInput,  SDL_SCANCODE_X);
	_inputs.mapScanCode(_dashInput,  SDL_SCANCODE_Z);

	// TODO: load stuff.
	loadEntities("entities.ldl", _entities.root());

//	AssetSP tileMapAsset = loader()->load<TileMapLoader>("map.json")->asset();
//	_tileMap = tileMapAsset->aspect<TileMapAspect>();

//	_tileLayer = _entities.createEntity(_entities.root(), "tile_layer");
//	TileLayerComponent* tileLayerComp = _tileLayers.addComponent(_tileLayer);
//	tileLayerComp->setTileMap(_tileMap);
//	_tileLayer.place(Vector3(120, 90, .5));

	_models      = _entities.findByName("__models__");
	_playerModel = _entities.findByName("player_model", _models);

	_scene       = _entities.findByName("scene");
	_gui         = _entities.findByName("gui");

//	loader()->load<SoundLoader>("sound.ogg");
//	//loader()->load<MusicLoader>("music.ogg");

	loader()->waitAll();

	// Set to true to debug OpenGL calls
	renderer()->context()->setLogCalls(false);

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

	_player = _entities.cloneEntity(_playerModel, _scene, "player");
	_player.placeAt(Vector3(window()->width() / 2.f, window()->height() / 2.f, 0));
	_playerVelocity.setZero();
	_playerMoveDir  = DIR_NONE;
	_jumpDuration   = 0;
	_jumpCount      = 0;
	_wallJumpDir    = DIR_NONE;
	_dashDuration   = 9999;
	_playerDir      = DIR_RIGHT;
	_playerAnim     = 0;
	_playerAnimTime = 0;

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

	// Player movement
	float accelTime    =  0.1 * TICKS_PER_SEC;
	float maxSpeed     =  8   * TILE_SIZE * TICK_LENGTH_IN_SEC;
	float playerAccel  = maxSpeed / accelTime;
	float airControl   =  0.5 * playerAccel;

	int   numJumps     = 2;
	int   jumpTicks    = 10;
	float gravity      = 24 * TILE_SIZE * TICK_LENGTH_IN_SEC * TICK_LENGTH_IN_SEC;
	float jumpSpeed    = 16 * TILE_SIZE * TICK_LENGTH_IN_SEC;
//	float jumpHeight   =  3.1   * TILE_SIZE;
//	float jumpSpeed    = .5 * (gravity + sqrt(gravity * gravity + 8 * gravity * jumpHeight));
	float jumpAccel    = jumpSpeed / jumpTicks;
	float maxFallSpeed = jumpSpeed;

	float wallJumpAccel = 0.5 * jumpAccel;
	float maxWallFallSpeed = 0.25 * maxFallSpeed;

	int   numDashes = 1;
	int   dashTicks = 15;
	float dashSpeed = 32   * TILE_SIZE * TICK_LENGTH_IN_SEC;

	float minx = 24;
	float maxx = window()->width() - 24;
	float miny = 0;
	float maxy = window()->height() - 48;

	Vector2 playerPos = _player.position2();

	unsigned playerHit = 0;
	if(playerPos(0) <= minx)
		playerHit |= DIR_LEFT;
	if(playerPos(0) >= maxx)
		playerHit |= DIR_RIGHT;
	if(playerPos(1) <= miny)
		playerHit |= DIR_DOWN;
	if(playerPos(1) >= maxy)
		playerHit |= DIR_UP;

//	log().info(_loop.tickCount(), ": p: ", playerPos.transpose(), ", v: ", _playerVelocity.transpose(),
//	           ", h: ", playerHit);

	bool onGround = playerHit & DIR_DOWN;
	bool onWall   = playerHit & (DIR_LEFT | DIR_RIGHT);
	bool isDashing = _dashDuration < dashTicks;

	bool moveLeft  = _leftInput->isPressed();
	bool moveRight = _rightInput->isPressed();
	if(_leftInput->justPressed() || (moveLeft && !moveRight))
		_playerMoveDir = DIR_LEFT;
	else if(_rightInput->justPressed() || (!moveLeft && moveRight))
		_playerMoveDir = DIR_RIGHT;
	else if(!moveLeft && !moveRight)
		_playerMoveDir = DIR_NONE;

	if(!isDashing && _playerMoveDir != DIR_NONE)
		_playerDir = _playerMoveDir;

	if(!isDashing && !onGround && onWall) {
		_playerDir = (playerHit & DIR_LEFT)? DIR_RIGHT: DIR_LEFT;
	}

	if(playerHit & (DIR_LEFT | DIR_RIGHT)) {
		_dashDuration = dashTicks;
	}

	if(_dashInput->justPressed() && _dashCount > 0) {
		_dashDuration = 0;
		_dashCount -= 1;
	}

	if(_dashDuration < dashTicks) {
		playerPos(0)  += (_playerDir == DIR_LEFT)? -dashSpeed: dashSpeed;
		_playerVelocity(0) = (_playerDir == DIR_LEFT)? -maxSpeed: maxSpeed;
		_playerVelocity(1) = 0;
		_dashDuration += 1;
	}
	else {
		Vector2 acceleration = Vector2::Zero();

		float targetXSpeed = (_playerMoveDir == DIR_LEFT)?  -maxSpeed:
							 (_playerMoveDir == DIR_RIGHT)?  maxSpeed: 0.f;
		float diff = targetXSpeed - _playerVelocity(0);
		if(_wallJumpDir == DIR_NONE && (
					onGround ||
					(_playerMoveDir == DIR_LEFT  && _playerVelocity(0) > -maxSpeed) ||
					(_playerMoveDir == DIR_RIGHT && _playerVelocity(0) <  maxSpeed))) {
			float accel = onGround? playerAccel: airControl;
			if(diff < 0)
				acceleration(0) = std::max(-accel, diff);
			else
				acceleration(0) = std::min( accel, diff);
		}

		if(onGround || onWall) {
			_jumpCount = numJumps;
			_wallJumpDir = DIR_NONE;
			_dashCount = numDashes;
		}
		if(onGround) {
			_jumpDuration = jumpTicks;
		}

		static float maxHeight = 0;
		maxHeight = std::max(maxHeight, playerPos(1));
		acceleration(1) = std::max(-gravity, -(onWall? maxWallFallSpeed: maxFallSpeed) - _playerVelocity(1));
		if(_jumpInput->justPressed() && _jumpCount > 0) {
			maxHeight = 0;
			_playerVelocity(1) = 0;
			_wallJumpDir = onGround?                DIR_NONE:
						   (playerHit & DIR_LEFT)?  DIR_RIGHT:
						   (playerHit & DIR_RIGHT)? DIR_LEFT: DIR_NONE;
			_jumpDuration = 0;
			_jumpCount -= 1;
		}

		if(_jumpDuration < jumpTicks) {
			if(_jumpInput->isPressed()) {
				_playerVelocity(1) += jumpAccel;
				if(_wallJumpDir == DIR_LEFT)
					_playerVelocity(0) -= wallJumpAccel;
				if(_wallJumpDir == DIR_RIGHT)
					_playerVelocity(0) += wallJumpAccel;
				_jumpDuration += 1;
			}
			else {
				_jumpDuration = jumpTicks;
			}
		}

		if(_jumpDuration >= jumpTicks) {
			_wallJumpDir = DIR_NONE;
		}

		log().info("Jump: c: ", _jumpCount, ", d: ", _jumpDuration, ", w", _wallJumpDir, ", h: ", playerHit);

		_playerVelocity += acceleration;
		playerPos       += _playerVelocity;
	}

	playerHit = 0;
	if(playerPos(0) <= minx)
		playerHit |= DIR_LEFT;
	if(playerPos(0) >= maxx)
		playerHit |= DIR_RIGHT;
	if(playerPos(1) <= miny)
		playerHit |= DIR_DOWN;
	if(playerPos(1) >= maxy)
		playerHit |= DIR_UP;

	if(playerHit & DIR_LEFT) {
		playerPos(0) = minx;
		_playerVelocity(0) = std::max(_playerVelocity(0), 0.f);
	}
	if(playerHit & DIR_RIGHT) {
		playerPos(0) = maxx;
		_playerVelocity(0) = std::min(_playerVelocity(0), 0.f);
	}
	if(playerHit & DIR_DOWN) {
		playerPos(1) = miny;
		_playerVelocity(1) = std::max(_playerVelocity(1), 0.f);
	}
	if(playerHit & DIR_UP) {
		playerPos(1) = maxy;
		_playerVelocity(1) = std::min(_playerVelocity(1), 0.f);
	}

	SpriteComponent* playerSprite = _sprites.get(_player);
	playerSprite->setTileIndex((_playerDir == DIR_LEFT)? 12: 8);

	_player.moveTo(playerPos);

	_entities.updateWorldTransforms();
}


void MainState::updateFrame() {
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
	             Vector3(window()->width(), // Big pixels
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
