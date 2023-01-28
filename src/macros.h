#pragma once

#define WASM_EXPORT __attribute__((visibility("default")))

#define ARRAY_COUNT(array) (sizeof((array)) / sizeof((array)[0]))

#define MEMBER_SIZE(type, member) sizeof(((type*)0)->member)

#define BITS_SET(variable, bits) ((variable & (bits)) == (bits))

#define MIN(a, b) ({      \
	__auto_type _a = (a); \
	__auto_type _b = (b); \
	_a < _b ? _a : _b;    \
})

#define MAX(a, b) ({      \
	__auto_type _a = (a); \
	__auto_type _b = (b); \
	_a > _b ? _a : _b;    \
})

#define CLAMP(x, lo, hi) MIN(MAX(x, lo), hi)

#define ALIGN_FORWARD(value, alignment) ({                                     \
	__auto_type _value     = (value);                                          \
	__auto_type _alignment = (alignment);                                      \
	(_alignment > 0) ? (_value + _alignment - 1) & ~(_alignment - 1) : _value; \
})

#define INCBIN(name, file)                                                 \
	__asm__(".section .rdata, \"dr\"\n"                                    \
	        ".global incbin_" #name "_start\n"                             \
	        ".balign 16\n"                                                 \
	        "incbin_" #name "_start:\n"                                    \
	        ".incbin \"" file "\"\n"                                       \
                                                                           \
	        ".global incbin_" #name "_end\n"                               \
	        ".balign 1\n"                                                  \
	        "incbin_" #name "_end:\n");                                    \
	extern const __attribute__((aligned(16))) void* incbin_##name##_start; \
	extern const void* incbin_##name##_end;
