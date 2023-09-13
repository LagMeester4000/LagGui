#include "lag_gui.hpp"
#include "basic.hpp"
#include "crc32.hpp"
#include <string.h>

namespace lgui {

Context* create_context()
{
	usize mem_size = LGUI_MB(8);
	Arena arena = Arena::from_memory(malloc(mem_size), mem_size);

	Context* ret = arena.allocate_one<Context>();
	ret->arena = arena;

	return ret;
}

void begin_frame()
{

}

void end_frame()
{

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
		if (panel->id) return panel;
	}

	Panel* ret = context->arena.allocate_one<Panel>();
	ret->id = get_id(context, id);

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

bool begin_panel(Context* context, const char* name, Rect rect, PanelFlag flags)
{
	Panel* panel = _get_or_create_panel(context, get_id(context, name));
	push_id(context, panel->id);

	panel->flags = flags;
	panel->rect = rect;

	LGUI_ASSERT(context->panel_stack_top < PANEL_STACK_SIZE, "Panel stack is full");
	context->panel_stack[context->panel_stack_top] = panel;
	++context->panel_stack_top;

	panel->draw_pos = panel->rect.top_left + v2{4, 4};

	draw_rectangle(panel->rect.top_left, panel->rect.size(), Color{1, 0, 0, 1});

	return true;
}

void end_panel(Context* context)
{
	pop_id(context);

	LGUI_ASSERT(context->panel_stack_top > 0, "There is no panel to end");
	--context->panel_stack_top;
}

v2 layout_next(Context* context)
{
	Panel* panel = get_current_panel(context);
	v2 ret = panel->draw_pos;
	panel->draw_pos.y += 25.f;
	return ret;
}

bool button(Context* context, const char* name)
{
	v2 pos = layout_next(context);
	draw_rectangle(pos, v2{50, 30}, Color{0, 1, 0, 1});
	return false;
}

void text(Context* context, const char* text)
{

}

}
