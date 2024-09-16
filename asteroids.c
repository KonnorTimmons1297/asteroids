#pragma warning(push, 1)
#define COBJMACROS
#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <d3d11.h>
#include <dxgi1_3.h>
#include <dxgidebug.h>

#include <intrin.h> // ceil, float, min, max
#include <stdlib.h> // srand, rand
#include <stdio.h> // snprintf
#include <time.h>

#include "kdtf_font.h"
#pragma warning(pop)

#include "base.h"


// COMPLETE:
// - Open window with default size of 800x600 and title "Maskeroids"
// - Set background color to gray
// - Draw Ship on the screen as a triangle
// - Rotate the ship with the arrow keys
// - Move the ship forward with the up arrow key
// - Allow ship to go off one side of the screen and appear on the other side
// - Fire a small rectangle from the front of the ship when the user
//   hits the spacebar
// - Draw meteor on the screen as a circle
// - Give the meteor a random radius
// - Give the meteor a velocity in a random direction
// - Add Collision detection between ship and the meteor
// - Add Collision detection between bullet and the meteor
// - Keep track of score
// - On collision with bullet, split meteor into a random number of
//   smaller meteors with random radius all in opposite directions
// - On collision with ship, game over

// TODO:
// - Add Pause menu with quit and restart buttons
// - Add audio
// - Generate outline for asteroid
//     * Select 8 points on the unit circle and give them
//       different lengths, making a jagged appearance.
// - Draw bitmaps for the ship, bullet, and meteor

#define DEFAULT_WINDOW_WIDTH 1280
#define DEFAULT_WINDOW_HEIGHT 960

#define BACKGROUND_COLOR 0xFF111111
// #define SHIP_COLOR 0xFFFFFFFF
#define SHIP_COLOR 0xFFFF553B

static i32 WindowWidth = DEFAULT_WINDOW_WIDTH;
static i32 WindowHeight = DEFAULT_WINDOW_HEIGHT;
static i32 MouseX = 0;
static i32 MouseY = 0;
static b8 MouseLeftButtonDown = 0;
static i32 gShouldCloseWindow = 0;
static b8 gPaused = false;
static KDTF_Font gFont = {0};

typedef struct {
	f32 x, y;
} Point;

typedef struct {
	Point a, b, c;
} Triangle;

Point Centroid(Triangle t) {
	Point result = {0};
	result.x = (t.a.x + t.b.x + t.c.x) / 3.0f;
	result.y = (t.a.y + t.b.y + t.c.y) / 3.0f;
	return result;
}

f32 my_max(f32 a, f32 b) {
	__m128 operand_1 = _mm_load1_ps(&a);
	__m128 operand_2 = _mm_load1_ps(&b);
	__m128 max = _mm_max_ps(operand_1, operand_2);
	f32 result = *max.m128_f32;
	return result;
}

f32 my_min(f32 a, f32 b) {
	__m128 operand_1 = _mm_load1_ps(&a);
	__m128 operand_2 = _mm_load1_ps(&b);
	__m128 min = _mm_min_ps(operand_1, operand_2);
	f32 result = *min.m128_f32;
	return result;
}

i32 max_i32(i32 x, i32 y) {
	return ((x > y) * x) + ((y > x) * y) + ((y == x) * x);
}

i32 min_i32(i32 x, i32 y) {
	return ((x < y) * x) + ((y < x) * y) + ((y == x) * x);
}

i32 my_ceil(f32 value) {
	__m128 operand = _mm_load1_ps(&value);
	__m128 ceil_value = _mm_ceil_ps(operand);
	__m128i ceil_int = _mm_cvtps_epi32(ceil_value);
	i32 result = *ceil_int.m128i_i32;
	return result;
}

i32 my_floor(f32 value) {
	__m128 operand = _mm_load1_ps(&value);
	__m128 floor_value = _mm_floor_ps(operand);
	__m128i floor_int = _mm_cvtps_epi32(floor_value);
	i32 result = *floor_int.m128i_i32;
	return result;
}

typedef struct {
	f32 x, y;
} Vec2;

typedef struct {
	i32 x, y;
} Vec2i;

typedef struct {
	Triangle ship_body;
	Vec2 forward;
	i32 posX, posY;	
} Ship;

#define DEG2RAD(X) (X * (PI/180.0f))
#define SHIP_ROTATION_STEP (2*PI)
#define SHIP_SPEED 250.0f
#define MISSILE_SPEED 500.0f

static int gRotateShipLeft = 0;
static int gRotateShipRight = 0;
static int gMoveShipForward = 0;
static int gShootMissile = 0;

f32 my_sqrt(f32 value) {
	__m128 operand = _mm_load1_ps(&value);
	__m128 square_root = _mm_sqrt_ps(operand);
	f32 result = *square_root.m128_f32;
	return result;
}

f32 my_acos(f32 value) {
	__m128 operand = _mm_load1_ps(&value);
	__m128 inverse_cosine = _mm_acos_ps(operand);
	f32 result = *inverse_cosine.m128_f32;
	return result;
}

f32 my_cos(f32 value) {
	__m128 operand = _mm_load1_ps(&value);
	__m128 cosine = _mm_cos_ps(operand);
	f32 result = *cosine.m128_f32;
	return result;
}

f32 my_sin(f32 value) {
	__m128 operand = _mm_load1_ps(&value);
	__m128 sine = _mm_sin_ps(operand);
	f32 result = *sine.m128_f32;
	return result;
}

f32 my_atan2(f32 y, f32 x) {
	__m128 operand_y = _mm_load1_ps(&y);
	__m128 operand_x = _mm_load1_ps(&x);
	__m128 arctan_2 = _mm_atan2_ps(operand_y, operand_x);
	f32 result = *arctan_2.m128_f32;
	return result;
}

i32 RoundNearest(f32 value) {
	__m128 operand = _mm_load1_ps(&value);
	__m128 round_ps = _mm_round_ps(operand, _MM_FROUND_TO_NEAREST_INT |_MM_FROUND_NO_EXC);
	__m128i rounded = _mm_cvtps_epi32(round_ps);
	i32 result = *rounded.m128i_i32;
	return result;
}

i32 Min(i32 x, i32 y, i32 z) {
	i32 result = x;
	
	if (y < result) {
		result = y;   
	}
	if (z < result) {
		result = z;
	}

	return result;
}

i32 Max(i32 x, i32 y, i32 z) {
	i32 result = x;
	
	if (y > result) {
		result = y;   
	}
	if (z > result) {
		result = z;
	}

	return result;
}

