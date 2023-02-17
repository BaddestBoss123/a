#define WIN32_LEAN_AND_MEAN
#define _CRT_SECURE_NO_WARNINGS
#include <Windows.h>
#define CGLTF_IMPLEMENTATION
#include "cgltf.h"

#pragma comment(lib, "kernel32")
#pragma comment(lib, "user32")
#pragma comment(lib, "vcruntime.lib")
#pragma comment(lib, "ucrt.lib")

void mainCRTStartup(void) {
	// cgltf_options options = { 0 };
	// cgltf_data* data;

	// cgltf_parse_file(&options, "", &data);
	// cgltf_load_buffers(&options, data, "");

	// loop over meshes only

	FILE* f;
	fopen_s(&f, "assets.h", "w");

	fprintf(f, "#include \"../math.h\"\n\
\n\
typedef struct KTX2 {\n\
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
} KTX2;\n\
\n\
typedef struct VertexPosition {\n\
	uint16_t x, y, z;\n\
} VertexPosition;\n\
\n\
typedef struct VertexAttributes {\n\
	float nx, ny, nz;\n\
	float tx, ty, tz, tw;\n\
	float u, v;\n\
} VertexAttributes;\n\
\n\
typedef struct Material {\n\
	uint8_t r, g, b, a;\n\
} Material;\n\
\n\
typedef struct Primitive {\n\
	uint32_t material;\n\
	uint32_t indexCount;\n\
	uint32_t firstIndex;\n\
	uint32_t vertexOffset;\n\
	Vec3 min;\n\
	Vec3 max;\n\
} Primitive;\n\
\n\
typedef struct Mesh {\n\
	Primitive* primitives;\n\
	float* weights;\n\
	uint32_t primitiveCount;\n\
	uint32_t weightsCount;\n\
} Mesh;\n");


// 	for (cgltf_size i = 0; i < data->meshes_count; i++) {


// 		fprintf(f,
// "static const Mesh mesh_%s = {\n\
// 	.primitives     = (const Primitive[]){\n", data->meshes[i].name);

// 		for (cgltf_size j = 0; j < data->meshes[i].primitives_count; j++) {

// 		}

// 		fprintf(f,
// "	},\n\
// 	.primitiveCount = %llu,\n\
// };\n\n", data->meshes[i].primitives_count);

// 	}

	ExitProcess(0);
}

int _fltused;