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


#include "main_state.h"
#include "level.h"

#include "components.h"


TriggerComponent::TriggerComponent(Manager* manager, _Entity* entity)
	: Component(manager, entity)
	, prevInside(false)
	, inside(false)
{
}


const PropertyList& TriggerComponent::properties() {
	static PropertyList props;
	if(props.nProperties() == 0) {
		props.addProperty("on_enter", &TriggerComponent::onEnter);
		props.addProperty("on_exit",  &TriggerComponent::onExit);
		props.addProperty("on_use",   &TriggerComponent::onUse);
	}
	return props;
}


TriggerComponentManager::TriggerComponentManager()
	: DenseComponentManager<TriggerComponent>("trigger", 128)
{
}


//TriggerComponent* TriggerComponentManager::addComponentFromJson(
//        EntityRef entity, const Json::Value& json, const Path& cd) {
//	TriggerComponent* comp = addComponent(entity);

//	comp->onEnter = json.get("on_enter", "").asString();
//	comp->onExit  = json.get("on_exit",  "").asString();
//	comp->onUse   = json.get("on_use",   "").asString();

//	return comp;
//}


//TriggerComponent* TriggerComponentManager::cloneComponent(EntityRef base, EntityRef entity) {
//	TriggerComponent* baseComp = get(base);
//	TriggerComponent* comp = _addComponent(entity, baseComp);

//	comp->onEnter = baseComp->onEnter;
//	comp->onExit  = baseComp->onExit;
//	comp->onUse   = baseComp->onUse;

//	return comp;
//}


CharacterComponent::CharacterComponent(Manager* manager, _Entity* entity)
	: Component(manager, entity)
    , lookDir(DIR_RIGHT)
    , animation(nullptr)
{
	reset();
}


void CharacterComponent::pressMove(unsigned directions) {
	dirPressed |= directions;
}


void CharacterComponent::pressJump(bool press) {
	jumpPressed = press;
}


void CharacterComponent::pressDash(bool press) {
	dashPressed = press;
}


void CharacterComponent::playAnimation(const CharAnimation* anim) {
	if(animation != anim) {
//		dbgLogger.info("Play animation ", anim->name);
		animation = anim;
		animTime = 0;
	}
}


bool CharacterComponent::animationDone() const {
	return animTime * animation->fps >= animation->frames.size();
}


void CharacterComponent::reset() {
	dirPressed     = 0;
	prevDirPressed = 0;
	jumpPressed    = false;
	dashPressed    = false;

	for(int i = 0; i < 4; ++i)
		penetration[i] = 0;
	touchDir     = 0;
	prevTouchDir = 0;

	velocity = Vector2::Zero();

	moveDir = DIR_NONE;
//	lookDir = DIR_RIGHT;

	jumpDuration = 0;
	jumpCount    = 0;
	wallJumpDir  = DIR_NONE;

	dashDuration = 9999;
	dashCount    = 0;
}


const PropertyList& CharacterComponent::properties() {
	static PropertyList props;
	if(props.nProperties() == 0) {
//		props.addProperty("velocity", &TriggerComponent::onEnter);
//		props.addProperty("move_dir",  &TriggerComponent::onExit);
//		props.addProperty("look_dir",   &TriggerComponent::onUse);
//		props.addProperty("jump_duration",   &TriggerComponent::onUse);
//		props.addProperty("jump_count",   &TriggerComponent::onUse);
//		props.addProperty("wall_jump_dir",   &TriggerComponent::onUse);
//		props.addProperty("dash_duration",   &TriggerComponent::onUse);
//		props.addProperty("dash_count",   &TriggerComponent::onUse);
	}
	return props;
}


CharacterComponentManager::CharacterComponentManager(MainState* mainState)
    : DenseComponentManager<CharacterComponent>("character", 128)
    , _mainState(mainState)
    , _idleAnim("idle", 4)
    , _walkAnim("walk", 4)
    , _jumpAnim("jump", 8, false)
    , _dashAnim("dash", 8)
    , _onWallAnim("onWall", 4)
    , _wallJumpAnim("wallJump", 8, false)
{
	_idleAnim.frames.push_back(0);

	_walkAnim.frames.push_back(1);
	_walkAnim.frames.push_back(0);

	_jumpAnim.frames.push_back(8);
	_jumpAnim.frames.push_back(7);
	_jumpAnim.frames.push_back(6);

	_dashAnim.frames.push_back(4);
	_dashAnim.frames.push_back(5);

	_onWallAnim.frames.push_back(14);

	_wallJumpAnim.frames.push_back(15);
	_wallJumpAnim.frames.push_back(14);
	_wallJumpAnim.frames.push_back(10);
}


