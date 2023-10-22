#include "lag_gui.hpp"
#include "basic.hpp"

#include <stdio.h>

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
	RetainedData* retained = get_retained_data(id);
	const Style& style = get_style();

	// Rect
	Font* font = style.default_font;
	f32 text_width = font->text_width(name, 0);
	Box* box = make_box(name, px(text_width + 4, font->height), BoxFlag_DrawText | BoxFlag_DrawRectangle);
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

static v2 _wrapped_text_size(const char* text, usize len, f32 width, Font* font)
{

}

void text_wrapped(const char* text, Size width, bool static_string)
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

	bool ret = true;
	if (retained->open && retained->active_t >= 0.99f)
	{
		{
			layout_horizontal(-1, 1, {fit(), fit()});
			spacer(tab);
		}

		Box* inner = layout_vertical(-1, 1, {fit(), fit()});
		inner->padding = padding;
	}
	else if (retained->active_t > 0.001f)
	{
		{
			layout_horizontal(-1, 1, {fit(), fit()});
			spacer(tab);
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

/*
InputResult button(const char* name)
{
	ID id = get_id(name);
	RetainedData* retained = get_retained_data(id);
	const Style& style = get_style();

	// Rect
	auto& painter = get_current_panel()->get_painter();
	Font* font = style.default_font;
	f32 text_width = font->text_width(name, 0);
	Rect rect = layout_next({text_width + 4, font->height});

	InputResult input = handle_element_input(rect, id);
	retained->update_t_towards(input.hover, input.down);
	Color color = lerp_color(lerp_color(style.button_background, style.button_background_hover, retained->hover_t), style.button_background_down, retained->active_t);

	// Rendering
	Rect text_rect = rect.center_size({text_width, font->height});
	painter.draw_rectangle(rect, color);
	painter.draw_text(font, name, text_rect.top_left, 0, {1, 1, 1, 1});

	return input;
}

InputResult checkbox(const char* name, bool* value)
{
	ID id = get_id(name);
	RetainedData* retained = get_retained_data(id);
	const Style& style = get_style();

	// Rect
	auto& painter = get_current_panel()->get_painter();
	Rect rect = layout_next({style.line_height(), style.line_height()});

	InputResult input = handle_element_input(rect, id);
	if (input.pressed)
	{
		*value = !*value;
	}

	retained->update_t_towards(input.hover, *value, 30);
	Color color = lerp_color(style.button_background, style.button_background_hover, retained->hover_t);

	// Rendering
	Rect inner = rect.pad(1);
	Rect checked = rect.pad(lerp(7, 5, retained->active_t));
	Color checked_color = {1, 1, 1, retained->active_t};
	painter.draw_rectangle(rect, style.checkbox_outline);
	painter.draw_rectangle(inner, color);
	painter.draw_rectangle(checked, checked_color);

	return input;
}

InputResult radio_button(const char* name, int option, int* selected)
{
	ID id = get_id(name);
	RetainedData* retained = get_retained_data(id);
	const Style& style = get_style();

	// Rect
	auto& painter = get_current_panel()->get_painter();
	Rect rect = layout_next({style.line_height(), style.line_height()});

	InputResult input = handle_element_input(rect, id);
	if (input.clicked)
	{
		*selected = option;
	}

	retained->update_t_towards(input.hover, *selected == option, 30);
	Color color = lerp_color(style.button_background, style.button_background_hover, retained->hover_t);

	// Rendering
	Rect inner = rect.pad(1);
	Rect checked = rect.pad(lerp(7, 5, retained->active_t));
	Color checked_color = {1, 1, 1, retained->active_t};
	painter.draw_circle(rect.center(), rect.height() / 2.f, 1, style.checkbox_outline);
	painter.draw_circle(inner.center(), inner.height() / 2.f, 1, color);
	painter.draw_circle(checked.center(), checked.height() / 2.f, 1, checked_color);

	return input;
}

InputResult drag_value(const char* name, f32* value)
{
	ID id = get_id(name);
	RetainedData* retained = get_retained_data(id);
	const Style& style = get_style();

	// Make text
	char text[16];
	sprintf_s(text, "%.3f", *value);

	// Rect
	auto& painter = get_current_panel()->get_painter();
	Font* font = style.default_font;
	f32 text_width = font->text_width(text, 0);
	Rect rect = layout_next({128, font->height});

	InputResult input = handle_element_input(rect, id, true);
	if (input.dragging && input.drag_delta.x != 0)
	{
		*value += input.drag_delta.x;
	}

	retained->update_t_towards(input.hover, input.down);
	Color color = lerp_color(lerp_color(style.button_background, style.button_background_hover, retained->hover_t), style.button_background_down, retained->active_t);

	// Rendering
	Rect text_rect = rect.center_size({text_width, font->height});
	painter.draw_rectangle(rect, color);
	painter.draw_text(font, text, text_rect.top_left, 0, {1, 1, 1, 1});

	return input;
}

// TODO: This could also use the layout to allocate a new rect for every line, that way the layout can decide the... layout of the text
// Returns height of text written
f32 text_in_rect(Painter* painter, Font* font, v2 pos, f32 width, const char* text, usize text_length, f32 spacing, Color color)
{
	f32 initial_width = font->text_width(text, text_length, spacing);
	if (initial_width <= width)
	{
		painter->draw_text(font, text, text_length, pos, spacing, color);
		return font->height;
	}
	else
	{
		usize offset = 0;
		f32 height = 0.f;

		// TODO: This has a bug that causes the last character to not render
		while (offset < text_length)
		{
			usize to_draw = font->find_text_width_fit(&text[offset], text_length - offset, spacing, width);
			to_draw = to_draw > 0 ? to_draw - 1 : 0;
			if (to_draw == 0) break;

			painter->draw_text(font, &text[offset], to_draw, pos, spacing, color);

			pos.y += font->height;
			height += font->height;
			offset += to_draw;
		}

		return height;
	}
}

void text(const char* text, bool wrap)
{
	const Style& style = get_style();

	if (wrap)
	{
		// TODO: Change to work with new layout system
		Panel* panel = get_current_panel();
		f32 height = text_in_rect(&panel->get_painter(), style.default_font, {}, panel->content.width(), text, strlen(text), 0, style.window_title_color);
	}
	else
	{
		Font* font = style.default_font;
		Rect rect = layout_next({font->text_width(text, 0.f), font->height});
		Painter& painter = get_painter();
		painter.draw_text(font, text, rect.top_left, 0, style.window_title_color);
	}
}

static void memmove_safe(void* destination, usize destination_size, const void* source, usize source_size)
{
	usize min_size = LGUI_MIN(destination_size, source_size);
	if (min_size > 0)
	{
		memmove(destination, source, min_size);
	}
}

static void string_remove_range(char* buffer, usize buffer_size, usize* text_length, usize insert_pos,
								usize insert_size)
{

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

bool collapse_header(const char* name)
{
	ID id = get_id(name);
	Panel* panel = get_current_panel();
	RetainedData* retained = get_retained_data(id);
	const Style& style = get_style();

	// Rect
	auto& painter = get_current_panel()->get_painter();
	Font* font = style.default_font;
	f32 text_width = font->text_width(name, 0);
	Rect rect = layout_next({layout_width(), style.line_height() - 2});

	InputResult input = handle_element_input(rect, id, true);
	if (input.clicked)
	{
		retained->open = !retained->open;
	}

	retained->update_t_towards(input.hover, retained->open);
	Color color = lerp_color(lerp_color(style.button_background, style.button_background_hover, retained->hover_t), style.button_background_down, retained->active_t);

	// Rendering
	painter.draw_rectangle(rect, color);
	Rect arrow_rect = rect.cut_left(rect.height()).pad(6);
	draw_open_triangle(&painter, arrow_rect.top_left, arrow_rect.width(), lerp(0, 90, retained->active_t), {1, 1, 1, 1});
	rect.cut_left(2); // Pad
	Rect text_rect = rect.align_size({text_width, font->height}, -1, 0);
	painter.draw_text(font, name, text_rect.top_left, 0, {1, 1, 1, 1});

	return retained->open;
}

void separator()
{
	Panel* panel = get_current_panel();
	const Style& style = get_style();

	// Rect
	auto& painter = get_current_panel()->get_painter();
	Font* font = style.default_font;
	Rect rect = layout_next({layout_width(), style.line_height()});

	// Rendering
	Color color = style.window_outline;
	f32 take = rect.height() / 10;
	painter.draw_rectangle(rect.center_size({rect.width() - take, take}), color);
}

void spacer(f32 size)
{
	Box& layout = get_layout();
	if (layout.flags & BoxFlag_IsHorizontal)
	{
		layout.allocate({size, 1.f});
	}
	else
	{
		layout.allocate({1.f, size});
	}
}

bool begin_fancy_collapse_header(const char* name)
{
	ID id = get_id(name);
	push_id_raw(id);
	RetainedData* retained = get_retained_data(id);
	const Style& style = get_style();

	lgui::layout_vertical(-1, -1, {layout_width(), 0.f}, false, 0.f);
	bool open = collapse_header(name);
	retained->open = open;
	retained->update_t_towards(false, open, 20.f);

	v2 pad = {3.f, 3.f};

	bool ret = true;
	if (open && retained->active_t >= 0.99f)
	{
		lgui::set_next_layout_background(style.window_title_background, style.window_outline, 2);
		lgui::layout_vertical(-1, 1, {layout_width(), 0.f}, false, -1, pad, BoxFlag_Clip | BoxFlag_FixedH);
	}
	else if (retained->active_t > 0.001f)
	{
		lgui::set_next_layout_background(style.window_title_background, style.window_outline, 2);
		lgui::layout_vertical(-1, 1, {layout_width(), retained->value_v2.y * retained->active_t}, false, -1.f, pad, BoxFlag_Clip | BoxFlag_FixedH | BoxFlag_FixedV);
	}
	else
	{
		ret = false;
		end_layout();
		pop_id();
	}

	return ret;
}

void end_fancy_collapse_header()
{
	v2 size = lgui::get_layout().get_stretched_size();
	end_layout();
	end_layout();
	ID id = peek_id();
	pop_id();

	RetainedData* retained = get_retained_data(id);
	retained->value_v2 = size;
}
*/

}
