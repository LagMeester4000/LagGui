#include "lag_gui.hpp"
#include "basic.hpp"
#include "crc32.hpp"
#include <string.h>

#include "raylib.h"
#include "rlgl.h"
#include "stb_truetype.h"
#include <stdio.h>

namespace lgui {

static f32 lerp(f32 v1, f32 v2, f32 t)
{
	return v1 + (v2 - v1) * t;
}

Context* create_context()
{
	usize mem_size = LGUI_MB(8);
	Arena arena = Arena::from_memory(malloc(mem_size), mem_size);

	Context* ret = arena.allocate_one<Context>();
	ret->arena = arena;
	ret->temp_arena = Arena::from_memory(malloc(mem_size), mem_size);

	ret->draw_buffer.allocate(ret);
	ret->current_frame = 1;

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
		// Must be done _before_ new input is inserted
		if (!mouse_down(context))
		{
			context->active_id = 0;
			context->mouse_dragging = false;
		}

		// Insert new mouse input
		{
			MouseState state{};
			state.buttons[0] = IsMouseButtonDown(0);
			state.buttons[1] = IsMouseButtonDown(1);
			state.buttons[2] = IsMouseButtonDown(2);
			state.pos = v2::from(GetMousePosition());

			context->mouse_states[1] = context->mouse_states[0];
			context->mouse_states[0] = state;

			for (int i = 0; i < 3; ++i)
			{
				if (mouse_pressed(context, i))
				{
					context->mouse_pressed_pos[i] = state.pos;
				}
			}
		}

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

void push_style(Context* context, const Style& style)
{
	LGUI_ASSERT(context->style_stack_top < STYLE_STACK_SIZE, "Out of bounds");
	context->style_stack[context->style_stack_top] = style;
	++context->style_stack_top;
}

void pop_style(Context* context)
{
	LGUI_ASSERT(context->style_stack_top > 0, "Out of bounds");
	++context->style_stack_top;
}

const Style& get_style(Context* context)
{
	LGUI_ASSERT(context->style_stack_top > 0, "No style to return");
	return context->style_stack[context->style_stack_top - 1];
}

void set_default_style(Context* context, const Style& style)
{
	// ??
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
		if (panel->id == id) return panel;
	}

	// TODO: Use first_free_panel
	Panel* ret = context->arena.allocate_one<Panel>();
	ret->id = id;

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

// Interpolation that can be called every frame without a T:
//   current = current + (target - current) * rate
static f32 interp_towards(f32 value, f32 target, f32 rate, f32 dt)
{
	return value + (target - value) * rate * dt;
}

// Rotation in degrees (0 degrees is right)
static void draw_open_triangle(Context* context, Painter* painter, v2 pos, f32 size, f32 rotation, Color color)
{
	f32 rad = rotation / 180.f * PI;
	f32 r_cos = cosf(rad);
	f32 r_sin = sinf(rad);

	auto fast_rot = [&](v2 p)
	{
		return v2{p.x * r_cos - p.y * r_sin, p.x * r_sin + p.y * r_cos};
	};

	v2 min = {-1, -1};
	f32 scale = size / 2.f;

	v2 top_left = (fast_rot({-1, -1}) - min) * scale + pos;
	v2 bottom_left = (fast_rot({-1, 1}) - min) * scale + pos;
	v2 center_right = (fast_rot({1, 0}) - min) * scale + pos;

	painter->begin_triangle_strip();
	painter->add_strip_triangle(context, top_left, color);
	painter->add_strip_triangle(context, center_right, color);
	painter->add_strip_triangle(context, bottom_left, color);
	painter->end_triangle_strip();
}

bool begin_panel(Context* context, const char* name, Rect rect, PanelFlag flags)
{
	Panel* panel = _get_or_create_panel(context, get_id(context, name));
	push_id(context, panel->id);

	// TODO: Check if there is already a panel, and end its painter
	panel->painter._start_painter(context);

	panel->flags = flags;

	if (panel->frame_last_updated == 0)
	{
		// First usage
		panel->rect = rect;
		panel->open = true;
	}
	panel->frame_last_updated = context->current_frame;

	// Push on stack
	LGUI_ASSERT(context->panel_stack_top < PANEL_STACK_SIZE, "Panel stack is full");
	context->panel_stack[context->panel_stack_top] = panel;
	++context->panel_stack_top;

	// Animate open/close
	f32 open_target = panel->open ? 1.f : 0.f;
	panel->open_anim = interp_towards(panel->open_anim, open_target, 15, 0.01666f);

	// Create rects
	const Style& style = get_style(context);
	Painter& painter = panel->painter;
	f32 line_height = style.line_height();
	Rect window_whole = panel->rect;
	// Animate
	window_whole.bottom_right.y = lerp(window_whole.top_left.y + line_height + 2, window_whole.bottom_right.y, panel->open_anim);
	Rect window_pad = window_whole.pad(1);
	Rect window_title_bar = window_pad.cut_top(style.line_height());

	// Input
	{
		// Title bar
		{
			InputResult r = handle_element_input(context, window_title_bar, panel->id, true);
			if (r.dragging)
			{
				panel->rect.move(r.drag_delta);

				// Also update the created rects
				window_whole.move(r.drag_delta);
				window_pad.move(r.drag_delta);
				window_title_bar.move(r.drag_delta);
			}

			if (r.pressed)
			{
				move_panel_to_front(context, panel);
			}
		}

		{
			Rect close_button = window_title_bar;
			close_button = close_button.cut_left(close_button.size().y);
			ID close_button_id = get_id(context, "__close_button");
			InputResult r = handle_element_input(context, close_button, close_button_id);
			if (r.clicked)
			{
				panel->open = !panel->open;
			}
		}
	}

	// Draw
	{
		painter.draw_rectangle(context, window_whole, style.window_outline);
		painter.draw_rectangle(context, window_pad, style.window_background);
		painter.draw_rectangle(context, window_title_bar, style.window_title_background);
		//painter.draw_text(context, style.default_font, panel->open ? "-" : "+", window_title_bar.top_left + v2{style.line_padding, style.line_padding}, 0, style.window_title_color);
		painter.draw_text(context, style.default_font, name, window_title_bar.top_left + v2{line_height, style.line_padding}, 0, style.window_title_color);

		f32 triangle_pad = style.line_padding + 3;
		draw_open_triangle(context, &painter, window_title_bar.top_left + v2{triangle_pad, triangle_pad}, line_height - triangle_pad * 2.f, lerp(0, 90, panel->open_anim), style.window_title_color);

		panel->content = window_pad.pad(2);
		panel->draw_pos = panel->content.top_left;
		painter.push_clip_rect(context, panel->content);
	}

	bool open = panel->open || panel->open_anim >= 0.001f;
	if (!open)
	{
		end_panel(context);
	}

	return open;
}

void end_panel(Context* context)
{
	Painter& painter = get_current_panel(context)->painter;
	painter._push_command(context);
	painter.pop_clip_rect(context);

	pop_id(context);

	LGUI_ASSERT(context->panel_stack_top > 0, "There is no panel to end");
	--context->panel_stack_top;
}

void move_panel_to_front(Context* context, Panel* panel)
{
	if (context->last_depth_panel == panel)
	{
		return;
	}

	if (context->first_depth_panel == panel)
	{
		context->first_depth_panel = panel->order_next;
	}

	if (panel->order_next)
	{
		LGUI_ASSERT(panel->order_next->order_prev == panel, "Provided panel is old");
		panel->order_next->order_prev = panel->order_prev;
	}
	if (panel->order_prev)
	{
		LGUI_ASSERT(panel->order_prev->order_next == panel, "Provided panel is old");
		panel->order_prev->order_next = panel->order_next;
	}

	if (context->last_depth_panel)
	{
		context->last_depth_panel->order_next = panel;
	}
	panel->order_next = nullptr;
	panel->order_prev = context->last_depth_panel;
	context->last_depth_panel = panel;
}

void RetainedData::update_t_linear(bool hover, bool active, f32 dt, f32 duration)
{
	f32 hover_dir = hover ? 1.f : -1.f;
	f32 active_dir = active ? 1.f : -1.f;

	hover_t += hover_dir * dt * (1.f / duration);
	hover_t = LGUI_CLAMP(0.f, 1.f, hover_t);
	active_t += active_dir * dt * (1.f / duration);
	active_t = LGUI_CLAMP(0.f, 1.f, active_t);
}

void RetainedData::update_t_towards(bool hover, bool active, f32 dt, f32 rate)
{
	f32 hover_goal = hover ? 1.f : 0.f;
	f32 active_goal = active ? 1.f : 0.f;

	hover_t += (hover_goal - hover_t) * dt * rate;
	hover_t = LGUI_CLAMP(0.f, 1.f, hover_t);
	active_t += (active_goal - active_t) * dt * rate;
	active_t = LGUI_CLAMP(0.f, 1.f, active_t);
}

v2 layout_next(Context* context)
{
	Panel* panel = get_current_panel(context);
	v2 ret = panel->draw_pos;
	const Style& style = get_style(context);
	panel->draw_pos.y += style.line_height();
	return ret;
}

InputResult handle_element_input(Context* context, Rect rect, ID id, bool enable_drag)
{
	InputResult ret{};

	v2 mouse = mouse_pos(context);

	if (context->overlap_panel == get_current_panel(context) &&
		rect.overlap(mouse))
	{
		context->overlap_id = id;
	}

	ret.hover = context->hover_id == id;

	// TODO: "active ID" system needs to work with other mouse buttons
	if (context->hover_id == id || context->active_id == id)
	{
		if (mouse_pressed(context, 0))
		{
			ret.pressed = true;
			context->active_id = id;
		}
		if (mouse_released(context, 0))
		{
			ret.released = true;

			if (context->active_id == id && !context->mouse_dragging)
			{
				ret.clicked = true;
			}
		}
		if (mouse_down(context, 0))
		{
			ret.down = true;

			// Dragging
			if (enable_drag && context->active_id == id)
			{
				if (!context->mouse_dragging)
				{
					v2 press_pos = context->mouse_pressed_pos[0];

					if ((press_pos - mouse).length() > MIN_DRAG_DISTANCE)
					{
						context->mouse_dragging = true;
						ret.dragging = true;
						ret.drag_start = press_pos;
						ret.drag_delta = mouse - press_pos;
					}
				}
				else
				{
					v2 press_pos = context->mouse_pressed_pos[0];
					v2 mouse_old = context->mouse_states[1].pos;
					context->mouse_dragging = true;
					ret.dragging = true;
					ret.drag_start = press_pos;
					ret.drag_delta = mouse - mouse_old;
				}
			}

		}
	}

	return ret;
}

bool mouse_pressed(Context* context, int button)
{
	return context->mouse_states[0].buttons[button] && !context->mouse_states[1].buttons[button];
}

bool mouse_released(Context* context, int button)
{
	return !context->mouse_states[0].buttons[button] && context->mouse_states[1].buttons[button];
}

bool mouse_down(Context* context, int button)
{
	return context->mouse_states[0].buttons[button];
}

v2 mouse_pos(Context* context)
{
	return context->mouse_states[0].pos;
}

RetainedData* get_retained_data(Context* context, ID id)
{
	Panel* panel = get_current_panel(context);

	RetainedData** first_retained_data = &panel->retained_data_lookup[id % RETAINED_TABLE_SIZE];
	for (RetainedData* it = *first_retained_data; it; it = it->next)
	{
		if (it->id == id)
		{
			return it;
		}
	}

	RetainedData* retained_data = context->arena.allocate_one<RetainedData>();
	retained_data->id = id;
	retained_data->next = *first_retained_data;
	if (*first_retained_data)
	{
		(*first_retained_data)->prev = retained_data;
	}
	*first_retained_data = retained_data;

	return retained_data;
}

static Color lerp_color(Color c1, Color c2, f32 t)
{
	return {
		lerp(c1.r, c2.r, t),
		lerp(c1.g, c2.g, t),
		lerp(c1.b, c2.b, t),
		lerp(c1.a, c2.a, t),
	};
}

InputResult button(Context* context, const char* name)
{
	v2 pos = layout_next(context);
	v2 size = v2{50, 30};
	Color color = Color{0, 1, 0, 1};
	ID id = get_id(context, name);
	RetainedData* retained = get_retained_data(context, id);
	const Style& style = get_style(context);

	// Rect
	auto& painter = get_current_panel(context)->painter;
	Font* font = style.default_font;
	f32 text_width = font->text_width(name, 0);
	Rect rect = Rect::from_pos_size(pos, {text_width + 4, font->height});

	InputResult input = handle_element_input(context, rect, id);
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

	retained->update_t_towards(input.hover, input.down, 0.016666f);
	color = lerp_color(lerp_color(style.button_background, style.button_background_hover, retained->hover_t), style.button_background_down, retained->active_t);

	// Rendering
	Rect text_rect = rect.center_size({text_width, font->height});
	painter.draw_rectangle(context, rect, color);
	painter.draw_text(context, font, name, text_rect.top_left, 0, {1, 1, 1, 1});

	return input;
}

InputResult checkbox(Context* context, const char* name, bool* value)
{
	v2 pos = layout_next(context);
	v2 size = v2{50, 30};
	Color color = Color{0, 1, 0, 1};
	ID id = get_id(context, name);
	RetainedData* retained = get_retained_data(context, id);
	const Style& style = get_style(context);

	// Rect
	auto& painter = get_current_panel(context)->painter;
	Rect rect = Rect::from_pos_size(pos, {25, 25});

	InputResult input = handle_element_input(context, rect, id);
	if (input.pressed)
	{
		*value = !*value;
	}

	retained->update_t_towards(input.hover, *value, 0.016666f, 30);
	color = lerp_color(style.button_background, style.button_background_hover, retained->hover_t);

	// Rendering
	Rect inner = rect.pad(1);
	Rect checked = rect.pad(lerp(7, 5, retained->active_t));
	Color checked_color = {1, 1, 1, retained->active_t};
	painter.draw_rectangle(context, rect, style.checkbox_outline);
	painter.draw_rectangle(context, inner, color);
	painter.draw_rectangle(context, checked, checked_color);

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
	vertex_buffer_length = short_max;
	index_buffer_length = short_max;

	vertex_buffer_top = 0;
	index_buffer_top = 0;

	vertex_buffer = (f32*)context->arena.allocate(vertex_buffer_length * vertex_size);
	index_buffer = (u16*)context->arena.allocate(index_buffer_length * index_size);
}

}
