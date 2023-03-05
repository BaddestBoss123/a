#pragma once

#define TICKS_PER_SECOND 50

#include "assets.h"

enum ClientMessageHeader {
	CLIENT_MESSAGE_ID,
	CLIENT_MESSAGE_UPDATE
};

enum ServerMessageHeader {
	SERVER_MESSAGE_ID,
	SERVER_MESSAGE_UPDATE
};

enum EntityFlags : uint16_t {
	ENTITY_CREATED = 1 << 0,
	ENTITY_DESTROYED = 1 << 1,
};

struct ServerEntity {
	uint16_t entityID;
	uint16_t flags;
	enum MeshID mesh;

	float x, y, z;
	float rx, ry, rz, rw;
	float sx, sy, sz;
	float speed;
};

struct Transform {
	Vec3 translation;
	Quat rotation;
	Vec3 scale;
};

struct Entity {
	struct Transform transform;
	float speed;
	uint16_t flags;
	enum MeshID mesh;
#ifdef SERVER
	int32_t clientID;
	int32_t next;
#endif
};

struct ClientMessageID {
	uint16_t clientID;
	uint16_t header;
};

struct ClientMessageUpdate {
	uint16_t clientID;
	uint16_t header;
	float x, y, z;
	float rx, ry, rz, rw;
	float sx, sy, sz;
};

struct ServerMessageID {
	uint16_t header;
	uint16_t clientID;
	uint16_t entityID;
};

struct ServerMessageUpdate {
	uint16_t header;
	uint16_t entityCount;
	struct ServerEntity entities[];
};