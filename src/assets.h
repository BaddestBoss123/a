#include "math.h"

struct KTX2 {
	char identifier[12];
	uint32_t vkFormat;
	uint32_t typeSize;
	uint32_t pixelWidth;
	uint32_t pixelHeight;
	uint32_t pixelDepth;
	uint32_t layerCount;
	uint32_t faceCount;
	uint32_t levelCount;
	uint32_t supercompressionScheme;
	uint32_t dfdByteOffset;
	uint32_t dfdByteLength;
	uint32_t kvdByteOffset;
	uint32_t kvdByteLength;
	uint64_t sgdByteOffset;
	uint64_t sgdByteLength;
	struct {
		uint64_t byteOffset;
		uint64_t byteLength;
		uint64_t uncompressedByteLength;
	} levels[];
};

struct VertexPosition {
	uint16_t x, y, z;
};

struct VertexAttributes {
	float nx, ny, nz;
	float tx, ty, tz, tw;
	float u, v;
};

struct Material {
	uint8_t r, g, b, a;
};

struct Primitive {
	uint32_t material;
	uint32_t indexCount;
	uint32_t firstIndex;
	uint32_t vertexOffset;
	Vec3 min;
	Vec3 max;
};

struct Mesh {
	uint32_t primitiveCount;
	uint32_t weightsCount;
	struct Primitive* primitives;
	float* weights;
};

static struct Mesh mesh_treePineSmall = {
	.primitiveCount = 2,
	.primitives     = (struct Primitive[]){
		{
			.indexCount = 870,
		}, {
			.indexCount = 54,
		}
	}
};
