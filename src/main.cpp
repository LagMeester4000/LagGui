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
	if (lgui::begin_panel(context, "Area", Rect::from_pos_size({}, {500, 500}), 0))
	{
		lgui::Panel* panel = lgui::get_current_panel(context);
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
			painter.draw_rectangle(context, line, line_color);
		}
		for (float y = -pos_screen_offset.y; y < rect.height(); y += cell_size.y)
		{
			float screen_y = rect.top_left.y + y;
			Rect line = Rect::from_pos_size({ rect.top_left.x, screen_y }, { rect.width(), 1 });
			painter.draw_rectangle(context, line, line_color);
		}


		lgui::InputResult rect_response = lgui::handle_element_input(context, rect, lgui::get_id(context, "background"), true);
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

			painter.draw_rectangle(context, note_rect, note_outline_color);
			painter.draw_rectangle_gradient(context, note_rect.pad(1), note_top_color, note_top_color, note_bottom_color, note_bottom_color);

			lgui::push_id(context, it.id);

			Rect right_side = note_rect.cut_right(5);
			lgui::InputResult input_main = lgui::handle_element_input(context, note_rect, lgui::get_id(context, "main"), true);
			lgui::InputResult input_right = lgui::handle_element_input(context, right_side, lgui::get_id(context, "right"), true);

			if (input_main.pressed)
			{
				area.note_on_press = it;
				area.mouse_on_press = (lgui::mouse_pos(context) - rect.top_left + pos) / cell_size;
			}
			if (input_main.dragging)
			{
				float x_offset = area.note_on_press.x - area.mouse_on_press.x;
				v2 mouse_local = (lgui::mouse_pos(context) - rect.top_left + pos) / cell_size;
				float mouse_x = mouse_local.x + x_offset;
				it.x = mouse_x - fmodf(mouse_x, width_snap);
				it.tone = (int)mouse_local.y;
			}
			if (input_right.dragging)
			{
				v2 mouse_local = (lgui::mouse_pos(context) - rect.top_left + pos) / cell_size;
				it.width = mouse_local.x - fmodf(mouse_local.x, width_snap) - it.x;
				it.width = LGUI_MAX(it.width, 0.05f);
			}
			if (input_main.clicked || input_right.clicked)
			{
				// Must be done last so it doesn't invalidate the pointer
				area.notes.erase(area.notes.begin() + i);
				--i;
			}

			lgui::pop_id(context);
		}

		if (rect_response.clicked)
		{
			v2 mouse_local = (lgui::mouse_pos(context) - rect.top_left + pos) / cell_size;
			Note note{};
			note.x = mouse_local.x;
			note.width = 0.25f;
			note.tone = (int)mouse_local.y;
			++area.id_counter;
			note.id = area.id_counter;
			area.notes.push_back(note);
		}

	
		lgui::end_panel(context);
	}
}


