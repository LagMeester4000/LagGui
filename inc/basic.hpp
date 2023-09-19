#pragma once
#include "basic.hpp"
#include <stdint.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <math.h>

inline void __lgui_assert()
{
	int stop = 0;
}

#define _LGUI_MACRO_STRING(a) #a
#define LGUI_MACRO_STRING(a) _LGUI_MACRO_STRING(a)
#define LGUI_ASSERT(condition, message) do { if (!(condition)) { printf(__FILE__ "," LGUI_MACRO_STRING(__LINE__) ": " message "\n"); __lgui_assert(); abort(); } } while (0)
#define LGUI_MIN(a, b) (((a) < (b)) ? (a) : (b))
#define LGUI_MAX(a, b) (((a) > (b)) ? (a) : (b))
#define LGUI_CLAMP(min, max, v) (LGUI_MAX(min, LGUI_MIN(max, v)))
#define LGUI_ABS(v) (((v) < 0) ? -(v) : (v))
#define LGUI_KB(v) ((v) * 1024)
#define LGUI_MB(v) (LGUI_KB(v) * 1024)
#define LGUI_GB(v) (LGUI_GB(v) * 1024)

namespace lgui {

using u8 = uint8_t;
using u16 = uint16_t;
using u32 = uint32_t;
using u64 = uint64_t;
using i8 = int8_t;
using i16 = int16_t;
using i32 = int32_t;
using i64 = int64_t;
using f32 = float;
using f64 = double;
using byte = u8;
using usize = size_t;
using isize = ptrdiff_t;

struct v2 {
	f32 x, y;

	v2 operator+(v2 other) const { return {x + other.x, y + other.y}; }
	v2& operator+=(v2 other) { x += other.x; y += other.y; return *this; }
	v2 operator-(v2 other) const { return {x - other.x, y - other.y}; }
	v2& operator-=(v2 other) { x -= other.x; y -= other.y; return *this; }
	v2 operator*(v2 other) const { return {x * other.x, y * other.y}; }
	v2& operator*=(v2 other) { x *= other.x; y *= other.y; return *this; }
	v2 operator*(f32 other) const { return {x * other, y * other}; }
	v2& operator*=(f32 other) { x *= other; y *= other; return *this; }
	v2 operator/(v2 other) const { return {x / other.x, y / other.y}; }
	v2& operator/=(v2 other) { x /= other.x; y /= other.y; return *this; }
	v2 operator/(f32 other) const { return {x / other, y / other}; }
	v2& operator/=(f32 other) { x /= other; y /= other; return *this; }

	// Used to convert unknown vector types into this one
	template<typename T>
	static v2 from(T other)
	{
		return {other.x, other.y};
	}

	f32 length() const { return sqrtf(x * x + y * y); }
};

inline v2 v2_min(v2 a, v2 b)
{
	return {LGUI_MIN(a.x, b.x), LGUI_MIN(a.y, b.y)};
}

inline v2 v2_max(v2 a, v2 b)
{
	return {LGUI_MAX(a.x, b.x), LGUI_MAX(a.y, b.y)};
}

template<typename T>
struct Slice {
	T* ptr;
	usize length;

	Slice<T> slice(usize new_begin) const { usize l = new_begin >= length ? 0 : length - new_begin; return Slice<T>{l == 0 ? nullptr : ptr + new_begin, l}; }
	//Slice<T> slice(usize new_begin, usize new_end) const { usize l = new_begin >= length ? 0 : length - new_begin; return Slice<T>{ l == 0 ? nullptr : ptr + new_begin, l }; }

	Slice<byte> to_bytes() const { return Slice<byte>{(byte*)ptr, length * sizeof(T)}; }

	T& operator[](usize i) { LGUI_ASSERT(i < length, "Out of bounds"); return ptr[i]; }
	const T& operator[](usize i) const { LGUI_ASSERT(i < length, "Out of bounds"); return ptr[i]; }

	// Iterator
	T* begin() { return ptr; }
	T* end() { return ptr + length; }
};

struct Arena;

// Marker that automatically returns its arena to the point where the marker was created (using RAII)
struct ArenaMarker {
	Arena* arena;
	usize used;

	~ArenaMarker();
};

// Simple arena allocator
// Any allocation (except raw) is initialized to 0 (ZII)
struct Arena {
	byte* ptr;
	usize length;
	usize used;

	static Arena from_memory(void* ptr, usize size)
	{
		Arena ret{};
		ret.ptr = (byte*)ptr;
		ret.length = size;
		return ret;
	}

	void reset()
	{
		used = 0;
	}

	// TODO: Alignment
	void* allocate_raw(usize size)
	{
		LGUI_ASSERT(used + size <= length, "Out of memory");
		void* ret = (void*)(ptr + used);
		used += size;
		return ret;
	}

	// TODO: Alignment
	void* allocate(usize size)
	{
		LGUI_ASSERT(used + size <= length, "Out of memory");
		void* ret = (void*)(ptr + used);
		memset(ret, 0, size);
		used += size;
		return ret;
	}

	template<typename T>
	Slice<T> allocate_array(usize count)
	{
		return Slice<T>{(T*)allocate(count * sizeof(T)), count};
	}

	template<typename T>
	T* allocate_one()
	{
		return (T*)allocate(sizeof(T));
	}

	ArenaMarker make_marker()
	{
		return {this, used};
	}
};

inline ArenaMarker::~ArenaMarker()
{
	LGUI_ASSERT(arena->used >= used, "Improper usage of marker (returned to higher value than current)");
	arena->used = used;
}

inline Slice<byte> read_file(Arena* arena, const char* filename)
{
	FILE* file;
	if (fopen_s(&file, filename, "rb"))
	{
		return {};
	}

	fseek(file, 0, SEEK_END);
	usize size = ftell(file);
	fseek(file, 0, SEEK_SET);
	void* ret = arena->allocate_raw(size);
	fread_s(ret, size, size, 1, file);
	fclose(file);

	return {(byte*)ret, size};
}

inline bool does_file_exist(const char* filename)
{
	FILE* file;
	if (fopen_s(&file, filename, "rb"))
	{
		return false;
	}
	fclose(file);
	return true;
}

inline const char* copy_string(Arena* arena, const char* str)
{
	usize len = strlen(str);
	char* ret = (char*)arena->allocate_raw(len + 1);
	memcpy((void*)ret, str, len);
	ret[len] = 0;
	return ret;
}

}
