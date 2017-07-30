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


#ifndef LD39_COMMANDS_H_
#define LD39_COMMANDS_H_


#include <map>

#include <lair/core/lair.h>
#include <lair/core/log.h>

#include <lair/ec/entity.h>


using namespace lair;


class MainState;


int echoCommand(MainState* state, EntityRef self, int argc, const char** argv);
int setSpawnCommand(MainState* state, EntityRef self, int argc, const char** argv);
int killCommand(MainState* state, EntityRef self, int argc, const char** argv);
int disableCommand(MainState* state, EntityRef self, int argc, const char** argv);
int creditsCommand(MainState* state, EntityRef self, int argc, const char** argv);

#endif
