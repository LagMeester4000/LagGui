#include <iostream>
#include "lag_gui.hpp"
#include "main.h"
#include "raylib.h"
#include "rlgl.h"

#define GREY(f) {f, f, f, 1.f}
#define COLOR_T(r, g, b, t) {(r) * (t), (g) * (t), (b) * (t), 1.f}

int main()
{
	printf("Hello world!\n");

	const int screenWidth = 800;
	const int screenHeight = 800;

	InitWindow(screenWidth, screenHeight, "raylib [core] example - basic window");
	SetWindowState(FLAG_MSAA_4X_HINT);
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
					lgui::Painter& painter = lgui::get_current_panel(context)->painter;
					//painter.draw_circle(context, {500, 500}, 100.f, 0.8, {0.f, 1.f, 1.f, 1.f});
				}

				lgui::end_panel(context);
			}

			if (lgui::begin_panel(context, "Other Window", lgui::Rect::from_pos_size(lgui::v2{100, 100}, lgui::v2{300, 300}), 0))
			{
				if (lgui::button(context, "Button3").clicked)
				{
					printf("Button 3 pressed!\n");
				}

				lgui::checkbox(context, "Checkbox", &test_value2);

				if (lgui::button(context, "Button4").clicked)
				{
					printf("Button 3 pressed!\n");
				}

				lgui::checkbox(context, "Checkbox2", &test_value);

				lgui::end_panel(context);
			}

			lgui::end_frame(context);


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
