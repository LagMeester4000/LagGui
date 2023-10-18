#include <iostream>
#include "lag_gui.hpp"
#include "main.h"
#include "raylib.h"
#include "rlgl.h"
#include <vector>

#define GREY(f) {f, f, f, 1.f}
#define COLOR_T(r, g, b, t) {(r) * (t), (g) * (t), (b) * (t), 1.f}

using v2 = lgui::v2;
using Rect = lgui::Rect;
using f32 = lgui::f32;
using Rect = lgui::Rect;

lgui::Font* g_font = 0;


/*
// Idea for easier 2d area implementations
struct AreaHelper {
	Rect screen_rect;

	// Camera
	v2 pos;
	v2 scale;

	// The size of a unit in pixels when scale = 1
	v2 cell_size;

	v2 local_to_screen(v2 local_pos) const;
	Rect local_to_screen(v2 local_pos, v2 size) const;

	v2 screen_to_local(v2 screen_pos) const;
	Rect screen_to_local(v2 screen_pos, v2 size) const;

	v2 screen_cell_size(v2 cell_size) const;

	v2 loop_offset(v2 cell_size) const;
	v2 loop_stride(v2 cell_size) const;

	// Iterator
	AreaIterator begin()
	{
		return ...;
	}

	AreaIterator end()
	{
		AreaIterator ret{};
		ret.done = true;
		return ret;
	}
};

// I think it's important to also add an iterator
struct AreaIterator {
	// Internal
	bool done;
	bool is_horizontal;
	v2 start_pos_pixels;
	f32 end_pos_pixels;
	int start_index;
	f32 offset_pixels; // Offset for start_pos (negative number)
	f32 cell_size_pixels; // Advance value

	// Iterator functions
	bool operator==(const AreaIterator& other) const { return done == other.done; }
	// ++ operator blah blah

	// Interface
	int cell_index; // Very useful for things like alternating background colors, or piano keys in piano roll
	f32 position; // Position along its axis
	f32 size; // Size along its axis (vertical or horizontal)
};
*/


struct Note {
	int tone;
	float x;
	float width;
	int id;
};

struct NoteArea {
	v2 pos;
	v2 scale;
	std::vector<Note> notes;
	int id_counter;

	v2 mouse_on_press;
	Note note_on_press;
};

