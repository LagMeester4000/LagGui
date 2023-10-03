#include "lag_gui.hpp"
#include "basic.hpp"
#include "crc32.hpp"
#include <corecrt_math.h>
#include <string.h>

#include "raylib.h"
#include "rlgl.h"
#include "stb_truetype.h"
#include <stdio.h>

namespace lgui {

// Forward decls
static Panel* _get_or_create_panel(Context* context, ID id);
static void root_dock_update(Context* context, Dock* dock);
static void dock_into(Context* context, Panel* from, Panel* into, DockEntry entry);
static void push_panel(Context* context, Panel* panel);
static void pop_panel(Context* context);

static f32 lerp(f32 v1, f32 v2, f32 t)
{
	return v1 + (v2 - v1) * t;
}

Context* create_context()
{
	usize mem_size = LGUI_MB(8);
	Arena arena = Arena::from_memory(malloc(mem_size), mem_size);

	Context* ret = arena.allocate_one<Context>();
	ret->arena = arena;
	ret->temp_arena = Arena::from_memory(malloc(mem_size), mem_size);

	ret->draw_buffer.allocate(ret);
	ret->current_frame = 1;

	return ret;
}

void begin_frame(Context* context)
{
	context->temp_arena.reset();

	// Find overlap panel
	{
		v2 mouse = v2::from(GetMousePosition());

		context->overlap_panel = nullptr;
		for (Panel* panel = context->first_depth_panel; panel; panel = panel->order_next)
		{
			if (panel->rect.overlap(mouse))
			{
				context->overlap_panel = panel;
			}
		}
	}

	// Input
	{
		// Must be done _before_ new input is inserted
		if (!mouse_down(context))
		{
			context->active_id = 0;
			context->mouse_dragging = false;
		}

		// Insert new mouse input
		{
			MouseState state{};
			state.buttons[0] = IsMouseButtonDown(0);
			state.buttons[1] = IsMouseButtonDown(1);
			state.buttons[2] = IsMouseButtonDown(2);
			state.pos = v2::from(GetMousePosition());

			context->mouse_states[1] = context->mouse_states[0];
			context->mouse_states[0] = state;

			for (int i = 0; i < 3; ++i)
			{
				if (mouse_pressed(context, i))
				{
					context->mouse_pressed_pos[i] = state.pos;
				}
			}
		}

		context->hover_id = 0;

		if (context->overlap_id)
		{
			context->hover_id = context->overlap_id;
			context->overlap_id = 0;
		}
	}

	++context->current_frame;
}

void end_frame(Context* context)
{
	// Run dock commands
	for (DockCommand* command = context->first_dock_command; command; command = command->next)
	{
		switch (command->type)
		{
		case DockCommandType_DockInto:
		{
			dock_into(context, command->panel1, command->panel2, command->entry);
		} break;
		}
	}
	context->first_dock_command = nullptr;
	context->last_dock_command = nullptr;

	// Update docks
	for (Dock* root = context->first_root_dock; root; root = root->next_root)
	{
		root_dock_update(context, root);
	}
	context->first_root_dock = nullptr;
	context->last_root_dock = nullptr;

	rl_render(context);

	context->draw_buffer.vertex_buffer_top = 0;
	context->draw_buffer.index_buffer_top = 0;
}

bool is_anything_hovered(Context* context)
{
	return context->hover_id != 0;
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

ID get_id(Context* context, void* ptr)
{
	return calc_id(context, (const byte*)ptr, sizeof(ptr));
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

void push_id(Context* context, void* ptr)
{
	_push_id(context, get_id(context, ptr));
}

void push_style(Context* context, const Style& style)
{
	LGUI_ASSERT(context->style_stack_top < STYLE_STACK_SIZE, "Out of bounds");
	context->style_stack[context->style_stack_top] = style;
	++context->style_stack_top;
}

void pop_style(Context* context)
{
	LGUI_ASSERT(context->style_stack_top > 0, "Out of bounds");
	++context->style_stack_top;
}

const Style& get_style(Context* context)
{
	LGUI_ASSERT(context->style_stack_top > 0, "No style to return");
	return context->style_stack[context->style_stack_top - 1];
}

void set_default_style(Context* context, const Style& style)
{
	// ??
}

// Is panel "current" more towards the screen than panel "other"?
static bool is_more_towards_screen(Panel* current, Panel* other)
{
	for (Panel* it = current; it; it = it->order_next)
	{
		if (it == other)
		{
			return false;
		}
	}
	return true;
}

#pragma region Docking

static void push_dock_command(Context* context, const DockCommand& command)
{
	DockCommand* new_command = context->temp_arena.allocate_one<DockCommand>();
	*new_command = command;
	LGUI_SLL_APPEND_END(new_command, next, context->first_dock_command, context->last_dock_command);
}

static bool dock_has_tab(Dock* dock, Panel* tab)
{
	for (Panel* it = dock->first_tab; it; it = it->next_dock_tab)
	{
		if (it == tab)
		{
			return true;
		}
	}
	return false;
}

static void dock_select_panel(Context* context, Panel* panel)
{
	LGUI_ASSERT(panel->is_docked(), "Panel must be docked");
	LGUI_ASSERT(dock_has_tab(panel->parent_dock, panel), "Panel is not a tab in parent dock");

	panel->parent_dock->selected_tab = panel;
}

inline static Dock* get_dock_root(Dock* dock)
{
	return dock->root_panel->parent_dock;
}

static Dock* make_dock(Context* context)
{
	// TODO: Add free list for docks
	Dock* ret = context->arena.allocate_one<Dock>();
	return ret;
}

static Dock* make_panel_docked(Context* context, Panel* panel)
{
	LGUI_ASSERT(!panel->is_docked(), "panel should not be docked already");
	Dock* dock = make_dock(context);

	dock->rect = panel->rect;

	panel->parent_dock = dock;
	dock->first_tab = panel;
	dock->last_tab = panel;
	panel->next_dock_tab = nullptr;
	panel->prev_dock_tab = nullptr;

	// Create a new root panel
	ID id = get_id(context, (void*)dock); // The value of the ID actually doesn't matter here, since the panel is stored by pointer
	Panel* root_panel = _get_or_create_panel(context, id);
	root_panel->rect = panel->rect;
	root_panel->is_dock_root = true;
	root_panel->root_dock_panel = root_panel; // Is this correct?
	root_panel->parent_dock = dock;
	panel->root_dock_panel = root_panel;
	dock->root_panel = root_panel;

	// Remove panel from depth list, only the parent needs to be in this list
	LGUI_LL_REMOVE(panel, order_prev, order_next, context->first_depth_panel, context->last_depth_panel);

	return dock;
}

static void dock_add_tab(Context* context, Dock* dock, Panel* new_tab)
{
	LGUI_ASSERT(dock->is_leaf(), "Dock must be a leaf node");
	LGUI_LL_APPEND_END(new_tab, prev_dock_tab, next_dock_tab, dock->first_tab, dock->last_tab);

	new_tab->root_dock_panel = dock->root_panel;
	new_tab->parent_dock = dock;
	dock->selected_tab = new_tab;

	// Remove panel from depth list
	LGUI_LL_REMOVE(new_tab, order_prev, order_next, context->first_depth_panel, context->last_depth_panel);
}

static Dock* _search_dock_root(Dock* dock)
{
	for (Dock* it = dock; it; it = it->parent_dock)
	{
		if (it->parent_dock == nullptr)
		{
			return it;
		}
	}
	LGUI_TRAP("This cannot happen");
	return nullptr;
}

static void dock_split(Context* context, Dock* dock, DockEntry direction, Panel* new_tab)
{
	LGUI_ASSERT(dock->first_tab, "Dock must be a leaf node in order to split");
	LGUI_ASSERT(direction != DockEntry_Into, "Can't split into a dock, choose a direction instead");

	Dock* new_parent = make_dock(context);
	new_parent->rect = dock->rect;

	Dock* new_child = make_dock(context);
	LGUI_LL_APPEND_END(new_tab, prev_dock_tab, next_dock_tab, new_child->first_tab, new_child->last_tab);
	new_tab->parent_dock = new_child;
	new_tab->root_dock_panel = dock->root_panel;

	// Remove panel from depth list
	LGUI_LL_REMOVE(new_tab, order_prev, order_next, context->first_depth_panel, context->last_depth_panel);

	if (dock->parent_dock)
	{
		new_parent->parent_dock = dock->parent_dock;

		if (dock->parent_dock->child_docks[0] == dock)
		{
			dock->parent_dock->child_docks[0] = new_parent;
		}
		else if (dock->parent_dock->child_docks[1] == dock)
		{
			dock->parent_dock->child_docks[1] = new_parent;
		}
	}
	dock->parent_dock = new_parent;
	new_child->parent_dock = new_parent;
	new_child->root_panel = dock->root_panel;
	new_parent->root_panel = dock->root_panel;

	bool horizontal = false;

	switch (direction)
	{
	case DockEntry_Right:
	{
		new_parent->child_docks[0] = dock;
		new_parent->child_docks[1] = new_child;
		new_parent->dock_direction = DockDirection_Right;
		horizontal = true;
	} break;
	case DockEntry_Left:
	{
		new_parent->child_docks[0] = new_child;
		new_parent->child_docks[1] = dock;
		new_parent->dock_direction = DockDirection_Right;
		horizontal = true;
	} break;
	case DockEntry_Top:
	{
		new_parent->child_docks[0] = new_child;
		new_parent->child_docks[1] = dock;
		new_parent->dock_direction = DockDirection_Down;
	} break;
	case DockEntry_Bottom:
	{
		new_parent->child_docks[0] = dock;
		new_parent->child_docks[1] = new_child;
		new_parent->dock_direction = DockDirection_Down;
	} break;
	case DockEntry_Into:
		LGUI_ASSERT(false, "Unreachable");
	}

	Dock* new_root = _search_dock_root(dock);
	dock->root_panel->parent_dock = new_root;

	// TODO: ??
	dock->selected_tab = dock->first_tab;
	new_child->selected_tab = new_child->first_tab;

	Rect rect = new_parent->rect;
	if (horizontal)
	{
		new_parent->split_pos = floorf(rect.width() / 2.f);
		dock->rect = rect.cut_right(new_parent->split_pos);
		new_child->rect = rect;
	}
	else
	{
		new_parent->split_pos = floorf(rect.height() / 2.f);
		dock->rect = rect.cut_bottom(new_parent->split_pos);
		new_child->rect = rect;
	}
}

static void dock_into(Context* context, Panel* from, Panel* into, DockEntry entry)
{
	// TODO: add separate functio that docks a root dock into another dock
	LGUI_ASSERT(!from->is_docked(), "From panel can't be docked if it's already docked");

	if (!into->parent_dock)
	{
		make_panel_docked(context, into);
	}

	if (entry == DockEntry_Into)
	{
		dock_add_tab(context, into->parent_dock, from);
	}
	else
	{
		dock_split(context, into->parent_dock, entry, from);
	}
}

const f32 DOCK_TITLE_BAR_HEIGHT = 20.f;

static void dock_calc_panel_and_title_rect(Dock* dock, Rect* title_rect, Rect* panel_rect)
{
	Rect rect = dock->rect.pad(1);
	Rect title_rect_ = rect.cut_top(DOCK_TITLE_BAR_HEIGHT);
	if (title_rect) *title_rect = title_rect_;
	if (panel_rect) *panel_rect = rect;
}

static void move_all_child_docks(Dock* dock, v2 movement)
{
	dock->rect.move(movement);
	Rect panel_rect{};
	dock_calc_panel_and_title_rect(dock, nullptr, &panel_rect);

	for (Panel* panel = dock->first_tab; panel; panel = panel->next_dock_tab)
	{
		panel->rect = panel_rect;
	}

	if (dock->is_root())
	{
		dock->root_panel->rect = dock->rect;
	}

	if (dock->child_docks[0])
	{
		move_all_child_docks(dock->child_docks[0], movement);
	}
	if (dock->child_docks[1])
	{
		move_all_child_docks(dock->child_docks[1], movement);
	}
}

static void root_dock_update_from_panel(Context* context, Dock* dock, Panel* panel)
{
	Dock* dock_root = get_dock_root(dock);
	if (dock_root->last_frame_registered == context->current_frame)
	{
		return;
	}
	dock_root->last_frame_registered = context->current_frame;

	// This should only happen on the first mention

	// Register to root list
	LGUI_SLL_APPEND_END(dock_root, next_root, context->first_root_dock, context->last_root_dock);

	// Handle window dragging
	// At this point, none of the child windows have been drawn yet, so we can move position directly
	{
		// TODO: Maybe change this to use each sub-window title bar instead, because that way I can select the window as well

		InputResult result = handle_element_input(context, dock_root->rect, get_id(context, "__panel_drag"), true);
		if (result.dragging)
		{
			move_all_child_docks(dock_root, result.drag_delta);
		}

		if (result.pressed)
		{
			move_panel_to_front(context, dock_root->root_panel);
		}
	}
}

// Please call this before pushing an ID (though it might work without doing that)
static void dock_update_from_panel(Context* context, Dock* dock, Panel* panel)
{
	root_dock_update_from_panel(context, dock, panel);

	LGUI_ASSERT(dock->first_tab, "The dock must have tabs if it's called from a panel");

	// Draw docked title bar (tabs)
	{
		// TODO: I shouldn't use style here, since the user won't know which window carries the style
		const Style& style = get_style(context);
		Font* font = style.default_font;

		Rect title_rect{};
		dock_calc_panel_and_title_rect(dock, &title_rect, nullptr);

		Painter& painter = panel->get_painter();

		// Background
		painter.draw_rectangle(context, title_rect, {0.4, 0.4, 1.0, 1.0});

		f32 tab_spacing = 2;
		f32 tab_height_reduction = 2;
		f32 tab_x_pad = 2;
		Color tab_color = {0, 0, 0, 1};
		Color tab_color_selected = {0, 1, 0, 1};
		Color tab_text_color = {1, 1, 1, 1};

		Rect title_cut = title_rect;
		title_cut.cut_left(tab_spacing);

		// Draw each tab
		for (Panel* tab = dock->first_tab; tab; tab = tab->next_dock_tab)
		{
			v2 text_size = {
				font->text_width(tab->name, 0),
				font->height
			};

			Rect tab_rect = title_cut.cut_left(text_size.x + tab_x_pad * 2);
			tab_rect.cut_top(tab_height_reduction);
			Rect text_rect = tab_rect.center_size(text_size);

			tab->dock_tab_rect = tab_rect;

			Color c = tab_color;
			if (tab == dock->selected_tab)
			{
				c = tab_color_selected;
			}

			painter.draw_rectangle(context, tab_rect, c);
			painter.draw_text(context, font, tab->name, text_rect.top_left, 0, tab_text_color);

			// Advance the title cut rect
			title_cut.cut_left(tab_spacing);
		}
	}
}

// Dock update at end of frame, from the root dock upwards
static void _end_dock_update_layout(Context* context, Dock* dock)
{
	if (dock->is_leaf())
	{
		Rect rect{};
		dock_calc_panel_and_title_rect(dock, nullptr, &rect);
		for (Panel* tab = dock->first_tab; tab; tab = tab->next_dock_tab)
		{
			tab->rect = rect;
		}
	}
	else
	{
		Rect rect = dock->rect;

		if (dock->dock_direction == DockDirection_Right)
		{
			dock->child_docks[0]->rect = rect.cut_left(dock->split_pos);
			dock->child_docks[1]->rect = rect;
		}
		else // Down
		{
			dock->child_docks[0]->rect = rect.cut_top(dock->split_pos);
			dock->child_docks[1]->rect = rect;
		}

		_end_dock_update_layout(context, dock->child_docks[0]);
		_end_dock_update_layout(context, dock->child_docks[1]);
	}
}

static void _end_dock_update_input(Context* context, Dock* dock)
{
	push_id(context, dock);

	if (dock->is_leaf())
	{
		pop_id(context);
		return;
	}

	Rect rect = dock->rect;
	Rect resize_rect;
	if (dock->dock_direction == DockDirection_Right)
	{
		rect.cut_left(dock->split_pos - 2);
		resize_rect = rect.cut_left(4);
	}
	else // Down
	{
		rect.cut_top(dock->split_pos - 2);
		resize_rect = rect.cut_top(4);
	}

	InputResult input = handle_element_input(context, resize_rect, get_id(context, "__resize_split_bar"), true);

	// Debug
	{
		dock->root_panel->get_painter().draw_rectangle(context, resize_rect, { 1, 0, 0, 1 });
	}

	if (input.dragging)
	{
		if (dock->dock_direction == DockDirection_Right)
		{
			dock->split_pos += input.drag_delta.x;
		}
		else // Down
		{
			dock->split_pos += input.drag_delta.y;
		}
	}

	if (dock->child_docks[0])
	{
		_end_dock_update_input(context, dock->child_docks[0]);
	}
	if (dock->child_docks[1])
	{
		_end_dock_update_input(context, dock->child_docks[1]);
	}

	pop_id(context);
}

static void root_dock_update(Context* context, Dock* dock)
{
	push_panel(context, dock->root_panel);
	push_id(context, dock->root_panel->id);
	Painter& painter = dock->root_panel->get_painter();
	painter._restart_painter(context);

	// Call this at the end of the frame, on the root dock

	// Calculate rectangles
	f32 resize_bar_size = 8.f;
	Rect rect = dock->rect;
	Rect panel_top = rect.cut_top(resize_bar_size);
	Rect panel_bottom = rect.cut_bottom(resize_bar_size);
	Rect panel_left = rect.cut_left(resize_bar_size);
	Rect panel_right = rect.cut_right(resize_bar_size);

	// Handle input
	{
		// Panel resizing (the lines in-between panels)
		
		InputResult top = handle_element_input(context, panel_top, get_id(context, "__resize_top"), true);
		InputResult bottom = handle_element_input(context, panel_bottom, get_id(context, "__resize_bottom"), true);
		InputResult left = handle_element_input(context, panel_left, get_id(context, "__resize_left"), true);
		InputResult right = handle_element_input(context, panel_right, get_id(context, "__resize_right"), true);

		if (top.dragging)
		{
			dock->rect.top_left.y += top.drag_delta.y;
		}
		if (bottom.dragging)
		{
			dock->rect.bottom_right.y += bottom.drag_delta.y;
		}
		if (left.dragging)
		{
			dock->rect.top_left.x += left.drag_delta.x;
		}
		if (right.dragging)
		{
			dock->rect.bottom_right.x += right.drag_delta.x;
		}

		_end_dock_update_input(context, dock);
	}

	// Draw
	{

	}

	// Update all layouts now that size is known
	dock->root_panel->rect = dock->rect;
	_end_dock_update_layout(context, dock);

	painter._push_command(context);
	pop_id(context);
	pop_panel(context);
}

static void check_docking(Context* context, Panel* panel, InputResult* input_result)
{
	if (input_result->dragging || input_result->released)
	{
		Painter& painter = panel->get_painter();

		for (Panel* it = context->first_panel; it; it = it->next_panel)
		{
			if (it == panel) continue;

			Rect rect = it->rect;
			if (!rect.overlap(panel->rect)) continue;

			Rect center = rect.center_size(v2{50, 50});
			Rect left = rect.get_left(50).center_size({50, 50});
			Rect right = rect.get_right(50).center_size({50, 50});
			Rect top = rect.get_top(50).center_size({50, 50});
			Rect bottom = rect.get_bottom(50).center_size({50, 50});

			Color c = {0, 0, 1, 1};
			painter.draw_rectangle(context, center, c);
			painter.draw_rectangle(context, left, c);
			painter.draw_rectangle(context, right, c);
			painter.draw_rectangle(context, top, c);
			painter.draw_rectangle(context, bottom, c);

			if (input_result->released)
			{
				if (center.overlap(mouse_pos(context)))
				{
					//dock_into(context, panel, it, DockEntry_Into);
					DockCommand command{};
					command.type = DockCommandType_DockInto;
					command.panel1 = panel;
					command.panel2 = it;
					command.entry = DockEntry_Into;
					push_dock_command(context, command);
					return;
				}
				else if (left.overlap(mouse_pos(context)))
				{
					DockCommand command{};
					command.type = DockCommandType_DockInto;
					command.panel1 = panel;
					command.panel2 = it;
					command.entry = DockEntry_Left;
					push_dock_command(context, command);
					return;
				}
				else if (right.overlap(mouse_pos(context)))
				{
					DockCommand command{};
					command.type = DockCommandType_DockInto;
					command.panel1 = panel;
					command.panel2 = it;
					command.entry = DockEntry_Right;
					push_dock_command(context, command);
					return;
				}
				else if (top.overlap(mouse_pos(context)))
				{
					DockCommand command{};
					command.type = DockCommandType_DockInto;
					command.panel1 = panel;
					command.panel2 = it;
					command.entry = DockEntry_Top;
					push_dock_command(context, command);
					return;
				}
				else if (bottom.overlap(mouse_pos(context)))
				{
					DockCommand command{};
					command.type = DockCommandType_DockInto;
					command.panel1 = panel;
					command.panel2 = it;
					command.entry = DockEntry_Bottom;
					push_dock_command(context, command);
					return;
				}
			}
		}
	}
}

// Docking
#pragma endregion

Panel* get_panel(Context* context, ID id)
{
	for (Panel* panel = context->first_depth_panel; panel; panel = panel->order_next)
	{
		if (panel->id) return panel;
	}
	return nullptr;
}

static Panel* try_get_prev_top_panel(Context* context)
{
	return context->panel_stack_top > 1 ? context->panel_stack[context->panel_stack_top - 2] : nullptr;
}

static Panel* try_get_current_panel(Context* context)
{
	return context->panel_stack_top > 0 ? context->panel_stack[context->panel_stack_top - 1] : nullptr;
}

Panel* get_current_panel(Context* context)
{
	LGUI_ASSERT(context->panel_stack_top > 0, "There is no current panel");
	return context->panel_stack[context->panel_stack_top - 1];
}

static Panel* _get_or_create_panel(Context* context, ID id)
{
	for (Panel* panel = context->first_panel; panel; panel = panel->next_panel)
	{
		if (panel->id == id) return panel;
	}

	// TODO: Use first_free_panel
	Panel* ret = context->arena.allocate_one<Panel>();
	ret->id = id;

	LGUI_LL_APPEND_END(ret, prev_panel, next_panel, context->first_panel, context->last_panel);

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

// Interpolation that can be called every frame without a T:
//   current = current + (target - current) * rate
static f32 interp_towards(f32 value, f32 target, f32 rate, f32 dt)
{
	return value + (target - value) * rate * dt;
}

// Rotation in degrees (0 degrees is right)
static void draw_open_triangle(Context* context, Painter* painter, v2 pos, f32 size, f32 rotation, Color color)
{
	f32 rad = rotation / 180.f * PI;
	f32 r_cos = cosf(rad);
	f32 r_sin = sinf(rad);

	auto fast_rot = [&](v2 p)
	{
		return v2{p.x * r_cos - p.y * r_sin, p.x * r_sin + p.y * r_cos};
	};

	v2 min = {-1, -1};
	f32 scale = size / 2.f;

	v2 top_left = (fast_rot({-1, -1}) - min) * scale + pos;
	v2 bottom_left = (fast_rot({-1, 1}) - min) * scale + pos;
	v2 center_right = (fast_rot({1, 0}) - min) * scale + pos;

	painter->begin_triangle_strip();
	painter->add_strip_triangle(context, top_left, color);
	painter->add_strip_triangle(context, center_right, color);
	painter->add_strip_triangle(context, bottom_left, color);
	painter->end_triangle_strip();
}

static void push_panel(Context* context, Panel* panel)
{
	// Push on stack
	LGUI_ASSERT(context->panel_stack_top < PANEL_STACK_SIZE, "Panel stack is full");
	context->panel_stack[context->panel_stack_top] = panel;
	++context->panel_stack_top;
}

static void pop_panel(Context* context)
{
	// Push on stack
	LGUI_ASSERT(context->panel_stack_top > 0, "Panel stack is empty");
	--context->panel_stack_top;
}

static bool is_painter_updated(Context* context, Painter& painter)
{
	return	painter.frame_last_updated == context->current_frame;
}

bool begin_panel(Context* context, const char* name, Rect rect, PanelFlag flags)
{
	Panel* panel = _get_or_create_panel(context, get_id(context, name));
	push_id(context, panel->id);
	push_panel(context, panel);
	Panel* prev_top_panel = try_get_prev_top_panel(context);

	copy_string_to_buffer(panel->name, PANEL_NAME_SIZE, name);

	// End painter of parent panel if it exists
	if (prev_top_panel)
	{
		prev_top_panel->get_painter()._push_command(context);
	}

	if (panel->is_docked())
	{
		Painter& painter = panel->get_painter();
		if (is_painter_updated(context, painter))
		{
			painter._restart_painter(context);
		}
		else
		{
			painter._start_painter(context);
		}

		dock_update_from_panel(context, panel->parent_dock, panel);
	}
	else
	{
		panel->get_painter()._start_painter(context);
	}

	panel->flags = flags;

	if (panel->frame_last_updated == 0)
	{
		// First usage
		panel->rect = rect;
		panel->open = true;
	}
	panel->frame_last_updated = context->current_frame;

	// Animate open/close
	f32 open_target = panel->open ? 1.f : 0.f;
	panel->open_anim = interp_towards(panel->open_anim, open_target, 15, 0.01666f);

	// Create rects
	const Style& style = get_style(context);
	Painter& painter = panel->get_painter();
	f32 line_height = style.line_height();
	Rect window_whole = panel->rect;
	
	// Animate open/close
	window_whole.bottom_right.y = lerp(window_whole.top_left.y + line_height + 2, window_whole.bottom_right.y, panel->open_anim);
	Rect window_pad = window_whole.pad(1);

	bool has_title_bar = !panel->is_docked();
	Rect window_title_bar{};
	if (has_title_bar)
	{
		window_title_bar = window_pad.cut_top(style.line_height());
	}

	// Close a docked but not selected panel
	bool deactivate = false;
	if (panel->is_docked() && panel->parent_dock->selected_tab != panel)
	{
		deactivate = true;
	}

	// Input
	{
		if (has_title_bar && !deactivate)
		{
			// Title bar
			{
				InputResult r = handle_element_input(context, window_title_bar, panel->id, true);
				if (r.dragging)
				{
					panel->rect.move(r.drag_delta);

					// Also update the created rects
					window_whole.move(r.drag_delta);
					window_pad.move(r.drag_delta);
					window_title_bar.move(r.drag_delta);
				}

				// Check for dockig
				if (!panel->is_docked())
				{
					check_docking(context, panel, &r);
				}

				if (r.pressed)
				{
					move_panel_to_front(context, panel);
				}
			}

			// Close button
			{
				Rect close_button = window_title_bar;
				close_button = close_button.cut_left(close_button.size().y);
				ID close_button_id = get_id(context, "__close_button");
				InputResult r = handle_element_input(context, close_button, close_button_id);
				if (r.clicked)
				{
					panel->open = !panel->open;
				}
			}
		}

		// Tab selection
		if (panel->is_docked())
		{
			// TODO: Change this into a dock command?
			// Currently this causes flicker when switching back the first panel (but not the other way around?)
			InputResult input = handle_element_input(context, panel->dock_tab_rect, get_id(context, "__tab"), true, true);
			if (input.clicked)
			{
				dock_select_panel(context, panel);
			}
		}
	}

	// Draw
	if (!deactivate)
	{
		painter.draw_rectangle(context, window_whole, style.window_outline);
		painter.draw_rectangle(context, window_pad, style.window_background);

		if (has_title_bar)
		{
			painter.draw_rectangle(context, window_title_bar, style.window_title_background);
			painter.draw_text(context, style.default_font, name, window_title_bar.top_left + v2{line_height, style.line_padding}, 0, style.window_title_color);

			f32 triangle_pad = style.line_padding + 3;
			draw_open_triangle(context, &painter, window_title_bar.top_left + v2{triangle_pad, triangle_pad}, line_height - triangle_pad * 2.f, lerp(0, 90, panel->open_anim), style.window_title_color);
		}

	}
	panel->content = window_pad.pad(2);
	panel->start_draw_pos = panel->content.top_left - panel->scroll_pos;
	panel->draw_pos = panel->start_draw_pos;
	painter.push_clip_rect(context, panel->content);


	bool open = panel->open || panel->open_anim >= 0.001f;

	// Close a docked but not selected panel
	if (deactivate)
	{
		open = false;
	}

	if (!open)
	{
		end_panel(context);
	}

	return open;
}

void end_panel(Context* context)
{
	Panel* panel = get_current_panel(context);
	Painter& painter = panel->get_painter();

	panel->end_draw_pos = panel->draw_pos;
	v2 draw_size = panel->end_draw_pos - panel->start_draw_pos;

	// Y Scroll
	if (draw_size.y > panel->content.height() && draw_size.y > 10.f)
	{
		v2 scroll_size = draw_size - panel->content.size();
		Rect rect = panel->content;
		rect = rect.cut_right(8);
		Rect scroll_base = rect.cut_top(50);

		Rect scroll = scroll_base;
		f32 scroll_per_pixel = rect.height() / scroll_size.y;
		scroll.move({0, scroll_per_pixel * panel->scroll_pos.y});
		
		InputResult input = handle_element_input(context, scroll, get_id(context, "__scroll_y"), true);
		if (input.dragging)
		{
			f32 movement = input.drag_delta.y / scroll_per_pixel;
			panel->scroll_pos.y += movement;
			panel->scroll_pos.y = LGUI_CLAMP(0.f, scroll_size.y, panel->scroll_pos.y);

			// Immediately move the rect
			scroll = scroll_base;
			f32 scroll_per_pixel = rect.height() / scroll_size.y;
			scroll.move({0, scroll_per_pixel * panel->scroll_pos.y});
		}

		painter.draw_rectangle(context, scroll, {0.4, 0.4, 0.4, 0.7});
	}

	painter.pop_clip_rect(context);

	// Restart painter of previous window if it exists
	Panel* prev_top_panel = try_get_prev_top_panel(context);
	if (prev_top_panel)
	{
		prev_top_panel->get_painter()._restart_painter(context);
	}

	pop_id(context);
	pop_panel(context);
}

void move_panel_to_front(Context* context, Panel* panel)
{
	// If the window is docked, only move the root panel to the front
	if (panel->is_docked() && !panel->is_dock_root)
	{
		LGUI_ASSERT(panel->root_dock_panel, "Docked panel must have root dock panel");
		move_panel_to_front(context, panel->root_dock_panel);
		return;
	}

	if (context->last_depth_panel == panel)
	{
		return;
	}

	if (context->first_depth_panel == panel)
	{
		context->first_depth_panel = panel->order_next;
	}

	if (panel->order_next)
	{
		LGUI_ASSERT(panel->order_next->order_prev == panel, "Provided panel is old");
		panel->order_next->order_prev = panel->order_prev;
	}
	if (panel->order_prev)
	{
		LGUI_ASSERT(panel->order_prev->order_next == panel, "Provided panel is old");
		panel->order_prev->order_next = panel->order_next;
	}

	if (context->last_depth_panel)
	{
		context->last_depth_panel->order_next = panel;
	}
	panel->order_next = nullptr;
	panel->order_prev = context->last_depth_panel;
	context->last_depth_panel = panel;
}

void RetainedData::update_t_linear(bool hover, bool active, f32 dt, f32 duration)
{
	f32 hover_dir = hover ? 1.f : -1.f;
	f32 active_dir = active ? 1.f : -1.f;

	hover_t += hover_dir * dt * (1.f / duration);
	hover_t = LGUI_CLAMP(0.f, 1.f, hover_t);
	active_t += active_dir * dt * (1.f / duration);
	active_t = LGUI_CLAMP(0.f, 1.f, active_t);
}

void RetainedData::update_t_towards(bool hover, bool active, f32 dt, f32 rate)
{
	f32 hover_goal = hover ? 1.f : 0.f;
	f32 active_goal = active ? 1.f : 0.f;

	hover_t += (hover_goal - hover_t) * dt * rate;
	hover_t = LGUI_CLAMP(0.f, 1.f, hover_t);
	active_t += (active_goal - active_t) * dt * rate;
	active_t = LGUI_CLAMP(0.f, 1.f, active_t);
}

v2 layout_next(Context* context)
{
	Panel* panel = get_current_panel(context);
	v2 ret = panel->draw_pos;
	const Style& style = get_style(context);
	panel->draw_pos.y += style.line_height();
	return ret;
}

InputResult handle_element_input(Context* context, Rect rect, ID id, bool enable_drag, bool ignore_clip)
{
	InputResult ret{};

	v2 mouse = mouse_pos(context);

	// Only root panels are checked
	Panel* current_panel = get_current_panel(context);
	if (current_panel->is_docked())
	{
		current_panel = current_panel->root_dock_panel;
	}

	// Clip the rect so you don't get invisible clickable elements
	if (!ignore_clip)
	{
		rect = rect.clip(current_panel->get_painter().get_clip_rect());
	}

	if (context->overlap_panel == current_panel &&
		rect.overlap(mouse))
	{
		context->overlap_id = id;
	}

	ret.hover = context->hover_id == id;

	// TODO: "active ID" system needs to work with other mouse buttons
	if (context->hover_id == id || context->active_id == id)
	{
		if (mouse_pressed(context, 0))
		{
			ret.pressed = true;
			context->active_id = id;
		}
		if (mouse_released(context, 0))
		{
			ret.released = true;

			if (context->active_id == id && !context->mouse_dragging)
			{
				ret.clicked = true;
			}
		}
		if (mouse_down(context, 0))
		{
			ret.down = true;

			// Dragging
			if (enable_drag && context->active_id == id)
			{
				if (!context->mouse_dragging)
				{
					v2 press_pos = context->mouse_pressed_pos[0];

					if ((press_pos - mouse).length() > MIN_DRAG_DISTANCE)
					{
						context->mouse_dragging = true;
						ret.dragging = true;
						ret.drag_start = press_pos;
						ret.drag_delta = mouse - press_pos;
					}
				}
				else
				{
					v2 press_pos = context->mouse_pressed_pos[0];
					v2 mouse_old = context->mouse_states[1].pos;
					context->mouse_dragging = true;
					ret.dragging = true;
					ret.drag_start = press_pos;
					ret.drag_delta = mouse - mouse_old;
				}
			}

		}
	}

	return ret;
}

bool mouse_pressed(Context* context, int button)
{
	return context->mouse_states[0].buttons[button] && !context->mouse_states[1].buttons[button];
}

bool mouse_released(Context* context, int button)
{
	return !context->mouse_states[0].buttons[button] && context->mouse_states[1].buttons[button];
}

bool mouse_down(Context* context, int button)
{
	return context->mouse_states[0].buttons[button];
}

v2 mouse_pos(Context* context)
{
	return context->mouse_states[0].pos;
}

RetainedData* get_retained_data(Context* context, ID id)
{
	Panel* panel = get_current_panel(context);

	RetainedData** first_retained_data = &panel->retained_data_lookup[id % RETAINED_TABLE_SIZE];
	for (RetainedData* it = *first_retained_data; it; it = it->next)
	{
		if (it->id == id)
		{
			return it;
		}
	}

	RetainedData* retained_data = context->arena.allocate_one<RetainedData>();
	retained_data->id = id;
	retained_data->next = *first_retained_data;
	if (*first_retained_data)
	{
		(*first_retained_data)->prev = retained_data;
	}
	*first_retained_data = retained_data;

	return retained_data;
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

InputResult button(Context* context, const char* name)
{
	v2 pos = layout_next(context);
	v2 size = v2{50, 30};
	Color color = Color{0, 1, 0, 1};
	ID id = get_id(context, name);
	RetainedData* retained = get_retained_data(context, id);
	const Style& style = get_style(context);

	// Rect
	auto& painter = get_current_panel(context)->get_painter();
	Font* font = style.default_font;
	f32 text_width = font->text_width(name, 0);
	Rect rect = Rect::from_pos_size(pos, {text_width + 4, font->height});

	InputResult input = handle_element_input(context, rect, id);
	if (input.pressed)
	{
		color.b = 1;
	}
	else if (input.released)
	{
		color.r = 1;
		color.b = 1;
	}
	else if (input.hover)
	{
		color.r = 1;
	}

	retained->update_t_towards(input.hover, input.down, 0.016666f);
	color = lerp_color(lerp_color(style.button_background, style.button_background_hover, retained->hover_t), style.button_background_down, retained->active_t);

	// Rendering
	Rect text_rect = rect.center_size({text_width, font->height});
	painter.draw_rectangle(context, rect, color);
	painter.draw_text(context, font, name, text_rect.top_left, 0, {1, 1, 1, 1});

	return input;
}

InputResult checkbox(Context* context, const char* name, bool* value)
{
	v2 pos = layout_next(context);
	v2 size = v2{50, 30};
	Color color = Color{0, 1, 0, 1};
	ID id = get_id(context, name);
	RetainedData* retained = get_retained_data(context, id);
	const Style& style = get_style(context);

	// Rect
	auto& painter = get_current_panel(context)->get_painter();
	Rect rect = Rect::from_pos_size(pos, {25, 25});

	InputResult input = handle_element_input(context, rect, id);
	if (input.pressed)
	{
		*value = !*value;
	}

	retained->update_t_towards(input.hover, *value, 0.016666f, 30);
	color = lerp_color(style.button_background, style.button_background_hover, retained->hover_t);

	// Rendering
	Rect inner = rect.pad(1);
	Rect checked = rect.pad(lerp(7, 5, retained->active_t));
	Color checked_color = {1, 1, 1, retained->active_t};
	painter.draw_rectangle(context, rect, style.checkbox_outline);
	painter.draw_rectangle(context, inner, color);
	painter.draw_rectangle(context, checked, checked_color);

	return input;
}

InputResult radio_button(Context* context, const char* name, int option, int* selected)
{
	v2 pos = layout_next(context);
	v2 size = v2{50, 30};
	Color color = Color{0, 1, 0, 1};
	ID id = get_id(context, name);
	RetainedData* retained = get_retained_data(context, id);
	const Style& style = get_style(context);

	// Rect
	auto& painter = get_current_panel(context)->get_painter();
	Rect rect = Rect::from_pos_size(pos, {25, 25});

	InputResult input = handle_element_input(context, rect, id);
	if (input.clicked)
	{
		*selected = option;
	}

	retained->update_t_towards(input.hover, *selected == option, 0.016666f, 30);
	color = lerp_color(style.button_background, style.button_background_hover, retained->hover_t);

	// Rendering
	Rect inner = rect.pad(1);
	Rect checked = rect.pad(lerp(7, 5, retained->active_t));
	Color checked_color = {1, 1, 1, retained->active_t};
	painter.draw_circle(context, rect.center(), rect.height() / 2.f, 1, style.checkbox_outline);
	painter.draw_circle(context, inner.center(), inner.height() / 2.f, 1, color);
	painter.draw_circle(context, checked.center(), checked.height() / 2.f, 1, checked_color);

	return input;
}

InputResult drag_value(Context* context, const char* name, f32* value)
{
	v2 pos = layout_next(context);
	v2 size = v2{50, 30};
	Color color = Color{0, 1, 0, 1};
	ID id = get_id(context, name);
	RetainedData* retained = get_retained_data(context, id);
	const Style& style = get_style(context);

	// Make text
	char text[16];
	sprintf_s(text, "%.3f", *value);

	// Rect
	auto& painter = get_current_panel(context)->get_painter();
	Font* font = style.default_font;
	f32 text_width = font->text_width(text, 0);
	Rect rect = Rect::from_pos_size(pos, {128, font->height});

	InputResult input = handle_element_input(context, rect, id, true);
	if (input.dragging && input.drag_delta.x != 0)
	{
		*value += input.drag_delta.x;
	}

	retained->update_t_towards(input.hover, input.down, 0.016666f);
	color = lerp_color(lerp_color(style.button_background, style.button_background_hover, retained->hover_t), style.button_background_down, retained->active_t);

	// Rendering
	Rect text_rect = rect.center_size({text_width, font->height});
	painter.draw_rectangle(context, rect, color);
	painter.draw_text(context, font, text, text_rect.top_left, 0, {1, 1, 1, 1});

	return input;
}

// TODO: This could also use the layout to allocate a new rect for every line, that way the layout can decide the... layout of the text
// Returns height of text written
f32 text_in_rect(Context* context, Painter* painter, Font* font, v2 pos, f32 width, const char* text, usize text_length, f32 spacing, Color color)
{
	f32 initial_width = font->text_width(text, text_length, spacing);
	if (initial_width <= width)
	{
		painter->draw_text(context, font, text, text_length, pos, spacing, color);
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

			painter->draw_text(context, font, &text[offset], to_draw, pos, spacing, color);

			pos.y += font->height;
			height += font->height;
			offset += to_draw;
		}

		return height;
	}
}

void text(Context* context, const char* text, bool wrap)
{
	const Style& style = get_style(context);
	v2 pos = layout_next(context);
	
	if (wrap)
	{
		Panel* panel = get_current_panel(context);
		f32 height = text_in_rect(context, &panel->get_painter(), style.default_font, pos, panel->content.width(), text, strlen(text), 0, style.window_title_color);

		// TODO: Remove after layout changes
		panel->draw_pos.y = pos.y + height + style.line_padding;
	}
	else
	{
		Painter& painter = get_current_panel(context)->get_painter();
		painter.draw_text(context, style.default_font, text, pos, 0, style.window_title_color);
	}
}

bool collapse_header(Context* context, const char* name)
{
	v2 pos = layout_next(context);
	v2 size = v2{50, 30};
	Color color = Color{0, 1, 0, 1};
	ID id = get_id(context, name);
	Panel* panel = get_current_panel(context);
	RetainedData* retained = get_retained_data(context, id);
	const Style& style = get_style(context);

	// Rect
	auto& painter = get_current_panel(context)->get_painter();
	Font* font = style.default_font;
	f32 text_width = font->text_width(name, 0);
	Rect rect = Rect::from_pos_size(pos, {panel->content.width(), style.line_height() - 2});

	InputResult input = handle_element_input(context, rect, id, true);
	if (input.clicked)
	{
		retained->open = !retained->open;
	}

	retained->update_t_towards(input.hover, retained->open, 0.016666f);
	color = lerp_color(lerp_color(style.button_background, style.button_background_hover, retained->hover_t), style.button_background_down, retained->active_t);

	// Rendering
	painter.draw_rectangle(context, rect, color);
	Rect arrow_rect = rect.cut_left(rect.height()).pad(6);
	draw_open_triangle(context, &painter, arrow_rect.top_left, arrow_rect.width(), lerp(0, 90, retained->active_t), {1, 1, 1, 1});
	rect.cut_left(2); // Pad
	Rect text_rect = rect.align_size({text_width, font->height}, -1, 0);
	painter.draw_text(context, font, name, text_rect.top_left, 0, {1, 1, 1, 1});

	return retained->open;
}

void DrawBuffer::allocate(Context* context)
{
	// TODO: Replace with growing solution? Especially for index buffer

	const usize vertex_size = sizeof(f32) * 2 + sizeof(f32) * 2 + sizeof(u32);
	const usize vertex_floats = 5;
	const usize index_size = sizeof(u16);
	const usize short_max = 1 << 16;
	vertex_buffer_length = short_max * vertex_floats;
	index_buffer_length = short_max;

	vertex_buffer_top = 0;
	index_buffer_top = 0;

	vertex_buffer = (f32*)context->arena.allocate(short_max * vertex_size);
	index_buffer = (u16*)context->arena.allocate(index_buffer_length * index_size);
}

Rect allocate_line(Layout* layout, v2 size)
{
	if (layout->same_line)
	{
		layout->same_line = false;
		return layout->prev_line;
	}

	if (layout->is_horizontal)
	{
		f32 height = layout->cross_axis_size;
		f32 width = LGUI_MAX(layout->min_line_size, size.x);
		Rect ret = Rect::from_pos_size(
			layout->start + v2{layout->cursor, 0},
			{width, height}
		);
		layout->cursor += width + layout->spacing;
		return ret;
	}
	else // Vertical
	{
		f32 width = layout->cross_axis_size;
		f32 height = LGUI_MAX(layout->min_line_size, size.y);
		Rect ret = Rect::from_pos_size(
			layout->start + v2{0, layout->cursor},
			{width, height}
		);
		layout->cursor += height + layout->spacing;
		return ret;
	}
}

void layout_check_stretch_cross_axis(Layout* layout, Rect rect)
{
	if (layout->is_horizontal)
	{
		if (layout->reverse)
		{
			if (rect.top_left.x < layout->start.x - layout->max_size.x)
			{
				layout->max_size.x = layout->start.x - rect.top_left.x;
			}
		}
		else
		{
			if (rect.bottom_right.x > layout->start.x + layout->max_size.x)
			{
				layout->max_size.x = rect.bottom_right.x - layout->start.x;
			}
		}
	}
	else // Vertical
	{
		if (layout->reverse)
		{
			if (rect.top_left.y < layout->start.y - layout->max_size.y)
			{
				layout->max_size.y = layout->start.y - rect.top_left.y;
			}
		}
		else
		{
			if (rect.bottom_right.y > layout->start.y + layout->max_size.y)
			{
				layout->max_size.y = rect.bottom_right.y - layout->start.y;
			}
		}
	}
}

Rect Layout::allocate(v2 size)
{
	// 1. Allocate a full cross size rect
	// 2. Cut a piece off this rect, which is the rect to be returned
	// 3. Store the remaining rect in prev_line

	Rect line = allocate_line(this, size);
	
	if (is_horizontal)
	{
		Rect ret{};
		if (v_align == -1)
		{
			ret = line.cut_top(size.x);
			line.cut_top(cross_spacing);
		}
		else if (v_align == 1)
		{
			ret = line.cut_bottom(size.y);
			line.cut_bottom(cross_spacing);
		}
		else
		{
			LGUI_ASSERT(v_align == 0, "v_align does not have a valid value");
			line.cut_top(line.height() / 2.f - size.y / 2.f);
			ret = line.cut_top(size.y);
			line.cut_top(cross_spacing);
		}

		prev_line = line;
		return ret.align_size(size, h_align, 0);
	}
	else // Vertical
	{
		Rect ret{};
		if (h_align == -1)
		{
			ret = line.cut_left(size.x);
			line.cut_left(cross_spacing);
		}
		else if (h_align == 1)
		{
			ret = line.cut_right(size.x);
			line.cut_right(cross_spacing);
		}
		else
		{
			LGUI_ASSERT(h_align == 0, "h_align does not have a valid value");
			line.cut_left(line.width() / 2.f - size.x / 2.f);
			ret = line.cut_left(size.x);
			line.cut_left(cross_spacing);
		}

		prev_line = line;
		return ret.align_size(size, 0, v_align);
	}
}

void debug_menu(Context* context)
{
	if (begin_panel(context, "Debug Window", Rect::from_pos_size({}, {400, 300}), 0))
	{
		const int buffer_size = 64;
		char buffer[buffer_size]{};

		snprintf(buffer, buffer_size, "hover_id = %d", context->hover_id);
		text(context, buffer);
		snprintf(buffer, buffer_size, "active_id = %d", context->active_id);
		text(context, buffer);
		snprintf(buffer, buffer_size, "overlap_id = %d", context->overlap_id);
		text(context, buffer);
		snprintf(buffer, buffer_size, "overlap_panel = %p", context->overlap_panel);
		text(context, buffer);
		
		end_panel(context);
	}
}

}
