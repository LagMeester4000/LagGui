#include <iostream>
#include "lag_gui.hpp"
#include "main.h"
#include "raylib.h"
#include "rlgl.h"

int main()
{
	printf("Hello world!\n");

	const int screenWidth = 800;
	const int screenHeight = 450;

	InitWindow(screenWidth, screenHeight, "raylib [core] example - basic window");
	SetTargetFPS(60);

	lgui::Context* context = lgui::create_context();

	while (!WindowShouldClose())
	{
		BeginDrawing();

			ClearBackground(RAYWHITE);

			DrawText("Congrats! You created your first window!", 190, 200, 20, LIGHTGRAY);

			rlDisableBackfaceCulling();

			lgui::begin_frame();

			if (lgui::begin_panel(context, "My Window", lgui::Rect::from_pos_size(lgui::v2{100, 100}, lgui::v2{300, 300}), 0))
			{
				if (lgui::button(context, "hehehehehe"))
				{
					printf("Pressed!\n");
				}

				lgui::end_panel(context);
			}

			lgui::end_frame();

		EndDrawing();
	}

	CloseWindow();

	return 0;
}