void area_test(lgui::Context* context, NoteArea& area)
{
	if (lgui::begin_panel("Area", Rect::from_pos_size({}, {500, 500}), 0))
	{
		lgui::Panel* panel = lgui::get_current_panel();
		lgui::Painter& painter = panel->get_painter();

		// The whole area
		Rect rect = panel->content;

		v2 base_cell_size = v2{100, 10};
		v2 cell_size = base_cell_size * area.scale;
		v2 pos = area.pos * cell_size;
		v2 pos_screen_offset = v2{fmodf(pos.x / cell_size.x, 1.f), fmodf(pos.y / cell_size.y, 1.f)} * cell_size;

		float width_snap = 0.125f;

		// Style
		lgui::Color line_color = {0, 0, 0, 1};
		lgui::Color note_outline_color =  COLOR_T(1.f, 0.2f, 0.2f, 0.7f);
		lgui::Color note_top_color =  COLOR_T(1.f, 0.2f, 0.2f, 1.f);
		lgui::Color note_bottom_color =  COLOR_T(1.f, 0.2f, 0.2f, 0.7f);


		// Draw lines
		for (float x = -pos_screen_offset.x; x < rect.width(); x += cell_size.x)
		{
			float screen_x = rect.top_left.x + x;
			Rect line = Rect::from_pos_size({ screen_x, rect.top_left.y }, { 1, rect.height() });
			painter.draw_rectangle(line, line_color);
		}
		for (float y = -pos_screen_offset.y; y < rect.height(); y += cell_size.y)
		{
			float screen_y = rect.top_left.y + y;
			Rect line = Rect::from_pos_size({ rect.top_left.x, screen_y }, { rect.width(), 1 });
			painter.draw_rectangle(line, line_color);
		}


		lgui::InputResult rect_response = lgui::handle_element_input(rect, lgui::get_id("background"), true);
		if (rect_response.dragging)
		{
			area.pos -= rect_response.drag_delta / cell_size;
			area.pos.x = LGUI_MAX(area.pos.x, 0.f);
			area.pos.y = LGUI_MAX(area.pos.y, 0.f);
		}

		if (IsKeyPressed(KEY_Q))
		{
			area.scale.x -= 0.2f;
			area.scale.x = LGUI_MAX(area.scale.x, 0.1f);
		}
		if (IsKeyPressed(KEY_E))
		{
			area.scale.x += 0.2f;
		}
		if (IsKeyPressed(KEY_A))
		{
			area.scale.y -= 0.2f;
			area.scale.y = LGUI_MAX(area.scale.y, 0.1f);
		}
		if (IsKeyPressed(KEY_D))
		{
			area.scale.y += 0.2f;
		}

		for (size_t i = 0; i < area.notes.size(); ++i)
		{
			auto& it = area.notes[i];

			Rect note_rect = Rect::from_pos_size(rect.top_left - pos + cell_size * v2{it.x, (float)it.tone},
				v2{it.width * cell_size.x, cell_size.y});
			note_rect.cut_bottom(-1);

			painter.draw_rectangle(note_rect, note_outline_color);
			painter.draw_rectangle_gradient(note_rect.pad(1), note_top_color, note_top_color, note_bottom_color, note_bottom_color);

			lgui::push_id(it.id);

			Rect right_side = note_rect.cut_right(5);
			lgui::InputResult input_main = lgui::handle_element_input(note_rect, lgui::get_id("main"), true);
			lgui::InputResult input_right = lgui::handle_element_input(right_side, lgui::get_id("right"), true);

			if (input_main.pressed)
			{
				area.note_on_press = it;
				area.mouse_on_press = (lgui::mouse_pos() - rect.top_left + pos) / cell_size;
			}
			if (input_main.dragging)
			{
				float x_offset = area.note_on_press.x - area.mouse_on_press.x;
				v2 mouse_local = (lgui::mouse_pos() - rect.top_left + pos) / cell_size;
				float mouse_x = mouse_local.x + x_offset;
				it.x = mouse_x - fmodf(mouse_x, width_snap);
				it.tone = (int)mouse_local.y;
			}
			if (input_right.dragging)
			{
				v2 mouse_local = (lgui::mouse_pos() - rect.top_left + pos) / cell_size;
				it.width = mouse_local.x - fmodf(mouse_local.x, width_snap) - it.x;
				it.width = LGUI_MAX(it.width, 0.05f);
			}
			if (input_main.clicked || input_right.clicked)
			{
				// Must be done last so it doesn't invalidate the pointer
				area.notes.erase(area.notes.begin() + i);
				--i;
			}

			lgui::pop_id();
		}

		if (rect_response.clicked)
		{
			v2 mouse_local = (lgui::mouse_pos() - rect.top_left + pos) / cell_size;
			Note note{};
			note.x = mouse_local.x;
			note.width = 0.25f;
			note.tone = (int)mouse_local.y;
			++area.id_counter;
			note.id = area.id_counter;
			area.notes.push_back(note);
		}

		lgui::end_panel();
	}
}

extern void ast_init();
extern void ast_update(lgui::Context* context);