void CharacterComponentManager::updatePhysics() {
	compactArray();

	for(unsigned ci = 0; ci < nComponents(); ++ci) {
		CharacterComponent& c = _components[ci];

		if(!c.isAlive() || !c.isEnabled() || !c.physics || !c.entity().isEnabledRec())
			continue;

		CharPhysicsParamsSP p = c.physics;

		Vector2 pos = c.entity().position2();

//		dbgLogger.info(_mainState->_loop.tickCount(), ": p: ", pos.transpose(), ", v: ", c.velocity.transpose(),
//		           ", h: ", c.touchDir);

//		float speed = 2;
//		if(c.dirPressed & DIR_LEFT)
//			pos(0) -= speed;
//		if(c.dirPressed & DIR_RIGHT)
//			pos(0) += speed;
//		if(c.dirPressed & DIR_DOWN)
//			pos(1) -= speed;
//		if(c.dirPressed & DIR_UP)
//			pos(1) += speed;

		bool onGround = c.touchDir & DIR_DOWN;
		bool onWall   = c.touchDir & (DIR_LEFT | DIR_RIGHT);
		bool isDashing = c.dashDuration < p->dashTicks;

		bool moveLeft  = c.dirPressed & DIR_LEFT;
		bool moveRight = c.dirPressed & DIR_RIGHT;
		bool justPressedLeft  = moveLeft  && !(c.prevDirPressed &DIR_LEFT);
		bool justPressedRight = moveRight && !(c.prevDirPressed &DIR_RIGHT);
		if(justPressedLeft || (moveLeft && !moveRight))
			c.moveDir = DIR_LEFT;
		else if(justPressedRight || (!moveLeft && moveRight))
			c.moveDir = DIR_RIGHT;
		else if(!moveLeft && !moveRight)
			c.moveDir = DIR_NONE;

		if(!isDashing) {
			if(onGround) {
				if(c.moveDir == DIR_NONE)
					c.playAnimation(&_idleAnim);
				if(c.moveDir != DIR_NONE)
					c.playAnimation(&_walkAnim);
			}

			if((   c.animation == &_jumpAnim
			    || c.animation == &_wallJumpAnim
			    || c.animation == &_dashAnim)
			&& c.animationDone())
				c.playAnimation(&_idleAnim);

			if(!onGround && onWall && p->wallJump)
				c.playAnimation(&_onWallAnim);
			if(c.animation == &_onWallAnim && !onWall)
				c.playAnimation(&_idleAnim);
		}

		if(!isDashing && c.moveDir != DIR_NONE
		&& (c.wallJumpDir == DIR_NONE || c.jumpDuration >= p->jumpTicks))
			c.lookDir = c.moveDir;

		if(p->wallJump && !isDashing && !onGround && onWall) {
			c.lookDir = (c.touchDir & DIR_LEFT)? DIR_RIGHT: DIR_LEFT;
		}

		if(c.touchDir & (DIR_LEFT | DIR_RIGHT)) {
			c.dashDuration = p->dashTicks;
		}

		if(c.dashPressed && c.dashCount > 0) {
			c.dashDuration = 0;
			c.dashCount -= 1;
			c.playAnimation(&_dashAnim);
			_mainState->playSound("dash.wav");
		}
		c.dashPressed = false;

		if(c.dashDuration < p->dashTicks) {
			pos(0)  += (c.lookDir == DIR_LEFT)? -p->dashSpeed: p->dashSpeed;
			c.velocity(0) = (c.lookDir == DIR_LEFT)? -p->maxSpeed: p->maxSpeed;
			c.velocity(1) = 0;
			c.dashDuration += 1;
		}
		else {
			Vector2 acceleration = Vector2::Zero();

			float targetXSpeed = (c.moveDir == DIR_LEFT)?  -p->maxSpeed:
								 (c.moveDir == DIR_RIGHT)?  p->maxSpeed: 0.f;
			float diff = targetXSpeed - c.velocity(0);
			if(c.wallJumpDir == DIR_NONE /*&& (
						onGround ||
						(c.moveDir == DIR_LEFT  && c.velocity(0) > -p->maxSpeed) ||
						(c.moveDir == DIR_RIGHT && c.velocity(0) <  p->maxSpeed))*/) {
				float accel = onGround? p->playerAccel: p->airControl;
				if(diff < 0)
					acceleration(0) = std::max(-accel, diff);
				else
					acceleration(0) = std::min( accel, diff);
			}

			if(c.jumpDuration >= p->jumpTicks && (onGround || (p->wallJump && onWall))) {
				c.jumpCount = p->numJumps;
				c.wallJumpDir = DIR_NONE;
				c.dashCount = p->numDashes;
			}
			if(onGround) {
				c.jumpDuration = p->jumpTicks;
			}

			static float maxHeight = 0;
			maxHeight = std::max(maxHeight, pos(1));
			acceleration(1) = std::max(-p->gravity, -((p->wallJump && onWall)? p->maxWallFallSpeed: p->maxFallSpeed) - c.velocity(1));

			bool justPressedJump = p->jump && c.jumpPressed && !c.prevJumpPressed;
			if(justPressedJump && (onGround || (p->wallJump && onWall) || c.jumpCount > 0)) {
				maxHeight = 0;
				c.velocity(1) = 0;
				c.wallJumpDir = (!p->wallJump || onGround)? DIR_NONE:
							    (c.touchDir & DIR_LEFT)?    DIR_RIGHT:
							    (c.touchDir & DIR_RIGHT)?   DIR_LEFT: DIR_NONE;
				c.jumpDuration = 0;
				if(!onGround && !(p->wallJump && onWall))
					c.jumpCount -= 1;

				if(c.wallJumpDir != DIR_NONE)
					c.playAnimation(&_wallJumpAnim);
				else
					c.playAnimation(&_jumpAnim);

				_mainState->playSound("jump.wav");
			}

			if(c.jumpDuration < p->jumpTicks) {
				if(c.jumpPressed) {
					c.velocity(1) += p->jumpAccel;
					if(c.wallJumpDir == DIR_LEFT)
						c.velocity(0) -= p->wallJumpAccel;
					if(c.wallJumpDir == DIR_RIGHT)
						c.velocity(0) += p->wallJumpAccel;
					c.jumpDuration += 1;
				}
				else {
					c.jumpDuration = p->jumpTicks;
				}
			}

			if(c.jumpDuration >= p->jumpTicks) {
				c.wallJumpDir = DIR_NONE;
			}

	//		log().info("Jump: c: ", c.jumpCount, ", d: ", c.jumpDuration, ", w", c.wallJumpDir, ", h: ", c.touchDir);

			c.velocity += acceleration;
			pos        += c.velocity;
		}

		if(pos != c.entity().position2()) {
			c.entity().moveTo(pos);
			CollisionComponent* cc = _mainState->_collisions.get(c.entity());
			if(cc)
				cc->setDirty();
		}

		c.entity().transform()(0, 0) = (c.lookDir == DIR_LEFT)? -1: 1;
		// DIRTY HACK: Make sure the scale is not interpolated !
		c.entity()._get()->prevWorldTransform(0, 0) = c.entity().transform()(0, 0);

		if(c.animation) {
			c.animTime += TICK_LENGTH_IN_SEC;
			unsigned index = unsigned(c.animTime * c.animation->fps);
			index = c.animation->repeat?
			            index % c.animation->frames.size():
			            std::min(index, unsigned(c.animation->frames.size() - 1));


			SpriteComponent* sprite = _mainState->_sprites.get(c.entity());
//			if(c.animation->frames[index] != sprite->tileIndex())
//				dbgLogger.info("  anim ", c.animation->name, ": ", index);
			sprite->setTileIndex(c.animation->frames[index]);
		}

		c.prevDirPressed  = c.dirPressed;
		c.prevJumpPressed = c.jumpPressed;
		c.prevTouchDir    = c.touchDir;

		c.dirPressed  = 0;
		c.jumpPressed = false;
		c.dashPressed = false;
		c.touchDir    = 0;

		for(int i = 0; i < 4; ++i)
			c.penetration[i] = 0;

		c._hits.clear();
	}
}


