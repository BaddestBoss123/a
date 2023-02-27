#pragma once

#define _USE_MATH_DEFINES
#include <math.h>
#include <stdint.h>
#include <xmmintrin.h>
#include <immintrin.h>

typedef float Mat3 __attribute__((matrix_type(3, 3)));
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

static inline Mat4 mat4Identity(void) {
	Mat4 ret;
	ret[0][0] = 1.f;
	ret[1][0] = 0.f;
	ret[2][0] = 0.f;
	ret[3][0] = 0.f;
	ret[0][1] = 0.f;
	ret[1][1] = 1.f;
	ret[2][1] = 0.f;
	ret[3][1] = 0.f;
	ret[0][2] = 0.f;
	ret[1][2] = 0.f;
	ret[2][2] = 1.f;
	ret[3][2] = 0.f;
	ret[0][3] = 0.f;
	ret[1][3] = 0.f;
	ret[2][3] = 0.f;
	ret[3][3] = 1.f;
	return ret;
}

static inline Mat4 mat4Inverse(Mat4 a) {
	// todo: simd
	float a00 = a[0][0], a01 = a[1][0], a02 = a[2][0], a03 = a[3][0];
	float a10 = a[0][1], a11 = a[1][1], a12 = a[2][1], a13 = a[3][1];
	float a20 = a[0][2], a21 = a[1][2], a22 = a[2][2], a23 = a[3][2];
	float a30 = a[0][3], a31 = a[1][3], a32 = a[2][3], a33 = a[3][3];
	float b00 = a00 * a11 - a01 * a10;
	float b01 = a00 * a12 - a02 * a10;
	float b02 = a00 * a13 - a03 * a10;
	float b03 = a01 * a12 - a02 * a11;
	float b04 = a01 * a13 - a03 * a11;
	float b05 = a02 * a13 - a03 * a12;
	float b06 = a20 * a31 - a21 * a30;
	float b07 = a20 * a32 - a22 * a30;
	float b08 = a20 * a33 - a23 * a30;
	float b09 = a21 * a32 - a22 * a31;
	float b10 = a21 * a33 - a23 * a31;
	float b11 = a22 * a33 - a23 * a32;
	float det = b00 * b11 - b01 * b10 + b02 * b09 + b03 * b08 - b04 * b07 + b05 * b06;

	det = 1.0 / det;
	Mat4 ret;
	ret[0][0] = (a11 * b11 - a12 * b10 + a13 * b09) * det;
	ret[1][0] = (a02 * b10 - a01 * b11 - a03 * b09) * det;
	ret[2][0] = (a31 * b05 - a32 * b04 + a33 * b03) * det;
	ret[3][0] = (a22 * b04 - a21 * b05 - a23 * b03) * det;
	ret[0][1] = (a12 * b08 - a10 * b11 - a13 * b07) * det;
	ret[1][1] = (a00 * b11 - a02 * b08 + a03 * b07) * det;
	ret[2][1] = (a32 * b02 - a30 * b05 - a33 * b01) * det;
	ret[3][1] = (a20 * b05 - a22 * b02 + a23 * b01) * det;
	ret[0][2] = (a10 * b10 - a11 * b08 + a13 * b06) * det;
	ret[1][2] = (a01 * b08 - a00 * b10 - a03 * b06) * det;
	ret[2][2] = (a30 * b04 - a31 * b02 + a33 * b00) * det;
	ret[3][2] = (a21 * b02 - a20 * b04 - a23 * b00) * det;
	ret[0][3] = (a11 * b07 - a10 * b09 - a12 * b06) * det;
	ret[1][3] = (a00 * b09 - a01 * b07 + a02 * b06) * det;
	ret[2][3] = (a31 * b01 - a30 * b03 - a32 * b00) * det;
	ret[3][3] = (a20 * b03 - a21 * b01 + a22 * b00) * det;
	return ret;
}

static inline Quat quatConjugate(Quat a) {
	return (Quat){ -a.x, -a.y, -a.z, a.w };
}

