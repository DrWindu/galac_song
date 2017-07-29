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


#ifndef LD39_GAME_H_
#define LD39_GAME_H_


#include <lair/utils/game_base.h>


using namespace lair;


class MainState;
class SplashState;


class GameConfig : public GameConfigBase {
public:
	GameConfig();
	~GameConfig() = default;

	virtual void setFromArgs(int& argc, char** argv);
	virtual const PropertyList& properties() const;

	static const PropertyList& staticProperties();

private:
};

class Game : public GameBase {
public:
	Game(int argc, char** argv);
	Game(const Game&)  = delete;
	Game(      Game&&) = delete;
	~Game();

	Game& operator=(const Game&)  = delete;
	Game& operator=(      Game&&) = delete;

	void initialize();
	void shutdown();

	GameConfig& config();

	SplashState* splashState();
	MainState*   mainState();

protected:
	GameConfig _config;

	std::unique_ptr<MainState> _mainState;
	std::unique_ptr<SplashState> _splashState;
};


#endif
