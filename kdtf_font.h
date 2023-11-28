#ifndef KDT_FONT_H
#define KDT_FONT_H

#if _WIN32
#include <Windows.h>
#endif

#include "intrin.h"

#include "base.h"

typedef void *(*KDTF_fn_alloc)(u64);
typedef void (*KDTF_fn_free)(void*);

#define KDTF_AllocArray(alloc_fn, count, type) (type*)alloc_fn(count * sizeof(type))

static int FLAG_ON_CURVE_POINT = 0x01;
static int FLAG_X_SHORT_VECTOR = 0x02;
static int FLAG_Y_SHORT_VECTOR = 0x04;
static int FLAG_REPEAT_FLAG = 0x08;
static int FLAG_X_IS_SAME_OR_POSITIVE_X_SHORT_VECTOR = 0x10;
static int FLAG_Y_IS_SAME_OR_POSITIVE_Y_SHORT_VECTOR = 0x20;
static int FLAG_OVERLAP_SIMPLE = 0x40;

typedef struct {
	u32 *pixels;
	i32 atlas_width, atlas_height;
	i32 glyph_width;
	i32 glyph_height;
	char *characters;
	i32 character_count;
	u32 glyph_color;
	b8 anti_aliasing;
} KDTF_GlyphAtlas;

typedef struct {
	i32 load_error;

	void *file_data_ptr;
	i16 index_to_loca_format;
	u32 loca_table_offset;
	u32 glyf_table_offset;

	u16 cmap_segment_count;
	u16 *cmap_end_codes;
	u16 *cmap_start_codes;
	u16 *cmap_id_delta;
	u16 *cmap_id_range_offsets;
	u16 *cmap_glyph_ids;

	u16 maxp_max_points;
	u16 maxp_max_contours;
	u16 maxp_max_composite_points;
	u16 maxp_max_composite_contours;

	u16 units_per_em;
	i16 ascender;
	i16 descender;

	u16 advance_width_max;
	i32 average_advance_width;
	f32 design_units_to_pixels;
	i32 line_height;

	f32 font_size_pixels;
	f32 max_font_size_pixels;
	f32 min_font_size_pixels;
	f32 font_size_pixels_step;
	
	b8 flip_y;

	KDTF_GlyphAtlas atlas;
} KDTF_Font;

typedef struct {
	f32 x, y;
} KDTF_GlyphPoint;

typedef struct KDTF_Glyph {
	u32 contour_count;
	u32 coordinate_count;
	u8 *flags;
	i32 min_x;
	i32 max_x;
	i32 min_y;
	i32 max_y;
	KDTF_GlyphPoint *coordinates;
	u16 *contour_end_pt_indices;

	u16 component_count;
	struct KDTF_Glyph *components;
} KDTF_Glyph;

typedef struct {
	KDTF_GlyphPoint a, b, c;
} KDTF_GlyphTriangle;

