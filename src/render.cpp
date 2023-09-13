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

}
