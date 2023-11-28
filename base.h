#ifndef EDIT_BASE_H
#define EDIT_BASE_H

#include <math.h>
#include <intrin.h>
#include <stdbool.h>

#define PI 3.141592654f

#define DEBUG 1

#if DEBUG
#define Assert(Expression) if(!(Expression)) { __debugbreak(); }
#else
#define Assert(Expression) if(!(Expression)) { *(int*)0 = 0; }
#endif

#define Bytes(x) (x * 8)
#define Kilobytes(x) (x * 1024)
#define Megabytes(x) (x * 1048576)
#define Gigabytes(x) (x * 1073741824)

#define SECONDS_TO_MILLISECONDS(X) (X * 1000)
#define SECONDS_TO_MICROSECONDS(X) (X * 1000000)

#define STACK_PUSH(Stack, Type, Value) *((Type*)Stack) = Value; Stack += sizeof(Type)
#define STACK_POP(Stack, Type) *((Type*)(Stack - sizeof(Type))); Stack -= sizeof(Type)

#define LIST_STRUCT(name, type) struct name { i32 count; type *data; }

#define F32_MAX (f32)0xFFFFFFFF
typedef float f32;
typedef double f64;

typedef unsigned long long u64;
typedef unsigned int u32;
typedef unsigned short u16;
typedef unsigned char u8;

typedef long long i64;
typedef int i32;
typedef int int32;
typedef short i16;
typedef char i8;

typedef bool b8;

#define CHANNEL_ALHPA 3
#define CHANNEL_RED 2
#define CHANNEL_GREEN 1
#define CHANNEL_BLUE 0

#define COLOR_WHITE MakeColor(0xFFFFFFFF)
#define COLOR_BLACK MakeColor(0xFF000000)
#define COLOR_LIGHT_GRAY MakeColor(0xFF555555)
#define COLOR_GRAY MakeColor(0xFF222222)
#define COLOR_RED MakeColor(0xFFFF0000)
#define COLOR_GREEN MakeColor(0xFF00FF00)
#define COLOR_BLUE MakeColor(0xFF0000FF)
#define COLOR_MY_BLUE MakeColor(0xFF38AFFF)
#define COLOR_LIGHT_BLUE MakeColor(0xFF0d3980)

i32 Floor(f32 value) {
	i32 result = (i32)value;
	return result;
}

i32 Ceil(f32 value) {
	i32 result = ((i32)value) + 1;
	return result;
}

i32 Round(f32 value) {
	i32 result = Floor(value);
	if ((value - (f32)(int)value) >= 0.5f) {
		result++;
	}
	return result;
}

i64 Abs(i64 value) {
	return ((value < 0) * (-1 * value)) + ((value >= 0) * value);
}

f32 ArcTangent(f32 y, f32 x) {
	__m128 operand_y = _mm_load1_ps(&y);
	__m128 operand_x = _mm_load1_ps(&x);
	__m128 arctan_2 = _mm_atan2_ps(operand_y, operand_x);
	f32 result = *arctan_2.m128_f32;
	return result;
}

f32 SquareRoot(f32 value) {
	__m128 operand = _mm_load1_ps(&value);
	__m128 square_root = _mm_sqrt_ps(operand);
	f32 result = *square_root.m128_f32;
	return result;
}

b8 is_whitespace(i32 character) {
	return (character == '\n' || character == '\r' || character == '\t' || character == ' ');
}

b8 is_num(i32 character) {
	return ((character >= '0') && (character <= '9'));
}

b8 is_alpha(char character) {
	return ((character >= 'A' && character <= 'Z') || (character >= 'a' && character <= 'z'));
}

b8 is_alpha_num(char character) {
	return ((character >= 'A' && character <= 'Z') || (character >= 'a' && character <= 'z') || (character >= '0' && character <= '9'));
}

b8 is_special(char code) {
	return code == '=' || code == '>' || code == '<' || 
		   code == '+' || code == '-' || code == '*' || 
		   code == '&' || code == ';' || code == '{' ||
		   code == '}' || code == '(' || code == ')' ||
		   code == ':' || code == '[' || code == ']' ||
		   code == '.' || code == '!' || code == '?' ||
		   is_whitespace(code);
}

#endif
