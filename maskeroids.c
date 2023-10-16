#define WIN32_LEAN_AND_MEAN
#include <windows.h>
int _fltused;

#include <intrin.h>
#include <emmintrin.h>
#include <math.h>


// COMPLETE:
// - Open window with default size of 800x600 and title "Maskeroids"
// - Set background color to gray
// - Draw Ship on the screen as a triangle
// - Rotate the ship with the arrow keys
// - Move the ship forward with the up arrow key
// - Allow ship to go off one side of the screen and appear on the other side
// - Fire a small rectangle from the front of the ship when the user
//   hits the spacebar

// TODO:
// - Draw meteor on the screen as a circle
// - Give the meteor a random radius
// - Give the meteor a velocity in a random direction
// - Add Collision detection between ship and the meteor
// - Add Collision detection between bullet and the meteor
// - Keep track of score
// - On collision with bullet, split meteor into a random number of
//   smaller meteors with random radius all in opposite directions
// - On collision with ship, game over
// - Add Pause menu with quit and restart buttons
// - Add audio
// - Draw bitmaps for the ship, bullet, and meteor

#define DEFAULT_WINDOW_WIDTH 800
#define DEFAULT_WINDOW_HEIGHT 600

#define BACKGROUND_COLOR 0xFF111111
// #define SHIP_COLOR 0xFFFFFFFF
#define SHIP_COLOR 0xFFFF553B

typedef int i32;
typedef unsigned int u32;
typedef float f32;
typedef double f64;

static i32 gShouldCloseWindow = 0;

typedef struct {
	BITMAPINFO bitmapInfo;
	u32 *pixels;
} Win32_Backbuffer;

static Win32_Backbuffer gBackbuffer = {0};

void Win32_CopyBackbufferToScreen(HDC dc) {
	i32 width = gBackbuffer.bitmapInfo.bmiHeader.biWidth;
	i32 height = gBackbuffer.bitmapInfo.bmiHeader.biHeight;
	StretchDIBits(dc, 
		0, 0, width, height,
		0, 0, width, height,
		gBackbuffer.pixels,
		&gBackbuffer.bitmapInfo,
		DIB_RGB_COLORS,
		SRCCOPY);	
}

void Win32_AllocateBackbuffer(i32 width, i32 height) {
	if (gBackbuffer.pixels) {
		VirtualFree(gBackbuffer.pixels, 0, MEM_RELEASE);
		gBackbuffer.pixels = 0;
	}

	gBackbuffer.bitmapInfo.bmiHeader.biSize = sizeof(gBackbuffer.bitmapInfo.bmiHeader);
	gBackbuffer.bitmapInfo.bmiHeader.biWidth = width;
	gBackbuffer.bitmapInfo.bmiHeader.biHeight = height;
	gBackbuffer.bitmapInfo.bmiHeader.biPlanes = 1;
	gBackbuffer.bitmapInfo.bmiHeader.biBitCount = 32;
	gBackbuffer.bitmapInfo.bmiHeader.biCompression = BI_RGB;

	i32 bytes_per_pixel = 4;
	i32 bitmap_mem_size = bytes_per_pixel * (width * height);
	gBackbuffer.pixels = (unsigned int *)VirtualAlloc(0, bitmap_mem_size, MEM_RESERVE|MEM_COMMIT, PAGE_READWRITE);
}

void DrawRectangle(i32 offset_x, i32 offset_y, i32 rectangle_width, i32 rectangle_height, u32 color) {
	for (i32 y = offset_y; y < offset_y + rectangle_height ; y++) {
		// Chose this approach because setting each pixel
		// sequentially was taking 1ms+ depending on the
		// size of the screen.
		__stosd(
			(unsigned long*)(gBackbuffer.pixels + offset_x + (y * gBackbuffer.bitmapInfo.bmiHeader.biWidth)),
			color,
			rectangle_width
		);
	}
}

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

int isPointInTriangle(f32 x, f32 y, Triangle *t) {
	// vector v0
	f32 v0x = (f32)(t->b.x - t->a.x);
	f32 v0y = (f32)(t->b.y - t->a.y);

	// vector v1
	f32 v1x = (f32)(t->c.x - t->a.x);
	f32 v1y = (f32)(t->c.y - t->a.y);

	// vector v2
	f32 v2x = x - t->a.x;
	f32 v2y = y - t->a.y;

	// Calculate barycentric coordinates using cramers rule
	f32 d00 = (v0x * v0x) + (v0y * v0y); // Dot Product: v0 . v0
	f32 d01 = (v0x * v1x) + (v0y * v1y); // dot product: v0 . v1
	f32 d11 = (v1x * v1x) + (v1y * v1y); // dot product: v1 . v1
	f32 d20 = (v2x * v0x) + (v2y * v0y); // dot product: v2 . v0
	f32 d21 = (v2x * v1x) + (v2y * v1y); // dot proudct: v2 . v1
	f32 denom = d00 * d11 - d01 * d01;

	f32 beta = (d11 * d20 - d01 * d21) / denom;
	f32 gamma = (d00 * d21 - d01 * d20) / denom;
	f32 alpha = 1.0f - beta - gamma;

	return (alpha >= 0.0 && alpha <= 1.0f) && 
		   (beta >= 0.0 && beta <= 1.0f) && 
		   (gamma >= 0.0 && gamma <= 1.0f);
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
	Triangle ship_body;
	Vec2 forward;
	i32 posX, posY;	
} Ship;

