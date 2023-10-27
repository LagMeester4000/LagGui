#include "basic.hpp"
#include "lag_gui.hpp"
#include "raylib.h"
#include "rlgl.h"
#include <math.h>

namespace lgui {

const usize VERTEX_SIZE_BYTES = sizeof(f32) * 4 + sizeof(u32);
const usize VERTEX_SIZE_FLOATS = 5;

void Painter::_push_command()
{
	Context* context = get_context();

	// Only push the command if it has any vertices
	if (current_command->vertex_start == current_command->vertex_end ||
		current_command->index_start == current_command->index_end)
	{
		return;
	}

	// Allocate command
	DrawCommand* command = current_command;
	command->prev = last_command;

	// Update global buffer index
	LGUI_ASSERT(context->draw_buffer.vertex_buffer_top == current_command->vertex_start,
		"Draw buffer has been written to before this command was completed");
	LGUI_ASSERT(context->draw_buffer.index_buffer_top == current_command->index_start,
		"Draw buffer has been written to before this command was completed");
	context->draw_buffer.vertex_buffer_top = current_command->vertex_end;
	context->draw_buffer.index_buffer_top = current_command->index_end;

	// Reset current command
	current_command = context->temp_arena->allocate_one<DrawCommand>();
	command->next = current_command;
	current_command->prev = command;
	current_command->clip_rect = command->clip_rect;
	current_command->is_layout = command->is_layout;
	current_command->layout_depth = command->layout_depth;
	current_command->vertex_start = context->draw_buffer.vertex_buffer_top;
	current_command->vertex_end = current_command->vertex_start;
	current_command->index_start = context->draw_buffer.index_buffer_top;
	current_command->index_end = current_command->index_start;
	current_command->texture_id = command->texture_id;

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

void Painter::_start_painter()
{
	Context* context = get_context();

	if (frame_last_updated != context->current_frame)
	{
		frame_last_updated = context->current_frame;

		first_command = nullptr;
		last_command = nullptr;

		current_command = context->temp_arena->allocate_one<DrawCommand>();
		current_command->clip_rect = get_clip_rect();
		current_command->vertex_start = context->draw_buffer.vertex_buffer_top;
		current_command->vertex_end = current_command->vertex_start;
		current_command->index_start = context->draw_buffer.index_buffer_top;
		current_command->index_end = current_command->index_start;
	}
	else
	{
		_push_command();
	}
}

void Painter::_restart_painter()
{
	Context* context = get_context();

	DrawCommand* new_command = context->temp_arena->allocate_one<DrawCommand>();
	new_command->clip_rect = get_clip_rect();
	new_command->vertex_start = context->draw_buffer.vertex_buffer_top;
	new_command->vertex_end = current_command->vertex_start;
	new_command->index_start = context->draw_buffer.index_buffer_top;
	new_command->index_end = current_command->index_start;
	current_command = new_command;
}

static void _move_draw_command_vertices(DrawCommand* it, usize vertex_start, usize vertex_end, v2 movement)
{
	DrawBuffer* buffer = &get_context()->draw_buffer;

	for (usize i = vertex_start; i < vertex_end; i += VERTEX_SIZE_FLOATS)
	{
		buffer->vertex_buffer[i + 0] += movement.x;
		buffer->vertex_buffer[i + 1] += movement.y;
	}
}

static void _internal_adjust_clip_rect_in_range(DrawCommand* command, v2 movement, bool replace, u32 layout_depth, Rect new_clip)
{
	if (replace && command->is_layout && command->layout_depth == layout_depth)
	{
		command->clip_rect = new_clip;
	}
	else if (command->layout_depth >= layout_depth)
	{
		command->clip_rect.move(movement);
		if (replace)
		{
			command->clip_rect = command->clip_rect.clip(new_clip);
		}
	}
}

void Painter::push_clip_rect(Rect rect)
{
	LGUI_ASSERT(clip_rect_stack_top < MAX_CLIP_RECT, "Out of bounds");

	_push_command();

	Rect clip = get_clip_rect();
	Rect new_rect = clip.clip(rect);

	clip_rect_stack[clip_rect_stack_top] = new_rect;
	++clip_rect_stack_top;

	current_command->clip_rect = new_rect;
}

Rect Painter::get_clip_rect()
{
	return clip_rect_stack_top > 0 ? 
		clip_rect_stack[clip_rect_stack_top - 1] : 
		Rect{{0, 0}, get_context()->app_window_size};
}

void Painter::pop_clip_rect()
{
	LGUI_ASSERT(clip_rect_stack_top > 0, "Out of bounds");

	_push_command();

	--clip_rect_stack_top;

	current_command->clip_rect = get_clip_rect();
}

struct ColorU32 {
	union {
		u32 as_int;
		u8 as_arr[4];
		f32 as_float; // To put it in the buffer
	};
};

// Has enough space for count amount of vertices
inline static bool has_vertex_space(Painter* painter, usize vert_count)
{
	Context* context = get_context();

	return painter->current_command->vertex_end + vert_count * VERTEX_SIZE_FLOATS <
		context->draw_buffer.vertex_buffer_length;
}

// Has enough space for count amount of vertices
inline static bool has_index_space(Painter* painter, usize tri_count)
{
	Context* context = get_context();

	return painter->current_command->index_end + tri_count < context->draw_buffer.index_buffer_length;
}

// Returns index
inline static DrawIndex push_vertex(Painter* painter, v2 pos, v2 uv, ColorU32 color)
{
	Context* context = get_context();

	usize index = painter->current_command->vertex_end;
	LGUI_ASSERT(index % VERTEX_SIZE_FLOATS == 0, "vertex buffer has incorrect number of floats");
	f32* ptr = context->draw_buffer.vertex_buffer + index;
	ptr[0] = pos.x;
	ptr[1] = pos.y;
	ptr[2] = uv.x;
	ptr[3] = uv.y;
	ptr[4] = color.as_float;
	painter->current_command->vertex_end += VERTEX_SIZE_FLOATS;
	auto ret = index / VERTEX_SIZE_FLOATS;
	// This shouldn't actually happen if the API is used properly, because we check for space before adding vertices
	LGUI_ASSERT(ret < DRAW_INDEX_MAX, "The returned index is higher than the possible amount of vertices, change the draw index type");
	return (DrawIndex)ret;
}

inline static void write_vertex(usize* vertex_index, v2 pos, v2 uv, ColorU32 color)
{
	Context* context = get_context();

	usize index = *vertex_index;
	LGUI_ASSERT(index % VERTEX_SIZE_FLOATS == 0, "vertex buffer has incorrect number of floats");
	f32* ptr = context->draw_buffer.vertex_buffer + index;
	ptr[0] = pos.x;
	ptr[1] = pos.y;
	ptr[2] = uv.x;
	ptr[3] = uv.y;
	ptr[4] = color.as_float;
	*vertex_index += VERTEX_SIZE_FLOATS;
}

inline static void push_index_triangle(Painter* painter, DrawIndex i1, DrawIndex i2, DrawIndex i3)
{
	Context* context = get_context();

	DrawIndex* ptr = context->draw_buffer.index_buffer + painter->current_command->index_end;
	ptr[0] = i1;
	ptr[1] = i2;
	ptr[2] = i3;
	painter->current_command->index_end += 3;
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

FORCE_INLINE
ColorU32 color32_from_f32_color(Color c)
{
	return rgba((u8)(c.r * 255.f), (u8)(c.g * 255.f), (u8)(c.b * 255.f), (u8)(c.a * 255.f));
}

FORCE_INLINE
inline static void draw_rect_fast(f32* vertex_ptr, usize vertex_off, usize first_vertex_index, 
	DrawIndex* index_ptr, usize index_off, v2 pos, v2 size, ColorU32 color32, v2 uv1, v2 uv2)
{
	//ColorU32 color32 = color32_from_f32_color(color);

	//LGUI_ASSERT(vertex_index % VERTEX_SIZE_FLOATS == 0, "vertex buffer has incorrect number of floats");

	vertex_ptr[vertex_off + 0 * VERTEX_SIZE_FLOATS + 0] = pos.x;
	vertex_ptr[vertex_off + 0 * VERTEX_SIZE_FLOATS + 1] = pos.y;
	vertex_ptr[vertex_off + 0 * VERTEX_SIZE_FLOATS + 2] = uv1.x;
	vertex_ptr[vertex_off + 0 * VERTEX_SIZE_FLOATS + 3] = uv1.y;
	vertex_ptr[vertex_off + 0 * VERTEX_SIZE_FLOATS + 4] = color32.as_float;

	vertex_ptr[vertex_off + 1 * VERTEX_SIZE_FLOATS + 0] = pos.x + size.x;
	vertex_ptr[vertex_off + 1 * VERTEX_SIZE_FLOATS + 1] = pos.y;
	vertex_ptr[vertex_off + 1 * VERTEX_SIZE_FLOATS + 2] = uv2.x;
	vertex_ptr[vertex_off + 1 * VERTEX_SIZE_FLOATS + 3] = uv1.y;
	vertex_ptr[vertex_off + 1 * VERTEX_SIZE_FLOATS + 4] = color32.as_float;

	vertex_ptr[vertex_off + 2 * VERTEX_SIZE_FLOATS + 0] = pos.x;
	vertex_ptr[vertex_off + 2 * VERTEX_SIZE_FLOATS + 1] = pos.y + size.y;
	vertex_ptr[vertex_off + 2 * VERTEX_SIZE_FLOATS + 2] = uv1.x;
	vertex_ptr[vertex_off + 2 * VERTEX_SIZE_FLOATS + 3] = uv2.y;
	vertex_ptr[vertex_off + 2 * VERTEX_SIZE_FLOATS + 4] = color32.as_float;

	vertex_ptr[vertex_off + 3 * VERTEX_SIZE_FLOATS + 0] = pos.x + size.x;
	vertex_ptr[vertex_off + 3 * VERTEX_SIZE_FLOATS + 1] = pos.y + size.y;
	vertex_ptr[vertex_off + 3 * VERTEX_SIZE_FLOATS + 2] = uv2.x;
	vertex_ptr[vertex_off + 3 * VERTEX_SIZE_FLOATS + 3] = uv2.y;
	vertex_ptr[vertex_off + 3 * VERTEX_SIZE_FLOATS + 4] = color32.as_float;

	index_ptr[index_off + 0] = (DrawIndex)first_vertex_index;
	index_ptr[index_off + 1] = (DrawIndex)first_vertex_index + 1;
	index_ptr[index_off + 2] = (DrawIndex)first_vertex_index + 2;

	index_ptr[index_off + 3] = (DrawIndex)first_vertex_index + 1;
	index_ptr[index_off + 4] = (DrawIndex)first_vertex_index + 2;
	index_ptr[index_off + 5] = (DrawIndex)first_vertex_index + 3;
}

void Painter::draw_rectangle(v2 pos, v2 size, Color color, v2 uv1, v2 uv2)
{
	/* TEMP
	if (!has_vertex_space(this, 4) || !has_index_space(this, 2))
	{
		return;
	}
	*/

	/*
	ColorU32 color32 = color32_from_f32_color(color);
	DrawIndex vx1 = push_vertex(this, pos, uv1, color32);
	DrawIndex vx2 = push_vertex(this, pos + v2{size.x, 0.f}, {uv2.x, uv1.y}, color32);
	DrawIndex vx3 = push_vertex(this, pos + v2{0.f, size.y}, {uv1.x, uv2.y}, color32);
	DrawIndex vx4 = push_vertex(this, pos + size, uv2, color32);

	push_index_triangle(this, vx1, vx2, vx3);
	push_index_triangle(this, vx2, vx3, vx4);
	*/

	// Optimised path (manual inlining)

	Context* context = get_context();

	//ColorU32 color32 = color32_from_f32_color(color);
	ColorU32 color32;// = rgba((u8)(c.r * 255.f), (u8)(c.g * 255.f), (u8)(c.b * 255.f), (u8)(c.a * 255.f));
	color32.as_arr[0] = (u8)(color.r * 255.f);
	color32.as_arr[1] = (u8)(color.g * 255.f);
	color32.as_arr[2] = (u8)(color.b * 255.f);
	color32.as_arr[3] = (u8)(color.a * 255.f);

	usize vertex_index = current_command->vertex_end;
	//LGUI_ASSERT(vertex_index % VERTEX_SIZE_FLOATS == 0, "vertex buffer has incorrect number of floats");

	f32* vertex_ptr = context->draw_buffer.vertex_buffer + vertex_index;

	vertex_ptr[0 * VERTEX_SIZE_FLOATS + 0] = pos.x;
	vertex_ptr[0 * VERTEX_SIZE_FLOATS + 1] = pos.y;
	vertex_ptr[0 * VERTEX_SIZE_FLOATS + 2] = uv1.x;
	vertex_ptr[0 * VERTEX_SIZE_FLOATS + 3] = uv1.y;
	vertex_ptr[0 * VERTEX_SIZE_FLOATS + 4] = color32.as_float;

	vertex_ptr[1 * VERTEX_SIZE_FLOATS + 0] = pos.x + size.x;
	vertex_ptr[1 * VERTEX_SIZE_FLOATS + 1] = pos.y;
	vertex_ptr[1 * VERTEX_SIZE_FLOATS + 2] = uv2.x;
	vertex_ptr[1 * VERTEX_SIZE_FLOATS + 3] = uv1.y;
	vertex_ptr[1 * VERTEX_SIZE_FLOATS + 4] = color32.as_float;

	vertex_ptr[2 * VERTEX_SIZE_FLOATS + 0] = pos.x;
	vertex_ptr[2 * VERTEX_SIZE_FLOATS + 1] = pos.y + size.y;
	vertex_ptr[2 * VERTEX_SIZE_FLOATS + 2] = uv1.x;
	vertex_ptr[2 * VERTEX_SIZE_FLOATS + 3] = uv2.y;
	vertex_ptr[2 * VERTEX_SIZE_FLOATS + 4] = color32.as_float;

	vertex_ptr[3 * VERTEX_SIZE_FLOATS + 0] = pos.x + size.x;
	vertex_ptr[3 * VERTEX_SIZE_FLOATS + 1] = pos.y + size.y;
	vertex_ptr[3 * VERTEX_SIZE_FLOATS + 2] = uv2.x;
	vertex_ptr[3 * VERTEX_SIZE_FLOATS + 3] = uv2.y;
	vertex_ptr[3 * VERTEX_SIZE_FLOATS + 4] = color32.as_float;

	current_command->vertex_end += VERTEX_SIZE_FLOATS * 4;
	auto first_vertex_index = vertex_index / VERTEX_SIZE_FLOATS;

	// This shouldn't actually happen if the API is used properly, because we check for space before adding vertices
	LGUI_ASSERT(first_vertex_index + 3 < DRAW_INDEX_MAX, "The returned index is higher than the possible amount of vertices, change the draw index type");

	DrawIndex* index_ptr = context->draw_buffer.index_buffer + current_command->index_end;

	index_ptr[0] = (DrawIndex)first_vertex_index;
	index_ptr[1] = (DrawIndex)first_vertex_index + 1;
	index_ptr[2] = (DrawIndex)first_vertex_index + 2;

	index_ptr[3] = (DrawIndex)first_vertex_index + 1;
	index_ptr[4] = (DrawIndex)first_vertex_index + 2;
	index_ptr[5] = (DrawIndex)first_vertex_index + 3;

	current_command->index_end += 6;
}

void Painter::draw_rectangle(v2 pos, v2 size, Color color)
{
	// 1.0 UV doesn't work
	v2 corner = {0.9999f, 0.9999f};
	draw_rectangle(pos, size, color, corner, corner);
}

void Painter::draw_rectangle_gradient(v2 pos, v2 size, Color c1, Color c2, Color c3, Color c4)
{
	if (!has_vertex_space(this, 4) || !has_index_space(this, 2))
	{
		return;
	}

	v2 uv = {0.9999f, 0.9999f};
	ColorU32 color1 = color32_from_f32_color(c1);
	ColorU32 color2 = color32_from_f32_color(c2);
	ColorU32 color3 = color32_from_f32_color(c3);
	ColorU32 color4 = color32_from_f32_color(c4);
	u32 vx1 = push_vertex(this, pos, uv, color1);
	u32 vx2 = push_vertex(this, pos + v2{size.x, 0.f}, uv, color2);
	u32 vx3 = push_vertex(this, pos + v2{0.f, size.y}, uv, color3);
	u32 vx4 = push_vertex(this, pos + size, uv, color4);

	push_index_triangle(this, vx1, vx2, vx3);
	push_index_triangle(this, vx2, vx3, vx4);
}

void Painter::draw_rectangle_gradient(Rect rect, Color c1, Color c2, Color c3, Color c4)
{
	draw_rectangle_gradient(rect.top_left, rect.size(), c1, c2, c3, c4);
}

void Painter::draw_rectangle_outline(v2 pos, v2 size, f32 thickness, Color color)
{
	draw_rectangle(pos, {size.x, thickness}, color);
	draw_rectangle({pos.x, pos.y + size.y - thickness}, {size.x, thickness}, color);
	draw_rectangle({pos.x, pos.y + thickness}, {thickness, size.y - thickness * 2.f}, color);
	draw_rectangle({pos.x + size.x - thickness, pos.y + thickness}, {thickness, size.y - thickness * 2.f}, color);
}

void Painter::draw_rectangle_outline(Rect rect, f32 thickness, Color color)
{
	draw_rectangle_outline(rect.top_left, rect.size(), thickness, color);
}

void Painter::draw_rectangle(Rect rect, Color color)
{
	// 1.0 UV doesn't work
	v2 corner = {0.9999f, 0.9999f};
	draw_rectangle(rect.top_left, rect.size(), color, corner, corner);
}

f32 Painter::draw_text(Font* font, const char* text, v2 pos, f32 spacing, Color color)
{
	/*
	// Flooring the position to prevent weird rendering issues
	pos = v2{floorf(pos.x), floorf(pos.y)};
	f32 x_off = 0.f;

	usize len = strlen(text);
	for (usize i = 0; i < len; ++i)
	{
		Codepoint codepoint = text[i];
		const Glyph& glyph = font->get_glyph(codepoint);

		draw_rectangle(pos + v2{x_off, 0} + glyph.pos, glyph.size, color, glyph.uv1, glyph.uv2);
		//v2 test_uv = v2{0.9999f, 0.999f};
		//draw_rectangle(context, pos + v2{x_off, 0} + glyph.pos, glyph.size, color, test_uv, test_uv);

		//x_off += glyph.size.x + spacing;
		x_off += glyph.advance_x + spacing;
	}

	return x_off;
	*/



	// Flooring the position to prevent weird rendering issues
	pos = v2{floorf(pos.x), floorf(pos.y)};
	f32 x_off = 0.f;
	usize len = strlen(text);



	Context* context = get_context();

	ColorU32 color32;// = rgba((u8)(c.r * 255.f), (u8)(c.g * 255.f), (u8)(c.b * 255.f), (u8)(c.a * 255.f));
	color32.as_arr[0] = (u8)(color.r * 255.f);
	color32.as_arr[1] = (u8)(color.g * 255.f);
	color32.as_arr[2] = (u8)(color.b * 255.f);
	color32.as_arr[3] = (u8)(color.a * 255.f);

	usize vertex_index = current_command->vertex_end;
	//LGUI_ASSERT(vertex_index % VERTEX_SIZE_FLOATS == 0, "vertex buffer has incorrect number of floats");

	f32* vertex_ptr = context->draw_buffer.vertex_buffer + vertex_index;

	current_command->vertex_end += VERTEX_SIZE_FLOATS * 4 * len;
	usize first_vertex_index = vertex_index / VERTEX_SIZE_FLOATS;

	// This shouldn't actually happen if the API is used properly, because we check for space before adding vertices
	LGUI_ASSERT(first_vertex_index + 3 < DRAW_INDEX_MAX, "The returned index is higher than the possible amount of vertices, change the draw index type");

	DrawIndex* index_ptr = context->draw_buffer.index_buffer + current_command->index_end;

	current_command->index_end += 6 * len;



	for (usize i = 0; i < len; ++i)
	{
		Codepoint codepoint = text[i];
		const Glyph& glyph = font->get_glyph(codepoint);

		//draw_rectangle(pos + v2{x_off, 0} + glyph.pos, glyph.size, color, glyph.uv1, glyph.uv2);
		draw_rect_fast(
			vertex_ptr, i * VERTEX_SIZE_FLOATS * 4, first_vertex_index + i * 4, 
			index_ptr, i * 6, 
			pos + v2{x_off, 0} + glyph.pos, glyph.size, color32, glyph.uv1, glyph.uv2
		);

		x_off += glyph.advance_x + spacing;
	}

	return x_off;
}

f32 Painter::draw_text(Font* font, const char* text, usize text_length, v2 pos, f32 spacing, Color color)
{
	/*
	// Flooring the position to prevent weird rendering issues
	pos = v2{floorf(pos.x), floorf(pos.y)};
	f32 x_off = 0.f;

	for (usize i = 0; i < text_length; ++i)
	{
		Codepoint codepoint = text[i];
		LGUI_ASSERT(codepoint != 0, "End of string reached before provided text length");
		const Glyph& glyph = font->get_glyph(codepoint);

		draw_rectangle(pos + v2{x_off, 0} + glyph.pos, glyph.size, color, glyph.uv1, glyph.uv2);
		x_off += glyph.advance_x + spacing;
	}

	return x_off;
	*/



	// Flooring the position to prevent weird rendering issues
	pos = v2{floorf(pos.x), floorf(pos.y)};
	f32 x_off = 0.f;
	const usize len = text_length;



	Context* context = get_context();

	ColorU32 color32;// = rgba((u8)(c.r * 255.f), (u8)(c.g * 255.f), (u8)(c.b * 255.f), (u8)(c.a * 255.f));
	color32.as_arr[0] = (u8)(color.r * 255.f);
	color32.as_arr[1] = (u8)(color.g * 255.f);
	color32.as_arr[2] = (u8)(color.b * 255.f);
	color32.as_arr[3] = (u8)(color.a * 255.f);

	usize vertex_index = current_command->vertex_end;
	//LGUI_ASSERT(vertex_index % VERTEX_SIZE_FLOATS == 0, "vertex buffer has incorrect number of floats");

	f32* vertex_ptr = context->draw_buffer.vertex_buffer + vertex_index;

	current_command->vertex_end += VERTEX_SIZE_FLOATS * 4 * len;
	usize first_vertex_index = vertex_index / VERTEX_SIZE_FLOATS;

	// This shouldn't actually happen if the API is used properly, because we check for space before adding vertices
	LGUI_ASSERT(first_vertex_index + 3 < DRAW_INDEX_MAX, "The returned index is higher than the possible amount of vertices, change the draw index type");

	DrawIndex* index_ptr = context->draw_buffer.index_buffer + current_command->index_end;

	current_command->index_end += 6 * len;



	for (usize i = 0; i < len; ++i)
	{
		Codepoint codepoint = text[i];
		const Glyph& glyph = font->get_glyph(codepoint);

		//draw_rectangle(pos + v2{x_off, 0} + glyph.pos, glyph.size, color, glyph.uv1, glyph.uv2);
		draw_rect_fast(
			vertex_ptr, i * VERTEX_SIZE_FLOATS * 4, first_vertex_index + i * 4, 
			index_ptr, i * 6, 
			pos + v2{x_off, 0} + glyph.pos, glyph.size, color32, glyph.uv1, glyph.uv2
		);

		x_off += glyph.advance_x + spacing;
	}

	return x_off;
}

bool Painter::draw_text_fit(Font* font, const char* text, Rect rect, f32 spacing, Color color, i8 h_align, i8 v_align)
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
		draw_text(font, text, text_rect.top_left, spacing, color);
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
		draw_text(font, text, fit_text_length, text_rect.top_left, spacing, color);
		draw_text(font, "...", text_rect.top_left + v2{fit_text_width, 0}, spacing, color);

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

void Painter::add_strip_triangle(v2 pos, Color color, v2 uv)
{
	if (!has_vertex_space(this, 1) || !has_index_space(this, 1))
	{
		return;
	}

	LGUI_ASSERT(triangle_strip_mode != TriangleStripMode_None, "We are not in a triangle strip");

	DrawIndex index = push_vertex(this, pos, uv, color32_from_f32_color(color));

	if (triangle_strip_counter >= 2)
	{
		push_index_triangle(this, triangle_strip_indices[0], triangle_strip_indices[1], index);
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

void Painter::add_strip_triangle(v2 pos, Color color)
{
	add_strip_triangle(pos, color, {0.9999f, 0.9999f});
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

void Painter::draw_circle(v2 pos, f32 size, f32 t, Color color)
{
	begin_convex_strip();

	add_strip_triangle(pos, color);

	for (f32 f = 0.f; f < t; f += 0.05f)
	{
		v2 rot = {sinf(f * 2.f * PI), cosf(f * 2.f * PI)};
		add_strip_triangle(pos + rot * size, color);
	}

	v2 rot = {sinf(t * 2.f * PI), cosf(t * 2.f * PI)};
	add_strip_triangle(pos + rot * size, color);

	end_convex_strip();
}

void Painter::draw_round_corner(v2 pos, v2 size, bool is_right, bool is_bottom, Color color, bool start_end_strip, bool reverse)
{
	if (start_end_strip)
	{
		begin_convex_strip();
		// Don't add the center point if it doesn't own the strip
		add_strip_triangle(pos, color);
	}

	f32 detail = LGUI_MAX(size.x, size.y) / 3.f;
	detail = LGUI_CLAMP(3.f, 15.f, detail);
	detail = 1.f / (floorf(detail));

	v2 mul = {
		is_right ? 1.f : -1.f,
		is_bottom ? 1.f : -1.f
	};
	v2 move = {
		is_right ? -1.f : 1.f,
		is_bottom ? -1.f : 1.f
	};

	if (reverse)
	{
		for (f32 f = 1.f; f > 0.f; f -= detail)
		{
			v2 rot = (v2{sinf(f * 0.5f * PI), cosf(f * 0.5f * PI)} * mul + move);
			add_strip_triangle(pos + rot * size, color);
		}

		v2 rot = (v2{sinf(0.f), cosf(0.f)} * mul + move);
		add_strip_triangle(pos + rot * size, color);
	}
	else
	{
		for (f32 f = 0.f; f < 1.f; f += detail)
		{
			v2 rot = (v2{sinf(f * 0.5f * PI), cosf(f * 0.5f * PI)} * mul + move);
			add_strip_triangle(pos + rot * size, color);
		}

		v2 rot = (v2{sinf(0.5f * PI), cosf(0.5f * PI)} * mul + move);
		add_strip_triangle(pos + rot * size, color);
	}

	if (start_end_strip) end_convex_strip();
}

void Painter::draw_rounded_rectangle(v2 pos, v2 size, f32 corner_size[4], Color color)
{
	begin_convex_strip();

	//v2 center = pos + size / 2.f;
	//add_strip_triangle(center, color);

	v2 corner0 = {corner_size[0], corner_size[0]};
	//draw_round_corner(pos + corner0, corner0, false, false, color, false, true);
	draw_round_corner(pos, corner0, false, false, color, false, true);

	v2 corner1 = {corner_size[1], corner_size[1]};
	//draw_round_corner(pos + v2{size.x - corner1.x, corner1.x}, corner1, true, false, color, false);
	draw_round_corner(pos + v2{size.x, 0.f}, corner1, true, false, color, false);

	v2 corner3 = {corner_size[3], corner_size[3]};
	//draw_round_corner(pos + size - corner3, corner3, true, true, color, false, true);
	draw_round_corner(pos + size, corner3, true, true, color, false, true);

	v2 corner2 = {corner_size[2], corner_size[2]};
	//draw_round_corner(pos + v2{corner2.x, size.y - corner2.x}, corner2, false, true, color, false);
	draw_round_corner(pos + v2{0.f, size.y}, corner2, false, true, color, false);

	end_convex_strip();
}

void Painter::draw_rounded_rectangle(Rect rect, f32 corner_size[4], Color color)
{
	draw_rounded_rectangle(rect.top_left, rect.size(), corner_size, color);
}

// TODO: remove raylib
void rl_render()
{
	Context* context = get_context();

	DrawBuffer& draw_buffer = context->draw_buffer;

	static bool initialized = false;
	static u32 vertex_buffer = 0;
	static u32 index_buffer = 0;
	static u32 vao = 0;
	static u32 shader = 0;
	static u32 uniform_screen_size = 0;
	if (!initialized)
	{
		initialized = true;
		
		vertex_buffer = rlLoadVertexBuffer(draw_buffer.vertex_buffer, draw_buffer.vertex_buffer_length * sizeof(f32), true);
		index_buffer = rlLoadVertexBufferElement(draw_buffer.index_buffer, draw_buffer.index_buffer_length * sizeof(DrawIndex), true);

		const char* vertex_shader_source = 
			"#version 330 core\n"
			"layout (location = 0) in vec2 a_pos;\n"
			"layout (location = 1) in vec2 a_uv;\n"
			"layout (location = 2) in vec4 a_color;\n"
			""
			"out vec2 f_uv;"
			"out vec4 f_color;"
			""
			"uniform vec2 u_screen_size;"
			""
			"void main()\n"
			"{\n"
			"   gl_Position = vec4(a_pos.x / u_screen_size.x * 2.0 - 1.0, (a_pos.y / u_screen_size.y * 2.0 - 1.0) * -1.0, 0.0, 1.0);\n"
			"   f_uv = a_uv;"
			"   f_color = a_color;"
			"}\0";

		const char* fragment_shader_source =
			"#version 330 core\n"
			"in vec2 f_uv;"
			"in vec4 f_color;"
			""
			"out vec4 FragColor;"
			""
			"uniform sampler2D u_texture;"
			""
			"void main()"
			"{"
			"	FragColor = texture(u_texture, f_uv) * f_color;"
			//"	FragColor = vec4(1.0, 0.0, 0.0, 1.0);"
			"}\0";

		shader = rlLoadShaderCode(vertex_shader_source, fragment_shader_source);
		uniform_screen_size = rlGetLocationUniform(shader, "u_screen_size");

		vao = rlLoadVertexArray();
		LGUI_ASSERT(rlEnableVertexArray(vao), "Huh");
		rlEnableVertexBuffer(vertex_buffer);
		rlEnableVertexBufferElement(index_buffer);
		rlEnableShader(shader);
		auto stride = (2 + 2) * sizeof(f32) + 4;
		rlSetVertexAttribute(0, 2, RL_FLOAT, false, stride, nullptr);
		rlEnableVertexAttribute(0);
		rlSetVertexAttribute(1, 2, RL_FLOAT, false, stride, (void*)(2 * sizeof(f32)));
		rlEnableVertexAttribute(1);
		rlSetVertexAttribute(2, 4, RL_UNSIGNED_BYTE, true, stride, (void*)(4 * sizeof(f32)));
		rlEnableVertexAttribute(2);
	}
	else
	{
		// Update buffers
		rlUpdateVertexBuffer(vertex_buffer, draw_buffer.vertex_buffer, draw_buffer.vertex_buffer_top * sizeof(f32), 0);
		rlUpdateVertexBufferElements(index_buffer, draw_buffer.index_buffer, draw_buffer.index_buffer_top * sizeof(DrawIndex), 0);
	}

	rlDisableBackfaceCulling();
	rlEnableScissorTest();

	v2 screen_size = context->app_window_size;

	rlEnableShader(shader);
	rlSetUniform(uniform_screen_size, &screen_size, RL_SHADER_UNIFORM_VEC2, 1);
	
	rlActiveTextureSlot(0);
	rlEnableTexture(context->atlas.texture_id);
	rlEnableVertexArray(vao);

	for (Panel* panel = context->first_depth_panel; panel; panel = panel->order_next)
	{
		for (DrawCommand* command = panel->painter.first_command; command; command = command->next)
		{
			v2 clip_pos = command->clip_rect.bottom_left();
			clip_pos.y = screen_size.y - clip_pos.y;
			v2 clip_size = command->clip_rect.size();
			rlScissor((int)clip_pos.x, (int)clip_pos.y, (int)clip_size.x, (int)clip_size.y);

			// TODO: custom textures

			rlDrawVertexArrayElements(command->index_start, command->index_end - command->index_start, nullptr);
		}
	}

	rlDisableScissorTest();
}

}
