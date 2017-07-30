/*
 *  Copyright (C) 2016-2017 the authors (see AUTHORS)
 *
 *  This file is part of ld38.
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


bool isSolid(TileMap::TileIndex tile) {
//	tile -= 1;
//	unsigned x = tile % TILE_SET_WIDTH;
//	unsigned y = tile / TILE_SET_WIDTH;
//	return (y > 2 && y < 9);
	return tile == 1;
}


Vector2i cellCoord(const Vector2& pos, float height) {
	return Vector2i(pos(0) / TILE_SIZE, height - pos(1) / TILE_SIZE);
}


//void updatePenetration(CollisionComponent* comp, const Box2& objBox, const Box2& otherBox) {
//	Vector2 offset = otherBox.center() - objBox.center();
//	Vector2 hsizes = (objBox.sizes() + otherBox.sizes()) / 2;

//	Box2 inter = objBox.intersection(otherBox);

//	bool hitX = objBox  .max()(0) > otherBox.min()(0)
//	         && otherBox.max()(0) > objBox  .min()(0);
//	bool hitY = objBox  .max()(1) > otherBox.min()(1)
//	         && otherBox.max()(1) > objBox  .min()(1);

//	if(hitY && (inter.isEmpty() || inter.sizes()(0) < inter.sizes()(1))) {
//		if(offset(0) < 0) {
//			float p = hsizes(0) + offset(0);
//			comp->setPenetration(LEFT,  std::max(comp->penetration(LEFT),  p));
//		}
//		else {
//			float p = hsizes(0) - offset(0);
//			comp->setPenetration(RIGHT, std::max(comp->penetration(RIGHT), p));
//		}
//	}
//	if(hitX && (inter.isEmpty() || inter.sizes()(1) < inter.sizes()(0))) {
//		if(offset(1) < 0) {
//			float p = hsizes(1) + offset(1);
//			comp->setPenetration(DOWN,  std::max(comp->penetration(DOWN),  p));
//		}
//		else {
//			float p = hsizes(1) - offset(1);
//			comp->setPenetration(UP,    std::max(comp->penetration(UP),    p));
//		}
//	}
//}

unsigned updateFlags(unsigned flags, unsigned bit, const Json::Value& obj, const std::string& key) {
	const Json::Value& b = obj.get(key, Json::Value(Json::nullValue));
	if(!b.isBool())
		return flags;
	if(b.asBool())
		return flags | bit;
	return flags & ~bit;
}


Box2 flipY(const Box2& box, float height) {
	Vector2 min = box.min();
	Vector2 max = box.max();
	min(1) = height - min(1);
	max(1) = height - max(1);
	std::swap(min(1), max(1));
	return Box2(min, max);
}



Level::Level(MainState* mainState, const Path& path)
	: _mainState(mainState)
	, _path(path)
{
}


void Level::preload() {
	_mainState->loader()->load<TileMapLoader>(_path);
}


void Level::initialize() {
	_mainState->log().info("Initialize level ", _path);

	AssetSP asset = _mainState->assets()->getAsset(_path);
	lairAssert(asset);

	_tileMapAspect = asset->aspect<TileMapAspect>();
	lairAssert(_tileMapAspect && _tileMapAspect->isValid());

	_tileMap = &_tileMapAspect->_get();

	_entityMap.clear();
	if(_levelRoot.isValid())
		_levelRoot.destroy();
	_levelRoot = _mainState->_entities.createEntity(_mainState->_scene, _path.utf8CStr());
	_levelRoot.setEnabled(false);

	_baseLayer = createLayer(0, "layer_base");
	_objects = _mainState->_entities.createEntity(_levelRoot, "objects");

	for(unsigned oli = 0; oli < _tileMap->nObjectLayer(); ++oli) {
		for(const Json::Value& obj: _tileMap->objectLayer(oli)["objects"]) {
			std::string type = obj.get("type", "<no_type>").asString();
			std::string name = obj.get("name", "<no_name>").asString();

			EntityRef entity;
			if(type == "spawn") {
				entity = _mainState->_entities.createEntity(_levelRoot, name.c_str());
				Box2 box = objectBox(obj);

				entity.placeAt(Vector2(objectBox(obj).center()));
//				entity.extra() = obj["properties"];
			}
//			else if(type == "trigger") {
//				entity = createTrigger(obj, name);
//			}
//			else if(type == "entity") {
//				entity = createEntity(obj, name);
//			}
//			else if(type == "item") {
//				entity = createItem(obj, name);
//			}
//			else if(type == "door") {
//				entity = createDoor(obj, name);
//			}

			if(!entity.isValid())
				_mainState->log().warning(_path, ": Failed to load entity \"", name, "\" of type \"", type, "\"");
			else
				_entityMap.emplace(name, entity);
		}
	}

	updateDepth();
}


void Level::start(const std::string& spawn) {
	_mainState->log().info("Start level ", _path);
	_levelRoot.setEnabled(true);

	EntityRef spawnEntity = entity(spawn);
	if(spawnEntity.isValid())
		_mainState->_player.placeAt(spawnEntity.position2());

//	HitEventQueue hitQueue;
//	_mainState->_collisions.findCollisions(hitQueue);
//	_mainState->updateTriggers(hitQueue, EntityRef(), true);

//	_mainState->orientPlayer(_mainState->_playerDir);

//	_mainState->setOverlay(1);
//	_mainState->exec(spawnEntity.extra().get("on_enter", "fade_in").asString());
}


void Level::stop() {
	_mainState->log().info("Stop level ", _path);
	_levelRoot.setEnabled(false);
}


Box2 Level::objectBox(const Json::Value& obj) const {
	try {
//		Json::Value props = obj["properties"];

		Vector2 min(obj["x"].asFloat(),
		            obj["y"].asFloat());
		Vector2 max(min(0) + obj["width"] .asFloat(),
		            min(1) + obj["height"].asFloat());

		float height = _tileMap->height(0) * TILE_SIZE;
		return flipY(Box2(min, max), height);
	}
	catch(Json::Exception& e) {
		_mainState->log().error(_path, ": Json error while loading: ", e.what());
		return Box2(Vector2(0, 0), Vector2(0, 0));
	}
}


EntityRef Level::createLayer(unsigned index, const char* name) {
	EntityRef layer = _mainState->_entities.createEntity(_levelRoot, name);

	TileLayerComponent* lc = _mainState->_tileLayers.addComponent(layer);
	lc->setTileMap(_tileMapAspect);
	lc->setLayerIndex(index);
	lc->setTextureFlags(Texture::BILINEAR_NO_MIPMAP | Texture::REPEAT);
	layer.placeAt(Vector3(0, 0, .01f * float(index)));

//	CollisionComponent* cc = _mainState->_collisions.addComponent(layer);
//	cc->setHitMask(HIT_SOLID);

//	Scalar height = _tileMap->height(index);
//	for(unsigned y = 0; y < _tileMap->height(index) - 1; ++y) {
//		for(unsigned x = 0; x < _tileMap->width(index) - 1; ++x) {
//			bool solid  = isSolid(_tileMap->tile(x,     y,     index));
//			bool solidX = isSolid(_tileMap->tile(x + 1, y,     index));
//			bool solidY = isSolid(_tileMap->tile(x    , y + 1, index));

//			Vector2 p10 = Vector2(x + 1, height - y    ) * TILE_SIZE;
//			Vector2 p11 = Vector2(x + 1, height - y - 1) * TILE_SIZE;
//			Vector2 p01 = Vector2(x    , height - y - 1) * TILE_SIZE;

//			if(solid && !solidX) {
//				cc->addShape(Shape::newOrientedLine(p10, p11));
//			}
//			else if(!solid && solidX) {
//				cc->addShape(Shape::newOrientedLine(p11, p10));
//			}

//			if(solid && !solidY) {
//				cc->addShape(Shape::newOrientedLine(p11, p01));
//			}
//			else if(!solid && solidY) {
//				cc->addShape(Shape::newOrientedLine(p01, p11));
//			}
//		}
//	}
//	dbgLogger.warning("Layer ", index, ": ", cc->shapes().size(), " shapes.");

	return layer;
}


//EntityRef Level::createTrigger(const Json::Value &obj, const std::string& name) {
//	Json::Value props = obj.get("properties", Json::Value());

//	Box2 box = objectBox(obj);
//	float margin = props.get("margin", 0).asFloat();
//	Vector2 half = box.sizes() / 2 + Vector2(margin, margin);
//	Box2 hitBox(-half, half);

//	EntityRef entity = _mainState->createTrigger(_objects, name.c_str(), hitBox);
//	entity.place((Vector3() << box.center(), 0.08).finished());
//	entity.setEnabled(props.get("enabled", true).asBool());

//	TriggerComponent* tc = _mainState->_triggers.addComponent(entity);
//	tc->onEnter = props.get("on_enter", "").asString();
//	tc->onExit  = props.get("on_exit",  "").asString();
//	tc->onUse   = props.get("on_use",   "").asString();
//	if(props.get("solid", false).asBool()) {
//		CollisionComponent* cc = _mainState->_collisions.get(entity);
//		cc->setHitMask(cc->hitMask() | HIT_SOLID_FLAG);
//	}

//	int gid = obj.get("gid", 0).asInt();
//	std::string sprite;
//	int tileH = 1;
//	int tileV = 1;
//	int tileIndex = 0;
//	if(gid) {
//		sprite = _tileMap->tileSet()->asset()->logicPath().utf8String();
//		tileH = TILE_SET_WIDTH;
//		tileV = TILE_SET_HEIGHT;
//		tileIndex = gid - 1;
//	}
//	sprite = props.get("sprite", sprite).asString();
//	if(!sprite.empty()) {
//		SpriteComponent* sc = _mainState->_sprites.addComponent(entity);
//		sc->setTexture(sprite);

//		tileIndex = props.get("tile_index", tileIndex).asInt();
//		sc->setTileIndex(tileIndex);
		
//		tileH = props.get("tile_h", tileH).asInt();
//		tileV = props.get("tile_v", tileV).asInt();
//		Vector2 anchor(props.get("anchor_x", 0.5).asFloat(),
//		               props.get("anchor_y", 0.5).asFloat());
//		sc->setTileGridSize(Vector2i(tileH, tileV));
//		sc->setAnchor(anchor);
//		sc->setBlendingMode(BLEND_ALPHA);

//		float scalex = props.get("scale_x", 1).asFloat();
//		entity.transform().matrix()(0, 0) = scalex;
//	}

//	return entity;
//}


//EntityRef Level::createItem(const Json::Value& obj, const std::string& name) {
//	Json::Value props = obj.get("properties", Json::Value());
//	Box2 box  = objectBox(obj);
//	int  item = props.get("item", 0).asInt();

//	EntityRef entity = _mainState->_entities.cloneEntity(_mainState->_itemModel, _levelRoot, name.c_str());
//	entity.place((Vector3() << box.center(), .09).finished());

//	SpriteComponent* sc = _mainState->_sprites.get(entity);
//	sc->setTileIndex(item);

//	return entity;
//}


//EntityRef Level::createDoor(const Json::Value& obj, const std::string& name) {
//	Json::Value props = obj.get("properties", Json::Value());
//	Box2 box = objectBox(obj);
//	bool horizontal = props.get("horizontal", true).asBool();
//	bool open = props.get("open", false).asBool();

//	EntityRef model = horizontal? _mainState->_doorHModel: _mainState->_doorVModel;
//	EntityRef entity = _mainState->_entities.cloneEntity(model, _levelRoot, name.c_str());

//	entity.place((Vector3() << box.center(), .2).finished());
//	setDoorOpen(_mainState, entity, open);

//	return entity;
//}


//EntityRef Level::createEntity(const Json::Value &obj, const std::string& name) {
//	Json::Value props = obj.get("properties", Json::Value());

//	Box2 box = objectBox(obj);
//	float depth = props.get("depth", 0.08).asFloat();
//	Vector2 offset(props.get("offset_x", 0.0).asFloat(),
//	               props.get("offset_y", 0.0).asFloat());
//	Vector2 anchor( 0.5, 0.5 );

//	String modelName = props.get("model", "").asString();
//	if(modelName.empty())
//		return EntityRef();

//	EntityRef model = _mainState->getEntity(modelName, _mainState->_models); //_mainState->_entities.createEntity(_levelRoot, name.c_str());
//	if(!model.isValid())
//		return EntityRef();

//	EntityRef entity = _mainState->_entities.cloneEntity(model, _objects, name.c_str());

//	SpriteComponent* sc = _mainState->_sprites.get(entity);
//	if(sc) {
//		std::string sprite = props.get("sprite", sc->texturePath().utf8String()).asString();
//		int tile  = props.get("tile_index", sc->tileIndex()).asInt();
//		int tileH = props.get("tile_h", sc->tileGridSize()(0)).asInt();
//		int tileV = props.get("tile_v", sc->tileGridSize()(1)).asInt();
//		anchor(0) = props.get("anchor_x", sc->anchor()(0)).asFloat();
//		anchor(1) = props.get("anchor_y", sc->anchor()(1)).asFloat();

//		sc->setTexture(sprite);
//		sc->setTileIndex(tile);
//		sc->setTileGridSize(Vector2i(tileH, tileV));
//		sc->setAnchor(anchor);
////		sc->setBlendingMode(BLEND_ALPHA);
//	}

//	Vector2 position = box.min() + box.sizes().cwiseProduct(anchor);
//	entity.place((Vector3() << position, depth).finished());

//	CollisionComponent* cc = _mainState->_collisions.get(entity);
//	if(cc) {
//		float margin = props.get("margin", 0).asFloat();
//		Box2 hitBox(box.min() - position + Vector2(margin, margin),
//		            box.max() - position - Vector2(margin, margin));

//		cc->setShape(Shape::newAlignedBox(hitBox));

//		unsigned hitMask = cc->hitMask();
//		hitMask = updateFlags(hitMask, HIT_PLAYER_FLAG,  props, "hitPlayer");
//		hitMask = updateFlags(hitMask, HIT_TRIGGER_FLAG, props, "hitTrigger");
//		hitMask = updateFlags(hitMask, HIT_USE_FLAG,     props, "hitUse");
//		hitMask = updateFlags(hitMask, HIT_SOLID_FLAG,   props, "hitSolid");
//		cc->setHitMask(hitMask);

//		unsigned ignoreMask = cc->ignoreMask();
//		ignoreMask = updateFlags(ignoreMask, HIT_PLAYER_FLAG,  props, "hitPlayer");
//		ignoreMask = updateFlags(ignoreMask, HIT_TRIGGER_FLAG, props, "hitTrigger");
//		ignoreMask = updateFlags(ignoreMask, HIT_USE_FLAG,     props, "hitUse");
//		ignoreMask = updateFlags(ignoreMask, HIT_SOLID_FLAG,   props, "hitSolid");
//		cc->setIgnoreMask(ignoreMask);
//	}

//	return entity;
//}


EntityRef Level::entity(const std::string& name) {
	EntityRange range = entities(name);
	if(range.begin() == range.end()) {
		_mainState->log().warning("Level::entity(\"", name, "\"): Entity not found.");
		return EntityRef();
	}
	auto it = range.begin();
	++it;
	if(it != range.end())
		_mainState->log().warning("Level::entity(\"", name, "\"): More than one entity found.");
	return *range.begin();
}


Level::EntityRange Level::entities(const std::string& name) {
	return EntityRange(_entityMap, name);
}


void Level::updateDepth(EntityRef entity) const {
	float depth = entity.transform()(1, 3) / _tileMap->width(0) / float(TILE_SIZE);
	entity.transform()(2, 3) = lerp(depth, 0.2f, 0.1f);
}

void Level::updateDepth() {
	EntityRef entity = _objects.firstChild();

	for(unsigned count = 1; entity.isValid(); ++count) {
		updateDepth(entity);
		entity = entity.nextSibling();
	}
}


//void Level::computeCollisions() {
//	// FIXME: The character can be stuck while sliding against a wall. Compute
//	// collision against thin walls.

//	EntityRef entity = _mainState->_player;

//	CollisionComponent* cc = _mainState->_collisions.get(entity);
//	if(!cc->isEnabled())
//		return;
//	lairAssert(cc && cc->shape() && cc->shape()->type() == SHAPE_ALIGNED_BOX);

//	TileLayerComponent* tlc = _mainState->_tileLayers.get(_baseLayer);
//	lairAssert(tlc && tlc->tileMap());
//	unsigned layer = tlc->layerIndex();

//	for(int i = 0; i < N_DIRECTIONS; ++i)
//		cc->setPenetration(Direction(i), -TILE_SIZE);

//	Vector2 pos  = entity.computeWorldTransform().translation().head<2>();
//	Box2    realBox(pos + cc->shape()->point(0), pos + cc->shape()->point(1));
//	Box2i box(cellCoord(realBox.corner(Box2::TopLeft),     _tileMap->height(layer)),
//	          cellCoord(realBox.corner(Box2::BottomRight), _tileMap->height(layer)));

////	Vector2 lpos = _baseLayer.worldTransform().translation().head<2>();
//	int width  = _tileMap->width(layer);
//	int height = _tileMap->height(layer);
//	int beginX = std::max(box.min()(0) - 1, 0);
//	int endX   = std::min(box.max()(0) + 2, width);
//	int beginY = std::max(box.min()(1) - 1, 0);
//	int endY   = std::min(box.max()(1) + 2, height);
//	for(int y = beginY; y < endY; ++y) {
//		for(int x = beginX; x < endX; ++x) {
//			if(isSolid(_tileMap->tile(x, y, layer))) {
//				Box2 tileBox(Vector2(x, height - y - 1) * TILE_SIZE,
//				             Vector2(x + 1, height - y) * TILE_SIZE);
//				updatePenetration(cc, realBox, tileBox);
//			}
//		}
//	}
//}