i32 Min2(i32 x, i32 y) {
	return ((x < y) * x) + ((y < x) * y) + ((y == x) * x);
}

i32 Max2(i32 x, i32 y) {
	return ((x > y) * x) + ((y > x) * y) + ((y == x) * x);
}

LRESULT CALLBACK WindowCallback(
	HWND window_handle,
	UINT msg,
	WPARAM wParam,
	LPARAM lParam)
{
	switch (msg) {
		case WM_CLOSE: {
			gShouldCloseWindow = 1;
		} break;

		case WM_KEYDOWN:
		case WM_KEYUP: {
			int pressed = (lParam & (1 << 31)) == 0;
			switch (wParam) {
				case VK_LEFT: {
					gRotateShipLeft = pressed;
					return 0;
				} break;

				case VK_RIGHT: {
					gRotateShipRight = pressed;
					return 0;
				} break;

				case VK_UP: {
					gMoveShipForward = pressed;
					return 0;
				} break;

				case VK_SPACE: {
					gShootMissile = pressed;
					return 0;
				} break;
				
				case VK_ESCAPE: {
					if (pressed) {
						gPaused = !gPaused;
					}
					return 0;
				} break;
			}
			return DefWindowProcW(window_handle, msg, wParam, lParam);
		} break;
		
		case WM_LBUTTONDOWN: {
			MouseLeftButtonDown = 1;
		} break;
		
		case WM_LBUTTONUP: {
			MouseLeftButtonDown = 0;
		} break;
		
		case WM_DESTROY: {
			PostQuitMessage(0);
			return 0;
		} break;
	}

	return DefWindowProcW(window_handle, msg, wParam, lParam);
}

typedef struct {
	i32 min_x, max_x;
	i32 min_y, max_y;
} Extents;

Extents CalculateExtents(Point *points, i32 point_count) {
	Extents result = {0};

	// Assert(point_count >= 2);

	f32 min_x = points->x;
	f32 max_x = points->x;
	f32 min_y = points->y;
	f32 max_y = points->y;

	for (i32 i = 1; i < point_count; i++) {
		Point *p = points + i;
		min_x = my_min(min_x, p->x);
		max_x = my_max(max_x, p->x);
		min_y = my_min(min_y, p->y);
		max_y = my_max(max_y, p->y);
	}

	result.min_x = my_floor(min_x);
	result.max_x = my_ceil(max_x);
	result.min_y = my_floor(min_y);
	result.max_y = my_ceil(max_y);

	return result;
}

void RotatePoints(Point *points, i32 point_count, Point center_of_rotation, f32 rotation_amount) {
	for (i32 i = 0; i < point_count; i++) {
		Point *p = points + i;

		// Orient the points so that they are relative to the point of rotation
		Point reoriented = { p->x - center_of_rotation.x, p->y - center_of_rotation.y };
		f32 radius = my_sqrt(((reoriented.x * reoriented.x) + (reoriented.y * reoriented.y)));

		// Calculate the angle that the points are currently at
		//   NOTE: Using atan2 because it takes the sign of the coordinates into account
		//         giving the actual angle. Other trignometric functions are only defined
		//         for specific angle ranges, so some offset needs to be applied. atan2
		//         takes does the offset for us so we don't have to worry about it.
		f32 theta = my_atan2(reoriented.y, reoriented.x);
		
		// Calculate new x component using with x = r * cos(original_angle + additional_rotation)
		p->x = radius * my_cos(theta + rotation_amount);
		// // Calculate new y component using with y = r * sin(original_angle + additional_rotation)
		p->y = radius * my_sin(theta + rotation_amount);

		// Orient the resulting points so that they are relative to the original origin point
		p->x += center_of_rotation.x;
		p->y += center_of_rotation.y;
	}
}

void TranslatePoints(Point *points, i32 point_count, f32 translation_x, f32 translation_y) {
	for (i32 i = 0; i < point_count; i++) {
		Point *p = points + i;
		p->x += translation_x;
		p->y += translation_y;
	}
}

typedef struct {
	Point points[4];
	Vec2 direction;
	i32 live;
} Missile;

f32 DotProduct(Vec2 v1, Vec2 v2) {
	return (v1.x * v2.x) + (v1.y * v2.y);
}

f32 VectorLength(Vec2 v) {
	return DotProduct(v, v);
}

typedef struct {
	Vec2 pos;
	Vec2 direction;
	i32 speed;
	i32 radius;
	b8 active;
} Meteor;

typedef struct {
	u32 *pixels;
	i32 width;
	i32 height;
} DrawSurface;

void DrawCircle(DrawSurface *surface, i32 radius, f32 pos_x, f32 pos_y) {	
	f32 angle_step = 0.01f;
	for (f32 angle = 0; angle <= 2 * PI; angle += angle_step) {
		i32 x = my_ceil(pos_x + (radius * my_cos(angle)));
		i32 y = my_ceil(pos_y + (radius * my_sin(angle)));
		if (x < 0 || x > surface->width || y < 0 || y >= surface->height) {
			continue;
		}
		*(surface->pixels + x + (y * surface->width)) = 0xFFFFFFFF;
	}
}

void DrawRectangle(DrawSurface *surface, i32 offset_x, i32 offset_y, i32 rectangle_width, i32 rectangle_height, u32 color) {
	for (i32 y = offset_y; y < offset_y + rectangle_height ; y++) {
		// Chose this approach because setting each pixel
		// sequentially was taking 1ms+ depending on the
		// size of the screen.
		__stosd(
			(unsigned long*)(surface->pixels + offset_x + (y * surface->width)),
			color,
			rectangle_width
		);
	}
}

f32 Clamp(f32 value, f32 min, f32 max) {
	if (value < min) {
		return min;
	} else if (value > max) {
		return max;	
	}
	return value;
}

