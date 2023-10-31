#include "lag_gui.hpp"
#include "basic.hpp"

#include <stdio.h>
#include <stdarg.h>

namespace lgui {

static f32 lerp(f32 v1, f32 v2, f32 t)
{
	return v1 + (v2 - v1) * t;
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

InputResult button(const char* name)
{
	ID id = get_id(name);
	const Style& style = get_style();

	// Rect
	Font* font = style.default_font;
	f32 text_width = font->text_width(name, 0);
	Box* box = make_box(name, px(text_width + 4, font->height), BoxFlag_DrawText | BoxFlag_DrawRectangle);
	box->font = font;
	box->text_length = strlen(name);
	box->text = copy_string(get_context()->temp_arena, name, box->text_length);
	box->h_align = 0;
	box->v_align = 0;

	InputResult input = handle_element_input(box->prev_rect(), id);
	box->update_t_towards(input.hover, input.down);
	Color color = lerp_color(lerp_color(style.button_background, style.button_background_hover, box->hover_t), style.button_background_down, box->active_t);

	box->color = color;
	box->text_color = style.button_text;

	return input;
}

InputResult button(const char* name, Size2 size)
{
	const Style& style = get_style();

	// Rect
	Font* font = style.default_font;
	f32 text_width = font->text_width(name, 0);
	Box* box = make_box(name, size, BoxFlag_DrawText | BoxFlag_DrawRectangle);
	box->font = font;
	box->text_length = strlen(name);
	box->text = copy_string(get_context()->temp_arena, name, box->text_length);
	box->h_align = 0;
	box->v_align = 0;

	InputResult input = handle_element_input(box->prev_rect(), box->id);
	box->update_t_towards(input.hover, input.down);
	Color color = lerp_color(lerp_color(style.button_background, style.button_background_hover, box->hover_t), style.button_background_down, box->active_t);

	box->color = color;
	box->text_color = style.button_text;

	return input;
}

void separator()
{
	const Style& style = get_style();
	Box* box = get_box();
	if (box->flags & BoxFlag_IsHorizontal)
	{
		layout_horizontal(0, 0, {px(style.separator_spacing), pc(1.f)});
		Box* box = make_box(box_generate_id(), {px(style.separator_size), pc(0.9f)}, 0);
		box->set_rectangle(style.separator);
		layout_end();
	}
	else
	{
		layout_vertical(0, 0, {pc(1.f), px(style.separator_spacing)});
		Box* box = make_box(box_generate_id(), {pc(0.9f), px(style.separator_size)}, 0);
		box->set_rectangle(style.separator);
		layout_end();
	}
}

void spacer(f32 size)
{
	Box* box = get_box();
	if (box->flags & BoxFlag_IsHorizontal)
	{
		make_box(box_generate_id(), px(size, 1.f), 0);
	}
	else
	{
		make_box(box_generate_id(), px(1.f, size), 0);
	}
}

void min_size(f32 size)
{
	Box* box = get_box();
	if (box->flags & BoxFlag_IsHorizontal)
	{
		make_box(box_generate_id(), px(0.f, size), 0);
	}
	else
	{
		make_box(box_generate_id(), px(size, 0.f), 0);
	}
}

Box* draw_hook(Size2 size, void* ud, DrawHook hook)
{
	Box* ret = make_box(box_generate_id(), size, 0);
	ret->set_draw_hook(ud, hook);
	return ret;
}

Box* draw_hook(Size2 size, DrawHook hook)
{
	Box* ret = make_box(box_generate_id(), size, 0);
	ret->set_draw_hook(nullptr, hook);
	return ret;
}

void text(const char* text, bool static_string)
{
	const Style& style = get_style();

	// Rect
	Font* font = style.default_font;
	f32 text_width = font->text_width(text, 0);
	Box* box = make_box(box_generate_id(), px(text_width, font->height), BoxFlag_DrawText);
	box->font = font;

	usize len = strlen(text);
	box->text_length = len;
	if (static_string)
	{
		box->text = text;
	}
	else
	{
		box->text = copy_string(get_context()->temp_arena, text, len);
	}

	box->h_align = 0;
	box->v_align = 0;
	box->text_color = {1.f, 1.f, 1.f, 1.f};
}

void text(const char* text, Size2 size, i8 h_align, i8 v_align, bool static_string)
{
	const Style& style = get_style();

	// Rect
	Font* font = style.default_font;
	f32 text_width = font->text_width(text, 0);
	Box* box = make_box(box_generate_id(), size, BoxFlag_DrawText);
	box->font = font;

	usize len = strlen(text);
	box->text_length = len;
	if (static_string)
	{
		box->text = text;
	}
	else
	{
		box->text = copy_string(get_context()->temp_arena, text, len);
	}

	box->h_align = h_align;
	box->v_align = v_align;
	box->text_color = {1.f, 1.f, 1.f, 1.f};
}

void textf(const char* format, ...)
{
	va_list args;
	va_start(args, format);

	int size = vsnprintf(nullptr, 0, format, args);
	if (size < 0) return;
	char* memory = (char*)get_context()->temp_arena->allocate_raw((usize)(size + 1));
	vsnprintf(memory, size + 1, format, args);

	va_end(args);

	text(memory, true);
}

static v2 _wrapped_text_size(const char* text, usize len, f32 width, Font* font)
{

}

InputResult radio_button(const char* name, int option, int* selected)
{
	ID id = get_id(name);
	RetainedData* retained = get_retained_data(id);
	const Style& style = get_style();

	// Box
	f32 size = style.line_height();
	Box* box = push_box(name, px(size, size), 0);
	box->h_align = 0;
	box->v_align = 0;

	// Input
	InputResult input = handle_element_input(box->prev_rect(), id);
	if (input.clicked)
	{
		*selected = option;
	}

	retained->update_t_towards(input.hover, *selected == option, 30);
	Color color = lerp_color(style.radio_button_background, style.radio_button_background_hover, retained->hover_t);
	box->set_circle(color, style.radio_button_outline, style.radio_button_outline_size);

	if (retained->active_t > 0.f)
	{
		// Second circle
		f32 inside_size = size * lerp(0.2f, 0.6f, retained->active_t);
		Box* inside = make_box(name, px(inside_size, inside_size), 0);
		Color c = style.radio_button_inside;
		c.a = retained->active_t;
		inside->set_circle(c);
	}

	pop_box();

	return input;
}

InputResult slider_value(const char* name, Size2 size, f32 min, f32 max, f32* value)
{
	ID id = get_id(name);
	RetainedData* retained = get_retained_data(id);
	const Style& style = get_style();

	// Rect
	Box* box = push_box(name, size, BoxFlag_DrawRectangle);
	box->h_align = -1;
	box->v_align = -1;
	box->padding = {3.f, 3.f};

	Rect prev_rect = box->prev_rect();
	InputResult input = handle_element_input(prev_rect, id, true);
	if (input.pressed || input.dragging)
	{
		v2 mouse = mouse_pos() - (prev_rect.top_left + box->padding);
		mouse /= prev_rect.size() - box->padding * 2.f;
		f32 new_value = min + (max - min) * mouse.x;
		*value = LGUI_CLAMP(min, max, new_value);
		input.changed = true;
	}

	Box* inner_box = make_box("in", pc((*value - min) / (max - min), 1.f), 0);
	inner_box->set_rectangle(style.radio_button_inside);

	retained->update_t_towards(input.hover, input.down);
	Color color = lerp_color(lerp_color(style.button_background, style.button_background_hover, retained->hover_t), style.button_background_down, retained->active_t);
	box->color = color;

	pop_box();

	return input;
}

InputResult checkbox(const char* name, bool* value)
{
	ID id = get_id(name);
	RetainedData* retained = get_retained_data(id);
	const Style& style = get_style();

	// Box
	f32 size = style.line_height();
	Box* box = push_box(name, px(size, size), 0);
	box->h_align = 0;
	box->v_align = 0;

	// Input
	InputResult input = handle_element_input(box->prev_rect(), id);
	if (input.clicked)
	{
		*value = !*value;
	}

	retained->update_t_towards(input.hover, *value, 30);
	Color color = lerp_color(style.radio_button_background, style.radio_button_background_hover, retained->hover_t);
	box->set_rectangle(color, style.radio_button_outline, style.radio_button_outline_size);

	if (retained->active_t > 0.f)
	{
		// Second circle
		f32 inside_size = size * lerp(0.2f, 0.6f, retained->active_t);
		Box* inside = make_box(name, px(inside_size, inside_size), 0);
		Color c = style.radio_button_inside;
		c.a = retained->active_t;
		inside->set_rectangle(c);
	}

	pop_box();

	return input;
}

static void _open_triangle(Size2 size, Color color, f32 t)
{
	Box* box = make_box(box_generate_id(), size, 0);
	box->active_t = t;
	box->color = color;
	box->set_draw_hook([](Box* box, Painter& painter, Rect rect) {
		draw_open_triangle(&painter, rect.top_left, rect.size().x, box->active_t, box->color);
	});
}

bool begin_fancy_collapse_header(const char* name)
{
	ID id = get_id(name);
	push_id_raw(id);
	RetainedData* retained = get_retained_data(id);
	const Style& style = get_style();

	Font* font = style.default_font;

	Box* vertical = layout_vertical(-1, -1, {pc(1.f), fit()});
	min_size(100.f);

	// Header
	{
		Box* header = layout_horizontal(-1, 0, {pc(1.f), px(style.line_height())});
		header->padding = {4.f, 4.f};
		header->set_rectangle(style.button_background);

		if (handle_element_input(header->prev_rect(), header->id).clicked)
		{
			retained->open = !retained->open;
		}
		retained->update_t_towards(false, retained->open, 20.f);

		_open_triangle({px(font->height, font->height)}, style.button_text, retained->active_t * 90.f);
		spacer(5.f);
		text(name);

		layout_end();
	}

	v2 padding = {3.f, 3.f};

	bool ret = true;
	if (retained->open && retained->active_t >= 0.99f)
	{
		Box* inner = layout_vertical(-1, 1, {pc(1.f), fit()});
		inner->set_rectangle(style.window_title_background);
		inner->padding = padding;
	}
	else if (retained->active_t > 0.001f)
	{
		Box* inner = layout_vertical(-1, 1, {pc(1.f), px(100)});
		inner->flags |= BoxFlag_Clip;
		inner->size[1] = px((inner->prev_used_size.y + padding.y * 2.f) * retained->active_t);
		inner->set_rectangle(style.window_title_background);
		inner->padding = padding;
	}
	else
	{
		ret = false;
		layout_end();
		pop_id();
	}

	return ret;
}

void end_fancy_collapse_header()
{
	layout_end();
	layout_end();
	pop_id();
}

bool begin_tree_node(const char* name)
{
	ID id = get_id(name);
	push_id_raw(id);
	RetainedData* retained = get_retained_data(id);
	const Style& style = get_style();

	Font* font = style.default_font;

	Box* vertical = layout_vertical(-1, -1, {fit(), fit()});
	min_size(100.f);

	// Header
	{
		Box* header = layout_horizontal(-1, 0, {fit(), px(font->height + 2.f)});

		if (handle_element_input(header->prev_rect(), header->id).clicked)
		{
			retained->open = !retained->open;
		}
		retained->update_t_towards(false, retained->open, 20.f);

		spacer(3.f);
		_open_triangle({px(font->height - 4.f, font->height - 4.f)}, style.button_text, retained->active_t * 90.f);
		spacer(3.f);
		text(name);

		layout_end();
	}

	v2 padding = {2.f, 2.f};
	f32 tab = 20.f;
	Color spacer_color = style.button_text;
	spacer_color.a = 0.7f;

	auto fancy_spacer = [&]() {
		layout_vertical(0, 1, {px(tab), pc(1.f)});
		Box* line = make_box(box_generate_id(), {px(2.f), rem(1.f)}, 0);
		line->set_rectangle(spacer_color);
		spacer(3.f);
		layout_end();
	};

	bool ret = true;
	if (retained->open && retained->active_t >= 0.99f)
	{
		{
			layout_horizontal(-1, 1, {fit(), fit()});
			//spacer(tab);
			fancy_spacer();
		}

		Box* inner = layout_vertical(-1, 1, {fit(), fit()});
		inner->padding = padding;
	}
	else if (retained->active_t > 0.001f)
	{
		{
			layout_horizontal(-1, 1, {fit(), fit()});
			//spacer(tab);
			fancy_spacer();
		}

		Box* inner = layout_vertical(-1, 1, {fit(), px(100)});
		inner->flags |= BoxFlag_Clip;
		inner->size[1] = px((inner->prev_used_size.y + padding.y * 2.f) * retained->active_t);
		inner->padding = padding;
	}
	else
	{
		ret = false;
		layout_end();
		pop_id();
	}

	return ret;
}

void end_tree_node()
{
	layout_end();
	layout_end();
	layout_end();
	pop_id();
}

bool begin_button_menu(const char* name)
{
	ID id = get_id(name);
	RetainedData* retained = get_retained_data(id);
	const Style& style = get_style();

	// Rect
	Font* font = style.default_font;
	f32 text_width = font->text_width(name, 0);
	Box* box = make_box(name, {px(text_width + 4), rem(1.f)}, BoxFlag_DrawText | BoxFlag_DrawRectangle);
	box->font = font;
	box->text = name;
	box->text_length = strlen(name);
	box->h_align = 0;
	box->v_align = 0;

	InputResult input = handle_element_input(box->prev_rect(), id);
	retained->update_t_towards(input.hover, input.down);
	Color color = lerp_color(lerp_color(style.button_background, style.button_background_hover, retained->hover_t), style.button_background_down, retained->active_t);

	box->color = color;
	box->text_color = style.button_text;

	if (input.clicked)
	{
		open_panel(id);
	}

	bool ret = begin_window(name, v2{1.f, 1.f}, PanelFlag_BeginClosed | PanelFlag_NoTitleBar |
		PanelFlag_AutoResizeHorizontal | PanelFlag_AutoResizeVertical);
	if (ret)
	{
		set_panel_anchor_point(box->prev_rect().bottom_left());
		Panel* current_panel = get_current_panel();
		move_panel_to_front(current_panel);
		bool mouse_press = mouse_pressed(0) || mouse_pressed(1) || mouse_pressed(2);
		if (mouse_press && get_context()->overlap_panel != current_panel)
			close_panel();
	}
	return ret;
}

void end_button_menu()
{
	end_window();
}

/*
// TODO: Port input text to layout v4

static void memmove_safe(void* destination, usize destination_size, const void* source, usize source_size)
{
	usize min_size = LGUI_MIN(destination_size, source_size);
	if (min_size > 0)
	{
		memmove(destination, source, min_size);
	}
}

bool _input_text(ID id, Rect rect, char* buffer, usize buffer_size, usize* text_length, bool wrap, f32 spacing = 0.f)
{
	Context* context = get_context();

	const Style& style = get_style();
	Font* font = style.default_font;
	RetainedData* data = get_retained_data(id);

	f32 outline_size = 1.f;
	Color outline_color = {1.f, 1.f, 1.f, 1.f};
	Color inside_color = {0.f, 0.f, 0.f, 1.f};
	Color text_color = {1.f, 1.f, 1.f, 1.f};
	Color selection_color = {0.3f, 1.f, 0.3f, 0.5f};
	Color cursor_color = {0.f, 1.f, 0.f, 1.f};
	f32 cursor_width = 2.f;

	Rect inside = rect.pad(outline_size);

	i32& cursor_index = data->value_int;
	i32& drag_cursor_index = data->value_int2;
	v2& camera_pos = data->value_v2;
	v2 mouse = mouse_pos();
	v2 relative_mouse_pos = mouse - inside.top_left;

	cursor_index = LGUI_CLAMP(0, (i32)*text_length, cursor_index);
	drag_cursor_index = LGUI_CLAMP(0, (i32)*text_length, drag_cursor_index);

	InputResult input = handle_element_input(rect, id, true);

	// Handle text input
	if (input.hover) // TODO: Replace with "selected" id, which doesn't exist yet
	{
		// TODO: Relace with non-raylib functions
		bool input_left = IsKeyPressed(KEY_LEFT);
		bool input_right = IsKeyPressed(KEY_RIGHT);
		bool input_up = IsKeyPressed(KEY_UP);
		bool input_down = IsKeyPressed(KEY_DOWN);
		bool input_backspace = IsKeyPressed(KEY_BACKSPACE);
		bool input_shift = IsKeyDown(KEY_LEFT_SHIFT) || IsKeyDown(KEY_RIGHT_SHIFT);
		bool input_control = IsKeyDown(KEY_LEFT_CONTROL) || IsKeyDown(KEY_RIGHT_CONTROL);
		bool input_copy = IsKeyPressed(KEY_C) && input_control;
		bool input_paste = IsKeyPressed(KEY_V) && input_control;
		auto get_clipboard_text = []() {
			return GetClipboardText();
		};
		auto set_clipboard_text = [&](char* buffer, usize len) {
			auto marker = context->temp_arena.make_marker();
			char* temp_buffer = (char*)context->temp_arena.allocate_raw(len + 1);
			memcpy(temp_buffer, buffer, len);
			temp_buffer[len] = 0;
			SetClipboardText(temp_buffer);
		};

		bool selecting = input_shift || input.dragging;

		i32 max_index = (i32)LGUI_MIN(*text_length, buffer_size);
		cursor_index = LGUI_CLAMP(0, max_index, cursor_index);
		drag_cursor_index = LGUI_CLAMP(0, max_index, drag_cursor_index);

		auto remove_range = [&](usize min, usize max) {
			usize min_index = (usize)LGUI_MIN(min, max);
			usize max_index = (usize)LGUI_MAX(min, max);
			min_index = LGUI_MIN(min_index, *text_length);
			max_index = LGUI_MIN(max_index, *text_length);

			usize move_size = LGUI_MIN(*text_length - max_index, buffer_size - max_index);
			if (move_size > 0)
			{
				memmove(&buffer[min_index], &buffer[max_index], move_size);
			}

			*text_length -= max_index - min_index;
		};
		auto remove_selection = [&]() {
			usize min_index = (usize)LGUI_MIN(cursor_index, drag_cursor_index);
			usize max_index = (usize)LGUI_MAX(cursor_index, drag_cursor_index);

			remove_range(min_index, max_index);

			cursor_index = (i32)min_index;
			drag_cursor_index = (i32)min_index;
		};
		auto has_selection = [&]() { return cursor_index != drag_cursor_index; };
		auto is_whitespace = [](char c) { return c == ' ' || c == '\t' || c == '\n'; };
		auto find_space_index_left = [&](usize index) -> usize {
			if (index == 0)
			{
				return 0;
			}

			char prev_char = buffer[index - 1];
			if (is_whitespace(prev_char))
			{
				// Find first non-space
				for (usize i = index - 1; i > 0; --i)
				{
					if (!is_whitespace(buffer[i]))
					{
						return i + 1;
					}
				}
				return 0;
			}
			else
			{
				// Find first space
				for (usize i = index - 1; i > 0; --i)
				{
					if (is_whitespace(buffer[i]))
					{
						return i + 1;
					}
				}
				return 0;
			}
		};
		auto find_space_index_right = [&](usize index) -> usize {
			usize max = LGUI_MIN(buffer_size, *text_length);
			if (index >= max)
			{
				return max;
			}

			char next_char = buffer[index];
			if (is_whitespace(next_char))
			{
				// Find first non-space
				for (usize i = index; i < max; ++i)
				{
					if (!is_whitespace(buffer[i]))
					{
						return i;
					}
				}
				return max;
			}
			else
			{
				// Find first space
				for (usize i = index; i < max; ++i)
				{
					if (is_whitespace(buffer[i]))
					{
						return i;
					}
				}
				return max;
			}
		};


		while (int codepoint = GetCharPressed())
		{
			if (has_selection())
			{
				remove_selection();
			}

			if (cursor_index < buffer_size)
			{
				usize insert_index = (usize)cursor_index;
				cursor_index = cursor_index + 1;
				if (cursor_index > buffer_size)
				{
					cursor_index = (i32)buffer_size;
				}
				drag_cursor_index = cursor_index;
				if (*text_length < buffer_size)
				{
					++*text_length;
				}

				if (cursor_index < *text_length)
				{
					LGUI_ASSERT(*text_length <= buffer_size, "Text length out of bounds of buffer");
					memmove_safe(&buffer[insert_index + 1], *text_length - (insert_index + 1), &buffer[insert_index], buffer_size - (insert_index));
				}

				buffer[insert_index] = (char)codepoint;
			}
		}

		if (input_paste)
		{
			if (has_selection())
			{
				remove_selection();
			}

			const char* clipboard = get_clipboard_text();
			usize clipboard_len = strlen(clipboard);

			if (cursor_index < *text_length)
			{
				LGUI_ASSERT(*text_length <= buffer_size, "Text length out of bounds of buffer");
				memmove_safe(&buffer[cursor_index + clipboard_len], *text_length - (cursor_index + clipboard_len), &buffer[cursor_index], buffer_size - (cursor_index));
			}

			memmove_safe(&buffer[cursor_index], buffer_size - (cursor_index), clipboard, clipboard_len);

			*text_length = LGUI_CLAMP(0, (i32)buffer_size, *text_length + (i32)clipboard_len);
			cursor_index = LGUI_CLAMP(0, (i32)*text_length, cursor_index + (i32)clipboard_len);
			drag_cursor_index = cursor_index;

			// TODO: Complete this (and copy and cut)
		}

		if (input_backspace)
		{
			if (has_selection())
			{
				remove_selection();
			}
			else if (input_control && cursor_index > 0)
			{
				usize min_index = find_space_index_left(cursor_index);

				remove_range(min_index, cursor_index);

				cursor_index = (i32)min_index;
				drag_cursor_index = (i32)min_index;
			}
			else if (cursor_index > 0)
			{
				cursor_index = cursor_index - 1;
				usize remove_index = cursor_index;
				if (cursor_index > buffer_size)
				{
					cursor_index = (i32)buffer_size;
				}
				drag_cursor_index = cursor_index;
				if (*text_length > 0)
				{
					--*text_length;
				}

				usize move_size = LGUI_MIN(*text_length - remove_index, buffer_size - remove_index);
				if (move_size > 0)
				{
					memmove(&buffer[remove_index], &buffer[remove_index + 1], move_size);
				}
			}
		}

		if (input_left && cursor_index > 0)
		{
			if (input_control)
			{
				cursor_index = (i32)find_space_index_left((usize)cursor_index);
			}
			else
			{
				--cursor_index;
			}

			if (!selecting)
			{
				drag_cursor_index = cursor_index;
			}
		}
		if (input_right && cursor_index < max_index)
		{
			if (input_control)
			{
				cursor_index = (i32)find_space_index_right((usize)cursor_index);
			}
			else
			{
				++cursor_index;
			}

			if (!selecting)
			{
				drag_cursor_index = cursor_index;
			}
		}
	}

	// Render
	{
		Painter& painter = get_current_panel()->get_painter();

		painter.draw_rectangle(rect, outline_color);
		painter.draw_rectangle(inside, inside_color);

		painter.push_clip_rect(inside);

		// Flooring the position to prevent weird rendering issues
		v2 pos = inside.top_left - camera_pos;
		pos = v2{floorf(pos.x), floorf(pos.y)};
		v2 pos_start = pos;
		v2 cursor_draw_pos = pos;
		v2 drag_cursor_draw_pos = pos;

		if (false)
		{
			// Multi-line

			for (usize i = 0; i < *text_length; ++i)
			{
				bool endline = false;
				usize word_end = i;
				for (; word_end < *text_length; ++word_end)
				{
					char c = buffer[word_end];
					if (c == '\n')
					{
						endline = true;
					}
					if (c == ' ')
					{
						break;
					}
				}

				f32 width = font->text_width(&buffer[i], word_end - i, 0);
				if (pos.x + width > inside.bottom_right.x)
				{
					pos.x = pos_start.x;
					pos.y += font->height;
				}

				//pos.x += painter.draw_text(context, font, )

			}
		}
		else
		{
			f32 x_off = 0.f;
			f32 prev_input_rect_x = 0.f;

			auto fn = [&](usize index, f32 glyph_advance_x)
			{
				f32 input_x_off = x_off + (glyph_advance_x + spacing) / 2.f;
				Rect input_char_rect = Rect::from_2_pos(
					{pos.x + prev_input_rect_x, pos.y},
					{pos.x + input_x_off, pos.y + font->height}
				);
				prev_input_rect_x = input_x_off;
				// DEBUG: painter.draw_rectangle(context, input_char_rect, {0.05f * x_off, 0.5f, 1.f - 0.05f * x_off, 0.3f});

				if (input.pressed && input_char_rect.overlap(mouse))
				{
					cursor_index = (i32)index;
					drag_cursor_index = (i32)index;
				}
				if (input.dragging && input_char_rect.overlap(mouse))
				{
					cursor_index = (i32)index;
				}
				if (index == cursor_index)
				{
					cursor_draw_pos = pos + v2{x_off, 0.f};

					// TODO: Improve cursor by adding 2 gradients
					Rect cursor_rect = Rect::from_pos_size(
						{pos.x + x_off - cursor_width / 2.f, pos.y},
						{cursor_width, font->height}
					);
					painter.draw_rectangle(cursor_rect, cursor_color);
				}
				if (index == drag_cursor_index)
				{
					drag_cursor_draw_pos = pos + v2{x_off, 0.f};
				}
			};

			for (usize i = 0; i < *text_length; ++i)
			{
				Codepoint codepoint = buffer[i];
				const Glyph& glyph = font->get_glyph(codepoint);

				painter.draw_rectangle(pos + v2{x_off, 0} + glyph.pos, glyph.size, text_color, glyph.uv1, glyph.uv2);

				fn(i, glyph.advance_x);

				x_off += glyph.advance_x + spacing;
			}

			fn(*text_length, font->get_glyph((Codepoint)' ').advance_x);

			// Draw selection
			Rect selection_rect = Rect::from_2_pos(cursor_draw_pos, drag_cursor_draw_pos + v2{0, font->height});
			painter.draw_rectangle(selection_rect, selection_color);

			// Adjust camera
			if (cursor_draw_pos.x >= inside.bottom_right.x)
			{
				camera_pos.x += (cursor_draw_pos.x + cursor_width) - inside.bottom_right.x;
			}
			if (cursor_draw_pos.x < inside.top_left.x)
			{
				camera_pos.x -=  inside.top_left.x - cursor_draw_pos.x + cursor_width;
			}

		}

		painter.pop_clip_rect();
	}

	return false;
}

bool input_text(char* buffer, usize buffer_size, bool wrap)
{
	Context* context = get_context();

	usize len = strlen(buffer);
	Rect rect = layout_next({100, 30});
	bool ret = _input_text(12342, rect, buffer, buffer_size, &len, wrap);
	buffer[len] = 0;
	return ret;
}
*/

}