static inline Quat quatIdentity(void) {
	return (Quat){ 0.f, 0.f, 0.f, 1.f };
}

static inline Quat quatFromAxisAngle(Vec3 axis, float rad) {
	rad *= 0.5f;
	float s = sinf(rad);
	return (Quat){ s * axis.x, s * axis.y, s * axis.z, cosf(rad) };
}

static inline Quat quatMultiply(Quat a, Quat b) {
	return (Quat){
		a.x * b.w + a.w * b.x + a.y * b.z - a.z * b.y,
		a.y * b.w + a.w * b.y + a.z * b.x - a.x * b.z,
		a.z * b.w + a.w * b.z + a.x * b.y - a.y * b.x,
		a.w * b.w - a.x * b.x - a.y * b.y - a.z * b.z
	};
}

static inline Quat quatRotateX(Quat a, float radians) {
	float bx = sinf(radians);
	float bw = cosf(radians);

	return (Quat){
		a.x * bw + a.w * bx,
		a.y * bw + a.z * bx,
		a.z * bw - a.y * bx,
		a.w * bw - a.x * bx,
	};
}

static inline Quat quatRotateY(Quat a, float radians) {
	float by = sinf(radians);
	float bw = cosf(radians);

	return (Quat){
		a.x * bw - a.z * by,
		a.y * bw + a.w * by,
		a.z * bw + a.x * by,
		a.w * bw - a.y * by,
	};
}

static inline Quat quatRotateZ(Quat a, float radians) {
	float bz = sinf(radians);
	float bw = cosf(radians);

	return (Quat){
		a.x * bw + a.y * bz,
		a.y * bw - a.x * bz,
		a.z * bw + a.w * bz,
		a.w * bw - a.z * bz,
	};
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

static inline float vec3Length(Vec3 a) {
	return sqrtf(a.x * a.x + a.y * a.y + a.z * a.z);
}

static inline float vec3Distance(Vec3 a, Vec3 b) {
	return vec3Length(b - a);
}

static inline Vec3 vec3Lerp(Vec3 a, Vec3 b, float t) {
	return a + t * (b - a);
}

static inline Vec3 vec3Normalize(Vec3 a) {
	return a / vec3Length(a);
}

static inline Vec3 vec3TransformQuat(Vec3 v, Quat q) {
	Vec3 uv = vec3Cross(q.xyz, v);
	return v + (uv * (2.f * q.w)) + (vec3Cross(q.xyz, uv) * 2.f);
}

static inline float vec4Dot(Vec4 a, Vec4 b) {
	return a.x * b.x + a.y * b.y + a.z * b.z + a.w * b.w;
}

static inline Vec4 vec4TransformMat4(Vec4 a, Mat4 m) {
	return (Vec4){
		m[0][0] * a.x + m[0][1] * a.y + m[0][2] * a.z + m[0][3] * a.w,
		m[1][0] * a.x + m[1][1] * a.y + m[1][2] * a.z + m[1][3] * a.w,
		m[2][0] * a.x + m[2][1] * a.y + m[2][2] * a.z + m[2][3] * a.w,
		m[3][0] * a.x + m[3][1] * a.y + m[3][2] * a.z + m[3][3] * a.w
	};
}

static inline double xorshift128(void) {
	static uint64_t s[2] = { 0x9F1BA8B45C823D64, 0xBABABBF358762342 };

	uint64_t x = s[0];
	uint64_t y = s[1];
	s[0] = y;

	x ^= x << 23;
	x ^= x >> 17;
	x ^= y;
	x ^= y >> 26;
	s[1] = x;

	static const uint64_t kExponentBits = 0x3FF0000000000000;
    union {
    	uint64_t u;
    	double d;
    } random = { .u = (s[0] >> 12) | kExponentBits };
    return random.d - 1.0;
}

static inline double random(double min, double max) {
	return xorshift128() * (max - min) + min;
}