int main()
{
	printf("Hello world!\n");

	const int screenWidth = 800;
	const int screenHeight = 800;

	InitWindow(screenWidth, screenHeight, "raylib [core] example - basic window");
	SetWindowState(FLAG_MSAA_4X_HINT | FLAG_WINDOW_RESIZABLE);
	SetTargetFPS(60);

	lgui::Context* context = lgui::create_context();

	lgui::Font* font = context->atlas.add_font(context, "Montserrat-Regular.ttf", 18);
	context->atlas.build(context);

	lgui::Style style{};
	style.default_font = font;
	style.line_padding = 4.f;
	style.window_outline = GREY(0.15);
	style.window_background = GREY(0.3);
	style.window_title_background = GREY(0.2);
	style.window_title_color = GREY(0.9);
	style.button_background = COLOR_T(0.4, 0.4, 0.9, 1.f);
	style.button_background_hover = COLOR_T(0.4, 0.4, 0.9, 0.8f);
	style.button_background_down = COLOR_T(0.4, 0.4, 0.9, 0.5f);
	style.button_padding = 2.f;
	style.checkbox_outline = COLOR_T(0.6, 0.6, 1.0, 1.f);
	lgui::push_style(context, style);

	bool test_value = false;
	bool test_value2 = false;
	int test_radio_value = 0;
	float test_drag_value = 0;
	float test_text_fit_value = 100;

	NoteArea area{};
	area.scale = {1, 1};

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

			lgui::begin_frame(context);

			if (lgui::begin_panel(context, "My Window", lgui::Rect::from_pos_size(lgui::v2{100, 100}, lgui::v2{300, 300}), 0))
			{
				if (lgui::button(context, "Button1").clicked)
				{
					printf("Button 1 pressed!\n");
				}

				if (lgui::button(context, "Button2").clicked)
				{
					printf("Button 2 pressed!\n");
				}

				lgui::checkbox(context, "Checkbox", &test_value);

				{
					lgui::Painter& painter = lgui::get_current_panel(context)->get_painter();
					//painter.draw_circle(context, {500, 500}, 100.f, 0.8, {0.f, 1.f, 1.f, 1.f});
				}

				lgui::end_panel(context);
			}

			if (lgui::begin_panel(context, "Other Window", lgui::Rect::from_pos_size(lgui::v2{300, 100}, lgui::v2{300, 500}), 0))
			{
				if (lgui::button(context, "Button3").clicked)
				{
					printf("Button 3 pressed!\n");
				}

				lgui::checkbox(context, "Checkbox", &test_value2);

				lgui::text(context, "");
				lgui::text(context, "New elements:");

				lgui::radio_button(context, "Radio1", 1, &test_radio_value);
				lgui::radio_button(context, "Radio2", 2, &test_radio_value);
				lgui::radio_button(context, "Radio3", 3, &test_radio_value);

				lgui::drag_value(context, "drag value", &test_drag_value);

				// draw_text_fit test
				if (lgui::collapse_header(context, "draw_text_fit test"))
				{
					lgui::text(context, "This is a piece of text that wraps around when reaching the end of the window, soon I will make it wrap on spaces instead of at any character ", true);

					lgui::drag_value(context, "fit value", &test_text_fit_value);

					lgui::Painter& painter = lgui::get_current_panel(context)->get_painter();
					//const char* text = "This is my long text that will be cut off eventually";
					const char* text = "This is my text that will be cut off";
					for (int i = -1; i < 2; ++i)
					{
						lgui::v2 pos = lgui::layout_next(context);
						lgui::Rect rect = lgui::Rect::from_pos_size(pos, {5 + test_text_fit_value, 25});
						painter.draw_rectangle(context, rect, {0, 0, 0, 1});
						painter.draw_text_fit(context, font, text, rect, 0, {1, 1, 1, 1}, i, 0);
					}

					lgui::v2 pos2 = lgui::layout_next(context);
					painter.draw_text(context, font, text, pos2, 0, {1, 1, 1, 1});
				}

				lgui::end_panel(context);
			}

			area_test(context, area);

			lgui::end_frame(context);


			/*
			//rlBegin(RL_QUADS);
			rlBegin(RL_TRIANGLES);
			//rlColor4ub(255, 200, 255, 255);
			rlColor4ub(255, 0, 0, 255);
			//rlBindImageTexture(context->atlas.texture_id, 0, RL_PIXELFORMAT_UNCOMPRESSED_R8G8B8A8, true);
			rlEnableTexture(context->atlas.texture_id);
			rlSetTexture(context->atlas.texture_id);
			int w = 512;
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
			*/

			//rlEnableTexture(context->atlas.texture_id);
			//DrawRectangle(10, 10, 100, 100, { 255, 200, 255, 255 });
			//DrawTexture(context->atlas.texture_obj, 10, 10, {255, 255, 255, 255});
			//DrawTextureV(context->atlas.texture_obj, { 10, 10 }, { 255, 255, 255, 255 });
			//DrawTextureEx(context->atlas.texture_obj, { 10, 10 }, 0, 4, { 255, 255, 255, 255 });
			//DrawTextureEx(context->atlas.texture_obj, { 10, 10 }, 0, 1, { 0, 0, 0, 255 });

			//DrawTexture(a)

			DrawFPS(1, 1);
		EndDrawing();
	}

	CloseWindow();

	return 0;
}
