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
#include "commands.h"

#include "main_state.h"


#define ONE_SEC (1000000000)


const float TICK_LENGTH_IN_SEC = 1.f / float(TICKS_PER_SEC);
const float FADE_DURATION = .5;

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
      _triggers(),
      _characters(this),
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
      _dashInput(nullptr),

      _state(STATE_PLAY),
      _transitionTime(0)
{
	_entities.registerComponentManager(&_sprites);
	_entities.registerComponentManager(&_collisions);
	_entities.registerComponentManager(&_triggers);
	_entities.registerComponentManager(&_characters);
	_entities.registerComponentManager(&_texts);
	_entities.registerComponentManager(&_tileLayers);

	_commands.emplace("echo",       echoCommand);
	_commands.emplace("set_spawn",  setSpawnCommand);
	_commands.emplace("kill",       killCommand);
	_commands.emplace("next_level", nextLevelCommand);
	_commands.emplace("disable",    disableCommand);
	_commands.emplace("no_jump",    noJumpCommand);
	_commands.emplace("slow",       slowCommand);
	_commands.emplace("credits",    creditsCommand);
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
	_downInput  = _inputs.addInput("down");
	_upInput    = _inputs.addInput("up");
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
	_playerDeathModel = _entities.findByName("player_death_model", _models);

	_background  = _entities.findByName("background");
	_scene       = _entities.findByName("scene");
	_gui         = _entities.findByName("gui");
	_fadeOverlay = _entities.findByName("fade_overlay");

	registerLevel("test_map.json");
	registerLevel("lvl1.json");
	registerLevel("lvl2.json");
	registerLevel("lvl3.json");
	registerLevel("lvl4.json");
	setNextLevel("lvl1.json");

	loadSound("arrival.wav");
	loadSound("dash.wav");
	loadSound("death.wav");
	loadSound("departure.wav");
	loadSound("jump.wav");
	loadSound("land.wav");

	loadMusic("ending.mp3");

	loader()->load<ImageLoader>("battery1.png");
	loader()->load<ImageLoader>("battery2.png");
	loader()->load<ImageLoader>("battery3.png");
	loader()->load<ImageLoader>("battery4.png");

	loader()->waitAll();

	for(auto& pathLevel: _levelMap) {
		LevelSP level = pathLevel.second;
		AssetSP levelAsset = assets()->getAsset(level->path());
		TileMap& tileMap = assets()->getAspect<TileMapAspect>(levelAsset)->_get();

		Path background = tileMap.properties().get("background", "background1.png").asString();
		loader()->load<ImageLoader>(background);

		Path tileset = tileMap.properties().get("tileset", "tileset.png").asString();
		loader()->load<ImageLoader>(tileset);
	}

	loader()->waitAll();

	// Set to true to debug OpenGL calls
	renderer()->context()->setLogCalls(false);

	// Physics !
	_playerPhysics.reset(new CharPhysicsParams);

	float tileSize = TILE_SIZE * 2;

	_playerPhysics->accelTime    =  0.1 * TICKS_PER_SEC;
	_baseMaxSpeed                =  8   * tileSize * TICK_LENGTH_IN_SEC;
	_playerPhysics->maxSpeed     = _baseMaxSpeed;
	_playerPhysics->playerAccel  = _playerPhysics->maxSpeed / _playerPhysics->accelTime;
	_playerPhysics->airControl   =  0.5 * _playerPhysics->playerAccel;

	_playerPhysics->jump         = true;
	_playerPhysics->numJumps     = 1;
	_playerPhysics->jumpTicks    = 10;
//	_playerPhysics->gravity      = 32 * tileSize * TICK_LENGTH_IN_SEC * TICK_LENGTH_IN_SEC;
//	_playerPhysics->jumpSpeed    = 19 * tileSize * TICK_LENGTH_IN_SEC;
	_playerPhysics->gravity      = 48 * tileSize * TICK_LENGTH_IN_SEC * TICK_LENGTH_IN_SEC;
	_playerPhysics->jumpSpeed    = 24.2 * tileSize * TICK_LENGTH_IN_SEC;
	_playerPhysics->jumpAccel    = _playerPhysics->jumpSpeed / _playerPhysics->jumpTicks;
	_playerPhysics->maxFallSpeed = _playerPhysics->jumpSpeed;

	_playerPhysics->wallJump         = true;
	_playerPhysics->wallJumpAccel    = 0.4 * _playerPhysics->jumpAccel;
	_playerPhysics->maxWallFallSpeed = 0.35 * _playerPhysics->maxFallSpeed;

	_playerPhysics->numDashes = 1;
	_playerPhysics->dashTicks = 8;
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


void MainState::exec(const std::string& cmds, EntityRef self) {
	CommandList commands;
	unsigned first = 0;
	for(unsigned ci = 0; ci <= cmds.size(); ++ci) {
		if(ci == cmds.size() || cmds[ci] == '\n' || cmds[ci] == ';') {
			commands.emplace_back(CommandExpr{ String(cmds.begin() + first, cmds.begin() + ci), self });
			first = ci + 1;
		}
	}
	exec(commands);
}


void MainState::exec(const CommandList& commands) {
	bool execNow = _commandList.empty();
	_commandList.insert(_commandList.begin(), commands.begin(), commands.end());
	if(execNow)
		execNext();
}


void MainState::execNext() {
	while(!_commandList.empty()) {
		CommandExpr cmd = _commandList.front();
		_commandList.pop_front();
		if(execSingle(cmd.command, cmd.self) == 0)
			return;
	}
}


int MainState::execSingle(const std::string& cmd, EntityRef self) {
#define MAX_CMD_ARGS 32

	std::string tokens = cmd;
	unsigned    size   = tokens.size();
	int ret = 0;
	for(unsigned ci = 0; ci < size; ) {
		int   argc = 0;
		const char* argv[MAX_CMD_ARGS];
		while(ci < size) {
			bool endLine = false;
			while(ci < size && std::isspace(tokens[ci])) {
				endLine = (tokens[ci] == '\n') || (tokens[ci] == ';');
				tokens[ci] = '\0';
				++ci;
			}
			if(endLine)
				break;

			argv[argc] = tokens.data() + ci;
			++argc;

			while(ci < size && !std::isspace(tokens[ci])) {
				++ci;
			}
		}

		if(argc) {
			ret = exec(argc, argv, self);
		}
	}

	return ret;
}


int MainState::exec(int argc, const char** argv, EntityRef self) {
	lairAssert(argc > 0);

	std::ostringstream out;
	out << argv[0];
	for(int i = 1; i < argc; ++i)
		out << " " << argv[i];
	dbgLogger.info(out.str());

	auto cmd = _commands.find(argv[0]);
	if(cmd == _commands.end()) {
		dbgLogger.warning("Unknown command \"", argv[0], "\"");
		return -1;
	}
	return cmd->second(this, self, argc, argv);
}


void MainState::setState(State state, State nextState) {
	log().info("Change state: ", state, " -> ", nextState);
	_state = state;
	_nextState = nextState;
	_transitionTime = 0;
}


void MainState::quit() {
	_running = false;
}


Game* MainState::game() {
	return static_cast<Game*>(_game);
}


LevelSP MainState::registerLevel(const Path& path) {
	LevelSP level(new Level(this, path));
	_levelMap.emplace(path, level);
	level->preload();

	return level;
}


void MainState::loadSound(const Path& sound) {
	loader()->load<SoundLoader>(sound);
}


void MainState::playSound(const Path& sound) {
	AssetSP asset = assets()->getAsset(sound);
	auto aspect = asset->aspect<SoundAspect>();
	aspect->_get().setVolume(0.3);
	audio()->playSound(asset);
}


void MainState::loadMusic(const Path& sound) {
	loader()->load<MusicLoader>(sound);
}


void MainState::playMusic(const Path& sound) {
	AssetSP asset = assets()->getAsset(sound);
	audio()->setMusicVolume(0.5);
	audio()->playMusic(assets()->getAsset(sound));
}


void MainState::loadLevel(const Path& level, const String& spawn) {
	if(_level)
		_level->stop();

	while(_scene.firstChild().isValid())
		_scene.firstChild().destroy();

	_level = _levelMap.at(level);
	_level->initialize();

	Path background = _level->tileMap()->properties().get("background", "background1.png").asString();
	_sprites.get(_background)->setTexture(background);

	Path tileset = _level->tileMap()->properties().get("tileset", "tileset.png").asString();
	AssetSP tilesetAsset = assets()->getAsset(tileset);
	assert(tilesetAsset);
	ImageAspectSP tilesetImage = assets()->getAspect<ImageAspect>(tilesetAsset);
	assert(tilesetImage);
	_level->tileMap()->_setTileSet(tilesetImage);

	EntityRef layer = _entities.findByName("layer_base");
	auto tileLayer = _tileLayers.get(layer);
	tileLayer->setBlendingMode(BLEND_ALPHA);
	tileLayer->setTextureFlags(Texture::BILINEAR_NO_MIPMAP | Texture::CLAMP);

	const Json::Value& props = _level->tileMap()->properties();
	_playerPhysics->numJumps  = props.get("double_jump", true).asBool()? 1: 0;
	_playerPhysics->numDashes = props.get("dash", true).asBool()? 1: 0;
	_playerPhysics->wallJump  = props.get("wall_jump", true).asBool();

	_player = _entities.cloneEntity(_playerModel, _scene, "player");
	CharacterComponent* pChar = _characters.addComponent(_player);
	pChar->physics = _playerPhysics;

	_playerDeath = _entities.cloneEntity(_playerDeathModel, _scene, "player_death");
	_playerDeath.setEnabled(false);

	_spawnName = spawn;
	_level->start(_spawnName);

	setState(_nextState);

//	dumpEntityTree(log(), _entities.root());
}


void MainState::setNextLevel(const Path& level, const String& spawn) {
	_nextLevel = level;
	_nextLevelSpawn = spawn;
}


void MainState::changeLevel(const Path& level, const String& spawn) {
	Path storyScreen = _level->tileMap()->properties().get("end_screen", "").asString();
	SpriteComponent* fadeSprite = _sprites.get(_fadeOverlay);
	if(storyScreen.empty()) {
		fadeSprite->setTexture("white.png");
		fadeSprite->setColor(Vector4(0, 0, 0, 0));
		setState(STATE_FADE_OUT, STATE_PLAY);
	}
	else{
		fadeSprite->setTexture(storyScreen);
		fadeSprite->setColor(Vector4(1, 1, 1, 0));
		setState(STATE_FADE_OUT, STATE_PAUSE);
	}

	setNextLevel(level, spawn);
	playSound("departure.wav");
}


EntityRef MainState::getEntity(const String& name, const EntityRef& ancestor) {
	EntityRef entity = _entities.findByName(name, ancestor);
	if(!entity.isValid()) {
		log().error("Entity \"", name, "\" not found.");
	}
	return entity;
}


EntityRef MainState::createTrigger(EntityRef parent, const char* name, const Box2& box) {
	EntityRef entity = _entities.createEntity(parent, name);

	CollisionComponent* cc = _collisions.addComponent(entity);
	cc->addShape(Shape::newAlignedBox(box));
	cc->setHitMask(HIT_PLAYER | HIT_TRIGGER);
	cc->setIgnoreMask(HIT_TRIGGER);

	_triggers.addComponent(entity);

	return entity;
}


void MainState::updateTriggers(bool disableCmds) {
	_triggers.compactArray();

	for(TriggerComponent& tc: _triggers) {
		if(tc.isEnabled() && tc.entity().isEnabledRec()) {
			tc.prevInside = tc.inside;
			tc.inside = false;
		}
	}

	for(HitEvent hit: _collisions.hitEvents()) {
		if(hit.entities[1] == _player) {
			std::swap(hit.entities[0], hit.entities[1]);
			hit.penetration = -hit.penetration;
		}

		if(hit.entities[0] == _player) {
			TriggerComponent* tc = _triggers.get(hit.entities[1]);
			if(tc) {
				tc->inside = true;
			}
		}
	}

	if(!disableCmds) {
		for(TriggerComponent& tc: _triggers) {
			if(tc.isEnabled() && tc.entity().isEnabledRec()) {
				if(!tc.prevInside && tc.inside && !tc.onEnter.empty())
					exec(tc.onEnter, tc.entity());
				if(tc.prevInside && !tc.inside && !tc.onExit.empty())
					exec(tc.onExit, tc.entity());
			}
		}
	}
}


void MainState::killPlayer() {
	// TODO: animation + sound
	setState(STATE_DEATH);

	playSound("death.wav");

	_player.setEnabled(false);
	_characters.get(_player)->reset();
	_playerDeath.setEnabled(true);
	_playerDeath.transform() = _player.transform();
	_sprites.get(_playerDeath)->setTileIndex(0);
}


void MainState::startGame() {
	setState(STATE_PLAY, STATE_FADE_IN);

	loadLevel(_nextLevel, _nextLevelSpawn);
	_nextLevel = Path();
	_nextLevelSpawn.clear();

	//audio()->playMusic(assets()->getAsset("music.ogg"));
//	audio()->playSound(assets()->getAsset("sound.ogg"), 2);
}


void MainState::updateTick() {
	loader()->finalizePending();

	if(_state == STATE_PLAY && !_nextLevel.empty()) {
		loadLevel(_nextLevel, _nextLevelSpawn);
		_nextLevel = Path();
		_nextLevelSpawn.clear();
	}

	_inputs.sync();

	_entities.setPrevWorldTransforms();

	if(_quitInput->justPressed()) {
		quit();
	}

	if(_state == STATE_PLAY) {
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
		updateTriggers();
	}
	else if(_state == STATE_DEATH) {
		_transitionTime += TICK_LENGTH_IN_SEC;
		int index = _transitionTime * 6;

		SpriteComponent* sprite = _sprites.get(_playerDeath);
		if(index < sprite->tileGridSize().prod()) {
			sprite->setTileIndex(index);
		}
		else {
			setState(STATE_PLAY);
			_player.setEnabled(true);
			_playerDeath.setEnabled(false);
			_level->spawnPlayer(_spawnName);
		}
	}
	else if(_state == STATE_FADE_IN || _state == STATE_FADE_OUT) {
		_transitionTime += TICK_LENGTH_IN_SEC;
		if(_transitionTime > FADE_DURATION) {
			if(!_nextLevel.empty()) {
				loadLevel(_nextLevel, _nextLevelSpawn);
				_nextLevel = Path();
				_nextLevelSpawn.clear();
			}
			else {
				setState(_nextState);
			}
		}
	}
	else if(_state == STATE_PAUSE) {
		if(_jumpInput->isPressed()) {
			setState(STATE_FADE_IN);
			playSound("arrival.wav");
		}
	}

	_entities.updateWorldTransforms();
}


void MainState::updateFrame() {
	// Update camera

	Vector3 h(960, 540, .5);
	Vector2 min = h.head<2>();
	Vector2 max(_level->tileMap()->width(0)  * TILE_SIZE - h(0),
	            _level->tileMap()->height(0) * TILE_SIZE - h(1));
	Vector3 c;
	c << _player.interpPosition2(_loop.frameInterp()), .5;
	c(0) = clamp(c(0), min(0), max(0));
	c(1) = clamp(c(1), min(1), max(1));
	Box3 viewBox(c - h, c + h);
	_camera.setViewBox(viewBox);

	// Update background

	_background.placeAt(Vector2(viewBox.min().head<2>()));

	SpriteComponent* bgSprite = _sprites.get(_background);
	Vector2 screenSize(1920, 1080);
	Vector2 bgSize(bgSprite->texture()->get().width(),
	               bgSprite->texture()->get().height());
	Vector2 b = (c.head<2>() - min) / 2;
	b(1) = screenSize(1) / 2 - b(1);
	Box2 bgView(b, b + screenSize);
	bgView = Box2(bgView.min().cwiseQuotient(bgSize),
	              bgView.max().cwiseQuotient(bgSize));
	bgSprite->setView(bgView);

	// Update GUI

	_gui.placeAt(Vector2(viewBox.min().head<2>()));

	if(_state == STATE_FADE_IN || _state == STATE_FADE_OUT || _state == STATE_PAUSE) {
		SpriteComponent* fadeSprite = _sprites.get(_fadeOverlay);

		_fadeOverlay.setEnabled(true);
		_fadeOverlay.transform()(0, 0) = screenSize(0) / fadeSprite->texture()->get().width();
		_fadeOverlay.transform()(1, 1) = screenSize(1) / fadeSprite->texture()->get().height();

		Vector4 color = fadeSprite->color();
		if(_state == STATE_PAUSE) {
			color(3) = 1;
		}
		else {
			color(3) = _transitionTime / FADE_DURATION;
			if(_state == STATE_FADE_IN)
				color(3) = 1 - color(3);
		}

		fadeSprite->setColor(color);
	}
	else {
		_fadeOverlay.setEnabled(false);
	}

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
