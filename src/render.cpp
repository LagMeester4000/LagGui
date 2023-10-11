#include "basic.hpp"
#include "lag_gui.hpp"
#include "raylib.h"
#include "rlgl.h"
#include <math.h>

namespace lgui {

void set_clip_rect(v2 start, v2 end)
{
	BeginScissorMode(start.x, start.y, end.x - start.x, end.y - start.y);
}

void reset_clip_rect()
{
	EndScissorMode();
}

void push_clip_rect(Context* context, Rect rect)
{
	LGUI_ASSERT(context->clip_rect_stack_top < MAX_CLIP_RECT, "Too many clip rects");

}

void pop_clip_rect(Context* context)
{
	LGUI_ASSERT(context->clip_rect_stack_top > 0, "No clip rects to pop");

}

void draw_triangle(v2 p1, v2 p2, v2 p3, Color color)
{
	rlBegin(RL_TRIANGLES);
	rlColor4f(color.r, color.g, color.b, color.a);
	rlVertex2f(p1.x, p1.y);
	rlVertex2f(p2.x, p2.y);
	rlVertex2f(p3.x, p3.y);
	rlEnd();
}

void draw_triangle(v2 p1, v2 p2, v2 p3, Color c1, Color c2, Color c3)
{
	rlBegin(RL_TRIANGLES);
	rlColor4f(c1.r, c1.g, c1.b, c1.a);
	rlVertex2f(p1.x, p1.y);
	rlColor4f(c2.r, c2.g, c2.b, c2.a);
	rlVertex2f(p2.x, p2.y);
	rlColor4f(c3.r, c3.g, c3.b, c3.a);
	rlVertex2f(p3.x, p3.y);
	rlEnd();
}

void draw_rectangle(v2 pos, v2 size, Color color)
{
	draw_triangle(pos, pos + v2{size.x, 0}, pos + v2{0, size.y}, color);
	draw_triangle(pos + v2{size.x, 0}, pos + size, pos + v2{0, size.y}, color);
}

void Painter::_push_command(Context* context)
{
	// Only push the command if it has any vertices
	if (current_command.vertex_start == current_command.vertex_end ||
		current_command.index_start == current_command.index_end)
	{
		return;
	}

	// Allocate command
	//DrawCommand* command = context->arena.allocate_one<DrawCommand>();
	DrawCommand* command = context->temp_arena.allocate_one<DrawCommand>();
	*command = current_command;
	command->prev = last_command;

	// Update global buffer index
	//context->draw_buffer.vertex_buffer_top += current_command.vertex_end - current_command.vertex_start;
	//context->draw_buffer.index_buffer_top += current_command.index_end - current_command.index_start;
	LGUI_ASSERT(context->draw_buffer.vertex_buffer_top == current_command.vertex_start,
		"Draw buffer has been written to before this command was completed");
	LGUI_ASSERT(context->draw_buffer.index_buffer_top == current_command.index_start,
		"Draw buffer has been written to before this command was completed");
	context->draw_buffer.vertex_buffer_top = current_command.vertex_end;
	context->draw_buffer.index_buffer_top = current_command.index_end;

	// Reset current command
	current_command = DrawCommand{};
	current_command.clip_rect = command->clip_rect;
	current_command.vertex_start = context->draw_buffer.vertex_buffer_top;
	current_command.vertex_end = current_command.vertex_start;
	current_command.index_start = context->draw_buffer.index_buffer_top;
	current_command.index_end = current_command.index_start;

	if (!first_command)
	{
		LGUI_ASSERT(!last_command, "Last command should be empty");
		first_command = command;
		last_command = command;
	}
	else
	{
		LGUI_ASSERT(last_command, "If the first command exists, the last one should also exist");
		last_command->next = command;
		last_command = command;
	}
}

void Painter::_start_painter(Context* context)
{
	if (frame_last_updated != context->current_frame)
	{
		frame_last_updated = context->current_frame;

		first_command = nullptr;
		last_command = nullptr;

		current_command.clip_rect = get_clip_rect();
		current_command.vertex_start = context->draw_buffer.vertex_buffer_top;
		current_command.vertex_end = current_command.vertex_start;
		current_command.index_start = context->draw_buffer.index_buffer_top;
		current_command.index_end = current_command.index_start;
	}
	else
	{
		_push_command(context);
	}
}

void Painter::_restart_painter(Context* context)
{
	current_command.vertex_start = context->draw_buffer.vertex_buffer_top;
	current_command.vertex_end = current_command.vertex_start;
	current_command.index_start = context->draw_buffer.index_buffer_top;
	current_command.index_end = current_command.index_start;
}

void Painter::push_clip_rect(Context* context, Rect rect)
{
	LGUI_ASSERT(clip_rect_stack_top < MAX_CLIP_RECT, "Out of bounds");

	_push_command(context);

	Rect clip = get_clip_rect();
	Rect new_rect = clip.clip(rect);
	clip_rect_stack[clip_rect_stack_top] = new_rect;
	++clip_rect_stack_top;

	current_command.clip_rect = new_rect;
}

void Painter::pop_clip_rect(Context* context)
{
	LGUI_ASSERT(clip_rect_stack_top > 0, "Out of bounds");

	_push_command(context);

	--clip_rect_stack_top;
	Rect clip = get_clip_rect();
	current_command.clip_rect = clip;
}

Rect Painter::get_clip_rect()
{
	// TODO: Map this to screen size
	return clip_rect_stack_top > 0 ? clip_rect_stack[clip_rect_stack_top - 1] : Rect{{0, 0}, {1920, 1080}};
}

const usize VERTEX_SIZE_BYTES = sizeof(f32) * 4 + sizeof(u32);
const usize VERTEX_SIZE_FLOATS = 5;

struct ColorU32 {
	union {
		u32 as_int;
		u8 as_arr[4];
		f32 as_float; // To put it in the buffer
	};
};

// Has enough space for count amount of vertices
inline static bool has_vertex_space(Context* context, Painter* painter, usize vert_count)
{
	return painter->current_command.vertex_end + vert_count * VERTEX_SIZE_FLOATS <
		context->draw_buffer.vertex_buffer_length;
}

// Has enough space for count amount of vertices
inline static bool has_index_space(Context* context, Painter* painter, usize tri_count)
{
	return painter->current_command.index_end + tri_count < context->draw_buffer.index_buffer_length;
}

// Returns index
inline static u32 push_vertex(Context* context, Painter* painter, v2 pos, v2 uv, ColorU32 color)
{
	//u32 index = context->draw_buffer.vertex_buffer_top;
	u32 index = painter->current_command.vertex_end;
	LGUI_ASSERT(index % VERTEX_SIZE_FLOATS == 0, "vertex buffer has incorrect number of floats");
	f32* ptr = context->draw_buffer.vertex_buffer + index;
	ptr[0] = pos.x;
	ptr[1] = pos.y;
	ptr[2] = uv.x;
	ptr[3] = uv.y;
	ptr[4] = color.as_float;
	//context->draw_buffer.vertex_buffer_top += VERTEX_SIZE_FLOATS;
	//painter->current_command.vertex_end = context->draw_buffer.vertex_buffer_top;
	painter->current_command.vertex_end += VERTEX_SIZE_FLOATS;
	return index / VERTEX_SIZE_FLOATS;
}

inline static void push_index_triangle(Context* context, Painter* painter, u16 i1, u16 i2, u16 i3)
{
	//u16* ptr = context->draw_buffer.index_buffer + context->draw_buffer.index_buffer_top;
	u16* ptr = context->draw_buffer.index_buffer + painter->current_command.index_end;
	ptr[0] = i1;
	ptr[1] = i2;
	ptr[2] = i3;
	//context->draw_buffer.index_buffer_top += 3;
	//painter->current_command.vertex_end = context->draw_buffer.vertex_buffer_top;
	painter->current_command.index_end += 3;
}

inline static ColorU32 rgba(u8 r, u8 g, u8 b, u8 a)
{
	ColorU32 ret;
	ret.as_arr[0] = r;
	ret.as_arr[1] = g;
	ret.as_arr[2] = b;
	ret.as_arr[3] = a;
	return ret;
}

inline static void write_vertex(f32** ptr, v2 pos, v2 uv, ColorU32 color)
{
	f32* arr = *ptr;
	arr[0] = pos.x;
	arr[1] = pos.y;
	arr[2] = uv.x;
	arr[3] = uv.y;
	arr[4] = color.as_float;
	*ptr += 5;
}

ColorU32 color32_from_f32_color(Color c)
{
	return rgba(c.r * 255.f, c.g * 255.f, c.b * 255.f, c.a * 255.f);
}

void Painter::draw_rectangle(Context* context, v2 pos, v2 size, Color color, v2 uv1, v2 uv2)
{
	if (!has_vertex_space(context, this, 4) || !has_index_space(context, this, 2))
	{
		return;
	}

	ColorU32 color32 = color32_from_f32_color(color);
	u32 vx1 = push_vertex(context, this, pos, uv1, color32);
	u32 vx2 = push_vertex(context, this, pos + v2{size.x, 0.f}, {uv2.x, uv1.y}, color32);
	u32 vx3 = push_vertex(context, this, pos + v2{0.f, size.y}, {uv1.x, uv2.y}, color32);
	u32 vx4 = push_vertex(context, this, pos + size, uv2, color32);

	push_index_triangle(context, this, vx1, vx2, vx3);
	push_index_triangle(context, this, vx2, vx3, vx4);
}

void Painter::draw_rectangle(Context* context, v2 pos, v2 size, Color color)
{
	// 1.0 UV doesn't work
	v2 corner = {0.9999f, 0.9999f};
	draw_rectangle(context, pos, size, color, corner, corner);
}

void Painter::draw_rectangle_gradient(Context* context, v2 pos, v2 size, Color c1, Color c2, Color c3, Color c4)
{
	if (!has_vertex_space(context, this, 4) || !has_index_space(context, this, 2))
	{
		return;
	}

	v2 uv = {0.9999f, 0.9999f};
	ColorU32 color1 = color32_from_f32_color(c1);
	ColorU32 color2 = color32_from_f32_color(c2);
	ColorU32 color3 = color32_from_f32_color(c3);
	ColorU32 color4 = color32_from_f32_color(c4);
	u32 vx1 = push_vertex(context, this, pos, uv, color1);
	u32 vx2 = push_vertex(context, this, pos + v2{size.x, 0.f}, uv, color2);
	u32 vx3 = push_vertex(context, this, pos + v2{0.f, size.y}, uv, color3);
	u32 vx4 = push_vertex(context, this, pos + size, uv, color4);

	push_index_triangle(context, this, vx1, vx2, vx3);
	push_index_triangle(context, this, vx2, vx3, vx4);
}

void Painter::draw_rectangle_gradient(Context* context, Rect rect, Color c1, Color c2, Color c3, Color c4)
{
	draw_rectangle_gradient(context, rect.top_left, rect.size(), c1, c2, c3, c4);
}

void Painter::draw_rectangle(Context* context, Rect rect, Color color)
{
	// 1.0 UV doesn't work
	v2 corner = {0.9999f, 0.9999f};
	draw_rectangle(context, rect.top_left, rect.size(), color, corner, corner);
}

f32 Painter::draw_text(Context* context, Font* font, const char* text, v2 pos, f32 spacing, Color color)
{
	// Flooring the position to prevent weird rendering issues
	pos = v2{floorf(pos.x), floorf(pos.y)};
	f32 x_off = 0.f;

	usize len = strlen(text);
	for (usize i = 0; i < len; ++i)
	{
		Codepoint codepoint = text[i];
		const Glyph& glyph = font->get_glyph(codepoint);

		draw_rectangle(context, pos + v2{x_off, 0} + glyph.pos, glyph.size, color, glyph.uv1, glyph.uv2);
		//v2 test_uv = v2{0.9999f, 0.999f};
		//draw_rectangle(context, pos + v2{x_off, 0} + glyph.pos, glyph.size, color, test_uv, test_uv);

		//x_off += glyph.size.x + spacing;
		x_off += glyph.advance_x + spacing;
	}

	return x_off;
}

f32 Painter::draw_text(Context* context, Font* font, const char* text, usize text_length, v2 pos, f32 spacing, Color color)
{
	// Flooring the position to prevent weird rendering issues
	pos = v2{floorf(pos.x), floorf(pos.y)};
	f32 x_off = 0.f;

	for (usize i = 0; i < text_length; ++i)
	{
		Codepoint codepoint = text[i];
		LGUI_ASSERT(codepoint != 0, "End of string reached before provided text length");
		const Glyph& glyph = font->get_glyph(codepoint);

		draw_rectangle(context, pos + v2{x_off, 0} + glyph.pos, glyph.size, color, glyph.uv1, glyph.uv2);
		x_off += glyph.advance_x + spacing;
	}

	return x_off;
}

bool Painter::draw_text_fit(Context* context, Font* font, const char* text, Rect rect, f32 spacing, Color color, i8 h_align, i8 v_align)
{
	usize len = strlen(text);
	if (len == 0)
	{
		return true;
	}

	f32 width = font->text_width(text, len, spacing);
	if (width <= rect.width())
	{
		Rect text_rect = rect.align_size({width, font->height}, h_align, v_align);
		draw_text(context, font, text, text_rect.top_left, spacing, color);
		return true;
	}
	else
	{
		f32 dots_width = font->text_width("...", spacing);
		f32 target_width = rect.width() - dots_width;

		if (target_width <= 0.f)
		{
			// There is no space to even draw any dots
			return false;
		}

		usize fit_text_length = 0;
		f32 fit_text_width = 0;
		for (usize i = len - 1; i > 0; --i)
		{
			Codepoint codepoint = text[i - 1];
			if (codepoint == ' ')
			{
				continue;
			}

			f32 w = font->text_width(text, i, spacing);
			if (w <= target_width)
			{
				fit_text_length = i;
				fit_text_width = w;
				break;
			}
		}

		Rect text_rect = rect.align_size({fit_text_width + dots_width, font->height}, -1, v_align);
		draw_text(context, font, text, fit_text_length, text_rect.top_left, spacing, color);
		draw_text(context, font, "...", text_rect.top_left + v2{fit_text_width, 0}, spacing, color);

		return false;
	}
}

void Painter::begin_triangle_strip()
{
	LGUI_ASSERT(triangle_strip_counter == 0 && triangle_strip_mode == TriangleStripMode_None,
		"There is already a triangle strip");

	triangle_strip_mode = TriangleStripMode_Strip;
	triangle_strip_counter = 0;
	triangle_strip_indices[0] = 0;
	triangle_strip_indices[1] = 0;
}

void Painter::end_triangle_strip()
{
	LGUI_ASSERT(triangle_strip_mode == TriangleStripMode_Strip,
		"There is already a triangle strip");

	triangle_strip_mode = TriangleStripMode_None;
	triangle_strip_counter = 0;
	triangle_strip_indices[0] = 0;
	triangle_strip_indices[1] = 0;
}

void Painter::add_strip_triangle(Context* context, v2 pos, Color color, v2 uv)
{
	if (!has_vertex_space(context, this, 1) || !has_index_space(context, this, 1))
	{
		return;
	}

	LGUI_ASSERT(triangle_strip_mode != TriangleStripMode_None, "We are not in a triangle strip");

	u16 index = push_vertex(context, this, pos, uv, color32_from_f32_color(color));

	if (triangle_strip_counter >= 2)
	{
		push_index_triangle(context, this, triangle_strip_indices[0], triangle_strip_indices[1], index);
	}
	else
	{
		++triangle_strip_counter;
	}

	if (triangle_strip_mode == TriangleStripMode_Strip)
	{
		triangle_strip_indices[0] = triangle_strip_indices[1];
		triangle_strip_indices[1] = index;
	}
	else // Convex
	{
		if (triangle_strip_counter == 1)
		{
			triangle_strip_indices[0] = index;
		}
		else
		{
			triangle_strip_indices[1] = index;
		}
	}
}

void Painter::add_strip_triangle(Context* context, v2 pos, Color color)
{
	add_strip_triangle(context, pos, color, {0.9999f, 0.9999f});
}

void Painter::begin_convex_strip()
{
	LGUI_ASSERT(triangle_strip_counter == 0 && triangle_strip_mode == TriangleStripMode_None,
		"There is already a triangle strip");

	triangle_strip_mode = TriangleStripMode_Convex;
	triangle_strip_counter = 0;
	triangle_strip_indices[0] = 0;
	triangle_strip_indices[1] = 0;
}

void Painter::end_convex_strip()
{
	LGUI_ASSERT(triangle_strip_mode == TriangleStripMode_Convex,
		"There is already a triangle strip");

	triangle_strip_mode = TriangleStripMode_None;
	triangle_strip_counter = 0;
	triangle_strip_indices[0] = 0;
	triangle_strip_indices[1] = 0;
}

void Painter::draw_circle(Context* context, v2 pos, f32 size, f32 t, Color color)
{
	begin_convex_strip();

	add_strip_triangle(context, pos, color);

	for (f32 f = 0.f; f < t; f += 0.05f)
	{
		v2 rot = {sinf(f * 2.f * PI), cosf(f * 2.f * PI)};
		add_strip_triangle(context, pos + rot * size, color);
	}

	v2 rot = {sinf(t * 2.f * PI), cosf(t * 2.f * PI)};
	add_strip_triangle(context, pos + rot * size, color);

	end_convex_strip();
}

void Painter::draw_round_corner(Context* context, v2 pos, v2 size, bool is_right, bool is_bottom, Color color)
{
	begin_convex_strip();

	add_strip_triangle(context, pos, color);

	f32 detail = LGUI_MAX(size.x, size.y) / 3.f;
	detail = LGUI_CLAMP(3.f, 15.f, detail);
	detail = 1.f / (floorf(detail));

	v2 mul = {
		is_right ? 1.f : -1.f,
		is_bottom ? 1.f : -1.f
	};
	v2 move = {
		is_right ? 0.f : -1.f,
		is_bottom ? 0.f : -1.f
	};

	for (f32 f = 0.f; f < 1.f; f += detail)
	{
		v2 rot = (v2{sinf(f * 0.5f * PI), cosf(f * 0.5f * PI)} * mul + move);
		add_strip_triangle(context, pos + rot * size, color);
	}

	v2 rot = (v2{sinf(0.5f * PI), cosf(0.5f * PI)} * mul + move);
	add_strip_triangle(context, pos + rot * size, color);

	end_convex_strip();
}

void Painter::draw_rounded_rectangle(Context* context, v2 pos, v2 size, f32 corner_size[4], Color color)
{
	f32 top_side_scale = (corner_size[0] + corner_size[1]) > 0.f ? LGUI_MIN(size.x / (corner_size[0] + corner_size[1]), 1.f) : 0.f;
	f32 bottom_side_scale = (corner_size[2] + corner_size[3]) > 0.f ? LGUI_MIN(size.x / (corner_size[2] + corner_size[3]), 1.f) : 0.f;
	f32 left_side_scale = (corner_size[0] + corner_size[2]) > 0.f ? LGUI_MIN(size.x / (corner_size[0] + corner_size[2]), 1.f) : 0.f;
	f32 right_side_scale = (corner_size[1] + corner_size[3]) > 0.f ? LGUI_MIN(size.x / (corner_size[1] + corner_size[3]), 1.f) : 0.f;

	f32 top_left = corner_size[0] * LGUI_MIN(top_side_scale, left_side_scale);
	f32 top_right = corner_size[1] * LGUI_MIN(top_side_scale, right_side_scale);
	f32 bottom_left = corner_size[2] * LGUI_MIN(bottom_side_scale, left_side_scale);
	f32 bottom_right = corner_size[3] * LGUI_MIN(bottom_side_scale, right_side_scale);

	bool top_side_flat = corner_size[0] == 0.f && corner_size[1] == 0.f;
	bool bottom_side_flat = corner_size[2] == 0.f && corner_size[3] == 0.f;
	bool left_side_flat = corner_size[0] == 0.f && corner_size[2] == 0.f;
	bool right_side_flat = corner_size[1] == 0.f && corner_size[3] == 0.f;


	if (top_side_flat && bottom_side_flat)
	{
		draw_rectangle(context, pos, size, color);
	}

	v2 top_left_v = pos;
	v2 top_right_v = pos + v2{size.x - top_right, 0.f};
	v2 bottom_left_v = pos + v2{0.f, size.y - bottom_left};
	v2 bottom_right_v = pos + size - v2{bottom_left, bottom_left};

	if (top_left)
	{
		draw_round_corner(context, top_left_v, {top_left, top_left}, false, false, color);
	}
	if (top_right)
	{
		draw_round_corner(context, top_right_v, {top_right, top_right}, true, false, color);
	}
	if (bottom_left)
	{
		draw_round_corner(context, bottom_left_v, {bottom_left, bottom_left}, false, true, color);
	}
	if (bottom_right)
	{
		draw_round_corner(context, bottom_right_v, { bottom_left, bottom_left }, true, true, color);
	}
}

/*void Painter::draw_rounded_rectangle(Context* context, Rect rect, Color color)
{

}*/

// TODO: remove raylib
void rl_render(Context* context)
{
	DrawBuffer& draw_buffer = context->draw_buffer;

	rlDisableBackfaceCulling();

	for (Panel* panel = context->first_depth_panel; panel; panel = panel->order_next)
	{
		for (DrawCommand* command = panel->painter.first_command; command; command = command->next)
		{
			v2 clip_pos = command->clip_rect.top_left;
			v2 clip_size = command->clip_rect.size();
			BeginScissorMode(clip_pos.x, clip_pos.y, clip_size.x, clip_size.y);

			// Has to be done after the scisor mode
			rlBegin(RL_TRIANGLES);

			// TODO: custom textures
			rlEnableTexture(context->atlas.texture_id);
			rlSetTexture(context->atlas.texture_id);

			for (u32 i = command->index_start; i < command->index_end; i += 3)
			{
				u16 i1 = draw_buffer.index_buffer[i + 0];
				u16 i2 = draw_buffer.index_buffer[i + 1];
				u16 i3 = draw_buffer.index_buffer[i + 2];

				f32* vx1 = draw_buffer.vertex_buffer + i1 * VERTEX_SIZE_FLOATS;
				f32* vx2 = draw_buffer.vertex_buffer + i2 * VERTEX_SIZE_FLOATS;
				f32* vx3 = draw_buffer.vertex_buffer + i3 * VERTEX_SIZE_FLOATS;

				rlTexCoord2f(vx1[2], vx1[3]);
				ColorU32 c1; c1.as_float = vx1[4];
				rlColor4ub(c1.as_arr[0], c1.as_arr[1], c1.as_arr[2], c1.as_arr[3]);
				rlVertex2f(vx1[0], vx1[1]);

				rlTexCoord2f(vx2[2], vx2[3]);
				ColorU32 c2; c2.as_float = vx2[4];
				rlColor4ub(c2.as_arr[0], c2.as_arr[1], c2.as_arr[2], c2.as_arr[3]);
				rlVertex2f(vx2[0], vx2[1]);

				rlTexCoord2f(vx3[2], vx3[3]);
				ColorU32 c3; c3.as_float = vx3[4];
				rlColor4ub(c3.as_arr[0], c3.as_arr[1], c3.as_arr[2], c3.as_arr[3]);
				rlVertex2f(vx3[0], vx3[1]);
			}

			rlEnd();

			EndScissorMode();
		}
	}
}

}