void CharacterComponentManager::processCollisions() {
	Scalar  skin = .5f;
	Vector2 vSkin = Vector2::Constant(skin);
	for(unsigned ci = 0; ci < nComponents(); ++ci) {
		CharacterComponent& c = _components[ci];

		if(!c.isAlive() || !c.isEnabled() || !c.physics || !c.entity().isEnabledRec())
			continue;

		CollisionComponent* coll = _mainState->_collisions.get(c.entity());
		Shape wShape;
		coll->shapes()[0]->setTransformed(wShape, c.entity().worldTransform().matrix());
		Box2 box = wShape.boundingBox();
		box = Box2(box.min() - vSkin, box.max() + vSkin);

		TileMap* tileMap = _mainState->_level->tileMap();
		int layer = 0;
		int width  = tileMap->width (layer);
		int height = tileMap->height(layer);
		Vector2i begin(std::floor(box.min()(0) / TILE_SIZE),
		               height - std::ceil (box.max()(1) / TILE_SIZE));
		Vector2i end  (std::ceil (box.max()(0) / TILE_SIZE),
		               height - std::floor(box.min()(1) / TILE_SIZE));
//		begin(0) = std::max(begin(0), 0);
//		begin(1) = std::max(begin(1), 0);
//		end(0) = std::min(end(0), width);
//		end(1) = std::min(end(1), height);
		for(int y = begin(1); y < end(1); ++y) {
			for(int x = begin(0); x < end(0); ++x) {
				if(x < 0 || x >= width || y < 0 || y >= height
				|| isSolid(tileMap->tile(x, y, layer))) {
					Box2 tileBox(Vector2(x,     height - y - 1) * TILE_SIZE,
					             Vector2(x + 1, height - y    ) * TILE_SIZE);

					Scalar dist[4];
					dist[LEFT]  = tileBox.max()(0) - box.min()(0);
					dist[RIGHT] = box.max()(0) - tileBox.min()(0);
					dist[DOWN]  = tileBox.max()(1) - box.min()(1);
					dist[UP]    = box.max()(1) - tileBox.min()(1);

					bool empty[4];
					empty[LEFT]  = x + 1 < width  && y >= 0 && y < height
					            && !isSolid(tileMap->tile(x + 1, y, layer));
					empty[RIGHT] = x - 1 >= 0     && y >= 0 && y < height
					            && !isSolid(tileMap->tile(x - 1, y, layer));
					empty[DOWN]  = y - 1 >= 0     && x >= 0 && x < width
					            && !isSolid(tileMap->tile(x, y - 1, layer));
					empty[UP]    = y + 1 < height && x >= 0 && x < width
					            && !isSolid(tileMap->tile(x, y + 1, layer));
//					dbgLogger.info(x, ", ", y, ": ", dist[LEFT], ", ", dist[RIGHT], ", ", dist[DOWN], ", ", dist[UP]);
//					dbgLogger.info("  empty: ", empty[LEFT], ", ", empty[RIGHT], ", ", empty[DOWN], ", ", empty[UP]);

					int dirs[] = { 0, 1, 2, 3 };
					std::sort(dirs, dirs + 4, [&dist](int d0, int d1) {
						return dist[d0] < dist[d1];
					});

					for(int di = 0; di < 4; ++di) {
						Direction d = Direction(dirs[di]);
						if(empty[d] && dist[d] < dist[(d+2)%4]) {
							c.penetration[d] = std::max(c.penetration[d], dist[d] - skin);
							if(dist[d] > -skin)
								c.touchDir |= 1 << d;
							break;
						}
					}
				}
			}
		}

		Vector2 offset = Vector2::Zero();

		if( c.penetration[RIGHT] == 0
		|| (c.penetration[LEFT] != 0 && c.penetration[LEFT] < c.penetration[RIGHT]))
			offset(0) += c.penetration[LEFT];
		else
			offset(0) -= c.penetration[RIGHT];

		if( c.penetration[UP] == 0
		|| (c.penetration[DOWN] != 0 && c.penetration[DOWN] < c.penetration[UP]))
			offset(1) += c.penetration[DOWN];
		else
			offset(1) -= c.penetration[UP];

		if(!offset.isZero()) {
			c.entity().moveTo(Vector2(c.entity().position2() + offset));
			coll->setDirty();
		}
//		dbgLogger.info("\"", c.entity().name(), "\" collisions: ",
//		               c.penetration[0], ", ", c.penetration[1], ", ",
//		        c.penetration[2], ", ", c.penetration[3], " - ",
//		        c.touchDir, " - ", (offset).transpose(), " - ", c.velocity.transpose());

		if(c.touchDir & DIR_LEFT && c.velocity(0) < 0)
			c.velocity(0) = 0;
		if(c.touchDir & DIR_RIGHT && c.velocity(0) > 0)
			c.velocity(0) = 0;
		if(c.touchDir & DIR_DOWN && c.velocity(1) < 0)
			c.velocity(1) = 0;
		if(c.touchDir & DIR_UP && c.velocity(1) > 0)
			c.velocity(1) = 0;
	}
}