void DrawLine(DrawSurface *surface, Point p1, Point p2, u32 color) {
	f32 min_x = my_min(p2.x, p1.x);
	f32 max_x = my_max(p2.x, p1.x);
	f32 min_y = my_min(p2.y, p1.y);
	f32 max_y = my_max(p2.y, p1.y);
	
	if (p1.x == p2.x) {
		// vertical line
		i32 miny = my_floor(min_y);
		i32 maxy = my_ceil(max_y);
		i32 x = RoundNearest(p1.x);
		for (i32 y = miny; y < maxy; y++) {
			if (y <= 0 || y >= surface->height) {
				continue;
			}
			u32 *p = surface->pixels + x + (y * surface->width);
			*p = color;
		}
	} else if (p1.y == p2.y) {
		// horizontal line
		i32 int_min_x = RoundNearest(min_x);
		i32 int_max_x = RoundNearest(max_x);
		i32 y_value = RoundNearest(p2.y);
		for (i32 x = int_min_x; x <= int_max_x; x++) {
			*(surface->pixels + x + (y_value * surface->width)) = color;
		}
	} else {
		f32 m = (p1.y - p2.y)/(p1.x - p2.x);
		f32 b = p2.y - m * p2.x;		

		// NOTE: Calculate pixels for both x and y because if the slope
		//       be is too small the generated points will be too spread
		//       far a part which is not desirable.
		
		for (f32 y = min_y; y <= max_y; y += 1.0f) {
			f32 x = ((f32)y - b) / m;
			if (x < min_x || x > max_x) {
				continue;
			}
			i32 actual_x = RoundNearest(x);
			i32 actual_y = RoundNearest(Clamp(y, 0.0f, (f32)surface->height));
			u32 *p = surface->pixels + actual_x + (actual_y * surface->width);
			*p = color;			
		}
		
		for (f32 x = min_x; x <= max_x; x += 1.0f) {
			f32 y = (m*x) + b;
			if (y < min_y || y > max_y) {
				continue;
			}
			i32 actual_x = RoundNearest(x);
			i32 actual_y = RoundNearest(Clamp(y, 0.0f, (f32)surface->height));
			u32 *p = surface->pixels + actual_x + (actual_y * surface->width);
			*p = color;			
		}
	}
}

Vec2i Subtracti(Vec2i a, Vec2i b) {
	Vec2i result = {0};
	result.x = a.x - b.x;
	result.y = a.y - b.y;
	return result;
}

Vec2 Subtract(Vec2 a, Vec2 b) {
	Vec2 result = {0};
	result.x = a.x - b.x;
	result.y = a.y - b.y;
	return result;
}

f32 Length(Vec2i v) {
	f32 result = my_sqrt((f32)(v.x*v.x)+(v.y*v.y));
	return result;
}

b8 AnyPointsInsideCircle(i32 circle_radius, Vec2 circle_position, Point *points, i32 point_count) {
	for (i32 i = 0; i < point_count; i++) {
		Point p = points[i];
		
		Vec2 testpoint = {0};
		testpoint.x = p.x;
		testpoint.y = p.y;
		
		Vec2 d = Subtract(circle_position, testpoint); // displacement from center
		i32 distance_from_center = my_floor(my_sqrt((f32)(d.x*d.x)+(f32)(d.y*d.y)));
		if (distance_from_center <= circle_radius) {
			return 1;
		}
	}
	
	return 0;
}

typedef char Flag;
#define FLAG_UNSET 0;
#define FLAG_SET 1;

#define MISSILE_POOL_SIZE 16
#define MISSILE_WIDTH 4.0f
#define MISSILE_HEIGHT 8.0f
#define METEOR_POOL_SIZE 32

Missile Missiles[MISSILE_POOL_SIZE];
Meteor Meteors[METEOR_POOL_SIZE];	

static Flag Flag_Initialized = FLAG_UNSET;
static Flag Flag_DrawShip = FLAG_SET;
static b8 PlayerDead = 0;
static b8 GameOver = 0;
static u32 PlayerLives = 3;
static i32 Score = 0;
static f32 TimeSincePlayerDied = 0.0f;
static f32 TimeSinceLastDrawShipFlagFlipped = 0.0f;
static Vec2 ShipPosition = {0};
static Vec2 ShipVelocity = {0};
static f32 ShipRotationRadians = 0.0f;

Flag RandomInitialized = FLAG_UNSET;

i32 GenerateRandomNumber(i32 min, i32 max) {
	if (!RandomInitialized) {
		srand((u32)time(0));
		RandomInitialized = FLAG_SET;
	}
	i32 random_number = (rand() % (max - min + 1)) + min;
	return random_number;
}

#include "stdarg.h"
#include "stdio.h"

static void Platform_WriteConsole(char *format, ...) {
	va_list args;
	va_start(args, format);
	char msg[1024] = {0};
	vsprintf_s(msg, 1024, format, args);
	OutputDebugString(msg);
	va_end(args);
}

void SpawnMeteor(Vec2 position, i32 radius, i32 speed) {
	// find inactive meteor
	// configure the meteor
	// set the meteor to active
	for (i32 i = 0; i < METEOR_POOL_SIZE; i++) {
		Meteor *m = Meteors + i;
		if (m->active) continue;
		
		m->pos = position;
		m->radius = radius;
		m->speed = speed;
		
		
		
		f32 rand_x = (f32)GenerateRandomNumber(0, 1000);
		if (GenerateRandomNumber(0, 10) >= 5) {
			rand_x *= -1.0f;
		}
		
		f32 rand_y = (f32)GenerateRandomNumber(0, 1000);
		if (GenerateRandomNumber(0, 10) >= 5) {
			rand_y *= -1.0f;
		}
		
		f32 length = my_sqrt((rand_x * rand_x) + (rand_y * rand_y));
		rand_x /= length;
		rand_y /= length;
		
		m->direction.x = rand_x;
		m->direction.y = rand_y;
		
		Platform_WriteConsole("SpawnMeteor() Dir: x=%f y=%f\n", m->direction.x,
			m->direction.y);
		
		m->active = 1;
		return;
	}
	OutputDebugString("Unable to spawn a Meteor, the pool is full!\n");
}

void DrawString(DrawSurface *surface, char *text, i32 x, i32 y) {
	i32 text_length = (i32)strlen(text);
	KDTF_DrawText(&gFont, text, text_length, 0xFFFFFFFF, &x, &y, surface->pixels, surface->width, surface->height);
}

i32 RoundNearestFloat(f32 value) {
	i32 floored = Floor(value);
	if ((value - (f32)floored) < 0.5f) {
		return floored;
	} else {
		return floored + 1;
	}
}

void NewLine(i32 *y_coordinate) {
	i32 new_coordinate = *y_coordinate + gFont.line_height;
	*y_coordinate = new_coordinate;
}

