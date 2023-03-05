#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stddef.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <unistd.h>
#include <time.h>

#include "protocol.h"
#include "freelist.h"

struct Client {
	struct sockaddr_in address;
	char ip[INET6_ADDRSTRLEN];
	uint16_t port;
	uint16_t idleTime;

	int32_t next;
	int32_t entity;
};

struct Transform {
	Vec3 translation;
	Quat rotation;
	Vec3 scale;
};

struct Entity {
	int32_t clientID;
	uint16_t flags;
	int32_t next;
	float speed;
	enum MeshID mesh;
	struct Transform transform;
};

static struct {
	int32_t head;
	int32_t firstFree;
	uint32_t elementCount;
	struct Client* data;
} clients = {
	.head = -1,
	.firstFree = -1,
	.data = (struct Client[UINT16_MAX]){ }
};

static struct {
	int32_t head;
	int32_t firstFree;
	uint32_t elementCount;
	struct Entity* data;
} entities = {
	.head = -1,
	.firstFree = -1,
	.data = (struct Entity[UINT16_MAX]){ }
};

static char scratchBuffer[4 * 1024 * 1024];
static uint64_t ticksElapsed;

int main() {
	int32_t entityID = FREE_LIST_INSERT(entities, ((struct Entity){
		.clientID = -1,
		.flags = ENTITY_CREATE,
		.transform = {
			.translation = { 0.f, 0.f, 0.f },
			.rotation = { 0.f, 0.f, 0.f, 1.f },
			.scale = { 0.00800000037997961f, 0.00800000037997961f, 0.00800000037997961f },
		},
		.mesh = MESH_0,
	}));

	int sock = socket(AF_INET, SOCK_DGRAM | SOCK_NONBLOCK, 0);

	(void)bind(sock, (struct sockaddr*)&(struct sockaddr_in){
		.sin_family = AF_INET,
		.sin_addr.s_addr = INADDR_ANY,
		.sin_port = htons(8080)
	}, sizeof(struct sockaddr_in));

	struct timespec start;
	clock_gettime(CLOCK_MONOTONIC, &start);

	for (;;) {
		char buffer[1024];
		struct sockaddr_in address;
		while (recvfrom(sock, buffer, sizeof(buffer), 0, (struct sockaddr*)&address, &(socklen_t){ sizeof(address) }) > 0) {
			uint16_t clientID = ((uint16_t*)buffer)[0];
			uint16_t header = ((uint16_t*)buffer)[1];

			char ip[INET6_ADDRSTRLEN];
			inet_ntop(address.sin_family, &address.sin_addr, ip, address.sin_family == AF_INET ? INET_ADDRSTRLEN : INET6_ADDRSTRLEN);
			uint16_t port = ntohs(address.sin_port);

			if (strcmp(clients.data[clientID].ip, ip) != 0 || clients.data[clientID].port != port) {
				bool alreadyConnected = false;

				int32_t idx = clients.head;
				while (idx != -1) {
					if (strcmp(clients.data[idx].ip, ip) == 0 && clients.data[idx].port == port) {
						clientID = idx;
						alreadyConnected = true;
						break;
					}
					idx = clients.data[idx].next;
				}

				if (!alreadyConnected) {
					clientID = FREE_LIST_INSERT(clients, ((struct Client){
						.address = address,
						.port = port
					}));
					memcpy(&clients.data[clientID].ip, ip, sizeof(ip));
					printf("new connection id %i: %s:%i\n", clientID, ip, port);

					struct ServerMessageUpdate* message = (struct ServerMessageUpdate*)scratchBuffer;
					message->header = SERVER_MESSAGE_UPDATE;
					message->entityCount = 0;

					idx = entities.head;
					while (idx != -1) {
						struct Entity* e = &entities.data[idx];
						message->entities[message->entityCount++] = (struct ServerEntity){
							.entityID = idx,
							.flags = e->flags | ENTITY_CREATE,
							.mesh = e->mesh,
							.x = e->transform.translation.x,
							.y = e->transform.translation.y,
							.z = e->transform.translation.z,
							.rx = e->transform.rotation.x,
							.ry = e->transform.rotation.y,
							.rz = e->transform.rotation.z,
							.rw = e->transform.rotation.w,
							.sx = e->transform.scale.x,
							.sy = e->transform.scale.y,
							.sz = e->transform.scale.z,
							.speed = e->speed
						};

						idx = e->next;
					}
					sendto(sock, scratchBuffer, offsetof(struct ServerMessageUpdate, entities) + message->entityCount * sizeof(struct ServerEntity), MSG_CONFIRM, (struct sockaddr*)&address, sizeof(struct sockaddr_in));

					entityID = clients.data[clientID].entity = FREE_LIST_INSERT(entities, ((struct Entity){
						.clientID = clientID,
						.flags = ENTITY_CREATE,
						.transform = {
							.translation = { 0.f, 0.f, 0.f },
							.rotation = { 0.f, 0.f, 0.f, 1.f },
							.scale = { 1.f, 1.f, 1.f }
						},
						.speed = 0.2f,
						.mesh = SUZANNE
					}));

					sendto(sock, (char*)&(struct ServerMessageID){
						.header = SERVER_MESSAGE_ID,
						.clientID = clientID,
						.entityID = entityID
					}, sizeof(struct ServerMessageID), MSG_CONFIRM, (struct sockaddr*)&address, sizeof(struct sockaddr_in));
				}
			}

			clients.data[clientID].idleTime = 0;
			switch (header) {
				case CLIENT_MESSAGE_UPDATE: {
					struct ClientMessageUpdate* message = (struct ClientMessageUpdate*)buffer;

					int32_t entityIndex = clients.data[clientID].entity;

					struct Entity* entity = &entities.data[entityIndex];
					entity->transform.translation = (Vec3){ message->x, message->y, message->z };
					entity->transform.rotation = (Quat){ message->rx, message->ry, message->rz, message->rw };
					entity->transform.scale = (Vec3){ message->sx, message->sy, message->sz };
				} break;
			}
		}

		while (({
			struct timespec now;
			clock_gettime(CLOCK_MONOTONIC, &now);

			time_t deltaSec = now.tv_sec - start.tv_sec;
			long deltaNsec = now.tv_nsec - start.tv_nsec;

			uint64_t targetTicksElapsed = (deltaSec * TICKS_PER_SECOND) + ((deltaNsec * TICKS_PER_SECOND) / (1 * 1000 * 1000 * 1000));
			ticksElapsed < targetTicksElapsed;
		})) {
			struct ServerMessageUpdate* message = (struct ServerMessageUpdate*)scratchBuffer;
			message->header = SERVER_MESSAGE_UPDATE;
			message->entityCount = 0;

			int32_t idx = entities.head;
			while (idx != -1) {
				struct Entity* e = &entities.data[idx];
				message->entities[message->entityCount++] = (struct ServerEntity){
					.entityID = idx,
					.flags = e->flags,
					.mesh = e->mesh,
					.x = e->transform.translation.x,
					.y = e->transform.translation.y,
					.z = e->transform.translation.z,
					.rx = e->transform.rotation.x,
					.ry = e->transform.rotation.y,
					.rz = e->transform.rotation.z,
					.rw = e->transform.rotation.w,
					.sx = e->transform.scale.x,
					.sy = e->transform.scale.y,
					.sz = e->transform.scale.z,
					.speed = e->speed
				};
				e->flags &= ~ENTITY_CREATE;
				idx = e->next;
			}

			idx = clients.head;
			while (idx != -1) {
				if (clients.data[idx].idleTime++ == TICKS_PER_SECOND * 5) {
					// remove from linked list

				}

				sendto(sock, (char*)message, offsetof(struct ServerMessageUpdate, entities) + message->entityCount * sizeof(struct ServerEntity), MSG_CONFIRM, (struct sockaddr*)&clients.data[idx].address, sizeof(struct sockaddr_in));

				idx = clients.data[idx].next;
			}

			ticksElapsed++;
		}

		nanosleep(&(struct timespec){
			.tv_sec = 0,
			.tv_nsec = 1 * 1000 * 1000
		}, NULL);
	}

	return 0;
}