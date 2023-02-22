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

void WinMainCRTStartup(void) {
	cgltf_options options = { 0 };
	cgltf_data* data;

	cgltf_parse_file(&options, "../assets/treePineSmall.glb", &data);
	cgltf_load_buffers(&options, data, "../assets");

	FILE* header;
	FILE* indexBuffer;
	FILE* vertexBuffer;
	fopen_s(&header, "assets.h", "w");
	fopen_s(&indexBuffer, "indices", "wb");
	fopen_s(&vertexBuffer, "indices", "wb");

	uint64_t totalIndexCount = 0;
	uint64_t totalVertexCount = 0;

	static struct {
		bool visited;
		uint32_t firstIndex;
	} visitedAccessors[1024];

	fprintf(header, "#include \"math.h\"\n\
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

	for (cgltf_size i = 0; i < data->meshes_count; i++) {
		cgltf_mesh mesh = data->meshes[i];

		uint32_t i = 0;
		while (mesh.name[i]) {
			if (mesh.name[i] == ' ')
				mesh.name[i] = '_';
			else
				mesh.name[i] = toupper(mesh.name[i]);
			i++;
		}

		fprintf(header, "\t%s,\n", mesh.name);
	}
	fseek(header, -3, SEEK_CUR);
	fprintf(header, "\n};\n");

	fprintf(header, "\nstatic struct Mesh meshes[] = {\n\t");

	for (cgltf_size i = 0; i < data->meshes_count; i++) {
		cgltf_mesh mesh = data->meshes[i];

		 fprintf(header, "[%s] = {\n\
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

			uint64_t accessorIndex = primitive.indices - data->accessors;
			if (!visitedAccessors[accessorIndex].visited) {
				visitedAccessors[accessorIndex].visited = true;

				void* indexData = ((char*)data->meshes[i].primitives[j].indices->buffer_view->buffer->data + data->meshes[i].primitives[j].indices->buffer_view->offset + data->meshes[i].primitives[j].indices->offset);
				static uint16_t indices[0x10000];

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

			fprintf(header, "{\n\
				.material     = %llu,\n\
				.indexCount   = %llu,\n\
				.firstIndex   = %llu,\n\
				.vertexOffset = %llu,\n\
				.min          = { %ff, %ff, %ff },\n\
				.max          = { %ff, %ff, %ff }\n\
			}, ", material, indexCount, firstIndex, vertexOffset, min.x, min.y, min.z, max.x, max.y, max.z);
		}

		fseek(header, -2, SEEK_CUR);
		fprintf(header, "\n\t\t}\n\t}\n};\n");

	}

	ExitProcess(0);
}

int _fltused;