b8 MenuButton(DrawSurface *surface, char *text, i32 at_x, i32 at_y) {
	i32 text_length = (i32)strlen(text);
	
	b8 MouseHoveringOverButton = 0;
	
	i32 text_width = gFont.atlas.glyph_width * text_length;
	if (MouseX >= at_x && MouseX <= (at_x + text_width)) {
		if (MouseY >= at_y && MouseY <= (at_y + gFont.atlas.glyph_height)) {
			MouseHoveringOverButton = 1;
		}
	}
	
	u32 TextColor = 0xFFFFFFFF;
	if (MouseHoveringOverButton) {
		TextColor = 0xFF3D3D3D;
	}
	
	KDTF_DrawText(&gFont, text, text_length, TextColor, &at_x, &at_y, surface->pixels, surface->width, surface->height);
	
	return MouseHoveringOverButton && MouseLeftButtonDown;
}

static void UpdateAndRender(f32 delta_time, DrawSurface *surface) {
	/*
		What could have been done better:
			* Writing the usage code first
				- Drawing text on the screen
				- Clearing the background color			
			* Use integers for position data instead of floats
			* Do more calculations on data when looping over an array.
			  I loop over the missile pool several times, performing
			  different operations each time. Memory is the slowest thing
			  you're going to be working with, so take advantage of the 
			  data when it's already in memory
			* Instead of using a boolean when there is a list of items,
			  keep the list of items separate from that boolean value.
			* Keep instruction level parallelism in mind
		
		What should I continue to do:
			* Using pools when you have several entities of the same type,
			  marking the entity as either alive/dead.
			* Don't insert a split between updating and rendering the game
			  artificially. In this case, the logic was much easier to understand.
		
		What I should stop doing:
			* Don't put things into a global state struct until you need it for
			  some reason.
			* Rushing through fixing something
	*/

	if (!Flag_Initialized) {
		Flag_DrawShip = FLAG_SET;
		PlayerDead = 0;
		PlayerLives = 3;
		Score = 0;
		TimeSincePlayerDied = 0.0f;
		TimeSinceLastDrawShipFlagFlipped = 0.0f;
	
		ShipPosition.x = (WindowWidth / 2.0f);
		ShipPosition.y = (WindowHeight / 2.0f);
		
		ShipVelocity.x = 0.0f;
		ShipVelocity.y = 0.0f;
		
		ShipRotationRadians = 0.0f;
		
		for (i32 i = 0; i < METEOR_POOL_SIZE; i++) {
			Meteor *m = Meteors + i;
			m->pos.x = 0.0f;
			m->pos.y = 0.0f;
			m->direction.x = 0.0f;
			m->direction.y = 0.0f;
			m->speed = 0;
			m->radius = 0;
			m->active = 0;
		}
		
		i32 NumberOfMeteorsToIntialize = 24;// rand() % METEOR_POOL_SIZE;
		for (i32 i = 0; i < NumberOfMeteorsToIntialize; i++) {
			Vec2 position = {0};
			position.x = (f32)(rand() % surface->width);
			position.y = (f32)(rand() % surface->height);
			i32 radius = 20 + (rand() % 30);
			i32 speed = 150 + (rand() % 100);
			SpawnMeteor(position, radius, speed);
		}
		
		/*
			Point points[4];
			Vec2 direction;
			i32 live;
		*/
		for (i32 i = 0; i < MISSILE_POOL_SIZE; i++) {
			Missile *m = Missiles + i;
			for (i32 k = 0; k < 4; k++) {
				m->points[k].x = 0.0f;
				m->points[k].y = 0.0f;
			}
			m->direction.x = 0.0f;
			m->direction.y = 0.0f;
			m->live = 0;
		}
		
		Flag_Initialized = FLAG_SET;
	}

//	if (!Playing) {
//		
//	}
	
	if (gPaused) {
		DrawRectangle(surface, 0, 0, surface->width, surface->height, BACKGROUND_COLOR);
		
//		DrawRectangle(surface, MouseX, MouseY, 5, 5, 0xFFFFFFFF);
		
		f32 relative_x = 0.6f;
		i32 x = my_floor((f32)surface->width * relative_x);
		f32 relative_y = 0.4f;
		i32 y = my_floor((f32)surface->height * relative_y);
		
		DrawString(surface, "ASTEROIDS!", x, y);
		
		NewLine(&y);
		NewLine(&y);
		b8 restart_pressed = MenuButton(surface, "Restart", x, y);
		if (restart_pressed) {
			Flag_Initialized = FLAG_UNSET;
			gPaused = 0;
		}
		
		NewLine(&y);
		NewLine(&y);
		b8 quit_pressed = MenuButton(surface, "Quit", x, y);
		if (quit_pressed) ExitProcess(0);
				
		return;
	}
	
	Triangle ship_triangle = {
		.a = { 0, 0 },
		.b = { 15, 45 },
		.c = { 30, 0 }
	};
	Point *ship_points = (Point*)&ship_triangle;
	i32 ship_point_count = 3;
	
	//////////////////////////////////////////////
	/// Apply Rotation
	///
	if (!PlayerDead) {
		if (gRotateShipLeft && !gRotateShipRight) {
			ShipRotationRadians -= delta_time * SHIP_ROTATION_STEP;
		} else if (gRotateShipRight) {
			ShipRotationRadians += delta_time * SHIP_ROTATION_STEP;
		}
	}

	// Use triangle centroid as the center of rotation
	Point rotation_origin = Centroid(ship_triangle);
	RotatePoints((Point*)&ship_triangle, 3, rotation_origin, ShipRotationRadians);
	
	Point ship_center = Centroid(ship_triangle);
	Vec2 ship_forward_direction = {0};
	ship_forward_direction.x = ship_triangle.b.x - ship_center.x;
	ship_forward_direction.y = ship_triangle.b.y - ship_center.y;
	f32 forward_vector_length = my_sqrt((ship_forward_direction.x*ship_forward_direction.x) + 
										(ship_forward_direction.y*ship_forward_direction.y));
	ship_forward_direction.x /= forward_vector_length;
	ship_forward_direction.y /= forward_vector_length;
	//////////////////////////////////////////////
	//////////////////////////////////////////////
	

	///////////////////////////////////////////////
	/// Apply Translation
	///
	ShipVelocity.x *= .9965f;
	ShipVelocity.y *= .9965f;

	if (!PlayerDead) {	
		if (gMoveShipForward) {		
			f32 acceleration = 3.0f; // u/s^2
	
			ShipVelocity.x += ship_forward_direction.x * (delta_time * acceleration);
			ShipVelocity.y += ship_forward_direction.y * (delta_time * acceleration);
		}
	}
	
	ShipPosition.x += ShipVelocity.x;
	ShipPosition.y += ShipVelocity.y;

	{
		Extents e = CalculateExtents((Point*)&ship_triangle, 3);
		i32 ship_draw_area_width = e.max_x - e.min_x;
		i32 ship_draw_area_height = e.max_y - e.min_y;	

		// don't check <= because ship_pos_x will be equal if
		// it goes off the right side of the screen
		if (ShipPosition.x < -ship_draw_area_width) {
			// went off left side of screen
			ShipPosition.x = (f32)(surface->width - 1);
		} else if (ShipPosition.x > (surface->width - 1)) {
			// went off right side of screen
			ShipPosition.x = (f32)-ship_draw_area_width;
		}

		// don't check <= because ship_pos_y will be equal if
		// it goes off the top side of the screen
		if (ShipPosition.y < -ship_draw_area_height) {
			// ship went off bottom of screen
			ShipPosition.y = (f32)(surface->height - 1);
		} else if (ShipPosition.y > (surface->height - 1)) {
			// ship went off top of screen
			ShipPosition.y = (f32)-ship_draw_area_height;
		}
	}

	TranslatePoints((Point*)&ship_triangle, 3, ShipPosition.x, ShipPosition.y);

	ship_center = Centroid(ship_triangle);
	ship_forward_direction.x = ship_triangle.b.x - ship_center.x;
	ship_forward_direction.y = ship_triangle.b.y - ship_center.y;
	forward_vector_length = my_sqrt((ship_forward_direction.x*ship_forward_direction.x) + (ship_forward_direction.y*ship_forward_direction.y));
	ship_forward_direction.x /= forward_vector_length;
	ship_forward_direction.y /= forward_vector_length;

	//////////////////////////////////////////////
	//////////////////////////////////////////////
	
	///////////////////////////////////////////////
	///////////////////////////////////////////////

	for (i32 i = 0; i < MISSILE_POOL_SIZE; i++) {
		Missile *missile = Missiles + i;

		if (!missile->live) continue;
		
		f32 speed = delta_time * MISSILE_SPEED;
		f32 missile_delta_x = speed * missile->direction.x;
		f32 missile_delta_y = speed * missile->direction.y;

		TranslatePoints(missile->points, 4, missile_delta_x, missile_delta_y);

		Extents e = CalculateExtents(missile->points, 4);
		if ((e.min_x < 0 && e.max_x < 0) || 
			(e.min_x > surface->width || e.max_x > surface->width) ||
			(e.min_y < 0 && e.max_y < 0) ||
			(e.min_y > surface->height || e.max_y > surface->height)) {
				OutputDebugString("Missile destroyed!\n");
				missile->live = 0;
		}
	}

	if (!PlayerDead && gShootMissile) {
		Missile *new_missile = 0;
		for (i32 i = 0; i < MISSILE_POOL_SIZE; i++) {
			Missile *m = Missiles + i;
			if (!m->live) {
				new_missile = m;
				break;
			}
		}

		if (new_missile) {
			// set the missile points(the missile will be reused)
			new_missile->points[0].x = 0;
			new_missile->points[0].y = 0;
			new_missile->points[1].x = 0;
			new_missile->points[1].y = MISSILE_HEIGHT;
			new_missile->points[2].x = MISSILE_WIDTH;
			new_missile->points[2].y = MISSILE_HEIGHT;
			new_missile->points[3].x = MISSILE_WIDTH;
			new_missile->points[3].y = 0;

			// rotate the missile so it's in the same direction as the ship
			Point missile_center = {0};
			missile_center.x = MISSILE_WIDTH / 2.0f;
			missile_center.y = MISSILE_HEIGHT / 2.0f;
			RotatePoints(new_missile->points, 4, missile_center, ShipRotationRadians);

			// put the missile at the tip of the ship
			TranslatePoints(new_missile->points, 4, ship_triangle.b.x, ship_triangle.b.y);

			new_missile->direction = ship_forward_direction;

			new_missile->live = 1;
		}

		gShootMissile = 0;
	}

	// Clear background
	DrawRectangle(surface, 0, 0, surface->width, surface->height, BACKGROUND_COLOR);	

	if (PlayerDead) {
		TimeSincePlayerDied += delta_time;
		TimeSinceLastDrawShipFlagFlipped += delta_time;
	
		if (TimeSincePlayerDied >= 3.0f) {
			PlayerDead = 0;
			Flag_DrawShip = 1;
			TimeSincePlayerDied = 0.0f;
			TimeSinceLastDrawShipFlagFlipped = 0.0f;
		} else if (TimeSinceLastDrawShipFlagFlipped >= 0.3f) {
			Flag_DrawShip = !Flag_DrawShip;
			TimeSinceLastDrawShipFlagFlipped = 0.0f;
		}
	}
	
	if (Flag_DrawShip) {
		for (i32 i = 0; i < ship_point_count; i++) {
			Point p1 = *(ship_points + i);
			Point p2 = {0};
			if (i == ship_point_count - 1) {
				p2 = *ship_points;
			} else {
				p2 = *(ship_points + i + 1);
			}
			
			DrawLine(surface, p1, p2, SHIP_COLOR);
		}
	}
	
	for (i32 i = 0; i < MISSILE_POOL_SIZE; i++) {
		Missile missile = Missiles[i];
		if (!missile.live) continue;

		// constrain how the number of points to check that in a the missile rectangle
		Extents e = CalculateExtents(missile.points, 4);

		// NOTE: Rasterize the rectangle by checking if a point is inside the rectangle. Do so
		//       by constructing vectors representing the sides of the rectangle, and a vector
		//       representing the test point. Project the vector representing the test point on
		//       to both vectors representing the sides of the rectangle, and if the projection
		//       length is within the respective range, then the point is inside the rectangle.

		Vec2 ab = {0};
		ab.x = missile.points[1].x - missile.points[0].x;
		ab.y = missile.points[1].y - missile.points[0].y;
		f32 dot_ab_ab = VectorLength(ab);

		Vec2 ad = {0};
		ad.x = missile.points[3].x - missile.points[0].x;
		ad.y = missile.points[3].y - missile.points[0].y;
		f32 dot_ad_ad = VectorLength(ad);

		for (i32 y = e.min_y; y < e.max_y; y++) {
			for (i32 x = e.min_x; x < e.max_x; x++) {
				Vec2 am = { .x = (x - missile.points[0].x), .y = (y - missile.points[0].y) };

				f32 dot_am_ab = DotProduct(am, ab);
				if (dot_am_ab < 0 || dot_am_ab > dot_ab_ab) {
					continue;
				}

				f32 dot_am_ad = DotProduct(am, ad);
				if (dot_am_ad < 0 || dot_am_ad > dot_ad_ad) {
					continue;
				}
				
				if (x < 0 || x >= surface->width) {
					continue;
				}
				if (y < 0 || y >= surface->height) {
					continue;
				}
				u32 *pixel = surface->pixels + x + (y * surface->width);
				*pixel = 0xFFFFFFFF;
			}
		}
	}

	for (i32 i = 0; i < METEOR_POOL_SIZE; i++) {
		Meteor *meteor = Meteors + i;
		if (!meteor->active) continue;
		meteor->pos.x += meteor->direction.x * (delta_time * meteor->speed);
		meteor->pos.y += meteor->direction.y * (delta_time * meteor->speed);
		
		if ((meteor->pos.x + meteor->radius) <= 0) {
			meteor->pos.x = (f32)(surface->width + meteor->radius);
		} else if ((meteor->pos.x - meteor->radius) >= surface->width) {
			meteor->pos.x = (f32)(-1 * meteor->radius);
		}
		
		if ((meteor->pos.y + meteor->radius) <= 0) {
			meteor->pos.y = (f32)(surface->height + meteor->radius);
		} else if ((meteor->pos.y - meteor->radius) >= surface->height) {
			meteor->pos.y = (f32)(-1 * meteor->radius);
		}
		
		DrawCircle(surface, meteor->radius, meteor->pos.x, meteor->pos.y);
	}
	
	// Collision detection between ship and meteor
	if (!PlayerDead) {
		for (i32 i = 0; i < METEOR_POOL_SIZE; i++) {
			Meteor *meteor = Meteors + i;
			if (!meteor->active) continue;
			
			if (AnyPointsInsideCircle(meteor->radius, meteor->pos, ship_points, 3)) {
				PlayerDead = 1;
				PlayerLives -= 1;
				if (PlayerLives == 0) {
					GameOver = 1;
				}
			}
		}
	}
	
	for (i32 i = 0; i < METEOR_POOL_SIZE; i++) {
		Meteor *meteor = Meteors + i;
		if (!meteor->active) continue;
		
		for (i32 j = 0; j < MISSILE_POOL_SIZE; j++) {
			Missile *missile = Missiles + j;
			if (!missile->live) continue;
			
			if (AnyPointsInsideCircle(meteor->radius, meteor->pos, missile->points, 4)) {
				meteor->active = 0;
				missile->live = 0;
				Score += 1;
				if (meteor->radius >= 35) {
					i32 radius = 15 + (rand() % 15);
					i32 speed = meteor->speed;
					SpawnMeteor(meteor->pos, radius, speed);
					radius = 15 + (rand() % 15);
					speed = meteor->speed;
					SpawnMeteor(meteor->pos, radius, speed);
				}
				break;
			}
		}
	}
	
	i32 active_meteor_count = 0;
	for (i32 i = 0; i < METEOR_POOL_SIZE; i++) {
		Meteor *meteor = Meteors + i;
		active_meteor_count += meteor->active;
	}
	if (active_meteor_count == 0) {
		MessageBox(
			NULL,
			"You won! You destoryed all the meteors!",
			"Asteroids!",
			MB_OK
		);
		ExitProcess(0);
	}

	if (GameOver) {
		f32 relative_x = 0.5f;
		i32 xPos = my_ceil((f32)surface->width * relative_x);
	
		f32 relative_y = 0.5f;
		i32 yPos = my_ceil((f32)surface->height * relative_y);
		
		DrawString(surface, "GAME OVER", xPos, yPos);		
	}
	
	f32 relative_x = 0.05f;
	i32 xPos = my_ceil((f32)surface->width * relative_x);

	f32 relative_y = 0.05f;
	i32 yPos = my_ceil((f32)surface->height * relative_y);

	char *score_text = "Score: ";
	i32 score_text_length = (i32)strlen(score_text);
	KDTF_DrawText(&gFont, score_text, score_text_length, 0xFFFFFFFF, &xPos, &yPos, surface->pixels, surface->width, surface->height);
	KDTF_DrawNumber(&gFont, Score, 0xFFFFFFFF, &xPos, &yPos, surface->pixels, surface->width, surface->height);

	xPos = my_ceil((f32)surface->width * relative_x);
	NewLine(&yPos);
	
	char *lives_text = "Lives: ";
	i32 lives_left_text_length = (i32)strlen(lives_text);
	KDTF_DrawText(&gFont, lives_text, lives_left_text_length, 0xFFFFFFFF, &xPos, &yPos, surface->pixels, surface->width, surface->height);
	KDTF_DrawNumber(&gFont, PlayerLives, 0xFFFFFFFF, &xPos, &yPos, surface->pixels, surface->width, surface->height);
}