int main()
{
	const int screenWidth = 800;
	const int screenHeight = 800;

	InitWindow(screenWidth, screenHeight, "raylib [core] example - basic window");
	SetWindowState(FLAG_MSAA_4X_HINT | FLAG_WINDOW_RESIZABLE);
	SetTargetFPS(60);

	lgui::Context* context = lgui::init();
	context->app_window_size = {(f32)screenWidth, (f32)screenHeight};

	lgui::Font* font = context->atlas.add_font("resources/fonts/montserrat/Montserrat-Regular.ttf", 18);
	lgui::Font* mono_font = context->atlas.add_font("resources/fonts/ubuntu/UbuntuMono-R.ttf", 18);
	context->atlas.build();
	g_font = mono_font;

	lgui::Style style{};
	style.default_font = font;
	style.line_padding = 4.f;
	style.default_layout_spacing = 2.f;
	style.window_outline = GREY(0.15f);
	style.window_background = GREY(0.3f);
	style.window_title_background = GREY(0.2f);
	style.window_title_color = GREY(0.9f);
	style.button_background = COLOR_T(0.4f, 0.4f, 0.9f, 1.f);
	style.button_background_hover = COLOR_T(0.4f, 0.4f, 0.9f, 0.8f);
	style.button_background_down = COLOR_T(0.4f, 0.4f, 0.9f, 0.5f);
	style.button_padding = 2.f;
	style.checkbox_outline = COLOR_T(0.6f, 0.6f, 1.0f, 1.f);
	lgui::push_style(style);

	bool test_value = false;
	bool test_value2 = false;
	int test_radio_value = 0;
	float test_drag_value = 0;
	float test_text_fit_value = 100;
	char text_buffer[32]{};
	size_t text_buffer_size = 32;

	bool test_layout_reverse = false;
	bool test_layout_horizontal = false;
	int test_layout_h_align = -1;
	int test_layout_v_align = -1;
	bool test_layout_enable = false;
	int test_layout_count = 0;

	NoteArea area{};
	area.scale = {1, 1};

	ast_init();

	while (!WindowShouldClose())
	{
		if (IsKeyPressed(KEY_F11))
		{
			ToggleFullscreen();
		}

		BeginDrawing();

		//ClearBackground(RAYWHITE);
		ClearBackground(SKYBLUE);

		rlDisableBackfaceCulling();

		// Enable for frame by frame
		if (IsKeyPressed(KEY_ENTER) || IsKeyDown(KEY_RIGHT_SHIFT))
		{
			lgui::begin_frame(GetFrameTime());

			/*
			// Stress test
			for (int i = 0; i < 30; ++i)
			{
				lgui::push_id(i);
				if (lgui::begin_panel("My Window", lgui::Rect::from_pos_size(lgui::v2{(f32)i * 2, (f32)i * 2}, lgui::v2{100, 100}), 0))
				{
					lgui::end_panel();
				}
				lgui::pop_id();
			}
			*/

			if (lgui::begin_panel("Layout test", lgui::Rect::from_pos_size(lgui::v2{250, 250}, lgui::v2{450, 450}), 0))
			{
				if (lgui::layout_line(-1))
				{
					lgui::checkbox("Layout reverse", &test_layout_reverse);
					lgui::text("Layout reverse");
					lgui::end_layout();
				}
				if (lgui::begin_fancy_collapse_header("hello sir"))
				{
					LGUI_H_LAYOUT(-1, 0) { lgui::text("hello 1"); lgui::button("BUTTON"); }
					LGUI_H_LAYOUT(-1, 0) { lgui::text("hello 2"); lgui::button("BUTTON"); }
					LGUI_H_LAYOUT(-1, 0) { lgui::text("hello 3"); lgui::checkbox("BUTTON", &test_value); }
					LGUI_H_LAYOUT(-1, 0) { lgui::text("hello 4"); lgui::button("BUTTON"); }
					LGUI_H_LAYOUT(-1, 0) { lgui::text("hello 5"); lgui::button("BUTTON"); }
					lgui::end_fancy_collapse_header();
				}
				if (lgui::layout_line(-1))
				{
					lgui::checkbox("Layout horizontal", &test_layout_horizontal);
					lgui::text("Layout horizontal");
					lgui::end_layout();
				}
				if (lgui::layout_line(-1))
				{
					lgui::checkbox("layout enable", &test_layout_enable);
					lgui::text("Layout enable");
					lgui::end_layout();
				}
				if (lgui::layout_line(-1))
				{
					if (lgui::button("-").clicked && test_layout_count > 0)
					{
						--test_layout_count;
					}
					if (lgui::button("+").clicked)
					{
						++test_layout_count;
					}

					const int buffer_size = 32;
					char buffer[buffer_size]{};
					snprintf(buffer, buffer_size, "Layout count (%d)", test_layout_count);
					lgui::text(buffer);

					lgui::end_layout();
				}

				if (lgui::layout_line(-1))
				{
					lgui::text("Layout h_align: ");
					lgui::radio_button("Layout h_align -1", -1, &test_layout_h_align);
					lgui::radio_button("Layout h_align 0", 0, &test_layout_h_align);
					lgui::radio_button("Layout h_align 1", 1, &test_layout_h_align);

					/*
					{
						Rect rect = lgui::layout_next({100, 100});
						lgui::Painter& painter = lgui::get_painter();
						f32 c[4] = {10.f, 10.f, 10.f, 10.f};
						painter.draw_rounded_rectangle(rect, c, {0, 1, 0, 0.5f});
					}
					*/

					lgui::end_layout();
				}
				if (lgui::layout_line(-1))
				{
					lgui::text("Layout v_align: ");
					lgui::radio_button("Layout v_align -1", -1, &test_layout_v_align);
					lgui::radio_button("Layout v_align 0", 0, &test_layout_v_align);
					lgui::radio_button("Layout v_align 1", 1, &test_layout_v_align);
					lgui::end_layout();
				}

				lgui::separator();

				if (test_layout_enable)
				{
					lgui::set_next_layout_background(GREY(0.4f), GREY(0.6f), 2.f);
					//if (lgui::layout_unknown(10, { lgui::layout_width(), 0.f }, test_layout_horizontal, test_layout_reverse, test_layout_h_align, test_layout_v_align, 2.f, {8.f, 8.f}))
					//if (lgui::layout_unknown(10, { 0.f, 0.f }, test_layout_horizontal, test_layout_reverse, test_layout_h_align, test_layout_v_align, 2.f, {8.f, 8.f}))
					if (lgui::layout_unknown(10, { lgui::layout_width(), 0.f }, test_layout_horizontal, test_layout_reverse, test_layout_h_align, test_layout_v_align, 2.f, { 8.f, 8.f }, lgui::LayoutFlag_FixedH))
					{
						if (lgui::button("Button1").clicked)
						{
							printf("Button 1 pressed!\n");
						}
						if (lgui::button("Button2").clicked)
						{
							printf("Button 2 pressed!\n");
						}
						if (lgui::button("Button3").clicked)
						{
							printf("Button 3 pressed!\n");
						}
						/*
						LGUI_V_LAYOUT(-1)
						{
							lgui::checkbox("Button2", &test_value);
							lgui::radio_button("Button3", 1, &test_radio_value);

							for (int i = 0; i < test_layout_count; ++i)
							{
								lgui::push_id(i);

								if (lgui::button("Button1").clicked)
								{
									printf("Button 1 pressed!\n");
								}
								lgui::radio_button("Button3", 1, &test_radio_value);

								lgui::pop_id();
							}
						}
						*/


						lgui::end_layout();
					}
				}

				lgui::end_panel();
			}

			/*
			if (lgui::begin_panel("My Window", lgui::Rect::from_pos_size(lgui::v2{100, 100}, lgui::v2{300, 300}), 0))
			{
				if (lgui::button("Button1").clicked)
				{
					printf("Button 1 pressed!\n");
				}

				if (lgui::button("Button2").clicked)
				{
					printf("Button 2 pressed!\n");
				}

				lgui::checkbox("Checkbox", &test_value);

				lgui::end_panel();
			}
			*/


			if (false && lgui::begin_panel("Other Window", lgui::Rect::from_pos_size(lgui::v2{300, 100}, lgui::v2{300, 500}), 0))
			{
				if (lgui::button("Button3").clicked)
				{
					printf("Button 3 pressed!\n");
				}

				lgui::checkbox("Checkbox", &test_value2);

				lgui::text("");
				lgui::text("New elements:");

				lgui::radio_button("Radio1", 1, &test_radio_value);
				lgui::radio_button("Radio2", 2, &test_radio_value);
				lgui::radio_button("Radio3", 3, &test_radio_value);

				lgui::drag_value("drag value", &test_drag_value);

				lgui::input_text(text_buffer, text_buffer_size);

				// draw_text_fit test
				if (lgui::collapse_header("draw_text_fit test"))
				{
					lgui::text("This is a piece of text that wraps around when reaching the end of the window, soon I will make it wrap on spaces instead of at any character ", true);

					lgui::drag_value("fit value", &test_text_fit_value);

					lgui::Painter& painter = lgui::get_current_panel()->get_painter();
					//const char* text = "This is my long text that will be cut off eventually";
					const char* text = "This is my text that will be cut off";
					for (int i = -1; i < 2; ++i)
					{
						lgui::Rect rect = lgui::layout_next({ 5 + test_text_fit_value, 25 });
						painter.draw_rectangle(rect, { 0, 0, 0, 1 });
						painter.draw_text_fit(font, text, rect, 0, { 1, 1, 1, 1 }, i, 0);
					}

					Rect pos2 = lgui::layout_next({ 10, 10 });
					painter.draw_text(font, text, pos2.top_left, 0, { 1, 1, 1, 1 });
				}

				lgui::end_panel();
			}

			//lgui::debug_menu();

			//area_test(area);

			//ast_update();

			lgui::end_frame();


#if 0
			// Draw font atlas
			rlBegin(RL_TRIANGLES);
			rlColor4ub(255, 255, 255, 255);
			rlEnableTexture(context->atlas.texture_id);
			rlSetTexture(context->atlas.texture_id);
			f32 w = 512.f;
			rlTexCoord2f(0, 0);
			rlVertex2f(0, 0);
			rlTexCoord2f(1, 0);
			rlVertex2f(w, 0);
			rlTexCoord2f(0, 1);
			rlVertex2f(0, w);

			rlTexCoord2f(1, 0);
			rlVertex2f(w, 0);
			rlTexCoord2f(0, 1);
			rlVertex2f(0, w);
			rlTexCoord2f(1, 1);
			rlVertex2f(w, w);
			rlEnd();
#endif

			//rlEnableTexture(context->atlas.texture_id);
			//DrawRectangle(10, 10, 100, 100, { 255, 200, 255, 255 });
			//DrawTexture(context->atlas.texture_obj, 10, 10, {255, 255, 255, 255});
			//DrawTextureV(context->atlas.texture_obj, { 10, 10 }, { 255, 255, 255, 255 });
			//DrawTextureEx(context->atlas.texture_obj, { 10, 10 }, 0, 4, { 255, 255, 255, 255 });
			//DrawTextureEx(context->atlas.texture_obj, { 10, 10 }, 0, 1, { 0, 0, 0, 255 });

			//DrawTexture(a)

		}

		lgui::draw_frame();
		DrawFPS(1, 1);
		EndDrawing();
	}

	lgui::deinit();

	CloseWindow();

	return 0;
}
