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


#include "lair/core/property.h"

#include "main_state.h"
#include "splash_state.h"

#include "game.h"


GameConfig::GameConfig()
	: GameConfigBase()
{
}

void GameConfig::setFromArgs(int& argc, char** argv) {
	GameConfigBase::setFromArgs(argc, argv);
}

const PropertyList& GameConfig::properties() const {
	return GameConfig::staticProperties();
}

const PropertyList& GameConfig::staticProperties() {
	return GameConfigBase::staticProperties();
}



Game::Game(int argc, char** argv)
    : GameBase(argc, argv),
      _mainState(),
      _splashState(),
      _levelPath("lvl1.json"),
      _spawnName("spawn") {
	serializer().registerType<ShapeSP>();
	serializer().registerType<ShapeSPVector>();

	if(argc > 1)
		_levelPath = argv[1];
	if(argc > 2)
		_spawnName = argv[2];
}


Game::~Game() {
}


void Game::initialize() {
	GameBase::initialize(_config);

#ifdef LAIR_DATA_DIR
	_dataPath = LAIR_DATA_DIR;
	_loader->setBasePath(_dataPath);
#endif

	window()->setUtf8Title("Lair - template");

	_splashState.reset(new SplashState(this));
	_mainState.reset(new MainState(this));

	_splashState->initialize();
	_mainState->initialize();
	_mainState->setNextLevel(_levelPath, _spawnName);

	_splashState->setup(_mainState.get(), "lair.png", 3);
}


void Game::shutdown() {
	_mainState->shutdown();
	_splashState->shutdown();

	// Required to ensure everything is freed
	_splashState.reset();
	_mainState.reset();

	GameBase::shutdown();
}


GameConfig& Game::config() {
	return _config;
}


SplashState* Game::splashState() {
	return _splashState.get();
}


MainState* Game::mainState() {
	return _mainState.get();
}