b8 Win32_ReadFile(char *filepath, void **out_bytes, u64 *out_byte_count) {
	HANDLE file_handle = CreateFileA(
		filepath, GENERIC_READ|GENERIC_WRITE, FILE_SHARE_READ, 
		0, OPEN_EXISTING, 0, 0);
	
	if (file_handle == INVALID_HANDLE_VALUE) {
		wchar_t message[256] = {0};
		wsprintfW(message, L"CreateFileW(%s) failed with error: %d\n", filepath, GetLastError());
		OutputDebugStringW(message);
		return false;
	}
	
	LARGE_INTEGER file_size;
	if (!GetFileSizeEx(file_handle, &file_size)) {
		wchar_t message[256] = {0};
		wsprintfW(message, L"ReadFile(%s) failed with error: %d\n", filepath, GetLastError());
		OutputDebugStringW(message);
		CloseHandle(file_handle);
		return false;
	}
	
	Assert(file_size.QuadPart >= 0);

	if (file_size.QuadPart == 0) {
		CloseHandle(file_handle);
		// the file is empty!
		return true;
	}
	
	u8 *file_contents = (u8*)VirtualAlloc(0, file_size.QuadPart, MEM_RESERVE|MEM_COMMIT, PAGE_READWRITE);
	Assert(file_contents != NULL);
	
	BOOL read_file = ReadFile(file_handle, file_contents, (DWORD)file_size.QuadPart, NULL, NULL);
	
	if (!read_file) {
		OutputDebugStringW(L"Failed to read file!\n");
		CloseHandle(file_handle);
		VirtualFree(file_contents, file_size.QuadPart, MEM_RELEASE);
		return false;
	}

	CloseHandle(file_handle);

	*out_byte_count = file_size.QuadPart;
	*out_bytes = file_contents;

	return true;
}

