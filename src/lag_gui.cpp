#include "lag_gui.hpp"
#include "basic.hpp"
#include "crc32.hpp"
#include <string.h>

#include "raylib.h"
#include "rlgl.h"
#include "stb_truetype.h"
#include <stdio.h>

namespace lgui {

Context* create_context()
{
	usize mem_size = LGUI_MB(8);
	Arena arena = Arena::from_memory(malloc(mem_size), mem_size);

	Context* ret = arena.allocate_one<Context>();
	ret->arena = arena;
	ret->temp_arena = Arena::from_memory(malloc(mem_size), mem_size);

	ret->draw_buffer.allocate(ret);
	ret->current_frame = 1;

	// TEMP: load font
	{
		static byte temp_buffer[LGUI_MB(2)];
		FILE* f = fopen("Montserrat-Regular.ttf", "rb");
		fread(temp_buffer, 1, LGUI_MB(2), f);
		fclose(f);

		static byte temp_pixels[512 * 512 * 4];
		ret->temp_font_height = 15.f;
		stbtt_BakeFontBitmap(
			temp_buffer,
			0,
			ret->temp_font_height,
			temp_pixels,
			512,
			512,
			32,
			96,
			ret->temp_font_cdata
		);

		ret->temp_font_texture = rlLoadTexture(temp_pixels, 512, 512, RL_PIXELFORMAT_UNCOMPRESSED_R8G8B8A8, 1);
	}

	return ret;
}

void begin_frame(Context* context)
{
	context->temp_arena.reset();

	// Find overlap panel
	{
		v2 mouse = v2::from(GetMousePosition());

		context->overlap_panel = nullptr;
		for (Panel* panel = context->first_depth_panel; panel; panel = panel->order_next)
		{
			if (panel->rect.overlap(mouse))
			{
				context->overlap_panel = panel;
			}
		}
	}

	// Input
	{
		context->hover_id = 0;

		if (context->overlap_id)
		{
			context->hover_id = context->overlap_id;
			context->overlap_id = 0;
		}
	}

	++context->current_frame;
}

void end_frame(Context* context)
{
	rl_render(context);

	context->draw_buffer.vertex_buffer_top = 0;
	context->draw_buffer.index_buffer_top = 0;
}

ID calc_id(Context* context, const byte* data, usize length)
{
	ID top = context->id_stack_top > 0 ? context->id_stack[context->id_stack_top - 1] : 123456;
	return xcrc32(data, length, top);
}

ID get_id(Context* context, const char* string)
{
	return calc_id(context, (const byte*)string, strlen(string));
}

ID get_id(Context* context, i32 i)
{
	return calc_id(context, (const byte*)&i, sizeof(i));
}

static void _push_id(Context* context, ID id)
{
	LGUI_ASSERT(context->id_stack_top < ID_STACK_SIZE, "Id stack overflow");
	context->id_stack[context->id_stack_top] = id;
	++context->id_stack_top;
}

void pop_id(Context* context)
{
	LGUI_ASSERT(context->id_stack_top > 0, "Can't pop ID because there are no IDs left");
	--context->id_stack_top;
}

void push_id(Context* context, const char* string)
{
	_push_id(context, get_id(context, string));
}

void push_id(Context* context, i32 i)
{
	_push_id(context, get_id(context, i));
}

Panel* get_panel(Context* context, ID id)
{
	for (Panel* panel = context->first_depth_panel; panel; panel = panel->order_next)
	{
		if (panel->id) return panel;
	}
	return nullptr;
}

Panel* get_current_panel(Context* context)
{
	LGUI_ASSERT(context->panel_stack_top > 0, "There is no current panel");
	return context->panel_stack[context->panel_stack_top - 1];
}

static Panel* _get_or_create_panel(Context* context, ID id)
{
	for (Panel* panel = context->first_depth_panel; panel; panel = panel->order_next)
	{
		if (panel->id) return panel;
	}

	Panel* ret = context->arena.allocate_one<Panel>();
	ret->id = get_id(context, id);

	// TODO: Maybe not all panels should go in the depth list?
	if (!context->first_depth_panel)
	{
		LGUI_ASSERT(!context->last_depth_panel, "This shouldn't exist");
		context->first_depth_panel = ret;
		context->last_depth_panel = ret;
	}
	else
	{
		LGUI_ASSERT(context->last_depth_panel, "This should exist");
		ret->order_prev = context->last_depth_panel;
		context->last_depth_panel->order_next = ret;
		context->last_depth_panel = ret;
	}

	return ret;
}

bool begin_panel(Context* context, const char* name, Rect rect, PanelFlag flags)
{
	Panel* panel = _get_or_create_panel(context, get_id(context, name));
	push_id(context, panel->id);

	// TODO: Check if there is already a panel, and end its painter
	panel->painter._start_painter(context);

	panel->flags = flags;
	panel->rect = rect;

	LGUI_ASSERT(context->panel_stack_top < PANEL_STACK_SIZE, "Panel stack is full");
	context->panel_stack[context->panel_stack_top] = panel;
	++context->panel_stack_top;

	panel->draw_pos = panel->rect.top_left + v2{4, 4};

	//draw_rectangle(panel->rect.top_left, panel->rect.size(), Color{1, 0, 0, 1});
	panel->painter.draw_rectangle(context, panel->rect, Color{1, 0, 0, 1});

	return true;
}

void end_panel(Context* context)
{
	get_current_panel(context)->painter._push_command(context);

	pop_id(context);

	LGUI_ASSERT(context->panel_stack_top > 0, "There is no panel to end");
	--context->panel_stack_top;
}

v2 layout_next(Context* context)
{
	Panel* panel = get_current_panel(context);
	v2 ret = panel->draw_pos;
	panel->draw_pos.y += 25.f;
	return ret;
}

InputResult handle_element_input(Context* context, Rect rect, ID id)
{
	// TODO: Replace raylib calls with custom solutions
	InputResult ret{};

	v2 mouse = v2::from(GetMousePosition());

	if (context->overlap_panel == get_current_panel(context) &&
		rect.overlap(mouse))
	{
		context->overlap_id = id;
	}

	if (context->hover_id == id)
	{
		ret.hover = true;

		if (IsMouseButtonPressed(0))
		{
			ret.pressed = true;
		}
		if (IsMouseButtonReleased(0))
		{
			ret.released = true;

			// TODO: Add proper clicked mechanism
			ret.clicked = true;
		}
	}

	return ret;
}

InputResult button(Context* context, const char* name)
{
	v2 pos = layout_next(context);
	v2 size = v2{50, 30};
	Color color = Color{0, 1, 0, 1};
	ID id = get_id(context, name);

	InputResult input = handle_element_input(context, Rect::from_pos_size(pos, size), id);
	if (input.pressed)
	{
		color.b = 1;
	}
	else if (input.released)
	{
		color.r = 1;
		color.b = 1;
	}
	else if (input.hover)
	{
		color.r = 1;
	}

	//draw_rectangle(pos, size, color);

	auto& painter = get_current_panel(context)->painter;
	Font* font = context->atlas.first_font;
	painter.draw_rectangle(context, pos, size, color);
	painter.draw_text(context, font, name, pos, 2, {1, 1, 1, 1});

	return input;
}

void text(Context* context, const char* text)
{

}

void DrawBuffer::allocate(Context* context)
{
	// TODO: Replace with growing solution? Especially for index buffer

	const usize vertex_size = sizeof(f32) * 2 + sizeof(f32) * 2 + sizeof(u32);
	const usize index_size = sizeof(u16);
	const usize short_max = 1 << 16;
	vertex_buffer_length = vertex_size * short_max;
	index_buffer_length = index_size * short_max;

	vertex_buffer_top = 0;
	index_buffer_top = 0;

	vertex_buffer = (f32*)context->arena.allocate(vertex_buffer_length);
	index_buffer = (u16*)context->arena.allocate(vertex_buffer_length);
}

}