#define SHIP_ROTATION_STEP 0.1f
#define SHIP_SPEED 3.0f

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

#define PI 3.14f
#define DEG2RAD(X) (X * (PI/180.0f))

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
		
		case WM_SIZE: {
			i32 window_width = LOWORD(lParam);
			i32 window_height = HIWORD(lParam);
			Win32_AllocateBackbuffer(window_width, window_height);
		} break;

		case WM_KEYDOWN:
		case WM_KEYUP: {
			int pressed = (lParam & (1 << 31)) == 0;
			switch (wParam) {
				case VK_LEFT: {
					gRotateShipLeft = pressed;
				} break;

				case VK_RIGHT: {
					gRotateShipRight = pressed;
				} break;

				case VK_UP: {
					gMoveShipForward = pressed;
				} break;

				case VK_SPACE: {
					gShootMissile = pressed;
				} break;

				default: {
					return DefWindowProc(window_handle, msg, wParam, lParam);
				} break;
			}
		} break;

		default: {
			return DefWindowProc(window_handle, msg, wParam, lParam);
		} break;
	}

	return 0;
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

int WinMainCRTStartup() {
	HINSTANCE hInstance = GetModuleHandle(0);

	char *window_class_name = "Maskeroids_Window_class";
	WNDCLASSA window_class = {0};
	window_class.hInstance = hInstance;
	window_class.lpszClassName = window_class_name;
	window_class.lpfnWndProc = WindowCallback;
	ATOM registered_class = RegisterClassA(&window_class);

	if (!registered_class) {
		MessageBox(
			NULL,
			"Unable to register window class!",
			"Window Registration Error",
			MB_OK
		);
		return 1;
	}

	HWND window = CreateWindowA(
		window_class_name, // lpClassName
		"Maskeroids!", // lpWindowName
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

	HDC device_context = GetDC(window);

	Win32_AllocateBackbuffer(DEFAULT_WINDOW_WIDTH, DEFAULT_WINDOW_HEIGHT);

	if (!gBackbuffer.pixels) {
		MessageBox(
			NULL,
			"Initializing backbuffer failed!",
			"Error",
			MB_OK
		);
		return 1;
	}
	
	ShowWindow(window, SW_SHOW);

	f32 ship_rotation_radians = 0;
	f32 ship_pos_x = (gBackbuffer.bitmapInfo.bmiHeader.biWidth / 2.0f);
	f32 ship_pos_y = (gBackbuffer.bitmapInfo.bmiHeader.biHeight / 2.0f);

#define MISSILE_POOL_SIZE 8
	Missile pool_of_missiles[MISSILE_POOL_SIZE] = {0};
	f32 missile_width = 4.0f;
	f32 missile_height = 8.0f;

	MSG msg = {0};
	while (!gShouldCloseWindow) {
		while (PeekMessageA(&msg, window, 0, 0, PM_REMOVE)) {
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}

		Triangle ship_triangle = {
			.a = { 0, 0 },
			.b = { 20, 60 },
			.c = { 40, 0 }
		};
		
		//////////////////////////////////////////////
		/// Apply Rotation
		///
		if (gRotateShipLeft && !gRotateShipRight) {
			ship_rotation_radians += SHIP_ROTATION_STEP;
		} else if (gRotateShipRight) {
			ship_rotation_radians -= SHIP_ROTATION_STEP;
		}

		// Use triangle centroid as the center of rotation
		Point rotation_origin = Centroid(ship_triangle);
		RotatePoints((Point*)&ship_triangle, 3, rotation_origin, ship_rotation_radians);
		
		Point ship_center = Centroid(ship_triangle);
		Vec2 ship_forward_direction = {0};
		ship_forward_direction.x = ship_triangle.b.x - ship_center.x;
		ship_forward_direction.y = ship_triangle.b.y - ship_center.y;
		f32 forward_vector_length = my_sqrt((ship_forward_direction.x*ship_forward_direction.x) + (ship_forward_direction.y*ship_forward_direction.y));
		ship_forward_direction.x /= forward_vector_length;
		ship_forward_direction.y /= forward_vector_length;
		//////////////////////////////////////////////
		//////////////////////////////////////////////
		

		///////////////////////////////////////////////
		/// Apply Translation
		///
		if (gMoveShipForward) {
			ship_pos_x += (ship_forward_direction.x * SHIP_SPEED);
			ship_pos_y += (ship_forward_direction.y * SHIP_SPEED);
		}

		{
			Extents e = CalculateExtents((Point*)&ship_triangle, 3);
			i32 ship_draw_area_width = e.max_x - e.min_x;
			i32 ship_draw_area_height = e.max_y - e.min_y;	

			// don't check <= because ship_pos_x will be equal if
			// it goes off the right side of the screen
			if (ship_pos_x < -ship_draw_area_width) {
				// went off left side of screen
				ship_pos_x = (f32)(gBackbuffer.bitmapInfo.bmiHeader.biWidth - 1);
			} else if (ship_pos_x > (gBackbuffer.bitmapInfo.bmiHeader.biWidth - 1)) {
				// went off right side of screen
				ship_pos_x = (f32)-ship_draw_area_width;
			}

			// don't check <= because ship_pos_y will be equal if
			// it goes off the top side of the screen
			if (ship_pos_y < -ship_draw_area_height) {
				// ship went off bottom of screen
				ship_pos_y = (f32)(gBackbuffer.bitmapInfo.bmiHeader.biHeight - 1);
			} else if (ship_pos_y > (gBackbuffer.bitmapInfo.bmiHeader.biHeight - 1)) {
				// ship went off top of screen
				ship_pos_y = (f32)-ship_draw_area_height;
			}
		}

		TranslatePoints((Point*)&ship_triangle, 3, ship_pos_x, ship_pos_y);

		ship_center = Centroid(ship_triangle);
		ship_forward_direction.x = ship_triangle.b.x - ship_center.x;
		ship_forward_direction.y = ship_triangle.b.y - ship_center.y;
		forward_vector_length = my_sqrt((ship_forward_direction.x*ship_forward_direction.x) + (ship_forward_direction.y*ship_forward_direction.y));
		ship_forward_direction.x /= forward_vector_length;
		ship_forward_direction.y /= forward_vector_length;
		///////////////////////////////////////////////
		///////////////////////////////////////////////

		for (i32 i = 0; i < MISSILE_POOL_SIZE; i++) {
			Missile *missile = pool_of_missiles + i;

			if (!missile->live) continue;

			f32 missile_delta_x = 6.0f * missile->direction.x;
			f32 missile_delta_y = 6.0f * missile->direction.y;

			TranslatePoints(missile->points, 4, missile_delta_x, missile_delta_y);

			Extents e = CalculateExtents(missile->points, 4);

			if ((e.min_x < 0 && e.max_x < 0) || 
				(e.min_x > gBackbuffer.bitmapInfo.bmiHeader.biWidth || e.max_x > gBackbuffer.bitmapInfo.bmiHeader.biWidth) ||
				(e.min_y < 0 && e.max_y < 0) ||
				(e.min_y > gBackbuffer.bitmapInfo.bmiHeader.biHeight || e.max_y > gBackbuffer.bitmapInfo.bmiHeader.biHeight)) {
					OutputDebugString("Missile destroy!\n");
					missile->live = 0;
			}
		}

		if (gShootMissile) {
			Missile *new_missile = 0;
			for (i32 i = 0; i < MISSILE_POOL_SIZE; i++) {
				Missile *m = pool_of_missiles + i;
				if (!m->live) {
					new_missile = m;
					break;
				}
			}

			if (new_missile) {
				// set the missile points(the missile may be reused)
				new_missile->points[0].x = 0;
				new_missile->points[0].y = 0;
				new_missile->points[1].x = 0;
				new_missile->points[1].y = missile_height;
				new_missile->points[2].x = missile_width;
				new_missile->points[2].y = missile_height;
				new_missile->points[3].x = missile_width;
				new_missile->points[3].y = 0;

				// rotate the missile so it's in the same direction as the ship
				Point missile_center = {0};
				missile_center.x = missile_width / 2.0f;
				missile_center.y = missile_height / 2.0f;
				RotatePoints(new_missile->points, 4, missile_center, ship_rotation_radians);

				// put the missile at the tip of the ship
				TranslatePoints(new_missile->points, 4, ship_triangle.b.x, ship_triangle.b.y);

				new_missile->direction = ship_forward_direction;

				new_missile->live = 1;
			}

			gShootMissile = 0;
		}

		// Clear background
		DrawRectangle(0, 0, gBackbuffer.bitmapInfo.bmiHeader.biWidth, gBackbuffer.bitmapInfo.bmiHeader.biHeight, BACKGROUND_COLOR);

		// Draw Ship
		Extents e = CalculateExtents((Point*)&ship_triangle, 3);
		for (i32 y = e.min_y; y < e.max_y; y++) {
			for (i32 x = e.min_x; x <= e.max_x; x++) {
				if (x < 0 || x >= gBackbuffer.bitmapInfo.bmiHeader.biWidth) {
					continue;
				}
				if (y < 0 || y >= gBackbuffer.bitmapInfo.bmiHeader.biHeight) {
					continue;
				}
				if (isPointInTriangle((f32)x, (f32)y, &ship_triangle)) {
					u32 *pixel = (u32*)gBackbuffer.pixels + x + (y * gBackbuffer.bitmapInfo.bmiHeader.biWidth);
					*pixel = SHIP_COLOR;
				}
			}
		}
		
		for (i32 i = 0; i < MISSILE_POOL_SIZE; i++) {
			Missile missile = pool_of_missiles[i];
			if (!missile.live) continue;

			// constrain how the number of points to check that in a the missile rectangle
			Extents e = CalculateExtents(missile.points, 4);

			// NOTE: Rasterize the rectangle by checking if a point is inside the rectangle. Do so
			//       by constructing vectors representing the sides of the rectangle, and a vector
			//       representing the test point. Project the vector representing the test point on
			//       to both vectors representing the sides of the rectangle, and if the projection
			//       length is within the respective range, then the point is inside the triangle.

			Vec2 ab = {0};
			ab.x = missile.points[1].x - missile.points[0].x;
			ab.y = missile.points[1].y - missile.points[0].y;
			f32 dot_ab_ab = (ab.x * ab.x) + (ab.y * ab.y);

			Vec2 ad = {0};
			ad.x = missile.points[3].x - missile.points[0].x;
			ad.y = missile.points[3].y - missile.points[0].y;
			f32 dot_ad_ad = (ad.x * ad.x) + (ad.y * ad.y);

			for (i32 y = e.min_y; y < e.max_y; y++) {
				for (i32 x = e.min_x; x < e.max_x; x++) {
					Vec2 am = { .x = (x - missile.points[0].x), .y = (y - missile.points[0].y) };

					f32 dot_am_ab = (am.x * ab.x) + (am.y * ab.y);
					if (dot_am_ab < 0 || dot_am_ab > dot_ab_ab) {
						continue;
					}

					f32 dot_am_ad = (am.x * ad.x) + (am.y * ad.y);
					if (dot_am_ad < 0 || dot_am_ad > dot_ad_ad) {
						continue;
					}
					
					if (x < 0 || x >= gBackbuffer.bitmapInfo.bmiHeader.biWidth) {
						continue;
					}
					if (y < 0 || y >= gBackbuffer.bitmapInfo.bmiHeader.biHeight) {
						continue;
					}
					u32 *pixel = (u32*)gBackbuffer.pixels + x + (y * gBackbuffer.bitmapInfo.bmiHeader.biWidth);
					*pixel = 0xFFFFFFFF;
				}
			}

		}

#if DEBUG_SHIP_FORWARD_VECTOR
		////////////////////////////////////////
		/// Debug Ship Forward Vector
		///

		// y = mx + b
		// b = y - mx

		if (ship_center.x == ship_triangle.b.x) {
			// vertical line!
			i32 miny = my_floor(my_min(ship_center.y, ship_triangle.b.y));
			i32 maxy = my_ceil(my_max(ship_center.y, ship_triangle.b.y));
			i32 x = RoundNearest(ship_center.x);
			for (i32 y = miny; y < maxy; y++) {
				if (y <= 0 || y >= gBackbuffer.bitmapInfo.bmiHeader.biHeight) {
					continue;
				}
				u32 *p = (u32*)gBackbuffer.pixels + x + (y * gBackbuffer.bitmapInfo.bmiHeader.biWidth);
				*p = 0xFFFFFFFF;
			}
		} else {
			f32 m = (ship_center.y - ship_triangle.b.y)/(ship_center.x - ship_triangle.b.x);
			f32 b = ship_triangle.b.y - m*ship_triangle.b.x;
			i32 forward_min_x = my_floor(my_min(ship_triangle.b.x, ship_center.x));
			i32 forward_max_x = my_ceil(my_max(ship_triangle.b.x, ship_center.x));

			for (i32 x = forward_min_x; x <= forward_max_x; x++) {
				i32 y = RoundNearest((m * x) + b);
				if (y <= 0 || y >= gBackbuffer.bitmapInfo.bmiHeader.biHeight) {
					continue;
				}
				u32 *p = (u32*)gBackbuffer.pixels + x + (y * gBackbuffer.bitmapInfo.bmiHeader.biWidth);
				*p = 0xFFFFFFFF;
			}
		}
		////////////////////////////////////////
		////////////////////////////////////////
#endif

		Win32_CopyBackbufferToScreen(device_context);
		Sleep(10);
	}

    return 0;
}