void *MyAlloc(u64 size) {
	return VirtualAlloc(0, size, MEM_RESERVE|MEM_COMMIT, PAGE_READWRITE);
}

void MyFree(void *ptr) {
	VirtualFree(ptr, 0, MEM_RELEASE);
}

#define CHARACTER_COUNT 93

int WinMainCRTStartup() {
	void *font_file_contents = 0;
	u64 font_file_size_bytes = 0;
	if (!Win32_ReadFile("JetBrainsMono-Regular.ttf", &font_file_contents, &font_file_size_bytes)) {
		MessageBox(NULL, "Failed to load font file!", "Font Error", MB_OK);
		ExitProcess(1);
	}
	
	char character_set[CHARACTER_COUNT] = {0};
	for (i32 i = 0; i < CHARACTER_COUNT; i++) {
		character_set[i] = '!' + (char)i;
	}
	
	gFont = KDTF_AllocateFont(
		"JetBrainsMono-Regular.ttf", character_set, true,
		false, 32.0f, 32.0f, 32.0f, 1.0f,
		MyAlloc, MyFree,
		MyAlloc, MyFree);
	if (gFont.load_error) {
		MessageBox(NULL, "Failed to parse font file!", "Font Error", MB_OK);
		ExitProcess(1);
	}
	
	HINSTANCE hInstance = GetModuleHandle(0);

	wchar_t *window_class_name = L"Asteroids_Window_class";
	WNDCLASSEXW window_class = {0};
	window_class.cbSize = sizeof(window_class);
	window_class.hInstance = hInstance;
	window_class.lpszClassName = window_class_name;
	window_class.lpfnWndProc = WindowCallback;
	
	ATOM registered_class = RegisterClassExW(&window_class);
	if (!registered_class) {
		MessageBox(
			NULL,
			"Unable to register window class!",
			"Window Registration Error",
			MB_OK
		);
		return 1;
	}

	HWND window = CreateWindowExW(
		WS_EX_APPWINDOW,
		window_class.lpszClassName, // lpClassName
		L"Asteroids!", // lpWindowName
		WS_OVERLAPPEDWINDOW & ~WS_THICKFRAME & ~WS_MAXIMIZEBOX, // dwStyle
		CW_USEDEFAULT, // X
		CW_USEDEFAULT, // Y
		DEFAULT_WINDOW_WIDTH, // nWidth
		DEFAULT_WINDOW_HEIGHT, // nHeight
		NULL, // hWndParent
		NULL, // hMenu
		hInstance, // hInstance(module instance)
		NULL // lParam
	);

	if (window == NULL) {
		OutputDebugString("Window creation failed!\n");
		return 1;
	}

	RECT client_rect = {0};
	GetClientRect(window, &client_rect);

	DXGI_SWAP_CHAIN_DESC swap_chain_description = {
		.BufferDesc = {
			.Width = WindowWidth,
			.Height = WindowHeight,
			.RefreshRate = { 60, 1 },
			.Format = DXGI_FORMAT_B8G8R8A8_UNORM
		},
		.SampleDesc = { 1, 0 },
		.BufferUsage = DXGI_USAGE_BACK_BUFFER,
		.BufferCount = 1,
		.OutputWindow = window,
		.Windowed = TRUE,
		.SwapEffect = DXGI_SWAP_EFFECT_DISCARD
	};

	i32 swap_chain_flags = 0;
	swap_chain_flags |= D3D11_CREATE_DEVICE_DEBUG; 

	HRESULT hResult = 0;

	IDXGISwapChain *swap_chain;
	ID3D11Device *d3d11_device;
	ID3D11DeviceContext *d3d11_device_context;
	hResult = D3D11CreateDeviceAndSwapChain(
		NULL,                      //IDXGIAdapter *pAdapter
		D3D_DRIVER_TYPE_HARDWARE,  
		NULL,                      //HMODULE Software
		swap_chain_flags,
		NULL,                      //D3D_FEATURE_LEVEL *pFeatureLevels
		0,                         //UINT FeatureLevels
		D3D11_SDK_VERSION,
		&swap_chain_description,
		&swap_chain,
		&d3d11_device,
		NULL,                      //D3D_FEATURE_LEVEL *pFeatureLevel,
		&d3d11_device_context);

	if (FAILED(hResult)) {
		// try to create a software rasterization driver
		OutputDebugString("Falling back to software driver\n");
		hResult = D3D11CreateDeviceAndSwapChain(
			NULL,
			D3D_DRIVER_TYPE_WARP,
			NULL,
			swap_chain_flags,
			NULL,
			0,
			D3D11_SDK_VERSION,
			&swap_chain_description,
			&swap_chain,
			&d3d11_device,
			NULL,                      //D3D_FEATURE_LEVEL *pFeatureLevel,
			&d3d11_device_context);
		if (FAILED(hResult)) {
			MessageBox(
				NULL,
				"Failed to initialize D3D11!",
				"D3D11 Error",
				MB_OK);
			return 1;
		}
	}
	
	{
		ID3D11InfoQueue *info_queue;
		ID3D11Device_QueryInterface(d3d11_device, &IID_ID3D11InfoQueue, (void**)&info_queue);
		ID3D11InfoQueue_SetBreakOnSeverity(info_queue, D3D11_MESSAGE_SEVERITY_CORRUPTION, TRUE);
		ID3D11InfoQueue_SetBreakOnSeverity(info_queue, D3D11_MESSAGE_SEVERITY_ERROR, TRUE);
		ID3D11InfoQueue_Release(info_queue);
	}
	
	{
		IDXGIInfoQueue *info_queue;
		hResult = DXGIGetDebugInterface1(0, &IID_IDXGIInfoQueue, (void**)&info_queue);
		Assert(SUCCEEDED(hResult));
		IDXGIInfoQueue_SetBreakOnSeverity(info_queue, DXGI_DEBUG_ALL, DXGI_INFO_QUEUE_MESSAGE_SEVERITY_CORRUPTION, TRUE);
		IDXGIInfoQueue_SetBreakOnSeverity(info_queue, DXGI_DEBUG_ALL, DXGI_INFO_QUEUE_MESSAGE_SEVERITY_ERROR, TRUE);
		IDXGIInfoQueue_Release(info_queue);
	}
	
	ID3D11Texture2D *gpubuffer = NULL;
	ID3D11Texture2D *cpubuffer = NULL;

	ShowWindow(window, SW_SHOW);

	LARGE_INTEGER now = {0};
	LARGE_INTEGER performance_freq = {0};
	QueryPerformanceFrequency(&performance_freq);
//	i64 ticks_per_second = performance_freq.QuadPart;


#define TARGET_FRAME_TIME_MICROS (1000000/60)

	b8 d3d11_initialized = 0;
	
	QueryPerformanceCounter(&now);
	i64 last_tick_count = now.QuadPart;
	
	u32 *pixels = 0;
	u32 cpu_buffer_width = WindowWidth;
	
	srand((u32)now.QuadPart);

	MSG msg = {0};
	while (!gShouldCloseWindow) {
		while (PeekMessageA(&msg, window, 0, 0, PM_REMOVE)) {
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}

		GetClientRect(window, &client_rect);
		i32 new_width = client_rect.right;
		i32 new_height = client_rect.bottom;

		b8 window_resized = WindowWidth != new_width || WindowHeight != new_height;
		if (window_resized && !d3d11_initialized) {
			if (gpubuffer) {
				ID3D11Texture2D_Release(gpubuffer);
			}
			if (cpubuffer) {
				ID3D11Texture2D_Release(cpubuffer);
			}
			gpubuffer = NULL;
			cpubuffer = NULL;

			WindowWidth = new_width;
			WindowHeight = new_height;

			b8 window_is_not_minimized = WindowWidth != 0 && WindowHeight != 0;
			if (window_is_not_minimized) {
				hResult = IDXGISwapChain_ResizeBuffers(
					swap_chain, 1, WindowWidth, WindowHeight, DXGI_FORMAT_B8G8R8A8_UNORM, 0);
				Assert(SUCCEEDED(hResult));

				hResult = IDXGISwapChain_GetBuffer(swap_chain, 0, &IID_ID3D11Texture2D, (void**)&gpubuffer);
				Assert(SUCCEEDED(hResult));

				D3D11_TEXTURE2D_DESC cpu_buffer_description = {
					.Width = WindowWidth,
					.Height = WindowHeight,
					.MipLevels = 1,
					.ArraySize = 1,
					.Format = DXGI_FORMAT_B8G8R8A8_UNORM,
					.SampleDesc = { 1, 0 },
					.Usage = D3D11_USAGE_DYNAMIC,
					.BindFlags = D3D11_BIND_SHADER_RESOURCE,
					.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE
				};

				hResult = ID3D11Device_CreateTexture2D(d3d11_device, &cpu_buffer_description, NULL, &cpubuffer);
				Assert(SUCCEEDED(hResult));
				
				D3D11_MAPPED_SUBRESOURCE cpu_buffer_subresource = {0};
				hResult = ID3D11DeviceContext_Map(d3d11_device_context, (ID3D11Resource*)cpubuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &cpu_buffer_subresource);
				Assert(SUCCEEDED(hResult));
				
				cpu_buffer_width = cpu_buffer_subresource.RowPitch / sizeof(u32);
				
				ID3D11DeviceContext_Unmap(d3d11_device_context, (ID3D11Resource*)cpubuffer, 0);
				
				pixels = VirtualAlloc(0, cpu_buffer_width * WindowHeight * sizeof(u32), MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
				Assert(pixels);
			}

			d3d11_initialized = 1;
		}

		QueryPerformanceCounter(&now);
		i64 current_tick_count = now.QuadPart;

//		i64 elapsed_ticks_since_last_frame = current_tick_count - last_tick_count;
//#pragma warning(disable: 4189)
//		f32 time_since_last_frame = (f32)(elapsed_ticks_since_last_frame) / ticks_per_second;
//#pragma warning(default: 4189)
		
//		char time_since_last_frame_msg[256] = {0};
//		snprintf(time_since_last_frame_msg, sizeof(time_since_last_frame_msg),
//			"Delta Time: %fs\n", time_since_last_frame);
//		OutputDebugString(time_since_last_frame_msg);


		last_tick_count = current_tick_count;
		
		POINT cursor = {0};
		GetCursorPos(&cursor);
		ScreenToClient(window, &cursor);
		MouseX = cursor.x;
		MouseY = cursor.y;
		
		Platform_WriteConsole("Mouse: x=%d y=%d\n", MouseX, MouseY);

		DrawSurface ds = {0};
		ds.pixels = pixels;
		ds.width = cpu_buffer_width;
		ds.height = WindowHeight;

		UpdateAndRender(0.003f, &ds);

		b8 window_visible = WindowWidth && WindowHeight;
		if (window_visible) {
			D3D11_MAPPED_SUBRESOURCE mapped_resource = {0};
			hResult = ID3D11DeviceContext_Map(d3d11_device_context, (ID3D11Resource*)cpubuffer, 0,
				D3D11_MAP_WRITE_DISCARD, 0, &mapped_resource);
			Assert(SUCCEEDED(hResult));

			for (u32 i = 0; i < (cpu_buffer_width * WindowHeight); i++) {
				((u32*)mapped_resource.pData)[i] = pixels[i];
			}			

			ID3D11DeviceContext_Unmap(d3d11_device_context, (ID3D11Resource*)cpubuffer, 0);
			ID3D11DeviceContext_CopyResource(d3d11_device_context, (ID3D11Resource*)gpubuffer, 
				(ID3D11Resource*)cpubuffer);
		}
			
		hResult = IDXGISwapChain_Present(swap_chain, 0, 0);
		if (hResult == DXGI_STATUS_OCCLUDED) {
			// window is not visible
			Sleep(10);
		} else if (!SUCCEEDED(hResult)) {
			hResult = ID3D11Device_GetDeviceRemovedReason(d3d11_device);
			Assert(false);
		}

//		QueryPerformanceCounter(&now);
//		i64 frame_timer_end_tick = now.QuadPart;
		
//		i64 elapsed_ticks = (frame_timer_end_tick - frame_timer_begin_tick);
//		f32 frame_elapsed_time_micros = (elapsed_ticks * 1000000.0f) / ticks_per_second;
//		f32 frame_elapsed_time_millis = (frame_elapsed_time_micros / 1000.0f);
//		char frame_time_msg[256] = {0};
//		snprintf(frame_time_msg, sizeof(frame_time_msg), "Frame: %fms (%fus)\n", 
//			frame_elapsed_time_millis, frame_elapsed_time_micros);
//		OutputDebugStringA(frame_time_msg);
	}
	
	// CRT not present, so have to manually exit process here
	ExitProcess(0);
}

