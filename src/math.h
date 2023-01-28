#pragma once

#define _USE_MATH_DEFINES
#include <math.h>

typedef float Mat4 __attribute__((matrix_type(4, 4)));
typedef float Vec2 __attribute__((ext_vector_type(2)));
typedef float Vec3 __attribute__((ext_vector_type(3)));
typedef float Vec4 __attribute__((ext_vector_type(4)));
typedef float Quat __attribute__((ext_vector_type(4)));

static inline Mat4 mat4FromRotationTranslationScale(Quat r, Vec3 t, Vec3 s) {
	Mat4 ret;
	ret[0][0] = (1.f - ((r.y * (r.y + r.y)) + (r.z * (r.z + r.z)))) * s.x;
	ret[1][0] = ((r.x * (r.y + r.y)) + (r.w * (r.z + r.z))) * s.x;
	ret[2][0] = ((r.x * (r.z + r.z)) - (r.w * (r.y + r.y))) * s.x;
	ret[3][0] = 0.f;
	ret[0][1] = ((r.x * (r.y + r.y)) - (r.w * (r.z + r.z))) * s.y;
	ret[1][1] = (1.f - ((r.x * (r.x + r.x)) + (r.z * (r.z + r.z)))) * s.y;
	ret[2][1] = ((r.y * (r.z + r.z)) + (r.w * (r.x + r.x))) * s.y;
	ret[3][1] = 0.f;
	ret[0][2] = ((r.x * (r.z + r.z)) + (r.w * (r.y + r.y))) * s.z;
	ret[1][2] = ((r.y * (r.z + r.z)) - (r.w * (r.x + r.x))) * s.z;
	ret[2][2] = (1.f - ((r.x * (r.x + r.x)) + (r.y * (r.y + r.y)))) * s.z;
	ret[3][2] = 0.f;
	ret[0][3] = t.x;
	ret[1][3] = t.y;
	ret[2][3] = t.z;
	ret[3][3] = 1.f;
	return ret;
}

static inline Vec3 vec3Cross(Vec3 a, Vec3 b) {
	return (Vec3){
		a.y * b.z - a.z * b.y,
		a.z * b.x - a.x * b.z,
		a.x * b.y - a.y * b.x
	};
}

static inline float vec3Dot(Vec3 a, Vec3 b) {
	return a.x * b.x + a.y * b.y + a.z * b.z;
}

static inline Vec3 vec3Lerp(Vec3 a, Vec3 b, float t) {
	return a + t * (b - a);
}

static inline Vec3 vec3TransformQuat(Vec3 v, Quat q) {
	Vec3 uv = vec3Cross(q.xyz, v);
	return v + (uv * (2.f * q.w)) + (vec3Cross(q.xyz, uv) * 2.f);
}

static inline Quat quatMultiply(Quat a, Quat b) {
	return (Quat){
		a.x * b.w + a.w * b.x + a.y * b.z - a.z * b.y,
		a.y * b.w + a.w * b.y + a.z * b.x - a.x * b.z,
		a.z * b.w + a.w * b.z + a.x * b.y - a.y * b.x,
		a.w * b.w - a.x * b.x - a.y * b.y - a.z * b.z
	};
}

static inline Quat quatConjugate(Quat a) {
	return (Quat){ -a.x, -a.y, -a.z, a.w };
}
