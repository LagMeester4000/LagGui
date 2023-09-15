#include "basic.hpp"
#include "lag_gui.hpp"
#include "raylib.h"
#include "rlgl.h"

namespace lgui {

void set_clip_rect(v2 start, v2 end)
{
	BeginScissorMode(start.x, start.y, end.x - start.x, end.y - start.y);
}

void reset_clip_rect()
{
	EndScissorMode();
}

void push_clip_rect(Context* context, Rect rect)
{
	LGUI_ASSERT(context->clip_rect_stack_top < MAX_CLIP_RECT, "Too many clip rects");

}

void pop_clip_rect(Context* context)
{
	LGUI_ASSERT(context->clip_rect_stack_top > 0, "No clip rects to pop");

}

void draw_triangle(v2 p1, v2 p2, v2 p3, Color color)
{
	rlBegin(RL_TRIANGLES);
	rlColor4f(color.r, color.g, color.b, color.a);
	rlVertex2f(p1.x, p1.y);
	rlVertex2f(p2.x, p2.y);
	rlVertex2f(p3.x, p3.y);
	rlEnd();
}

void draw_triangle(v2 p1, v2 p2, v2 p3, Color c1, Color c2, Color c3)
{
	rlBegin(RL_TRIANGLES);
	rlColor4f(c1.r, c1.g, c1.b, c1.a);
	rlVertex2f(p1.x, p1.y);
	rlColor4f(c2.r, c2.g, c2.b, c2.a);
	rlVertex2f(p2.x, p2.y);
	rlColor4f(c3.r, c3.g, c3.b, c3.a);
	rlVertex2f(p3.x, p3.y);
	rlEnd();
}

void draw_rectangle(v2 pos, v2 size, Color color)
{
	draw_triangle(pos, pos + v2{size.x, 0}, pos + v2{0, size.y}, color);
	draw_triangle(pos + v2{size.x, 0}, pos + size, pos + v2{0, size.y}, color);
}

void Painter::_push_command(Context* context)
{
	// Allocate command
	DrawCommand* command = context->arena.allocate_one<DrawCommand>();
	*command = current_command;
	command->prev = last_command;

	// Update global buffer index
	context->draw_buffer.vertex_buffer_top += current_command.vertex_end - current_command.vertex_start;
	context->draw_buffer.index_buffer_top += current_command.index_end - current_command.index_start;

	// Reset current command
	current_command = DrawCommand{};
	current_command.clip_rect = command->clip_rect;

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

}
