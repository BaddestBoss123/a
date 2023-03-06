#pragma comment(lib, "kernel32")
#pragma comment(lib, "user32")
#pragma comment(lib, "vcruntime.lib")
#pragma comment(lib, "ucrt.lib")
#define WIN32_LEAN_AND_MEAN
#define _CRT_SECURE_NO_WARNINGS
#include <Windows.h>
#undef near
#undef far
#include <stdbool.h>
#define CGLTF_IMPLEMENTATION
#include "cgltf.h"
#include "math.h"
struct VertexPosition {
	uint16_t x, y, z;
};
struct VertexAttributes {
	float nx, ny, nz;
	float tx, ty, tz, tw;
	float u, v;
};
int _fltused;
void WinMainCRTStartup(void) {
	FILE* ktx2_cmd;
	FILE* assets_h;
	FILE* indexBuffer;
	FILE* vertexBuffer;
	FILE* attributeBuffer;
	fopen_s(&ktx2_cmd, "ktx2.cmd", "w");
	fopen_s(&assets_h, "assets.h", "w");
	fopen_s(&indexBuffer, "indices", "wb");
	fopen_s(&vertexBuffer, "vertices", "wb");
	fopen_s(&attributeBuffer, "attributes", "wb");
	uint64_t totalIndexCount = 0;
	uint64_t totalVertexCount = 0;
	cgltf_options options = { 0 };
	cgltf_data* assets[2];
	static struct {
		bool visited;
		uint32_t firstIndex;
	} visitedAccessors[_countof(assets)][0x100000];
	static struct {
		bool visited;
		uint32_t index;
	} visitedImages[_countof(assets)][0x1000];
	fprintf(assets_h, "#pragma once\n\
#include \"macros.h\"\n\
#include \"math.h\"\n\
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
struct VertexPosition {\n\
	uint16_t x, y, z;\n\
};\n\
struct VertexAttributes {\n\
	float nx, ny, nz;\n\
	float tx, ty, tz, tw;\n\
	float u, v;\n\
};\n\
struct Material {\n\
	uint8_t r, g, b, a;\n\
	uint16_t colorIndex;\n\
};\n\
struct Primitive {\n\
	uint32_t material;\n\
	uint32_t indexCount;\n\
	uint32_t firstIndex;\n\
	uint32_t vertexOffset;\n\
	Vec3 min;\n\
	Vec3 max;\n\
};\n\
struct Mesh {\n\
	uint32_t primitiveCount;\n\
	uint32_t weightsCount;\n\
	struct Primitive* primitives;\n\
	float* weights;\n\
};\n\
enum MeshID {\n\
	MESH_NONE = -1,\n");
	cgltf_parse_file(&options, "../assets/Sponza/glTF/Sponza.gltf", &assets[0]);
	cgltf_load_buffers(&options, assets[0], "../assets/Sponza/glTF/");
	cgltf_parse_file(&options, "../assets/Suzanne.glb", &assets[1]);
	cgltf_load_buffers(&options, assets[1], "../assets/");
	uint32_t meshCount = 0;
	for (uint32_t i = 0; i < _countof(assets); i++) {
		cgltf_data* data = assets[i];
		for (cgltf_size j = 0; j < data->meshes_count; j++) {
			cgltf_mesh* mesh = &data->meshes[j];
			if (mesh->name == NULL) {
				mesh->name = calloc(128, sizeof(char));
				mesh->name[0] = 'M';
				mesh->name[1] = 'E';
				mesh->name[2] = 'S';
				mesh->name[3] = 'H';
				mesh->name[4] = '_';
				_itoa(meshCount++, &mesh->name[5], 10);
			}
			uint32_t k = 0;
			while (mesh->name[k]) {
				if (mesh->name[k] == ' ' || mesh->name[k] == '.')
					mesh->name[k] = '_';
				else
					mesh->name[k] = toupper(mesh->name[k]);
				k++;
			}
			fprintf(assets_h, "\t%s,\n", mesh->name);
		}
	}
	fseek(assets_h, -3, SEEK_CUR);
	fprintf(assets_h, "\n};\n\n#ifdef CLIENT\n");
	uint32_t imageCount = 0;
	char* ktx2Images = calloc(0x10000, sizeof(char));
	for (uint32_t i = 0; i < _countof(assets); i++) {
		cgltf_data* data = assets[i];
		for (cgltf_size j = 0; j < data->materials_count; j++) {
			cgltf_material material = data->materials[j];
			uint64_t imageIndex = material.pbr_metallic_roughness.base_color_texture.texture->image - data->images;
			if (!visitedImages[i][imageIndex].visited) {
				visitedImages[i][imageIndex].visited = true;
				visitedImages[i][imageIndex].index = imageCount;
				sprintf(ktx2Images, "%s\t(struct KTX2*)&incbin_image_%i_ktx2_start,\n", ktx2Images, imageCount);
				fprintf(assets_h, "INCBIN(image_%i_ktx2, \"ktx2/%i.ktx2\");\n", imageCount, imageCount);
				fprintf(ktx2_cmd, "compressonatorcli -fd BC7 -EncodeWith GPU -Quality 1.0 -mipsize 8 assets/Sponza/glTF/%s ktx2/%i.ktx2\n", material.pbr_metallic_roughness.base_color_texture.texture->image->uri, imageCount);
				imageCount++;
			}
		}
	}
	fprintf(assets_h, "\nstatic struct KTX2* ktx2ImageFiles[] = {\n%s", ktx2Images);
	fseek(assets_h, -3, SEEK_CUR);
	fprintf(assets_h, "\n};\n\n");
	fprintf(assets_h, "static struct Material materials[] = {\n	");
	for (uint32_t i = 0; i < _countof(assets); i++) {
		cgltf_data* data = assets[i];
		for (cgltf_size j = 0; j < data->materials_count; j++) {
			cgltf_material material = data->materials[j];
			fprintf(assets_h, "{\n"
				"\t\t.r = %i,\n"
				"\t\t.g = %i,\n"
				"\t\t.b = %i,\n"
				"\t\t.a = %i,\n"
				"\t\t.colorIndex = %i,\n"
				"\t}, ", (uint8_t)(material.pbr_metallic_roughness.base_color_factor[0] * 255),
			           (uint8_t)(material.pbr_metallic_roughness.base_color_factor[1] * 255),
			           (uint8_t)(material.pbr_metallic_roughness.base_color_factor[2] * 255),
			           (uint8_t)(material.pbr_metallic_roughness.base_color_factor[3] * 255),
			           visitedImages[i][material.pbr_metallic_roughness.base_color_texture.texture->image - data->images].index);
		}
	}
	fseek(assets_h, -2, SEEK_CUR);
	fprintf(assets_h, "\n};\n\n");
	fprintf(assets_h, "static struct Mesh meshes[] = {\n\t");
	for (uint32_t i = 0; i < _countof(assets); i++) {
		cgltf_data* data = assets[i];
		for (cgltf_size j = 0; j < data->meshes_count; j++) {
			cgltf_mesh mesh = data->meshes[j];
			fprintf(assets_h, "[%s] = {\n"
				"\t\t.primitiveCount = %llu,\n"
				"\t\t.primitives     = (struct Primitive[]){\n\t\t\t", mesh.name, mesh.primitives_count);
			for (cgltf_size k = 0; k < mesh.primitives_count; k++) {
				cgltf_primitive primitive = mesh.primitives[k];
				uint64_t material = primitive.material - data->materials;
				uint64_t indexCount = primitive.indices->count;
				uint64_t firstIndex = 0;
				uint64_t vertexOffset = 0;
				Vec3 min = { 0 };
				Vec3 max = { 0 };
				static uint16_t indices[0x100000];
				static struct VertexPosition positions[0x100000];
				static struct VertexAttributes attributes[0x100000];
				uint64_t accessorIndex = primitive.indices - data->accessors;
				if (!visitedAccessors[i][accessorIndex].visited) {
					visitedAccessors[i][accessorIndex].visited = true;
					void* indexData = ((char*)primitive.indices->buffer_view->buffer->data + primitive.indices->buffer_view->offset + primitive.indices->offset);
					for (cgltf_size l = 0; l < primitive.indices->count; l++) {
						if (primitive.indices->stride == 4)
							indices[l] = (uint16_t)((uint32_t*)indexData)[l];
						else if (primitive.indices->stride == 2)
							indices[l] = (uint16_t)((uint16_t*)indexData)[l];
						else if (primitive.indices->stride == 1)
							indices[l] = (uint16_t)((uint8_t*)indexData)[l];
					}
					fwrite(indices, sizeof(uint16_t), primitive.indices->count, indexBuffer);
					visitedAccessors[i][accessorIndex].firstIndex = totalIndexCount;
					totalIndexCount += primitive.indices->count;
				}
				firstIndex = visitedAccessors[i][accessorIndex].firstIndex;
				// todo: if ALL of the accessors (including indices) as a group have been visited before, then skip the copy
				for (cgltf_size l = 0; l < primitive.attributes_count; l++) {
					cgltf_attribute attribute = primitive.attributes[l];
					accessorIndex = primitive.indices - data->accessors;
					switch (attribute.type) {
						case cgltf_attribute_type_position: {
							vertexOffset = totalVertexCount;
							totalVertexCount += attribute.data->count;
							min = (Vec3){ attribute.data->min[0], attribute.data->min[1], attribute.data->min[2] };
							max = (Vec3){ attribute.data->max[0], attribute.data->max[1], attribute.data->max[2] };
							float* positionsIn = (float*)((char*)attribute.data->buffer_view->buffer->data + attribute.data->buffer_view->offset + attribute.data->offset);
							for (cgltf_size m = 0; m < attribute.data->count; m++) {
								positions[m] = (struct VertexPosition){
									.x = (uint16_t)((positionsIn[m * 3 + 0] - min.x) / (max.x - min.x) * UINT16_MAX),
									.y = (uint16_t)((positionsIn[m * 3 + 1] - min.y) / (max.y - min.y) * UINT16_MAX),
									.z = (uint16_t)((positionsIn[m * 3 + 2] - min.z) / (max.z - min.z) * UINT16_MAX),
								};
							}
							fwrite(positions, sizeof(struct VertexPosition), attribute.data->count, vertexBuffer);
						} break;
						case cgltf_attribute_type_normal: {
							float* attributesIn = (float*)((char*)attribute.data->buffer_view->buffer->data + attribute.data->buffer_view->offset + attribute.data->offset);
							for (cgltf_size m = 0; m < attribute.data->count; m++) {
								attributes[m].nx = attributesIn[m * 3 + 0];
								attributes[m].ny = attributesIn[m * 3 + 1];
								attributes[m].nz = attributesIn[m * 3 + 2];
							}
						} break;
						case cgltf_attribute_type_tangent: {
							float* attributesIn = (float*)((char*)attribute.data->buffer_view->buffer->data + attribute.data->buffer_view->offset + attribute.data->offset);
							for (cgltf_size m = 0; m < attribute.data->count; m++) {
								attributes[m].tx = attributesIn[m * 4 + 0];
								attributes[m].ty = attributesIn[m * 4 + 1];
								attributes[m].tz = attributesIn[m * 4 + 2];
								attributes[m].tw = attributesIn[m * 4 + 3];
							}
						} break;
						case cgltf_attribute_type_texcoord: {
							float* attributesIn = (float*)((char*)attribute.data->buffer_view->buffer->data + attribute.data->buffer_view->offset + attribute.data->offset);
							for (cgltf_size m = 0; m < attribute.data->count; m++) {
								attributes[m].u = attributesIn[m * 2 + 0];
								attributes[m].v = attributesIn[m * 2 + 1];
							}
						} break;
						default: { } break;
					}
				}
				fwrite(attributes, sizeof(struct VertexAttributes), totalVertexCount - vertexOffset, attributeBuffer);
				fprintf(assets_h, "{\n"
					"\t\t\t\t.material = %llu,\n"
					"\t\t\t\t.indexCount = %llu,\n"
					"\t\t\t\t.firstIndex = %llu,\n"
					"\t\t\t\t.vertexOffset = %llu,\n"
					"\t\t\t\t.min = { %ff, %ff, %ff },\n"
					"\t\t\t\t.max = { %ff, %ff, %ff }\n"
					"\t\t\t}, ", material, indexCount, firstIndex, vertexOffset, min.x, min.y, min.z, max.x, max.y, max.z);
			}
			fseek(assets_h, -2, SEEK_CUR);
			fprintf(assets_h, "\n\t\t}\n\t}, ");
		}
	}
	fseek(assets_h, -2, SEEK_CUR);
	fprintf(assets_h, "\n};\n#endif\n");
	// fprintf(scene_h, "\nstatic struct Entity entities[] = {\n\t");
	// for (uint32_t i = 0; i < _countof(assets); i++) {
	// 	cgltf_data* data = assets[i];
	// 	for (cgltf_size j = 0; j < data->nodes_count; j++) {
	// 		cgltf_node node = data->nodes[j];
	// 		float px = 0.f;
	// 		float py = 0.f;
	// 		float pz = 0.f;
	// 		float rx = 0.f;
	// 		float ry = 0.f;
	// 		float rz = 0.f;
	// 		float rw = 1.f;
	// 		float sx = 0.f;
	// 		float sy = 0.f;
	// 		float sz = 0.f;
	// 		if (node.has_translation) {
	// 			px = node.translation[0];
	// 			py = node.translation[1];
	// 			pz = node.translation[2];
	// 		}
	// 		if (node.has_rotation) {
	// 			rx = node.rotation[0];
	// 			ry = node.rotation[1];
	// 			rz = node.rotation[2];
	// 			rw = node.rotation[3];
	// 		}
	// 		if (node.has_scale) {
	// 			sx = node.scale[0];
	// 			sy = node.scale[1];
	// 			sz = node.scale[2];
	// 		}
	// 		if (node.has_matrix) {
	// 			__debugbreak();
	// 		}
	// 		fprintf(scene_h, "{\n"
	// 			"\t\t.transform = {\n"
	// 			"\t\t\t.translation = { %f, %f, %f },\n"
	// 			"\t\t\t.rotation    = { %f, %f, %f, %f },\n"
	// 			"\t\t\t.scale       = { %f, %f, %f }\n"
	// 			"\t\t},\n"
	// 			"\t\t.mesh      = %s,\n"
	// 			"\t}, ", px, py, pz, rx, ry, rz, rw, sx, sy, sz, node.mesh->name);
	// 	}
	// }
	// fseek(scene_h, -2, SEEK_CUR);
	// fprintf(scene_h, "\n};\n");
	// fprintf(scene_h, "\nstatic struct Scene scenes[] = {\n\t");
	// for (uint32_t i = 0; i < _countof(assets); i++) {
	// 	cgltf_data* data = assets[i];
	// 	for (cgltf_size j = 0; j < data->scenes_count; j++) {
	// 		cgltf_scene scene = data->scenes[j];
	// 		fprintf(scene_h, "{\n"
	// 		"\t\t.entityCount = %llu,\n"
	// 		"\t\t.entities = (uint16_t[]){ ", scene.nodes_count);
	// 		for (cgltf_size k = 0; k < scene.nodes_count; k++)
	// 			fprintf(scene_h, "%llu, ", scene.nodes[k] - data->nodes);
	// 		fseek(scene_h, -2, SEEK_CUR);
	// 		fprintf(scene_h, " }\n\t}, ");
	// 	}
	// }
	// fseek(scene_h, -2, SEEK_CUR);
	// fprintf(scene_h, "\n};\n");
	ExitProcess(0);
}