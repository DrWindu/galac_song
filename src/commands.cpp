/*
 *  Copyright (C) 2016 the authors (see AUTHORS)
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


#include <sstream>

#include <lair/sys_sdl2/audio_module.h>

#include "game.h"
#include "main_state.h"
#include "splash_state.h"
#include "level.h"

#include "components.h"


int echoCommand(MainState* state, EntityRef self, int argc, const char** argv) {
	std::ostringstream out;
	out << argv[0];
	for(int i = 1; i < argc; ++i)
		out << " " << argv[i];
	dbgLogger.info(out.str());
	state->execNext();

	return 0;
}


int setSpawnCommand(MainState* state, EntityRef self, int argc, const char** argv) {
	if(argc != 2) {
		dbgLogger.warning(argv[0], ": wrong number of argument.");
		return -2;
	}

	if(state->getEntity(argv[1]).isValid())
		state->_spawnName = argv[1];
	else
		dbgLogger.warning("Cannot set spawn \"", argv[1], "\": entity not found.");

	return 0;
}


int killCommand(MainState* state, EntityRef self, int argc, const char** argv) {
	if(argc != 1) {
		dbgLogger.warning(argv[0], ": wrong number of argument.");
		return -2;
	}

	state->killPlayer();

	return 0;
}


int nextLevelCommand(MainState* state, EntityRef self, int argc, const char** argv) {
	if(argc != 2 && argc != 3) {
		dbgLogger.warning("nextLevelCommand: wrong number of argument.");
		return -2;
	}

	if(argc == 2)
		state->changeLevel(argv[1]);
	else
		state->changeLevel(argv[1], argv[2]);

	return 0;
}


//int playSoundCommand(MainState* state, EntityRef self, int argc, const char** argv) {
//	if(argc != 2) {
//		dbgLogger.warning("playSoundCommand: wrong number of argument.");
//		return -2;
//	}

//	state->playSound(argv[1]);
//	state->execNext();

//	return 0;
//}


int disableCommand(MainState* state, EntityRef self, int argc, const char** argv) {
	if(argc != 2) {
		dbgLogger.warning(argv[0], ": wrong number of argument.");
		return -2;
	}

	EntityRef target = state->getEntity(argv[1]);
	if(!target.isValid()) {
		dbgLogger.warning(argv[0], ": target not found: ", argv[1]);
		return -2;
	}

	target.setEnabled(false);
	state->execNext();

	return 0;
}


int creditsCommand(MainState* state, EntityRef self, int argc, const char** argv) {
	if(argc != 1) {
		dbgLogger.warning(argv[0], ": wrong number of argument.");
		return -2;
	}

	state->game()->splashState()->setup(nullptr, "credits.png");
	state->game()->setNextState(state->game()->splashState());
	state->quit();
	state->execNext();

	return 0;
}
