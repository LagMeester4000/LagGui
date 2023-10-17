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

}