b8 KDTF_isPointInTriangle(f32 x, f32 y, f32 ax, f32 ay, f32 bx, f32 by, f32 cx, f32 cy) {
	// vector v0
	f32 v0x = bx - ax;
	f32 v0y = by - ay;

	// vector v1
	f32 v1x = cx - ax;
	f32 v1y = cy - ay;

	// vector v2
	f32 v2x = x - ax;
	f32 v2y = y - ay;

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

b8 KDTF_isPointInTriangle2(f32 x, f32 y, KDTF_GlyphTriangle *t) {
	return KDTF_isPointInTriangle(x, y, t->a.x, t->a.y, t->b.x, t->b.y, t->c.x, t->c.y);
}

i32 Factorial(i32 value) {
	if (value == 0) return 1;
	return value * Factorial(value - 1);
}

i32 CalculateBinomialCoefficent(i32 n, i32 k) {
	i32 n_factorial = Factorial(n);
	i32 k_factorial = Factorial(k);
	i32 n_minus_k_factorial = Factorial(n - k);
	i32 result = n_factorial / (k_factorial * n_minus_k_factorial);
	return result;
}

f32 Power(f32 value, i32 power) {
	f32 result = 1.0f;

	if (power >= 1) {
		for (i32 i = 1; i <= power; i++) {
			result *= value;
		}
	}

	return result;
}

void KDTF_GenerateBezierCurve(
	KDTF_GlyphPoint *start,
	KDTF_GlyphPoint *end,
	KDTF_GlyphPoint *control_points,
	u32 control_point_count,
	KDTF_GlyphPoint *out,
	u32 *out_count,
	u32 number_of_points_to_generate) {

	f32 step = 1.0f / (f32)number_of_points_to_generate;
	f32 t = step;

	u32 generated_point_count = 0;
	u32 total_point_count = control_point_count + 2; // add two to include start and end

	while (t < 1.0f) {
		f32 x = 0.0f;
		f32 y = 0.0f;

		f32 t1 = (1.0f - t);

		i32 terms = control_point_count + 1;

		for (i32 i = 0; i <= terms; i++) {
			i32 bionmial_coefficient = CalculateBinomialCoefficent(terms, i);
			f32 one_minus_t = Power(1.0f - t, terms - i);
			f32 power_of_t = Power(t, i);
			KDTF_GlyphPoint *p = 0;
			if (i == 0) {
				p = start;
			} else if (i == terms) {
				p = end;
			} else {
				p = control_points + (i - 1);
			}
			x += bionmial_coefficient * one_minus_t * power_of_t * p->x;
			y += bionmial_coefficient * one_minus_t * power_of_t * p->y;
		}

		// if (control_point_count == 1) {
		// 	KDTF_GlyphPoint *control_point = control_points;
		// 	// quadratic bezier
			
		// 	// x = (Power(t1, terms)*start->x) + (2*Power(t1, terms - 1)*Power(t, terms - 1)*control_point->x) + (Power(t, terms)*end->x);
		// 	// y = (Power(t1, terms)*start->y) + (2*Power(t1, terms - 1)*Power(t, terms - 1)*control_point->y) + (Power(t, terms)*end->y);
		// } else if (control_point_count == 2) {
		// 	KDTF_GlyphPoint *cp_1 = control_points;
		// 	KDTF_GlyphPoint *cp_2 = control_points + 1;
		// 	// cubic bezier
		// 	x = (t1*t1*t1*start->x) + (3*t1*t1*t*cp_1->x) + (3*t1*t*t*cp_2->x) + (t*t*t*end->x);
		// 	y = (t1*t1*t1*start->y) + (3*t1*t1*t*cp_1->y) + (3*t1*t*t*cp_2->y) + (t*t*t*end->y);
		// } else if (control_point_count == 3) {
		// 	KDTF_GlyphPoint *cp_1 = control_points;
		// 	KDTF_GlyphPoint *cp_2 = control_points + 1;
		// 	KDTF_GlyphPoint *cp_3 = control_points + 2;
		// 	x = (t1*t1*t1*t1*start->x) + (4*t1*t1*t1*t*cp_1->x) + (6*t1*t1*t*t*cp_2->x) + (4*t1*t*t*t*cp_3->x) + (t*t*t*t*end->x);
		// 	y = (t1*t1*t1*t1*start->y) + (4*t1*t1*t1*t*cp_1->y) + (6*t1*t1*t*t*cp_2->y) + (4*t1*t*t*t*cp_3->y) + (t*t*t*t*end->y);
		// } else if (control_point_count == 4) {
		// 	KDTF_GlyphPoint *cp_1 = control_points;
		// 	KDTF_GlyphPoint *cp_2 = control_points + 1;
		// 	KDTF_GlyphPoint *cp_3 = control_points + 2;
		// 	KDTF_GlyphPoint *cp_4 = control_points + 3;
		// 	x = (t1*t1*t1*t1*t1*start->x) + (5*t1*t1*t1*t1*t*cp_1->x) + (10*t1*t1*t1*t*t*cp_2->x) + (10*t1*t1*t*t*t*cp_3->x) + (5*t1*t*t*t*t*cp_4->x) + (t*t*t*t*t*end->x);
		// 	y = (t1*t1*t1*t1*t1*start->y) + (5*t1*t1*t1*t1*t*cp_1->y) + (10*t1*t1*t1*t*t*cp_2->y) + (10*t1*t1*t*t*t*cp_3->y) + (5*t1*t*t*t*t*cp_4->y) + (t*t*t*t*t*end->y);
		// } else if (control_point_count == 5) {
		// 	KDTF_GlyphPoint *cp_1 = control_points;
		// 	KDTF_GlyphPoint *cp_2 = control_points + 1;
		// 	KDTF_GlyphPoint *cp_3 = control_points + 2;
		// 	KDTF_GlyphPoint *cp_4 = control_points + 3;
		// 	KDTF_GlyphPoint *cp_5 = control_points + 4;
		// 	x = (1  * (t1*t1*t1*t1*t1*t1 * start->x)) + 
		// 		(6  * (t1*t1*t1*t1*t1*t  * cp_1->x)) +
		// 		(15 * (t1*t1*t1*t1*t *t  * cp_2->x)) +
		// 		(20 * (t1*t1*t1*t *t *t  * cp_3->x)) +
		// 		(15 * (t1*t1*t *t *t *t  * cp_4->x)) +
		// 		(6  * (t1*t *t *t *t *t  * cp_5->x)) +
		// 		(1  * (t *t *t *t *t *t  * end->x));
		// 	y = (1  * (t1*t1*t1*t1*t1*t1 * start->y)) + 
		// 		(6  * (t1*t1*t1*t1*t1*t  * cp_1->y)) +
		// 		(15 * (t1*t1*t1*t1*t *t  * cp_2->y)) +
		// 		(20 * (t1*t1*t1*t *t *t  * cp_3->y)) +
		// 		(15 * (t1*t1*t *t *t *t  * cp_4->y)) +
		// 		(6  * (t1*t *t *t *t *t  * cp_5->y)) +
		// 		(1  * (t *t *t *t *t *t  * end->y));
		// } else if (control_point_count == 6) {
		// 	KDTF_GlyphPoint *cp_1 = control_points;
		// 	KDTF_GlyphPoint *cp_2 = control_points + 1;
		// 	KDTF_GlyphPoint *cp_3 = control_points + 2;
		// 	KDTF_GlyphPoint *cp_4 = control_points + 3;
		// 	KDTF_GlyphPoint *cp_5 = control_points + 4;
		// 	KDTF_GlyphPoint *cp_6 = control_points + 5;
		// 	x = (1  * (t1*t1*t1*t1*t1*t1*t1 * start->x)) + 
		// 		(7  * (t1*t1*t1*t1*t1*t1*t  * cp_1->x)) +
		// 		(21 * (t1*t1*t1*t1*t1*t *t  * cp_2->x)) +
		// 		(35 * (t1*t1*t1*t1*t *t *t  * cp_3->x)) +
		// 		(35 * (t1*t1*t1*t *t *t *t  * cp_4->x)) +
		// 		(21 * (t1*t1*t *t *t *t *t  * cp_5->x)) +
		// 		(7  * (t1*t *t *t *t *t *t  * cp_6->x)) +
		// 		(1  * (t *t *t *t *t *t *t  * end->x));
		// 	y = (1  * (t1*t1*t1*t1*t1*t1*t1 * start->y)) + 
		// 		(7  * (t1*t1*t1*t1*t1*t1*t  * cp_1->y)) +
		// 		(21 * (t1*t1*t1*t1*t1*t *t  * cp_2->y)) +
		// 		(35 * (t1*t1*t1*t1*t *t *t  * cp_3->y)) +
		// 		(35 * (t1*t1*t1*t *t *t *t  * cp_4->y)) +
		// 		(21 * (t1*t1*t *t *t *t *t  * cp_5->y)) +
		// 		(7  * (t1*t *t *t *t *t *t  * cp_6->y)) +
		// 		(1  * (t *t *t *t *t *t *t  * end->y));
		// } else {
		// 	Assert(false); // unsupported number of control points...
		// }
        


		KDTF_GlyphPoint *output_point = out++;
		generated_point_count++;
		output_point->x = x;
		output_point->y = y;

		t += step;
	}

	*out_count = generated_point_count;
}

typedef struct {
	KDTF_GlyphPoint start;
	KDTF_GlyphPoint end;
	f32 slope;
	f32 intercept;
} KDTF_LineSegment;

KDTF_LineSegment KDTF_MakeLineSegment(KDTF_GlyphPoint start, KDTF_GlyphPoint end) {
	KDTF_LineSegment result = {0};

	result.start = start;
	result.end = end;

	if (start.x == end.x) {
		result.slope = F32_MAX; // vertical line
	} else {
		result.slope = (end.y - start.y) / (end.x - start.x);
		result.intercept = (start.y) - (result.slope * start.x);
	}

	return result;
}

f32 KDTF_CalculateDisplacementBetweenPoints(KDTF_GlyphPoint p1, KDTF_GlyphPoint p2) {
	f32 x = p1.x - p2.x;
	f32 y = p1.y - p2.y;
	f32 test1_tmp = (x * x) + (y * y);
	f32 result = SquareRoot(test1_tmp);
	return result;
}

typedef struct {
	u32 point_count;
	KDTF_GlyphPoint *points;
} KDTF_GlyphContour;

f32 KDTF_Min2f(f32 a, f32 b) {
	if (a > b) {
		return b;
	} else {
		return a;
	}
}

f32 KDTF_Max2f(f32 a, f32 b) {
	if (a > b) {
		return a;
	} else {
		return b;
	}
}

KDTF_LineSegment KDTF_CalculateCutLineForContourHole(KDTF_GlyphContour *contour, KDTF_GlyphContour *hole) {
	// assuming that the hole is correct...

	// Find point farthest on the x axis for the hole
	// Form edges on outer polygon
	// Calculate intersections 
	// Select point of edge that is closest
	// Line between farthest point on x axis and closest point on edge will be the 'cut-point'

	// find the point farthest on x axis for hole.
	KDTF_GlyphPoint hole_point_farthest_on_x = {0};
	for (u32 i = 0; i < hole->point_count; i++) {
		if (hole->points[i].x > hole_point_farthest_on_x.x) {
			hole_point_farthest_on_x = hole->points[i];
		}
	}

	// shoot a ray and find intersecting edge
	KDTF_GlyphPoint right_infinity = { .x = hole_point_farthest_on_x.x + 999999999.0f, .y = hole_point_farthest_on_x.y };
	//KDTF_LineSegment ray = KDTF_MakeLineSegment(hole_point_farthest_on_x, right_infinity);
	
	KDTF_LineSegment intersecting_edge = {0};
	u32 ray_interesection_count = 0;

	for (u32 i = 0; i < contour->point_count; i++) {
		u32 start_point_index = i;
		u32 end_point_index = i + 1;
		if (end_point_index == contour->point_count) {
			end_point_index = 0;
		}
		
		KDTF_LineSegment edge = KDTF_MakeLineSegment(contour->points[start_point_index], contour->points[end_point_index]);
		f32 edge_max_x = KDTF_Max2f(edge.start.x, edge.end.x);
		f32 edge_min_x = KDTF_Min2f(edge.start.x, edge.end.x);
		f32 edge_max_y = KDTF_Max2f(edge.start.y, edge.end.y);
		f32 edge_min_y = KDTF_Min2f(edge.start.y, edge.end.y);

		if (hole_point_farthest_on_x.x > edge_max_x || 
			hole_point_farthest_on_x.y <= edge_min_y || 
			hole_point_farthest_on_x.y > edge_max_y) {
			// ignore if outside of range
			continue;
		}
		
		if (edge.slope == F32_MAX) {
			intersecting_edge = edge;
			ray_interesection_count++;
		} else {
			// y = mx + b ===> x = (y - b) / m
			f32 x = (hole_point_farthest_on_x.y - edge.intercept) / edge.slope;
			if (x >= edge_min_x && x <= edge_max_x) {
				intersecting_edge = edge;
				ray_interesection_count++;
			}
		}
	}

	Assert(ray_interesection_count == 1);

	// determine which point to select to perform cut..
	// select point with shortest link between the edge and the point in question.
	f32 test1_length = KDTF_CalculateDisplacementBetweenPoints(hole_point_farthest_on_x, intersecting_edge.start);
	f32 test2_length = KDTF_CalculateDisplacementBetweenPoints(hole_point_farthest_on_x, intersecting_edge.end);

	KDTF_GlyphPoint cut_point = {0};

	if (test1_length < test2_length) {
		cut_point = intersecting_edge.start;
	} else {
		cut_point = intersecting_edge.end;
	}

	// start point will be a hole point
	// end point will be a outer point
	return KDTF_MakeLineSegment(hole_point_farthest_on_x, cut_point);
}


b8 KDTF_isGlyphContourClockwise(KDTF_GlyphContour *contour) {
	Assert(contour->point_count >= 2);
	// Determine orientation by calculating signed area...
	f32 area = 0;

	u32 n = contour->point_count;

	for (u32 i = 0; i <= n - 1; i++) {
		KDTF_GlyphPoint a = contour->points[i];
		KDTF_GlyphPoint b = contour->points[(i + 1) % n];
		area += (a.x * b.y - a.y * b.x);
	}

	if (area > 0) {
		return false;
	}
	return true;
}

typedef struct {
	i32 count;
	KDTF_GlyphContour *contours;
} KDTF_GenerateGlyphContoursResult;

i32 KDTF_GetTotalContourCountForGlyph(KDTF_Glyph *g) {
	i32 result = g->contour_count;
	for (i32 i = 0; i < g->component_count; i++) {
		KDTF_Glyph *component = g->components + i;
		result += KDTF_GetTotalContourCountForGlyph(component);
	}
	return result;
}

typedef struct {
	i32 index;
	KDTF_GenerateGlyphContoursResult result;
} KDTF_GenerateGlyphContoursInteralState;

static void KDTF_GenerateGlyphContoursInternal(KDTF_GenerateGlyphContoursInteralState *state, KDTF_Glyph *g, KDTF_Font *f, KDTF_fn_alloc alloc) {
	for (u32 i = 0; i < g->contour_count; i++) {
		KDTF_GlyphContour *contour = state->result.contours + state->index;


		u16 contour_coordinate_start_index = 0;
		if (i > 0) {
			contour_coordinate_start_index = g->contour_end_pt_indices[i - 1] + 1;
		}
		u16 contour_coordinate_end_index = g->contour_end_pt_indices[i];



		u16 max_number_points = f->maxp_max_points;
		if (g->contour_count < 0) {
			max_number_points = f->maxp_max_composite_points;
		}

		u32 new_point_count = 0;
		KDTF_GlyphPoint *new_points = (KDTF_GlyphPoint*)alloc(max_number_points * sizeof(KDTF_GlyphPoint));

		u8 bezier_point_count = 0;
		KDTF_GlyphPoint *bezier_points = (KDTF_GlyphPoint*)alloc(30 * sizeof(KDTF_GlyphPoint));
		b8 bezier = false;




		u32 subdivisions = 2;




		for (u16 k = contour_coordinate_start_index; k <= contour_coordinate_end_index; k++) {
			KDTF_GlyphPoint point = g->coordinates[k];
			u8 coordinate_flags = g->flags[k];

			if (coordinate_flags & FLAG_ON_CURVE_POINT) {
				if (bezier) {
					KDTF_GlyphPoint *start = new_points + (new_point_count - 1);
					u32 generated_bezier_curve_point_count = 0;
					KDTF_GenerateBezierCurve(
						start,
						&point,
						bezier_points,
						bezier_point_count,
						new_points + new_point_count,
						&generated_bezier_curve_point_count,
						subdivisions
					);
					new_point_count += generated_bezier_curve_point_count;

					bezier = false;
					bezier_point_count = 0;
				}
				*(new_points + new_point_count++) = point;
			} else {
				bezier = true;
				*(bezier_points + bezier_point_count++) = point;

				if (k >= contour_coordinate_end_index && bezier) {
					KDTF_GlyphPoint *start = new_points + (new_point_count - 1);
					u32 generated_bezier_curve_point_count = 0;
					KDTF_GenerateBezierCurve(
						start,
						new_points,
						bezier_points,
						bezier_point_count,
						new_points + new_point_count,
						&generated_bezier_curve_point_count,
						subdivisions
					);
					new_point_count += generated_bezier_curve_point_count;
				}
			}
		}




		contour->point_count = new_point_count;
		contour->points = new_points;

		state->index++;
	}

	for (i32 i = 0; i < g->component_count; i++) {
		KDTF_Glyph *component = g->components + i;
		KDTF_GenerateGlyphContoursInternal(state, component, f, alloc);
	}
}

KDTF_GenerateGlyphContoursResult KDTF_GenerateGlyphContours(KDTF_Glyph *g, KDTF_Font *f, KDTF_fn_alloc alloc) {
	Assert(g != NULL); // can't do anything if I don't have this...

	// TODO - If the contour count is zero, then don't was the cycles.

	KDTF_GenerateGlyphContoursInteralState internalState = {0};
	internalState.result.count = KDTF_GetTotalContourCountForGlyph(g);
	internalState.result.contours = KDTF_AllocArray(alloc, internalState.result.count, KDTF_GlyphContour);
	KDTF_GenerateGlyphContoursInternal(&internalState, g, f, alloc);

	return internalState.result;
}

// NOTE: out should be contour_count * sizeof(b8)
void KDTF_ClassifyContours(KDTF_GlyphContour *contours, i32 contour_count, b8 *out) {
	for (i32 i = 0; i < contour_count; i++) {
		KDTF_GlyphContour *contour = contours + i;
		*(out + i) = KDTF_isGlyphContourClockwise(contour);
	}
}

typedef struct {
	KDTF_GlyphTriangle *triangles;
	i32 expected_triangle_count, actual_triangle_count;
	i32 point_index;
	b8 *clipped;
} KDTF_GlyphContourTriangulationState;

typedef struct {
	KDTF_GlyphContour *contours;
	i32 contour_count;
	KDTF_GlyphContourTriangulationState *contour_triangulation_states;
	i32 contour_index;
} KDTF_GlyphTriangulationState;

KDTF_GlyphTriangulationState KDTF_BeginTriangulation(KDTF_GlyphContour *contours, i32 contour_count, KDTF_fn_alloc alloc) {
	
	KDTF_GlyphTriangulationState start_state = {0};

	u32 total_point_count = 0;
	for (i32 i = 0; i < contour_count; i++) {
		KDTF_GlyphContour contour = contours[i];
		total_point_count += contour.point_count;
	}

	start_state.contour_count = contour_count;
	start_state.contours = contours;
	start_state.contour_triangulation_states = (KDTF_GlyphContourTriangulationState*)alloc(contour_count * sizeof(KDTF_GlyphContourTriangulationState));
	for (i32 i = 0; i < contour_count; i++) {
		KDTF_GlyphContour contour = contours[i];
		KDTF_GlyphContourTriangulationState *contour_state = start_state.contour_triangulation_states + i;

		contour_state->actual_triangle_count = 0;
		contour_state->point_index = 0;
		contour_state->expected_triangle_count = contour.point_count - 2;
		contour_state->triangles = (KDTF_GlyphTriangle*)alloc(contour_state->expected_triangle_count * sizeof(KDTF_GlyphTriangle));
		contour_state->clipped = (b8*)alloc(contour.point_count * sizeof(b8));
	}

	return start_state;
}

// returns true if triangulation is complete
b8 KDTF_NextTriangle(KDTF_GlyphTriangulationState *state) {
	KDTF_GlyphContour contour = state->contours[state->contour_index];
	KDTF_GlyphContourTriangulationState *contour_state = state->contour_triangulation_states + state->contour_index;

	if (contour.point_count == 0) {
		// there is nothing to process here...
		return true;
	}

	if (contour_state->actual_triangle_count == contour_state->expected_triangle_count) {
		state->contour_index++;
		
		if (state->contour_index == state->contour_count) {
			return true;
		}

		return false;
	}

	if (contour_state->point_index == contour.point_count) {
		return true; // processed all the points
	}

	for (i32 i = contour_state->point_index; i < (i32)contour.point_count; i++) {
		b8 clipped = contour_state->clipped[i];
		if (!clipped) {
			contour_state->point_index = i;
			break;
		}
	}

	i32 current_point_index = contour_state->point_index;
	i32 next_point_index = -1;
	i32 previous_point_index = -1;

	// find the next point that has not been clipped
	for (i32 i = current_point_index + 1; i <= (i32)contour.point_count; i++) {
		if (i >= (i32)contour.point_count) {
			i = 0;
		}
		if (!contour_state->clipped[i]) {
			next_point_index = i;
			break;
		}
	}

	Assert(next_point_index != -1);

	// find the previous point that has not been clipped
	for (i32 i = current_point_index - 1; i >= -1; i--) {
		if (i <= -1) {
			i = contour.point_count - 1;
		}
		if (!contour_state->clipped[i]) {
			previous_point_index = i;
			break;
		}
	}

	Assert(previous_point_index != -1);

	KDTF_GlyphPoint a = contour.points[previous_point_index];
	KDTF_GlyphPoint b = contour.points[current_point_index];
	KDTF_GlyphPoint c = contour.points[next_point_index];

	f32 b_to_a__x = b.x - a.x;
	f32 b_to_a__y = b.y - a.y;
	f32 b_to_c__x = b.x - c.x;
	f32 b_to_c__y = b.y - c.y;
	f32 dot_product = (b_to_a__x * b_to_c__x) + (b_to_a__y * b_to_c__y);
	f32 det = (b_to_a__x * b_to_c__y) - (b_to_a__y * b_to_c__x);
	f32 angle_between_deg = ArcTangent(det, dot_product) * (180.0f / PI);

	if (angle_between_deg < 0) {
		angle_between_deg += 360.0f;
	}
	
	if (angle_between_deg >= 180.0f) {
		contour_state->point_index++;
		return false;
	}

	// we found an ear, confirm that there are no points inside it.
	// using barycentric coordinates
	bool is_ear = true;
	for (u32 j = 0; j < contour.point_count - 1; j++) {
		if (contour_state->clipped[j] || j == previous_point_index || j == current_point_index || j == next_point_index) {
			continue; // doesn't count if it's been clipped, or we're currently examining the point...
		}
		KDTF_GlyphPoint p = contour.points[j];
		if (p.x == a.x && p.y == a.y) {
			continue;
		}
		if (p.x == b.x && p.y == b.y) {
			continue;
		}
		if (p.x == c.x && p.y == c.y) {
			continue;
		}
		if (KDTF_isPointInTriangle(p.x, p.y, a.x, a.y, b.x, b.y, c.x, c.y)) {
			is_ear = false;
			break;
		}
	}

	if (is_ear) {
		contour_state->clipped[contour_state->point_index] = true;
		KDTF_GlyphTriangle *triangle = contour_state->triangles + contour_state->actual_triangle_count++;
		triangle->a = a;
		triangle->b = b;
		triangle->c = c;
		contour_state->point_index = 0;
	} else {
		contour_state->point_index++;
	}

	return false;
}

f32 KDTF_Max3f(f32 a, f32 b, f32 c) {
	if (a > b && a > c) {
		return a;
	} else if (b > a && b > c) {
		return b;
	} else {
		return c;
	}
}

f32 KDTF_Min3f(f32 a, f32 b, f32 c) {
	if (a < b && a < c) {
		return a;
	} else if (b < a && b < c) {
		return b;
	} else {
		return c;
	}
}

typedef struct {
	i32 max_x;
	i32 max_y;
	i32 min_x;
	i32 min_y;
} KDTF_GlyphTriangleExtents;

KDTF_GlyphTriangleExtents KDTF_GetGlyphTriangleExtents(KDTF_GlyphTriangle *t) {
	KDTF_GlyphTriangleExtents result = {0};
	result.max_x = Ceil(KDTF_Max3f(t->a.x, t->b.x, t->c.x));
	result.max_y = Ceil(KDTF_Max3f(t->a.y, t->b.y, t->c.y));
	result.min_x = Floor(KDTF_Min3f(t->a.x, t->b.x, t->c.x));
	result.min_y = Floor(KDTF_Min3f(t->a.y, t->b.y, t->c.y));
	return result;
}

i32 KDTF_Min2i(i32 a, i32 b) {
	if (a < b) {
		return a;
	} else {
		return b;
	}
}

i32 KDTF_Max2i(i32 a, i32 b) {
	if (a > b) {
		return a;
	} else {
		return b;
	}
}

void KDTF_DrawGlyphTriangles(
	u32 *surface, i32 surface_width, i32 surface_height,
	KDTF_GlyphTriangle *triangles, i32 triangle_count,
	u32 color_value, b8 anti_aliasing) {

	if (anti_aliasing) {
		i32 min_x = -1;
		i32 max_x = -1;
		i32 min_y = -1;
		i32 max_y = -1;

		for (i32 triangle_index = 0; triangle_index < triangle_count; triangle_index++) {
			KDTF_GlyphTriangle *t = triangles + triangle_index;

			KDTF_GlyphTriangleExtents triangle_extents = KDTF_GetGlyphTriangleExtents(t);

			if (min_x == -1) {
				min_x = triangle_extents.min_x;
			} else {
				min_x = KDTF_Min2i(min_x, triangle_extents.min_x);
			}

			if (max_x == -1) {
				max_x = triangle_extents.max_x;
			} else {
				max_x = KDTF_Max2i(max_x, triangle_extents.max_x);
			}

			if (min_y == -1) {
				min_y = triangle_extents.min_y;
			} else {
				min_y = KDTF_Min2i(min_y, triangle_extents.min_y);
			}

			if (max_y == -1) {
				max_y = triangle_extents.max_y;
			} else {
				max_y = KDTF_Max2i(max_y, triangle_extents.max_y);
			}
		}

		for (i32 x = min_x; x <= max_x; x++) {
			for (i32 y = min_y; y <= max_y; y++) {
				if (y < 0 || y >= surface_height || x < 0 || x >= surface_width) {
					continue;
				}

				u32 pixel_color = 0x00000000;

				f32 top_left_x = x + 0.25f;
				f32 top_left_y = y + 0.25f;
				b8 top_left_in_triangle = false;

				f32 top_right_x = x + 0.50f;
				f32 top_right_y = y + 0.25f;
				b8 top_right_in_triangle = false;

				f32 bottom_left_x = x + 0.25f;
				f32 bottom_left_y = y + 0.50f;
				b8 bottom_left_in_trianlge = false;

				f32 bottom_right_x = x + 0.75f;
				f32 bottom_right_y = y + 0.75f;
				b8 bottom_right_in_triangle = false;

				for (i32 triangle_index = 0; triangle_index < triangle_count; triangle_index++) {
					KDTF_GlyphTriangle *triangle = triangles + triangle_index;

					if (!top_left_in_triangle) {
						if (KDTF_isPointInTriangle2(top_left_x, top_left_y, triangle)) {
							pixel_color += 0x3F000000;
							top_left_in_triangle = true;
						}
					}

					if (!top_right_in_triangle) {
						if (KDTF_isPointInTriangle2(top_right_x, top_right_y, triangle)) {
							pixel_color += 0x3F000000;
							top_right_in_triangle = true;
						}
					}

					if (!bottom_left_in_trianlge) {
						if (KDTF_isPointInTriangle2(bottom_left_x, bottom_left_y, triangle)) {
							pixel_color += 0x3F000000;
							bottom_left_in_trianlge = true;
						}
					}

					if (!bottom_right_in_triangle) {
						if (KDTF_isPointInTriangle2(bottom_right_x, bottom_right_y, triangle)) {
							pixel_color += 0x3F000000;
							bottom_right_in_triangle = true;
						}
					}

					if (bottom_right_in_triangle && top_right_in_triangle && bottom_left_in_trianlge && top_left_in_triangle) {
						break;
					}
				}

				if (pixel_color) {
					u32 *pixel = (u32*)surface + x + (y * surface_width);
					*pixel = pixel_color + 0x01FFFFFF;
				}
			}
		}

	} else {
		for (int i = 0; i < triangle_count; i++) {
			KDTF_GlyphTriangle *t = triangles + i;

			KDTF_GlyphTriangleExtents triangle_extents = KDTF_GetGlyphTriangleExtents(t);

			for (i32 y = triangle_extents.min_y; y < triangle_extents.max_y; y++) {
				for (i32 x = triangle_extents.min_x; x < triangle_extents.max_x; x++) {
					if (KDTF_isPointInTriangle((f32)x, (f32)y, t->a.x, t->a.y, t->b.x, t->b.y, t->c.x, t->c.y)) {
						u32 *pixel = (u32*)surface + x + (y * surface_width);
						*pixel = color_value;
					}
				}
			}
		}
	}
}

typedef struct {
	KDTF_GlyphContour *contours;
	i32 count;
} KDTF_MergeContoursResult;

typedef struct {
	KDTF_GlyphContour *contour;
	b8 is_hole;

	b8 is_hole_for_this_entity;
	KDTF_LineSegment cut_line;
	b8 inserted;
} KDTF_MergeContourEntity;

KDTF_MergeContoursResult KDTF_MergeContours(KDTF_GlyphContour *contours, i32 contour_count, KDTF_fn_alloc alloc) {
	KDTF_MergeContoursResult result = {0};
	result.contours = KDTF_AllocArray(alloc, contour_count, KDTF_GlyphContour);

	// Assumes that the holes will have a different orientation than the one it will be inserted into
	// Determine a 'cut-point' between the hole and the outer polyon
	// Loops over points and when 'cut-point' reached, then insert holes
	// Finishes by inserting the remaining points from outer polygon.

	// Determine which contours are holes and which aren't
	// Insert holes into non-holes
	// Triangulate the resulting contours

	KDTF_MergeContourEntity *entities = KDTF_AllocArray(alloc, contour_count, KDTF_MergeContourEntity);

	for (i32 i = 0; i < contour_count; i++) {
		KDTF_MergeContourEntity *entity = entities + i;
		KDTF_GlyphContour *contour = contours + i;
		entity->contour = contour;
		entity->is_hole = !KDTF_isGlyphContourClockwise(contour);
	}

	for(i32 i = 0; i < contour_count; i++) {
		KDTF_MergeContourEntity *entity = entities + i;
		if (entity->is_hole) continue;

		// prepare this contour for hole testing
		KDTF_GlyphTriangulationState triangulation_state = KDTF_BeginTriangulation(entity->contour, 1, alloc);
		while (!KDTF_NextTriangle(&triangulation_state)) {
			// do nothing
		}

		i32 entity_triangle_count = triangulation_state.contour_triangulation_states[0].actual_triangle_count;
		KDTF_GlyphTriangle *entity_triangles = triangulation_state.contour_triangulation_states[0].triangles;
		
		for (i32 k = 0; k < contour_count; k++) {
			if (k == i) continue;
			KDTF_MergeContourEntity *second_entity = entities + k;
			if (!second_entity->is_hole) continue;

			b8 second_entity_inside_this_entity = true;
			for (u32 j = 0; j < second_entity->contour->point_count; j++) {
				KDTF_GlyphPoint p = second_entity->contour->points[j];
				
				b8 p_is_inside_contour = false;
				for (i32 n = 0; n < entity_triangle_count; n++) {
					KDTF_GlyphTriangle t = entity_triangles[n];
					p_is_inside_contour |= KDTF_isPointInTriangle(p.x, p.y, t.a.x, t.a.y, t.b.x, t.b.y, t.c.x,  t.c.y);
				}
				second_entity_inside_this_entity &= p_is_inside_contour;
			}

			second_entity->is_hole_for_this_entity = second_entity_inside_this_entity;
		}





		KDTF_GlyphContour *result_contour = result.contours + result.count++;

		result_contour->point_count = entity->contour->point_count;
		for (i32 k = 0; k < contour_count; k++) {
			KDTF_MergeContourEntity *hole_entity = entities + k;
			if (!hole_entity->is_hole_for_this_entity) continue;
			result_contour->point_count += hole_entity->contour->point_count + 2;
			hole_entity->cut_line = KDTF_CalculateCutLineForContourHole(entity->contour, hole_entity->contour);
		}
		result_contour->points = KDTF_AllocArray(alloc, result_contour->point_count, KDTF_GlyphPoint);

		i32 output_point_index = 0;
		for (u32 k = 0; k < entity->contour->point_count; k++) {
			KDTF_GlyphPoint contour_point = entity->contour->points[k];
			
			// output this point because it's coming from the contour
			KDTF_GlyphPoint *output_point = result_contour->points + output_point_index++;
			output_point->x = contour_point.x;
			output_point->y = contour_point.y;

			for (i32 n = 0; n < contour_count; n++) {
				KDTF_MergeContourEntity *hole_entity = entities + n;
				if (!hole_entity->is_hole_for_this_entity) continue;
				if (hole_entity->inserted) continue;

				// start point will be a hole point
				// end point will be a outer point
				KDTF_LineSegment cut_line = hole_entity->cut_line;

				if (contour_point.x != cut_line.end.x || contour_point.y != cut_line.end.y) {
					// if this isn't the point that we need to start inserting the point at
					continue;
				}

				KDTF_GlyphContour hole = *hole_entity->contour;

				u32 hole_point_index = 0;
				for (u32 i = 0; i < hole.point_count; i++) {
					KDTF_GlyphPoint hole_point = hole.points[i];
					if (hole_point.x == cut_line.start.x && hole_point.y == cut_line.start.y) {
						hole_point_index = i;
						break;
					}
				}

				u32 inserted_point_count = 0;
				while (inserted_point_count < hole.point_count) {
					result_contour->points[output_point_index++] = hole.points[hole_point_index];
					inserted_point_count++;
					if (hole_point_index == hole.point_count - 1) {
						hole_point_index = 0;
					} else {
						hole_point_index++;
					}
				}

				result_contour->points[output_point_index++] = cut_line.start;
				result_contour->points[output_point_index++] = cut_line.end;

				hole_entity->inserted = true;
			}
		}

		// reset the entities...
		for (i32 k = 0; k < contour_count; k++) {
			KDTF_MergeContourEntity *e = entities + k;
			if (k == i) {
				continue;
			}
			KDTF_LineSegment reset_line_segment = {0};
			e->cut_line = reset_line_segment;
			e->inserted = false;
			e->is_hole_for_this_entity = false;
		}
	}

	return result;
}


void KDTF_RasterizeGlyph(
	KDTF_Glyph *glyph, KDTF_Font *font, 
	u32 color, b8 anti_aliasing, i32 x_offset, i32 y_offset,
	u32 *surface, i32 surface_width, i32 surface_height, 
	KDTF_fn_alloc alloc) {
	
	KDTF_GenerateGlyphContoursResult glyph_contours = KDTF_GenerateGlyphContours(glyph, font, alloc);
    KDTF_MergeContoursResult merged_contours = KDTF_MergeContours(glyph_contours.contours, glyph_contours.count, alloc);

	KDTF_GlyphTriangulationState triangulation_state = KDTF_BeginTriangulation(merged_contours.contours, merged_contours.count, alloc);
	while (!KDTF_NextTriangle(&triangulation_state));

	for (i32 k = 0; k < triangulation_state.contour_count; k++) {
		KDTF_GlyphContourTriangulationState state = triangulation_state.contour_triangulation_states[k];
		for (i32 j = 0; j < state.actual_triangle_count; j++) {
			KDTF_GlyphTriangle *triangle = state.triangles + j;
			triangle->a.x *= font->design_units_to_pixels;
			triangle->a.y *= font->design_units_to_pixels;
			triangle->b.x *= font->design_units_to_pixels;
			triangle->b.y *= font->design_units_to_pixels;
			triangle->c.x *= font->design_units_to_pixels;
			triangle->c.y *= font->design_units_to_pixels;
		}

		for (i32 j = 0; j < state.actual_triangle_count; j++) {
			KDTF_GlyphTriangle *triangle = state.triangles + j;
			triangle->a.x += x_offset;
			triangle->a.y += y_offset;
			triangle->b.x += x_offset;
			triangle->b.y += y_offset;
			triangle->c.x += x_offset;
			triangle->c.y += y_offset;
		}

		KDTF_DrawGlyphTriangles(
			surface, surface_width, surface_height,
			state.triangles, state.actual_triangle_count,
			color, anti_aliasing);
	}

	if (glyph->component_count > 0) {
		for (i32 i = 0; i < glyph->component_count; i++) {
			KDTF_Glyph *component = glyph->components + i;
			KDTF_RasterizeGlyph(component, font, color, anti_aliasing, x_offset, y_offset, surface, surface_width, surface_height, alloc);
		}
	}
}

u32 KDTF_BlendColors(u32 source_color, u32 destination_color, u32 draw_color) {
	f32 alpha = ((source_color >> 24) & 0xFF) / 255.0f;
	f32 one_minus_alpha = 1.0f - alpha;

	f32 draw_color_red = (f32)((draw_color >> 16) & 0xFF);
	f32 draw_color_green = (f32)((draw_color >> 8) & 0xFF);
	f32 draw_color_blue = (f32)(draw_color & 0xFF);

	f32 dest_color_red = (f32)((destination_color >> 16) & 0xFF);
	f32 dest_color_green = (f32)((destination_color >> 8) & 0xFF);
	f32 dest_color_blue = (f32)(destination_color & 0xFF);

	f32 r = (one_minus_alpha*dest_color_red) + (alpha*draw_color_red);
	f32 g = (one_minus_alpha*dest_color_green) + (alpha*draw_color_green);
	f32 b = (one_minus_alpha*dest_color_blue) + (alpha*draw_color_blue);

	u32 result = (((u32)r << 16) |
			     ( (u32)g <<  8) | 
			     ( (u32)b <<  0));
	return result;
}

//////////////////////////////////////////////////////////////////////////////////////
/// Font Data Loading Implementation
///
u16 KDTF_takeTwoBytes(u8 *p) {
	// 0x21, big-endian
	// 0x12, little-endian
	return p[0] * 256 + p[1];
}

u32 KDTF_takeFourBytes(u8 *p) {
	// 0x4321, big-endian
	// 0x2143, little-endian
	return (p[0]<<24) + (p[1]<<16) + (p[2]<<8) + p[3];
}

u16 KDTF_toU16(u8* p) {
	// taken from sean barrets stb_truetype
	return p[0] * 256 + p[1];
}

u32 KDTF_toU32(u8* p) {
	return (p[0]<<24) + (p[1]<<16) + (p[2]<<8) + p[3];
}

u16 KDTF_ReadU16(u8 **p) {
	u16 result = KDTF_takeTwoBytes(*p);
	*p += 2;
	return result;
}

i16 KDTF_ReadI16(u8 **p) {
	i16 result = KDTF_takeTwoBytes(*p);
	*p += 2;
	return result;
}

u32 KDTF_ReadU32(u8 **p) {
	u32 result = KDTF_takeFourBytes(*p);
	*p += 4;
	return result;
}

u16 KDTF_ConvertU16LittleEndian(u16 big_endian_value) {
	u16 result = 0;
	result |= (big_endian_value >> Bytes(1)) & 0x00FF;
	result |= (big_endian_value << Bytes(1)) & 0xFF00;
	return result;
}

bool KDTF_TagEquals(u8 *record_tag, char *table_name) {
	return table_name[0] == record_tag[3] && table_name[1] == record_tag[2] && table_name[2] == record_tag[1] && table_name[3] == record_tag[0];
}

// RETURNS: 0 on success, non-zero for failure
KDTF_Font KDTF_CreateFont(void *font_file_contents_ptr) {
	KDTF_Font font = {0};

	if (font_file_contents_ptr == NULL) {
		font.load_error = 1;
		return font;
	}

	// TODO(konnor): Make this a parameter
	u16 platform_to_use = 0; // default to unicode
	
	font.file_data_ptr = font_file_contents_ptr;
	
	void *font_read_ptr = font_file_contents_ptr;
	
	u32 cmap_table_offset = 0;
	u32 head_table_offset = 0;
	u32 hhea_table_offset = 0;
	u32 hmtx_table_offset = 0;
	u32 os2_table_offset = 0;
	u32 maxp_table_offset = 0;

	///////////////////////////////////
	/// Read Font Directory
	///
	u8 *font_table_ptr = (u8*)font_read_ptr;
	font_table_ptr += 4; // skipping sfntVersion
	u16 table_count = KDTF_ReadU16(&font_table_ptr);
	font_table_ptr += 2; // skipping searchRange
	font_table_ptr += 2; // skipping entrySelector
	font_table_ptr += 2; // skipping rangeShift

	union {
		u32 value;
		u8 *chars[4];
	} tag;

	for (int i = 0; i < table_count; i++) {
		tag.value = KDTF_ReadU32(&font_table_ptr);
		font_table_ptr += 4; // skipping checksum
		u32 offset = KDTF_ReadU32(&font_table_ptr);
		font_table_ptr += 4; // skipping length

		u8 *record_tag = (u8*)tag.chars;
		if (KDTF_TagEquals(record_tag, "cmap")) {
			cmap_table_offset = offset;
		} else if (KDTF_TagEquals(record_tag, "head")) {
			head_table_offset = offset;
		} else if (KDTF_TagEquals(record_tag, "loca")) {
			font.loca_table_offset = offset;
		} else if (KDTF_TagEquals(record_tag, "glyf")) {
			font.glyf_table_offset = offset;
		} else if (KDTF_TagEquals(record_tag, "hhea")) {
			hhea_table_offset = offset;
		} else if (KDTF_TagEquals(record_tag, "hmtx")) {
			hmtx_table_offset = offset;
		} else if (KDTF_TagEquals(record_tag, "OS/2")) {
			os2_table_offset = offset;
		} else if (KDTF_TagEquals(record_tag, "maxp")) {
			maxp_table_offset = offset;
		}
	}
	
	///////////////////////////////////////////////////
	/// Read Unicode Platform Subtable Offset
	///
	Assert(cmap_table_offset >= 0); // crash if we couldn't find the cmap table offset

	u32 cmap_platform_subtable_offset = 0;

	u8 *cmap_ptr = (((u8*)font_file_contents_ptr) + cmap_table_offset);
	cmap_ptr += 2; // skipping cmap table version
	u16 encoding_records_count = KDTF_ReadU16(&cmap_ptr);
	for (int i = 0; i < encoding_records_count; i++) {
		u16 platform_id = KDTF_ReadU16(&cmap_ptr);
		if (platform_id == platform_to_use) {
			cmap_ptr += 2; // skipping the platform specific id 
			cmap_platform_subtable_offset = KDTF_ReadU32(&cmap_ptr);
			break;
		}
	}
	
	//////////////////////////////////////////////////
	/// Read Unicode Platform Table Record
	///
	Assert(cmap_platform_subtable_offset >= 0); // crash if we couldn't find the cmap table offset
	
	u8 *unicode_platform_table_ptr = ((u8*)font_file_contents_ptr) + cmap_table_offset + cmap_platform_subtable_offset;
	// init_platform_record_table_format4(&out.platform, unicode_platform_table_ptr);
	unicode_platform_table_ptr += 2; // skipping format
	unicode_platform_table_ptr += 2; // skipping length
	unicode_platform_table_ptr += 2; // skipping language
	u16 segment_count_x2 = KDTF_ReadU16(&unicode_platform_table_ptr);
	font.cmap_segment_count = segment_count_x2 / 2;
	unicode_platform_table_ptr += 2; // skipping search_range
	unicode_platform_table_ptr += 2; // skipping entry_selector
	unicode_platform_table_ptr += 2; // skpping rangeShift
	font.cmap_end_codes = (u16*)unicode_platform_table_ptr;
	unicode_platform_table_ptr += segment_count_x2;
	unicode_platform_table_ptr += 2; // padding
	font.cmap_start_codes = (u16*)unicode_platform_table_ptr;
	unicode_platform_table_ptr += segment_count_x2;
	font.cmap_id_delta = (u16*)unicode_platform_table_ptr;
	unicode_platform_table_ptr += segment_count_x2;
	font.cmap_id_range_offsets = (u16*)unicode_platform_table_ptr;
	unicode_platform_table_ptr += segment_count_x2;
	font.cmap_glyph_ids = (u16*)unicode_platform_table_ptr;


	///////////////////////////////////
	/// Read index_to_loca_format
	///
	Assert(head_table_offset >= 0);

	u8 *head_table_ptr = (((u8*)font_file_contents_ptr) + head_table_offset);
	head_table_ptr += 2; // skipping version_major
	head_table_ptr += 2; // skipping version_minor
	head_table_ptr += 4; // skipping revision
	head_table_ptr += 4; // skipping checksum_adjustment
	head_table_ptr += 4; // skipping magic_number
	head_table_ptr += 2; // skipping flags
	font.units_per_em = KDTF_ReadU16(&head_table_ptr);
	head_table_ptr += 8; // skipping time_created
	head_table_ptr += 8; // skipping time_modified
	head_table_ptr += 2; // skipping glyph_min_x
	head_table_ptr += 2; // skipping glyph_min_y 
	head_table_ptr += 2; // skipping glyph_max_x
	head_table_ptr += 2; // skipping glyph_max_y
	head_table_ptr += 2; // skipping mac_style
	head_table_ptr += 2; // skipping lowest_rec_ppem
	head_table_ptr += 2; // skipping font_direction_hint
	font.index_to_loca_format = KDTF_ReadI16(&head_table_ptr);
	// skipping glyph data format as well...

	////////////////////////////////////////////
	/// Read line and character metrics
	///
	Assert(hhea_table_offset >= 0);
	u8 *hhea_table_ptr = (((u8*)font_file_contents_ptr) + hhea_table_offset);
	hhea_table_ptr += 2; // skipping major_version
	hhea_table_ptr += 2; // skipping major_version
	font.ascender = KDTF_ReadI16(&hhea_table_ptr);
	font.descender = KDTF_ReadI16(&hhea_table_ptr);
	hhea_table_ptr += 2; // skipping line_gap
	font.advance_width_max = KDTF_ReadU16(&hhea_table_ptr);
	hhea_table_ptr += 2; // skipping min_left_side_bearing
	hhea_table_ptr += 2; // skipping min_right_side_bearing
	hhea_table_ptr += 2; // skipping x_max_extent
	hhea_table_ptr += 2; // skipping caret_slope_rise
	hhea_table_ptr += 2; // skipping caret_slope_run
	hhea_table_ptr += 2; // skipping caret_offset
	hhea_table_ptr += 2; // skipping reserved
	hhea_table_ptr += 2; // skipping reserved
	hhea_table_ptr += 2; // skipping reserved
	hhea_table_ptr += 2; // skipping reserved
	hhea_table_ptr += 2; // skipping metric_data_format
	hhea_table_ptr += 2; // skipping horizontal_metric_count

	//////////////////////////////////////
	/// Read maximum component depth
	///
	Assert(maxp_table_offset >= 0);
	u8 *maxp_table_ptr = (((u8*)font_file_contents_ptr) + maxp_table_offset);
	u32 maxp_table_version = KDTF_ReadU32(&maxp_table_ptr);
	u16 maxp_table_version_major = (u16)(maxp_table_version >> 16);
	u16 maxp_table_version_minor = (u16)maxp_table_version;
	
	maxp_table_ptr += 2; // skipping num_glyphs
	if (maxp_table_version_major == 1 && maxp_table_version_minor == 0) {
		font.maxp_max_points = KDTF_ReadU16(&maxp_table_ptr);
		font.maxp_max_contours = KDTF_ReadU16(&maxp_table_ptr);
		font.maxp_max_composite_points = KDTF_ReadU16(&maxp_table_ptr);
		font.maxp_max_composite_contours = KDTF_ReadU16(&maxp_table_ptr); // skipping max_composite_glyph_points_count
		maxp_table_ptr += 2; // skipping max_zones
		maxp_table_ptr += 2; // skipping twighlight_points
		maxp_table_ptr += 2; // skipping max_storage
		maxp_table_ptr += 2; // skipping max_function_defs
		maxp_table_ptr += 2; // skipping max_instruction_defs
		maxp_table_ptr += 2; // skipping max_stack_elements
		maxp_table_ptr += 2; // skipping max_size_of_instructions
		maxp_table_ptr += 2; // skipping max_component_elements
		maxp_table_ptr += 2; // skipping max_component_depth
	}

	return font;
}

KDTF_Font KDTF_CreateFontFromFile(char *filepath) {
	void *font_file_bytes = 0;

#if _WIN32
	HANDLE file_handle = CreateFileA(filepath, 
							GENERIC_READ|GENERIC_WRITE, 
							FILE_SHARE_READ, 
							NULL, 
							OPEN_EXISTING, 
							0, 
							NULL);
	
	if (file_handle == INVALID_HANDLE_VALUE) {
		wchar_t message[256] = {0};
		wsprintfW(message, L"KDTF: CreateFileA(%s) failed with error: %d\n", filepath, GetLastError());
		OutputDebugStringW(message);
		return KDTF_CreateFont(0);
	}
	
	LARGE_INTEGER file_size;
	if (!GetFileSizeEx(file_handle, &file_size)) {
		wchar_t message[256] = {0};
		wsprintfW(message, L"KDTF: ReadFile(%s) failed with error: %d\n", filepath, GetLastError());
		OutputDebugStringW(message);
		CloseHandle(file_handle);
		return KDTF_CreateFont(0);
	}
	
	Assert(file_size.QuadPart >= 0);

	if (file_size.QuadPart == 0) {
		CloseHandle(file_handle);
		wchar_t message[256] = {0};
		wsprintfW(message, L"KDTF: File is empty!\n");
		OutputDebugStringW(message);
		return KDTF_CreateFont(0);
	}
	
	u8 *file_contents = (u8*)VirtualAlloc(0, file_size.QuadPart, MEM_RESERVE|MEM_COMMIT, PAGE_READWRITE);
	Assert(file_contents != NULL);
	
	BOOL read_file = ReadFile(file_handle, file_contents, (DWORD)file_size.QuadPart, NULL, NULL);
	
	if (!read_file) {
		OutputDebugStringW(L"KDTF: Failed to read file!\n");
		CloseHandle(file_handle);
		VirtualFree(file_contents, 0, MEM_RELEASE);
		return KDTF_CreateFont(0);
	}

	CloseHandle(file_handle);


	font_file_bytes = file_contents;
#else
Assert(false); // support reading file for platform
#endif

	return KDTF_CreateFont(font_file_bytes);
}

static const u32 ARG_1_AND_2_ARE_WORDS = 0x0001;
static const u32 ARGS_ARE_XY_VALUES = 0x0002;
static const u32 ROUND_XY_TO_GRID = 0x0004;
static const u32 WE_HAVE_A_SCALE = 0x0008;
static const u32 MORE_COMPONENTS = 0x0020;
static const u32 WE_HAVE_AN_X_AND_Y_SCALE = 0x0040;
static const u32 WE_HAVE_A_TWO_BY_TWO = 0x0080;
static const u32 WE_HAVE_INSTRUCTIONS = 0x0100;
static const u32 USE_MY_METRICS = 0x0200;
static const u32 OVERLAP_COMPOUND = 0x0400;
static const u32 SCALED_COMPONENT_OFFSET = 0x0800;
static const u32 UNSCALED_COMPONENT_OFFSET = 0x1000;

typedef struct {
	i16 contour_count;
	i16 min_x;
	i16 min_y;
	i16 max_x;
	i16 max_y;
} KDTF_GlyphHeader;

KDTF_GlyphHeader KDTF_ParseGlyphHeader(u8 **glyf_ptr) {
	KDTF_GlyphHeader result = {0};
	result.contour_count = KDTF_ReadI16(glyf_ptr);
	result.min_x = KDTF_ReadI16(glyf_ptr);
	result.min_y = KDTF_ReadI16(glyf_ptr);
	result.max_x = KDTF_ReadI16(glyf_ptr);
	result.max_y = KDTF_ReadI16(glyf_ptr);
	return result;
}

void KDTF_ParseSimpleGlyphTable(u16 glyph_index, KDTF_Font *font, KDTF_Glyph *result, u8 *glyph_data_ptr, KDTF_fn_alloc alloc) {
	KDTF_GlyphHeader header = KDTF_ParseGlyphHeader(&glyph_data_ptr);

	u16 *end_pts_of_contours = (u16*)glyph_data_ptr;
	
	u16 instruction_length = KDTF_toU16(glyph_data_ptr + (header.contour_count * 2));
	u8 *instructions = glyph_data_ptr + (header.contour_count * 2) + 2;
	
	u8 *vertices_ptr = instructions + instruction_length;
	
	u32 vertex_count = 1 + KDTF_toU16((u8*)end_pts_of_contours + header.contour_count*2-2);
	
	u8 *glyph_flags = KDTF_AllocArray(alloc, vertex_count, u8);
	
	u8 FLAG_REPEAT = 8;
	
	i32 flag_count = 0;
	u8 flags = 0;
	for (u32 i = 0; i < vertex_count; ++i) {
		if (flag_count == 0) {
			flags = *vertices_ptr++;
			if (flags & FLAG_REPEAT) {
				flag_count = *vertices_ptr++;
			}
		} else {
			--flag_count;
		}
		u8 *vertex_flags = glyph_flags + i;
		*vertex_flags = flags;
	}

	KDTF_GlyphPoint *coordinates = KDTF_AllocArray(alloc, vertex_count, KDTF_GlyphPoint);
	i16 x = 0;
	for (u32 i = 0; i < vertex_count; ++i) {
		u8 vertex_flags = *(glyph_flags + i);
		if (vertex_flags & FLAG_X_SHORT_VECTOR) {
			i16 tmp_x = *vertices_ptr++;
			x += (vertex_flags & FLAG_X_IS_SAME_OR_POSITIVE_X_SHORT_VECTOR) ? tmp_x : -tmp_x;
		} else {
			if (!(vertex_flags & FLAG_X_IS_SAME_OR_POSITIVE_X_SHORT_VECTOR)) {
				x += KDTF_toU16(vertices_ptr);
				vertices_ptr+= 2;
			}
		}
		(coordinates + i)->x = x;
	}

	i16 y = 0;
	for (u32 i = 0; i < vertex_count; ++i) {
		u8 vertex_flags = *(glyph_flags + i);
		if (vertex_flags & FLAG_Y_SHORT_VECTOR) {
			i16 tmp_y = *vertices_ptr++;
			y += (vertex_flags & FLAG_Y_IS_SAME_OR_POSITIVE_Y_SHORT_VECTOR) ? tmp_y : -tmp_y;
		} else {
			if (!(vertex_flags & FLAG_Y_IS_SAME_OR_POSITIVE_Y_SHORT_VECTOR)) {
				y += KDTF_toU16(vertices_ptr);
				vertices_ptr+= 2;
			}
		}
		(coordinates + i)->y = y;
	}

	result->contour_end_pt_indices = KDTF_AllocArray(alloc, header.contour_count, u16);
	for (int i = 0; i < header.contour_count; i++) {
		u16 contour_end_point_index = KDTF_toU16((u8*)(end_pts_of_contours + i));
		result->contour_end_pt_indices[i] = contour_end_point_index;
	}
	
	result->contour_count = header.contour_count;
	result->min_x = header.min_x;
	result->min_y = header.min_y;
	result->max_x = header.max_x;
	result->max_y = header.max_y;
	result->coordinate_count = vertex_count;
	result->flags = glyph_flags;
	result->coordinates = coordinates;
}

typedef struct {
	i32 component_count;
	u16 *flags;
	u16 *glyph_index;
} KDTF_ComponentInfo;

void KDTF_ApplyOffsetToGlyph(KDTF_Glyph *glyph, i16 x_offset, i16 y_offset) {
	for (u32 i = 0; i < glyph->coordinate_count; i++) {
		KDTF_GlyphPoint *coordinate = glyph->coordinates + i;
		coordinate->x += x_offset;
		coordinate->y += y_offset;
	}
}

typedef struct {
	i32 count;
	u16 *glyph_index;
	i16 *x_offsets;
	i16 *y_offsets;
} KDTF_CompositeGlyphTable;

KDTF_CompositeGlyphTable KDTF_ParseCompositeGlyphTable(u8 *glyf_data_ptr, KDTF_fn_alloc alloc) {
	KDTF_CompositeGlyphTable result = {0};

	u64 ptr_offset = 0;

	u8 more_components = 0;
	do {
		// Loading composite glyph header
		u16 flags = KDTF_takeTwoBytes(glyf_data_ptr + ptr_offset);
		ptr_offset += 2;
		
		ptr_offset += 2;

		if (flags & ARG_1_AND_2_ARE_WORDS) {
			ptr_offset += 2;
			ptr_offset += 2;
		} else {
			ptr_offset += 2;
		}

		if (flags & WE_HAVE_A_SCALE) {
			ptr_offset += 2;
		} else if (flags & WE_HAVE_AN_X_AND_Y_SCALE) {
			ptr_offset += 2;
			ptr_offset += 2;
		} else if (flags & WE_HAVE_A_TWO_BY_TWO) {
			ptr_offset += 2;
			ptr_offset += 2;
			ptr_offset += 2;
			ptr_offset += 2;
		}

		if (flags & WE_HAVE_INSTRUCTIONS) {
			u16 instruction_count = KDTF_takeTwoBytes(glyf_data_ptr + ptr_offset);
			ptr_offset += 2;
			// ignoring instructions for now
			ptr_offset += instruction_count;
		}

		result.count += 1;
		more_components = flags & MORE_COMPONENTS;
	} while(more_components);

	result.glyph_index = KDTF_AllocArray(alloc, result.count, u16);
	result.x_offsets = KDTF_AllocArray(alloc, result.count, i16);
	result.y_offsets = KDTF_AllocArray(alloc, result.count, i16);

	for (i32 i = 0; i < result.count; i++) {
		// Loading composite glyph header
		u16 flags = KDTF_ReadU16(&glyf_data_ptr);
		result.glyph_index[i] = KDTF_ReadU16(&glyf_data_ptr);

		if (flags & ARG_1_AND_2_ARE_WORDS) {
			i16 x_offset = KDTF_ReadI16(&glyf_data_ptr);
			i16 y_offset = KDTF_ReadI16(&glyf_data_ptr);
			result.x_offsets[i] = x_offset;
			result.y_offsets[i] = y_offset;
		} else {
			u16 xandyoffset = KDTF_ReadU16(&glyf_data_ptr);
			u8 x_offset = (u8)(xandyoffset << 8);
			u8 y_offset = (u8)xandyoffset;
			result.x_offsets[i] = x_offset;
			result.y_offsets[i] = y_offset;
		}

		if (flags & WE_HAVE_A_SCALE) {
			*glyf_data_ptr += 2;
		} else if (flags & WE_HAVE_AN_X_AND_Y_SCALE) {
			*glyf_data_ptr += 2;
			*glyf_data_ptr += 2;
		} else if (flags & WE_HAVE_A_TWO_BY_TWO) {
			*glyf_data_ptr += 2;
			*glyf_data_ptr += 2;
			*glyf_data_ptr += 2;
			*glyf_data_ptr += 2;
		}

		if (flags & WE_HAVE_INSTRUCTIONS) {
			u16 instruction_count = KDTF_ReadU16(&glyf_data_ptr);
			// ignoring instructions for now
			*glyf_data_ptr += instruction_count;
		}
	}

	return result;
}

void KDTF_ParseGlyphAtIndex(u16 glyph_index, KDTF_Font *font, KDTF_Glyph *result, KDTF_fn_alloc alloc) {
	Assert(font->index_to_loca_format == 0 || font->index_to_loca_format == 1);

	i32 glyf_offset = 0;

	i32 tmp1, tmp2;
	
	u8 *data = (u8*)font->file_data_ptr;
	if (font->index_to_loca_format == 0) {
		// short offsets
		tmp1 = (i32)font->glyf_table_offset + KDTF_toU16(data + font->loca_table_offset + glyph_index * 2) * 2;
		tmp2 = (i32)font->glyf_table_offset + KDTF_toU16(data + font->loca_table_offset + glyph_index * 2 + 2) * 2;
	} else {
		// long offsets
		tmp1 = (i32)font->glyf_table_offset + KDTF_toU32(data + font->loca_table_offset + glyph_index * 4);
		tmp2 = (i32)font->glyf_table_offset + KDTF_toU32(data + font->loca_table_offset + glyph_index * 4 + 4);
	}

	if (tmp1 == tmp2) {
		return;
	} else {
		glyf_offset = tmp1;
	}
	
	Assert(glyf_offset >= 0);

	u8 *glyf_ptr = (u8*)font->file_data_ptr + glyf_offset;

	i16 glyph_contour_count = KDTF_takeTwoBytes((u8*)font->file_data_ptr + glyf_offset);

	if (glyph_contour_count >= 0) {
		KDTF_ParseSimpleGlyphTable(glyph_index, font, result, glyf_ptr, alloc);
	} else {
		u8 *glyf_ptr_test = (u8*)font->file_data_ptr + tmp1;
		KDTF_ParseGlyphHeader(&glyf_ptr_test);
		// manually increment the pointer here, instead of `KDTF_ParseGlypHeader` incrementing the pointer
		u8 *glyf_ptr_test2 = (u8*)font->file_data_ptr + tmp1 + sizeof(KDTF_GlyphHeader);
		KDTF_CompositeGlyphTable headers = KDTF_ParseCompositeGlyphTable(glyf_ptr_test2, alloc);

		result->components = KDTF_AllocArray(alloc, headers.count, KDTF_Glyph);
		result->component_count = headers.count;

		for (i32 i = 0; i < headers.count; i++) {
			KDTF_Glyph *component = result->components + i;
			i32 component_glyph_index = headers.glyph_index[i];
			KDTF_ParseGlyphAtIndex(component_glyph_index, font, component, alloc);
			u16 x_offset = headers.x_offsets[i];
			u16 y_offset = headers.y_offsets[i];
			KDTF_ApplyOffsetToGlyph(component, x_offset, y_offset);
		}
	}
}

i32 KDTF_GetGlyphForCodepoint(i32 codepoint, KDTF_Font *font, KDTF_Glyph *out, KDTF_fn_alloc alloc) {
	// TODO: Revisit whether KDTF_u16_to_little_endian is needed, or if something
	// more common can be used instead.
	u16 glyph_index = 0;
	
	// Find the segment the codepoint is in
	i32 segment_index = -1;
	for (int i = 0; i < font->cmap_segment_count; i++) {
		u16 endCode = KDTF_ConvertU16LittleEndian(font->cmap_end_codes[i]);
		if (endCode >= codepoint) {
			segment_index = i;
			break;
		}
	}
	
	Assert(segment_index >= 0);
	
	// Calculate the index using cmap segment
	u16 start_code = KDTF_ConvertU16LittleEndian(font->cmap_start_codes[segment_index]);
	if (start_code <= codepoint) {
		u16 id_range_offset = KDTF_ConvertU16LittleEndian(font->cmap_id_range_offsets[segment_index]);
		u16 id_delta = KDTF_ConvertU16LittleEndian(font->cmap_id_delta[segment_index]);
		if (id_range_offset != 0) {
			u16 *tmp_ptr = ((font->cmap_id_range_offsets + segment_index + id_range_offset/2) + 
							codepoint - start_code);
			u16 value = KDTF_ConvertU16LittleEndian(*tmp_ptr);
			if (value != 0) {
				glyph_index = value + id_delta;
			}
		} else {
			glyph_index = codepoint + id_delta;
		}
	}

	KDTF_ParseGlyphAtIndex(glyph_index, font, out, alloc);

	return 0;
}

void KDTF_GetDefaultGlyph(KDTF_Font *font, KDTF_Glyph *result, KDTF_fn_alloc alloc) {
	KDTF_ParseGlyphAtIndex(0, font, result, alloc);
}

void KDTF_SetFontSize(KDTF_Font *font, f32 height_in_pixels) {
	font->font_size_pixels = height_in_pixels;
	font->design_units_to_pixels = height_in_pixels / (f32)font->units_per_em;
	font->average_advance_width = Ceil(font->advance_width_max * font->design_units_to_pixels);
	font->line_height = Ceil((font->ascender + (-1 * font->descender)) * font->design_units_to_pixels);
}

void KDTF_IncreaseFontSize(KDTF_Font *font) {
	if (font->font_size_pixels >= font->max_font_size_pixels) {
		return;
	}
	KDTF_SetFontSize(font, font->font_size_pixels + font->font_size_pixels_step);
}

void KDTF_DecreaseFontSize(KDTF_Font *font) {
	if (font->font_size_pixels <= font->min_font_size_pixels) {
		return;
	}
	KDTF_SetFontSize(font, font->font_size_pixels - font->font_size_pixels_step);
}

KDTF_GlyphAtlas KDTF_AllocateGlyphAtlas(KDTF_Font *font, char *character_set, i32 character_set_size, b8 subpixel_rendering, KDTF_fn_alloc bitmap_memory_allocator) {
	KDTF_GlyphAtlas result = {0};

	result.atlas_height = font->line_height;
	result.atlas_width = character_set_size * font->average_advance_width;
	result.pixels = KDTF_AllocArray(bitmap_memory_allocator, result.atlas_width * result.atlas_height, u32);
	result.characters = character_set;
	result.character_count = character_set_size;
	result.glyph_width = font->average_advance_width;
	result.glyph_height = font->line_height;
	result.anti_aliasing = subpixel_rendering;

	return result;
}

void KDTF_FreeGlyphAtlas(KDTF_GlyphAtlas *atlas, KDTF_fn_free free_fn) {
	free_fn(atlas->pixels);
}

// NOTE:
//  - atlas_memory needs to be glyph height by (glyph advance width * character count)
//  - calculation_memory needs to be experimented with, not sure how much is going to be needed per font
void KDTF_InitializeGlyphAtlas(KDTF_GlyphAtlas *atlas, KDTF_Font *font, KDTF_fn_alloc calculations_memory_allocator) {
	i32 x_offset = 0;
	i32 y_offset = (i32)((f32)(-1 * font->descender) * font->design_units_to_pixels);

	for (i32 i = 0; i < atlas->character_count; i++) {
		char character = atlas->characters[i];

		if (character == ' ') {
			// do nothing for the space character, since it has no contours
			continue;
		}

		KDTF_Glyph glyph = {0};
		i32 get_glyph_failed = KDTF_GetGlyphForCodepoint(character, font, &glyph, calculations_memory_allocator);
		if (get_glyph_failed) {
			continue;
		}

		KDTF_RasterizeGlyph(
			&glyph, font, 0xFFFFFFFF, atlas->anti_aliasing, x_offset, y_offset, 
			atlas->pixels, atlas->atlas_width, atlas->atlas_height, calculations_memory_allocator
		);

		x_offset += font->average_advance_width;
	}
}

i32 KDTF_GetXOffsetForGlyph(KDTF_GlyphAtlas *atlas, char character) {
	i32 result = -1;
	for (i32 i = 0; i < atlas->character_count; i++) {
		char c = atlas->characters[i];
		if (c == character) {
			result = i * atlas->glyph_width;
			break;
		}
	}
	return result;
}

KDTF_Font KDTF_AllocateFont(
	char *filepath, char *character_set, b8 flip_y,
	b8 anti_aliasing, f32 min_font_size, f32 max_font_size, f32 font_size, f32 font_size_step,
	KDTF_fn_alloc bitmap_alloc, KDTF_fn_free bitmap_free,
	KDTF_fn_alloc tmp_alloc, KDTF_fn_free tmp_free) {
	
	KDTF_Font font = KDTF_CreateFontFromFile(filepath);
	if (font.load_error) {
		return font;
	}
	
	font.flip_y = flip_y;

	font.min_font_size_pixels = min_font_size;
	font.max_font_size_pixels = max_font_size;
	font.font_size_pixels_step = font_size_step;
	KDTF_SetFontSize(&font, font_size);

    i32 character_set_size = 0;
	char *character_set_ptr = character_set;
	while (*character_set_ptr++) {
		character_set_size += 1;
	}

	font.atlas = KDTF_AllocateGlyphAtlas(&font, character_set, character_set_size, true, bitmap_alloc);
	KDTF_InitializeGlyphAtlas(&font.atlas, &font, tmp_alloc);

	return font;
}

void KDTF_DrawCharacter(KDTF_Font *font, char character, u32 color,
	i32 *xPos, i32 *yPos,
	u32 *surface, u32 surface_width, u32 surface_height) {

	KDTF_GlyphAtlas *atlas = &font->atlas;

	if (character == ' ') {
		// do nothing
		return;
	}

	i32 glyph_x_offset = KDTF_GetXOffsetForGlyph(atlas, character);
	if (glyph_x_offset == -1) {
		return;
	}

	for (i32 y = 0; y < atlas->glyph_height; y++) {
		i32 source_y = y;
		i32 dest_y;
		if (font->flip_y) {
			dest_y = *yPos - y + atlas->glyph_height;
		} else {
			dest_y = *yPos + y;
		}
		
		u32 *glyph_bitmap_pixels = atlas->pixels + (source_y * atlas->atlas_width);
		u32 *destination_bitmap_pixels_row = surface + (dest_y * surface_width);
		
		for (i32 x = 0; x < atlas->glyph_width; x++) {
			u32 *glyph_pixels = glyph_bitmap_pixels + x + glyph_x_offset;
			u32 *destination_pixels = destination_bitmap_pixels_row + x + *xPos;

			u32 blended_color = KDTF_BlendColors(*glyph_pixels, *destination_pixels, color);

			*destination_pixels = blended_color;
		}
	}
	
	*xPos += font->atlas.glyph_width;
}

void KDTF_DrawText(
	KDTF_Font *font, 
	char *text, i32 text_length, 
	u32 color, i32 *xPos, i32 *yPos, 
	u32 *surface, u32 surface_width, u32 surface_height) {
	
	for (i32 i = 0; i < text_length; i++) {
		char character = text[i];
		KDTF_DrawCharacter(font, character, color, xPos, yPos, surface, surface_width, surface_height);
	}
}

void KDTF_DrawNumber(
	KDTF_Font *font, 
	i32 number,
	u32 color, i32 *xPos, i32 *yPos, 
	u32 *surface, u32 surface_width, u32 surface_height) {
	
	if (number == 0) {
		KDTF_DrawCharacter(font, '0', color, xPos, yPos, surface, surface_width, surface_height);
		return;
	}
	
	i32 digit_count_to_power_of_ten = 1;
	i32 digit_count = 1;
	while (true) {
		i32 next_power_of_10 = digit_count_to_power_of_ten * 10;
		if (next_power_of_10 > number) {
			break;
		}
		digit_count++;
		digit_count_to_power_of_ten = next_power_of_10;
	}
	
	b8 did_draw_number = false;
	while (digit_count_to_power_of_ten > 0) {
		i32 tmp = number / digit_count_to_power_of_ten;
		if (tmp > 0) {
			number -= (tmp * digit_count_to_power_of_ten);
			char code = (char)(tmp + '0');
			KDTF_DrawCharacter(font, code, color, xPos, yPos, surface, surface_width, surface_height);
			did_draw_number = true;
		} else if (tmp == 0 && did_draw_number) {
			KDTF_DrawCharacter(font, '0', color, xPos, yPos, surface, surface_width, surface_height);
		}

		digit_count_to_power_of_ten /= 10;
	}	
}

#endif
