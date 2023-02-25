#define WIN32_LEAN_AND_MEAN
#define _CRT_SECURE_NO_WARNINGS
#include <Windows.h>
#include <stdbool.h>
#define CGLTF_IMPLEMENTATION
#include "cgltf.h"

#pragma comment(lib, "kernel32")
#pragma comment(lib, "user32")
#pragma comment(lib, "vcruntime.lib")
#pragma comment(lib, "ucrt.lib")

typedef float Mat4 __attribute__((matrix_type(4, 4)));
typedef float Vec2 __attribute__((ext_vector_type(2)));
typedef float Vec3 __attribute__((ext_vector_type(3)));
typedef float Vec4 __attribute__((ext_vector_type(4)));
typedef float Quat __attribute__((ext_vector_type(4)));

struct VertexPosition {
	uint16_t x, y, z;
};

struct VertexAttributes {
	float nx, ny, nz;
	float tx, ty, tz, tw;
	float u, v;
};

void WinMainCRTStartup(void) {
	cgltf_options options = { 0 };
	cgltf_data* data;

	cgltf_parse_file(&options, "../assets/Sponza/glTF/Sponza.gltf", &data);
	cgltf_load_buffers(&options, data, "../assets/Sponza/glTF/");

	FILE* scene_h;
	FILE* assets_h;
	FILE* indexBuffer;
	FILE* vertexBuffer;
	FILE* attributeBuffer;
	fopen_s(&scene_h, "scene.h", "w");
	fopen_s(&assets_h, "assets.h", "w");
	fopen_s(&indexBuffer, "indices", "wb");
	fopen_s(&vertexBuffer, "vertices", "wb");
	fopen_s(&attributeBuffer, "attributes", "wb");

	uint64_t totalIndexCount = 0;
	uint64_t totalVertexCount = 0;

	static struct {
		bool visited;
		uint32_t firstIndex;
	} visitedAccessors[0x100000];

	fprintf(assets_h, "#pragma once\n\
\n\
#include \"math.h\"\n\
\n\
struct KTX2 {\n\
	char identifier[12];\n\
	uint32_t vkFormat;\n\
	uint32_t typeSize;\n\
	uint32_t pixelWidth;\n\
	uint32_t pixelHeight;\n\
	uint32_t pixelDepth;\n\
	uint32_t layerCount;\n\
	uint32_t faceCount;\n\
	uint32_t levelCount;\n\
	uint32_t supercompressionScheme;\n\
	uint32_t dfdByteOffset;\n\
	uint32_t dfdByteLength;\n\
	uint32_t kvdByteOffset;\n\
	uint32_t kvdByteLength;\n\
	uint64_t sgdByteOffset;\n\
	uint64_t sgdByteLength;\n\
	struct {\n\
		uint64_t byteOffset;\n\
		uint64_t byteLength;\n\
		uint64_t uncompressedByteLength;\n\
	} levels[];\n\
};\n\
\n\
struct VertexPosition {\n\
	uint16_t x, y, z;\n\
};\n\
\n\
struct VertexAttributes {\n\
	float nx, ny, nz;\n\
	float tx, ty, tz, tw;\n\
	float u, v;\n\
};\n\
\n\
struct Material {\n\
	uint8_t r, g, b, a;\n\
};\n\
\n\
struct Primitive {\n\
	uint32_t material;\n\
	uint32_t indexCount;\n\
	uint32_t firstIndex;\n\
	uint32_t vertexOffset;\n\
	Vec3 min;\n\
	Vec3 max;\n\
};\n\
\n\
struct Mesh {\n\
	uint32_t primitiveCount;\n\
	uint32_t weightsCount;\n\
	struct Primitive* primitives;\n\
	float* weights;\n\
};\n\
\n\
enum MeshID {\n\
	MESH_NONE = -1,\n");

	fprintf(scene_h, "#pragma once\n\
\n\
#include \"assets.h\"\n\
\n\
struct Transform {\n\
	Vec3 translation;\n\
	Quat rotation;\n\
	Vec3 scale;\n\
};\n\
\n\
struct Entity {\n\
	struct Transform;\n\
	struct Entity** children;\n\
	uint32_t childCount;\n\
	enum MeshID mesh;\n\
};\n\
\n\
struct Portal {\n\
	struct Transform;\n\
	uint32_t link;\n\
};\n\
\n\
struct Camera {\n\
	Vec3 position;\n\
	Vec3 right;\n\
	Vec3 up;\n\
	Vec3 forward;\n\
};\n\
struct Scene {\n\
	uint16_t* entities;\n\
	uint32_t entityCount;\n\
};\n");

	for (cgltf_size i = 0; i < data->meshes_count; i++) {
		cgltf_mesh* mesh = &data->meshes[i];

		if (mesh->name == NULL) {
			mesh->name = calloc(128, sizeof(char));
			mesh->name[0] = 'M';
			mesh->name[1] = 'E';
			mesh->name[2] = 'S';
			mesh->name[3] = 'H';
			mesh->name[4] = '_';
			_itoa(i, &mesh->name[5], 10);
		}

		uint32_t i = 0;
		while (mesh->name[i]) {
			if (mesh->name[i] == ' ' || mesh->name[i] == '.')
				mesh->name[i] = '_';
			else
				mesh->name[i] = toupper(mesh->name[i]);
			i++;
		}

		fprintf(assets_h, "\t%s,\n", mesh->name);
	}
	fseek(assets_h, -3, SEEK_CUR);
	fprintf(assets_h, "\n};\n");

	fprintf(assets_h, "\nstatic struct Mesh meshes[] = {\n\t");
	for (cgltf_size i = 0; i < data->meshes_count; i++) {
		cgltf_mesh mesh = data->meshes[i];

		 fprintf(assets_h, "[%s] = {\n\
		.primitiveCount = %llu,\n\
		.primitives     = (struct Primitive[]){\n\t\t\t", data->meshes[i].name, mesh.primitives_count);

		for (cgltf_size j = 0; j < data->meshes[i].primitives_count; j++) {
			cgltf_primitive primitive = mesh.primitives[j];

			uint64_t material = 0;
			uint64_t indexCount = primitive.indices->count;
			uint64_t firstIndex = 0;
			uint64_t vertexOffset = 0;
			Vec3 min = { 0 };
			Vec3 max = { 0 };

			static uint16_t indices[0x100000];
			static struct VertexPosition positions[0x100000];
			static struct VertexAttributes attributes[0x100000];

			uint64_t accessorIndex = primitive.indices - data->accessors;
			if (!visitedAccessors[accessorIndex].visited) {
				visitedAccessors[accessorIndex].visited = true;

				void* indexData = ((char*)primitive.indices->buffer_view->buffer->data + primitive.indices->buffer_view->offset + primitive.indices->offset);
				for (cgltf_size k = 0; k < primitive.indices->count; k++) {
					if (primitive.indices->stride == 4)
						indices[k] = (uint16_t)((uint32_t*)indexData)[k];
					else if (primitive.indices->stride == 2)
						indices[k] = (uint16_t)((uint16_t*)indexData)[k];
					else if (primitive.indices->stride == 1)
						indices[k] = (uint16_t)((uint8_t*)indexData)[k];
				}

				fwrite(indices, sizeof(uint16_t), primitive.indices->count, indexBuffer);

				visitedAccessors[accessorIndex].firstIndex = totalIndexCount;
				totalIndexCount += primitive.indices->count;
			}

			firstIndex = visitedAccessors[accessorIndex].firstIndex;

			// todo: if ALL of the accessors (including indices) as a group have been visited before, then skip the copy

			for (cgltf_size k = 0; k < primitive.attributes_count; k++) {
				cgltf_attribute attribute = primitive.attributes[k];
				accessorIndex = primitive.indices - data->accessors;
				switch (attribute.type) {
					case cgltf_attribute_type_position: {
						vertexOffset = totalVertexCount;
						totalVertexCount += attribute.data->count;

						min = (Vec3){ attribute.data->min[0], attribute.data->min[1], attribute.data->min[2] };
						max = (Vec3){ attribute.data->max[0], attribute.data->max[1], attribute.data->max[2] };

						float* positionsIn = (float*)((char*)attribute.data->buffer_view->buffer->data + attribute.data->buffer_view->offset + attribute.data->offset);
						for (cgltf_size l = 0; l < attribute.data->count; l++) {

							#define MAP(n, start1, stop1, start2, stop2) ((n - start1) / (stop1 - start1) * (stop2 - start2) + start2)

							positions[l] = (struct VertexPosition){
								.x = (uint16_t)(MAP(positionsIn[l * 3 + 0], min.x, max.x, 0.f, 1.f) * UINT16_MAX),
								.y = (uint16_t)(MAP(positionsIn[l * 3 + 1], min.y, max.y, 0.f, 1.f) * UINT16_MAX),
								.z = (uint16_t)(MAP(positionsIn[l * 3 + 2], min.z, max.z, 0.f, 1.f) * UINT16_MAX),
							};
						}

						fwrite(positions, sizeof(struct VertexPosition), attribute.data->count, vertexBuffer);
					} break;
					case cgltf_attribute_type_normal: {
						float* normalsIn = (float*)((char*)attribute.data->buffer_view->buffer->data + attribute.data->buffer_view->offset + attribute.data->offset);

						for (cgltf_size l = 0; l < attribute.data->count; l++) {
							attributes[l].nx = normalsIn[l * 3 + 0];
							attributes[l].ny = normalsIn[l * 3 + 1];
							attributes[l].nz = normalsIn[l * 3 + 2];
						}
					} break;
					case cgltf_attribute_type_tangent: {

					} break;
					case cgltf_attribute_type_texcoord: {

					} break;
					default: {

					} break;
				}
			}

			fwrite(attributes, sizeof(struct VertexAttributes), totalVertexCount - vertexOffset, attributeBuffer);

			fprintf(assets_h, "{\n\
				.material     = %llu,\n\
				.indexCount   = %llu,\n\
				.firstIndex   = %llu,\n\
				.vertexOffset = %llu,\n\
				.min          = { %ff, %ff, %ff },\n\
				.max          = { %ff, %ff, %ff }\n\
			}, ", material, indexCount, firstIndex, vertexOffset, min.x, min.y, min.z, max.x, max.y, max.z);
		}

		fseek(assets_h, -2, SEEK_CUR);
		fprintf(assets_h, "\n\t\t}\n\t}, ");
	}
	fseek(assets_h, -2, SEEK_CUR);
	fprintf(assets_h, "\n};\n");

	fprintf(scene_h, "\nstatic struct Entity entities[] = {\n");
	for (cgltf_size i = 0; i < data->nodes_count; i++) {
		cgltf_node node = data->nodes[i];

		float px = 0.f;
		float py = 0.f;
		float pz = 0.f;
		float rx = 0.f;
		float ry = 0.f;
		float rz = 0.f;
		float rw = 1.f;
		float sx = 0.f;
		float sy = 0.f;
		float sz = 0.f;

		if (node.has_translation) {
			px = node.translation[0];
			py = node.translation[1];
			pz = node.translation[2];
		}
		if (node.has_rotation) {
			rx = node.rotation[0];
			ry = node.rotation[1];
			rz = node.rotation[2];
			rw = node.rotation[3];
		}
		if (node.has_scale) {
			sx = node.scale[0];
			sy = node.scale[1];
			sz = node.scale[2];
		}
		if (node.has_matrix) {
			__debugbreak();
		}

		fprintf(scene_h, "\t{\n"
			"\t\t.translation = { %f, %f, %f },\n"
			"\t\t.rotation = { %f, %f, %f, %f },\n"
			"\t\t.scale = { %f, %f, %f },\n"
			"\t\t.mesh = %s,\n"
			"\t}, ", px, py, pz, rx, ry, rz, rw, sx, sy, sz, node.mesh->name);
	}
	fseek(scene_h, -2, SEEK_CUR);
	fprintf(scene_h, "\n};\n");

	fprintf(scene_h, "\nstatic struct Scene scenes[] = {\n\t");
	for (cgltf_size i = 0; i < data->scenes_count; i++) {
		cgltf_scene scene = data->scenes[i];

		fprintf(scene_h, "{\n\
		.entityCount = %llu,\n\
		.entities = (uint16_t[]){ ", scene.nodes_count);

		for (cgltf_size j = 0; j < scene.nodes_count; j++)
			fprintf(scene_h, "%llu, ", scene.nodes[j] - data->nodes);

		fseek(scene_h, -2, SEEK_CUR);
		fprintf(scene_h, " }\n\t}, ");
	}
	fseek(scene_h, -2, SEEK_CUR);
	fprintf(scene_h, "\n};\n");

	ExitProcess(0);
}

int _fltused;