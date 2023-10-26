#include "lag_gui.hpp"
#include "basic.hpp"
#include "crc32.hpp"
#include <string.h>

#include "raylib.h"
#include "rlgl.h"
#include "stb_truetype.h"
#include <stdio.h>

namespace lgui {

#define LGUI_DEBUG_INFO
#ifdef LGUI_DEBUG_INFO
// Debug values
static bool d_show_layout_rects;
static bool d_show_layout_allocations;
#endif

Context* g_context = nullptr;

Context* get_context()
{
	LGUI_ASSERT(g_context, "Library is not initialized");
	return g_context;
}

// Forward decls
static Panel* _get_or_create_panel(ID id);
static void root_dock_update(Dock* dock);
static void dock_into(Panel* from, Panel* into, DockEntry entry);
static void push_panel(Panel* panel);
static void pop_panel();
static void _delete_old_panels();
static void _draw_boxes(Painter& painter, Box* root, v2 start_pos);

static f32 lerp(f32 v1, f32 v2, f32 t)
{
	return v1 + (v2 - v1) * t;
}

Context* init()
{
	LGUI_ASSERT(g_context == nullptr, "The library has already been initialized");

	usize mem_size = LGUI_MB(8);
	Arena arena = Arena::from_memory(malloc(mem_size), mem_size);

	Context* ret = arena.allocate_one<Context>();
	g_context = ret;

	ret->arena = arena;
	ret->temp_arena_arr[0] = Arena::from_memory(malloc(mem_size), mem_size);
	ret->temp_arena_arr[1] = Arena::from_memory(malloc(mem_size), mem_size);
	ret->temp_arena = &ret->temp_arena_arr[0];

	ret->draw_buffer.allocate();
	ret->current_frame = 1;

	return ret;
}

void deinit()
{
	free(g_context->temp_arena_arr[0].ptr);
	free(g_context->temp_arena_arr[1].ptr);
	free(g_context->arena.ptr);

	g_context = nullptr;
}

void begin_frame(f32 delta_time)
{
	Context* context = get_context();

	context->draw_buffer.vertex_buffer_top = 0;
	context->draw_buffer.index_buffer_top = 0;

	_delete_old_panels();

	context->delta_time = delta_time;

	// Swap temorary arenas
	context->temp_arena = &context->temp_arena_arr[context->current_frame % 2];
	context->temp_arena->reset();

	// Input
	{
		// Must be done _before_ new input is inserted
		if (!mouse_down())
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
			state.scroll_wheel = v2::from(GetMouseWheelMoveV());

			context->mouse_states[1] = context->mouse_states[0];
			context->mouse_states[0] = state;

			for (int i = 0; i < 3; ++i)
			{
				if (mouse_pressed(i))
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

	// Focus clicked panels
	if (context->overlap_panel && (!(context->overlap_panel->flags & PanelFlag_AlwaysBackground)) && mouse_pressed())
	{
		move_panel_to_front(context->overlap_panel);
	}

	++context->current_frame;
}

void end_frame()
{
	Context* context = get_context();

	// Run dock commands
	for (DockCommand* command = context->first_dock_command; command; command = command->next)
	{
		switch (command->type)
		{
		case DockCommandType_DockInto:
		{
			dock_into(command->panel1, command->panel2, command->entry);
		} break;
		case DockCommandType_SelectTab:
		{
			// TODO: This
		} break;
		case DockCommandType_Remove:
		{
			// TODO: This
		} break;
		}
	}
	context->first_dock_command = nullptr;
	context->last_dock_command = nullptr;

	// Update docks
	for (Dock* root = context->first_root_dock; root; root = root->next_root)
	{
		root_dock_update(root);
	}
	context->first_root_dock = nullptr;
	context->last_root_dock = nullptr;

	// Input
	{
		// Reset keys for next frame
		{
			// Shift all inputs to the previous frame position, or null
			for (usize i = 0; i < Key::MAX; ++i)
			{
				u8& key = context->keys[i];
				// Set the first bit to the state of the second bit
				// If the key was pressed, it will become held
				// If the key was released, it will become 0
				key &= Key::CURRENT_FRAME_MASK;
				key |= (key >> 1);
			}
		}

		// Reset codepoints
		context->codepoints_pressed_length = 0;
	}
}

void draw_frame()
{
	rl_render();
}

bool is_anything_hovered()
{
	Context* context = get_context();

	return context->hover_id != 0;
}

ID calc_id(const byte* data, usize length)
{
	Context* context = get_context();

	ID top = context->id_stack_top > 0 ? context->id_stack[context->id_stack_top - 1] : 123456;
	return xcrc32(data, (int)length, top);
}

ID get_id(const char* string)
{
	Context* context = get_context();

	// Find any 

	return calc_id((const byte*)string, strlen(string));
}

ID get_id(i32 i)
{
	Context* context = get_context();

	return calc_id((const byte*)&i, sizeof(i));
}

ID get_id(void* ptr)
{
	Context* context = get_context();

	return calc_id((const byte*)ptr, sizeof(ptr));
}

ID peek_id()
{
	Context* context = get_context();
	LGUI_ASSERT(context->id_stack_top > 0, "No ID on stack");
	return context->id_stack[context->id_stack_top - 1];
}

static void _push_id(ID id)
{
	Context* context = get_context();

	LGUI_ASSERT(context->id_stack_top < ID_STACK_SIZE, "Id stack overflow");
	context->id_stack[context->id_stack_top] = id;
	++context->id_stack_top;
}

void pop_id()
{
	Context* context = get_context();

	LGUI_ASSERT(context->id_stack_top > 0, "Can't pop ID because there are no IDs left");
	--context->id_stack_top;
}

void push_id(const char* string)
{
	_push_id(get_id(string));
}

void push_id(i32 i)
{
	_push_id(get_id(i));
}

void push_id(void* ptr)
{
	_push_id(get_id(ptr));
}

void push_id_raw(ID id)
{
	_push_id(id);
}

void push_style(const Style& style)
{
	Context* context = get_context();

	LGUI_ASSERT(context->style_stack_top < STYLE_STACK_SIZE, "Out of bounds");
	context->style_stack[context->style_stack_top] = style;
	++context->style_stack_top;
}

void pop_style()
{
	Context* context = get_context();

	LGUI_ASSERT(context->style_stack_top > 0, "Out of bounds");
	++context->style_stack_top;
}

const Style& get_style()
{
	Context* context = get_context();

	LGUI_ASSERT(context->style_stack_top > 0, "No style to return");
	return context->style_stack[context->style_stack_top - 1];
}

void set_default_style(const Style& style)
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

static void push_dock_command(const DockCommand& command)
{
	Context* context = get_context();

	DockCommand* new_command = context->temp_arena->allocate_one<DockCommand>();
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

static void dock_select_panel(Panel* panel)
{
	LGUI_ASSERT(panel->is_docked(), "Panel must be docked");
	LGUI_ASSERT(dock_has_tab(panel->parent_dock, panel), "Panel is not a tab in parent dock");

	panel->parent_dock->selected_tab = panel;
}

inline static Dock* get_dock_root(Dock* dock)
{
	return dock->root_panel->parent_dock;
}

static Dock* make_dock()
{
	Context* context = get_context();

	// TODO: Add free list for docks
	Dock* ret = context->arena.allocate_one<Dock>();
	return ret;
}

static Dock* make_panel_docked(Panel* panel)
{
	Context* context = get_context();

	LGUI_ASSERT(!panel->is_docked(), "panel should not be docked already");
	Dock* dock = make_dock();

	dock->rect = panel->rect;

	panel->parent_dock = dock;
	dock->first_tab = panel;
	dock->last_tab = panel;
	panel->next_dock_tab = nullptr;
	panel->prev_dock_tab = nullptr;

	// Create a new root panel
	ID id = get_id((void*)dock); // The value of the ID actually doesn't matter here, since the panel is stored by pointer
	Panel* root_panel = _get_or_create_panel(id);
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

static void dock_add_tab(Dock* dock, Panel* new_tab)
{
	Context* context = get_context();

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

static void dock_split(Dock* dock, DockEntry direction, Panel* new_tab)
{
	Context* context = get_context();

	LGUI_ASSERT(dock->first_tab, "Dock must be a leaf node in order to split");
	LGUI_ASSERT(direction != DockEntry_Into, "Can't split into a dock, choose a direction instead");

	Dock* new_parent = make_dock();
	new_parent->rect = dock->rect;

	Dock* new_child = make_dock();
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

static void dock_into(Panel* from, Panel* into, DockEntry entry)
{
	// TODO: add separate functio that docks a root dock into another dock
	LGUI_ASSERT(!from->is_docked(), "From panel can't be docked if it's already docked");

	if (!into->parent_dock)
	{
		make_panel_docked(into);
	}

	if (entry == DockEntry_Into)
	{
		dock_add_tab(into->parent_dock, from);
	}
	else
	{
		dock_split(into->parent_dock, entry, from);
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

static void root_dock_update_from_panel(Dock* dock, Panel* panel)
{
	Context* context = get_context();

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

		InputResult result = handle_element_input(dock_root->rect, get_id("__panel_drag"), true);
		if (result.dragging)
		{
			move_all_child_docks(dock_root, result.drag_delta);
		}

		if (result.pressed)
		{
			move_panel_to_front(dock_root->root_panel);
		}
	}
}

// Please call this before pushing an ID (though it might work without doing that)
static void dock_update_from_panel(Dock* dock, Panel* panel)
{
	root_dock_update_from_panel(dock, panel);

	LGUI_ASSERT(dock->first_tab, "The dock must have tabs if it's called from a panel");

	// Draw docked title bar (tabs)
	{
		// TODO: I shouldn't use style here, since the user won't know which window carries the style
		const Style& style = get_style();
		Font* font = style.default_font;

		Rect title_rect{};
		dock_calc_panel_and_title_rect(dock, &title_rect, nullptr);

		Painter& painter = panel->get_painter();

		// Background
		painter.draw_rectangle(title_rect, {0.4f, 0.4f, 1.0f, 1.0f});

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

			painter.draw_rectangle(tab_rect, c);
			painter.draw_text(font, tab->name, text_rect.top_left, 0, tab_text_color);

			// Advance the title cut rect
			title_cut.cut_left(tab_spacing);
		}
	}
}

// Dock update at end of frame, from the root dock upwards
static void _end_dock_update_layout(Dock* dock)
{
	Context* context = get_context();

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

		_end_dock_update_layout(dock->child_docks[0]);
		_end_dock_update_layout(dock->child_docks[1]);
	}
}

static void _end_dock_update_input(Dock* dock)
{
	push_id(dock);

	if (dock->is_leaf())
	{
		pop_id();
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

	InputResult input = handle_element_input(resize_rect, get_id("__resize_split_bar"), true);

	// Debug
	{
		dock->root_panel->get_painter().draw_rectangle(resize_rect, { 1, 0, 0, 1 });
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
		_end_dock_update_input(dock->child_docks[0]);
	}
	if (dock->child_docks[1])
	{
		_end_dock_update_input(dock->child_docks[1]);
	}

	pop_id();
}

static void root_dock_update(Dock* dock)
{
	push_panel(dock->root_panel);
	push_id(dock->root_panel->id);
	Painter& painter = dock->root_panel->get_painter();
	painter._restart_painter();

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

		InputResult top = handle_element_input(panel_top, get_id("__resize_top"), true);
		InputResult bottom = handle_element_input(panel_bottom, get_id("__resize_bottom"), true);
		InputResult left = handle_element_input(panel_left, get_id("__resize_left"), true);
		InputResult right = handle_element_input(panel_right, get_id("__resize_right"), true);

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

		_end_dock_update_input(dock);
	}

	// Draw
	{

	}

	// Update all layouts now that size is known
	dock->root_panel->rect = dock->rect;
	_end_dock_update_layout(dock);

	painter._push_command();
	pop_id();
	pop_panel();
}

static void check_docking(Panel* panel, InputResult* input_result)
{
	Context* context = get_context();

	if (input_result->dragging || input_result->released)
	{
		Painter& painter = panel->get_painter();

		// Iterate over all existing panels
		for (usize i = 0; i < PANEL_MAP_SIZE; ++i)
		{
			for (Panel* it = context->panel_map[i]; it; it = it->hash_next)
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
				painter.draw_rectangle(center, c);
				painter.draw_rectangle(left, c);
				painter.draw_rectangle(right, c);
				painter.draw_rectangle(top, c);
				painter.draw_rectangle(bottom, c);

				if (input_result->released)
				{
					if (center.overlap(mouse_pos()))
					{
						//dock_into(context, panel, it, DockEntry_Into);
						DockCommand command{};
						command.type = DockCommandType_DockInto;
						command.panel1 = panel;
						command.panel2 = it;
						command.entry = DockEntry_Into;
						push_dock_command(command);
						return;
					}
					else if (left.overlap(mouse_pos()))
					{
						DockCommand command{};
						command.type = DockCommandType_DockInto;
						command.panel1 = panel;
						command.panel2 = it;
						command.entry = DockEntry_Left;
						push_dock_command(command);
						return;
					}
					else if (right.overlap(mouse_pos()))
					{
						DockCommand command{};
						command.type = DockCommandType_DockInto;
						command.panel1 = panel;
						command.panel2 = it;
						command.entry = DockEntry_Right;
						push_dock_command(command);
						return;
					}
					else if (top.overlap(mouse_pos()))
					{
						DockCommand command{};
						command.type = DockCommandType_DockInto;
						command.panel1 = panel;
						command.panel2 = it;
						command.entry = DockEntry_Top;
						push_dock_command(command);
						return;
					}
					else if (bottom.overlap(mouse_pos()))
					{
						DockCommand command{};
						command.type = DockCommandType_DockInto;
						command.panel1 = panel;
						command.panel2 = it;
						command.entry = DockEntry_Bottom;
						push_dock_command(command);
						return;
					}
				}
			}
		}
	}
}

// Docking
#pragma endregion

Panel* get_panel(ID id)
{
	Context* context = get_context();

	for (Panel* panel = context->first_depth_panel; panel; panel = panel->order_next)
	{
		if (panel->id) return panel;
	}
	return nullptr;
}

static Panel* try_get_prev_top_panel()
{
	Context* context = get_context();

	return context->panel_stack_top > 1 ? context->panel_stack[context->panel_stack_top - 2] : nullptr;
}

static Panel* try_get_current_panel()
{
	Context* context = get_context();

	return context->panel_stack_top > 0 ? context->panel_stack[context->panel_stack_top - 1] : nullptr;
}

Panel* get_current_panel()
{
	Context* context = get_context();

	LGUI_ASSERT(context->panel_stack_top > 0, "There is no current panel");
	return context->panel_stack[context->panel_stack_top - 1];
}

static Panel* _get_or_create_panel(ID id)
{
	Context* context = get_context();

	// Try to find panel
	for (Panel* it = context->panel_map[id % PANEL_MAP_SIZE]; it; it = it->hash_next)
	{
		if (it->id == id) return it;
	}

	// Create new panel object
	Panel* ret{};
	if (context->first_free_panel)
	{
		ret = context->first_free_panel;
		context->first_free_panel = context->first_free_panel->order_next;
		memset(ret, 0, sizeof(Panel));
	}
	else
	{
		ret = context->arena.allocate_one<Panel>();
	}
	ret->id = id;

	// Add new panel to panel hash map
	{
		Panel*& first = context->panel_map[id % PANEL_MAP_SIZE];
		if (first)
		{
			first->hash_prev = ret;
		}
		ret->hash_next = first;
		first = ret;
	}

	// Add panel on top of the depth panel list
	LGUI_LL_APPEND_END(ret, order_prev, order_next, context->first_depth_panel, context->last_depth_panel);

	return ret;
}

static void _delete_panel(Panel* panel)
{
	Context* context = get_context();

	// Remove docking
	if (panel->is_docked())
	{
		// TODO: Add docking deletion code here
	}

	// Remove retained data
	{
		for (usize i = 0; i < RETAINED_TABLE_SIZE; ++i)
		{
			// Keep the already existing list structure when inserting them into the free list
			RetainedData* bucket = panel->retained_data_lookup[i];
			if (context->first_free_retained_data)
			{
				// Find last element in bucket, then append the current free list
				// TODO: It would be nice if I didn't have to find the end pointer here, could be an optimization if needed
				RetainedData* it = bucket;
				while (it->next) { it = it->next; }
				it->next = context->first_free_retained_data;
			}
			context->first_free_retained_data = bucket;
		}
	}

	// Remove from panel hash map
	{
		Panel*& first = context->panel_map[panel->id % PANEL_MAP_SIZE];
		if (panel->hash_prev)
		{
			panel->hash_prev->hash_next = panel->hash_next;
		}
		if (panel->hash_next)
		{
			panel->hash_next->hash_prev = panel->hash_prev;
		}
		if (first == panel)
		{
			first = panel->hash_next;
		}
	}

	// Remove panel from depth list
	LGUI_LL_REMOVE(panel, order_prev, order_next, context->first_depth_panel, context->last_depth_panel);

	// Add to free list
	if (context->first_free_panel)
	{
		// Use order pointer as free list pointer
		panel->order_next = context->first_free_panel;
		context->first_free_panel = panel;
	}
}

struct ToDeletePanel {
	ToDeletePanel* next;
	Panel* panel;
};

static void _delete_old_panels()
{
	Context* context = get_context();

	ToDeletePanel* to_delete = nullptr;

	auto current_frame = context->current_frame;
	for (usize i = 0; i < PANEL_MAP_SIZE; ++i)
	{
		for (Panel* it = context->panel_map[i]; it; it = it->hash_next)
		{
			if (it->frame_last_updated != current_frame)
			{
				// Schedule panel to be deleted
				ToDeletePanel* delete_node = context->temp_arena->allocate_one<ToDeletePanel>();
				delete_node->panel = it;
				delete_node->next = to_delete;
				to_delete = delete_node;
			}
		}
	}

	// Perform scheduled deletion
	for (ToDeletePanel* it = to_delete; it; it = it->next)
	{
		// TODO: Temporarily disabled because it causes docking windows to be deleted
		_delete_panel(it->panel);
	}
}

// Interpolation that can be called every frame without a T:
//   current = current + (target - current) * rate
static f32 interp_towards(f32 value, f32 target, f32 rate, f32 dt)
{
	return value + (target - value) * rate * dt;
}

// Rotation in degrees (0 degrees is right)
void draw_open_triangle(Painter* painter, v2 pos, f32 size, f32 rotation, Color color)
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
	painter->add_strip_triangle(top_left, color);
	painter->add_strip_triangle(center_right, color);
	painter->add_strip_triangle(bottom_left, color);
	painter->end_triangle_strip();
}

static void push_panel(Panel* panel)
{
	Context* context = get_context();

	// Push on stack
	LGUI_ASSERT(context->panel_stack_top < PANEL_STACK_SIZE, "Panel stack is full");
	context->panel_stack[context->panel_stack_top] = panel;
	++context->panel_stack_top;
}

static void pop_panel()
{
	Context* context = get_context();

	// Push on stack
	LGUI_ASSERT(context->panel_stack_top > 0, "Panel stack is empty");
	--context->panel_stack_top;
}

static bool is_painter_updated(Painter& painter)
{
	Context* context = get_context();

	return painter.frame_last_updated == context->current_frame;
}

bool begin_panel(const char* name, Rect rect, PanelFlag flags)
{
	Context* context = get_context();

	Panel* panel = _get_or_create_panel(get_id(name));
	push_id(panel->id);
	push_panel(panel);
	Panel* prev_top_panel = try_get_prev_top_panel();

	copy_string_to_buffer(panel->name, PANEL_NAME_SIZE, name);

	// End painter of parent panel if it exists
	if (prev_top_panel)
	{
		prev_top_panel->get_painter()._push_command();
	}

	// Initialize the painter
	if (panel->is_docked())
	{
		Painter& painter = panel->get_painter();
		if (is_painter_updated(painter))
		{
			painter._restart_painter();
		}
		else
		{
			painter._start_painter();
		}

		dock_update_from_panel(panel->parent_dock, panel);
	}
	else
	{
		panel->get_painter()._start_painter();
	}

	// Reset values
	panel->first_unknown_fit[0] = nullptr;
	panel->first_unknown_fit[1] = nullptr;
	panel->last_unknown_fit[0] = nullptr;
	panel->last_unknown_fit[1] = nullptr;
	panel->last_unknown_pc[0] = nullptr;
	panel->last_unknown_pc[1] = nullptr;
	panel->flags = flags;

	if (flags & PanelFlag_AlwaysResetRect)
	{
		panel->rect = rect;
	}
	if (flags & PanelFlag_AlwaysBackground && !panel->is_docked() && panel->open)
	{
		LGUI_LL_REMOVE(panel, order_prev, order_next, context->first_depth_panel, context->last_depth_panel);
		LGUI_LL_APPEND_START(panel, order_prev, order_next, context->first_depth_panel, context->last_depth_panel);
	}

	if (panel->frame_last_updated == 0)
	{
		// First usage
		panel->rect = rect;
		panel->open = true;

		// Reset box lookup
		panel->box_lookup[0] = context->temp_arena->allocate_array<Box*>(BOX_TABLE_SIZE).ptr;
		panel->box_lookup[1] = context->temp_arena->allocate_array<Box*>(BOX_TABLE_SIZE).ptr;
	}
	else if (panel->frame_last_updated + 1 < context->current_frame)
	{
		// Reset box lookup
		panel->box_lookup[0] = context->temp_arena->allocate_array<Box*>(BOX_TABLE_SIZE).ptr;
		panel->box_lookup[1] = context->temp_arena->allocate_array<Box*>(BOX_TABLE_SIZE).ptr;
	}
	else
	{
		// Normal new frame
		panel->box_lookup[context->current_frame % 2] = context->temp_arena->allocate_array<Box*>(BOX_TABLE_SIZE).ptr;
	}
	panel->frame_last_updated = context->current_frame;

	// Close a docked but not selected panel
	bool deactivate = false;
	if (panel->is_docked() && panel->parent_dock->selected_tab != panel)
	{
		deactivate = true;
	}

	// Input
	{
		// Tab selection
		if (panel->is_docked())
		{
			// TODO: Change this into a dock command?
			// Currently this causes flicker when switching back the first panel (but not the other way around?)
			InputResult input = handle_element_input(panel->dock_tab_rect, get_id("__tab"), true, true);
			if (input.clicked)
			{
				dock_select_panel(panel);
			}
		}
	}

	bool open = panel->open;

	// Close a docked but not selected panel
	if (deactivate)
	{
		open = false;
	}

	Size size_x = flags & PanelFlag_AutoResizeHorizontal ? fit() : px(panel->rect.width());
	Size size_y = flags & PanelFlag_AutoResizeVertical ? fit() : px(panel->rect.height());
	u32 root_flags = BoxFlag_Static | BoxFlag_IsRoot;
	root_flags |= flags & PanelFlag_NoClipContent ? 0 : BoxFlag_Clip;
	panel->root_box = push_box("root_panel", {size_x, size_y}, root_flags);
	
	if (!(flags & PanelFlag_NoBackground))
	{
		const Style& style = get_style();
		panel->root_box->set_rectangle(style.window_background, style.window_outline, 1.f);
	}

	if (!open)
	{
		end_panel();
	}

	return open;
}

void end_panel()
{
	Panel* panel = get_current_panel();
	Painter& painter = panel->get_painter();

	pop_box();

	// Push panel into bounds
	{
		v2 app_size = get_context()->app_window_size;
		if (panel->rect.bottom_right.x > app_size.x)
		{
			panel->rect.move(v2{app_size.x - panel->rect.bottom_right.x, 0.f});
		}
		if (panel->rect.bottom_right.y > app_size.y)
		{
			panel->rect.move(v2{0.f, app_size.y - panel->rect.bottom_right.y});
		}
		if (panel->rect.top_left.x < 0.f)
		{
			panel->rect.move(v2{-panel->rect.top_left.x, 0.f});
		}
		if (panel->rect.top_left.y < 0.f)
		{
			panel->rect.move(v2{0.f, -panel->rect.top_left.y});
		}
	}

	// Calculate missing box sizes
	for (int i = 0; i < 2; ++i)
	{
		for (Box* it = panel->last_unknown_pc[i]; it; it = it->next_unknown_size[i])
		{
			it->post_calculate_percent(i);
		}
	}
	for (int i = 0; i < 2; ++i)
	{
		for (Box* it = panel->first_unknown_fit[i]; it; it = it->next_unknown_size[i])
		{
			it->post_calculate_fit(i);
		}
	}

	// Draw boxes
	_draw_boxes(painter, panel->root_box, panel->rect.top_left);

	// Store window size if resizable
	if (panel->flags & PanelFlag_AutoResizeHorizontal)
	{
		panel->rect.bottom_right.x = panel->rect.top_left.x + panel->root_box->calculated_size.x;
	}
	if (panel->flags & PanelFlag_AutoResizeVertical)
	{
		panel->rect.bottom_right.y = panel->rect.top_left.y + panel->root_box->calculated_size.y;
	}

	// Restart painter of previous window if it exists
	Panel* prev_top_panel = try_get_prev_top_panel();
	if (prev_top_panel)
	{
		prev_top_panel->get_painter()._restart_painter();
	}

	painter._push_command();

	pop_id();
	pop_panel();
}

bool begin_window(const char* name, Rect rect, PanelFlag flags)
{
	bool ret = begin_panel(name, rect, flags);

	if (ret)
	{
		const Style& style = get_style();
		Panel* panel = get_current_panel();

		// Add title bar
		if (!(flags & PanelFlag_NoTitleBar))
		{
			LGUI_H_LAYOUT(-1, 0, {pc(1.f), px(style.line_height())})
			{
				Box* title_bar = get_box();
				title_bar->set_rectangle(style.window_title_background);
				title_bar->padding = {2.f, 2.f};

				spacer(style.line_padding);
				text(name);

				// Drag input
				if (!(flags & PanelFlag_NoMove))
				{
					InputResult result = handle_element_input(title_bar->prev_rect(), title_bar->id, true, true);
					if (result.dragging)
					{
						panel->rect.move(result.drag_delta);
					}
				}
			}
		}

		Size size_x = flags & PanelFlag_AutoResizeHorizontal ? fit() : pc(1.f);
		Size size_y = flags & PanelFlag_AutoResizeVertical ? fit() : rem(1.f);

		u32 inner_flags = BoxFlag_Clip;
		inner_flags |= flags & PanelFlag_AutoResizeHorizontal ? 0 : (BoxFlag_ScrollX | BoxFlag_FixedH);
		inner_flags |= flags & PanelFlag_AutoResizeVertical ? 0 : (BoxFlag_ScrollY | BoxFlag_FixedV);

		Box* inner = layout_vertical(-1, -1, {size_x, size_y}, inner_flags);
		inner->padding = {style.window_content_padding, style.window_content_padding};
	}

	return ret;
}

bool begin_window(const char* name, v2 size, PanelFlag flags)
{
	return begin_window(name, Rect::from_pos_size({}, size), flags);
}

void end_window()
{
	layout_end();
	end_panel();
}

void move_panel_to_front(Panel* panel)
{
	Context* context = get_context();

	// If the window is docked, only move the root panel to the front
	if (panel->is_docked() && !panel->is_dock_root)
	{
		LGUI_ASSERT(panel->root_dock_panel, "Docked panel must have root dock panel");
		move_panel_to_front(panel->root_dock_panel);
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

Painter& get_painter()
{
	return get_current_panel()->get_painter();
}

void RetainedData::update_t_linear(bool hover, bool active, f32 duration)
{
	f32 dt = get_context()->delta_time;
	f32 hover_dir = hover ? 1.f : -1.f;
	f32 active_dir = active ? 1.f : -1.f;

	hover_t += hover_dir * dt * (1.f / duration);
	hover_t = LGUI_CLAMP(0.f, 1.f, hover_t);
	active_t += active_dir * dt * (1.f / duration);
	active_t = LGUI_CLAMP(0.f, 1.f, active_t);
}

void RetainedData::update_t_towards(bool hover, bool active, f32 rate)
{
	f32 dt = get_context()->delta_time;
	f32 hover_goal = hover ? 1.f : 0.f;
	f32 active_goal = active ? 1.f : 0.f;

	hover_t += (hover_goal - hover_t) * dt * rate;
	hover_t = LGUI_CLAMP(0.f, 1.f, hover_t);
	active_t += (active_goal - active_t) * dt * rate;
	active_t = LGUI_CLAMP(0.f, 1.f, active_t);
}

InputResult handle_element_input(Rect rect, ID id, bool enable_drag, bool ignore_clip)
{
	Context* context = get_context();

	InputResult ret{};

	v2 mouse = mouse_pos();

	// Only root panels are checked
	Panel* current_panel = get_current_panel();
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
		if (mouse_pressed(0))
		{
			ret.pressed = true;
			context->active_id = id;
		}
		if (mouse_released(0))
		{
			ret.released = true;

			if (context->active_id == id && context->hover_id == id &&
				((enable_drag && !context->mouse_dragging) || (!enable_drag)))
			{
				ret.clicked = true;
			}
		}
		if (mouse_down(0))
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

bool is_mouse_overlapping(Rect rect)
{
	Context* context = get_context();
	return context->overlap_panel == get_current_panel() && rect.overlap(mouse_pos());
}

bool mouse_pressed(int button)
{
	Context* context = get_context();

	return context->mouse_states[0].buttons[button] && !context->mouse_states[1].buttons[button];
}

bool mouse_released(int button)
{
	Context* context = get_context();

	return !context->mouse_states[0].buttons[button] && context->mouse_states[1].buttons[button];
}

bool mouse_down(int button)
{
	Context* context = get_context();

	return context->mouse_states[0].buttons[button];
}

v2 mouse_pos()
{
	Context* context = get_context();

	return context->mouse_states[0].pos;
}

v2 mouse_scroll()
{
	Context* context = get_context();

	return context->mouse_states[0].scroll_wheel;
}

void input_register_key_press(u32 key)
{
	LGUI_ASSERT(key < Key::MAX, "Key out of bounds");

	Context* context = get_context();
	// Set current frame bit
	context->keys[key] |= Key::CURRENT_FRAME_MASK;
}

void input_register_key_release(u32 key)
{
	LGUI_ASSERT(key < Key::MAX, "Key out of bounds");

	Context* context = get_context();
	// Remove current frame bit
	context->keys[key] &= ~Key::CURRENT_FRAME_MASK;
}

void input_register_key_down(u32 key, bool down)
{
	LGUI_ASSERT(key < Key::MAX, "Key out of bounds");

	Context* context = get_context();
	if (down)
	{
		context->keys[key] |= Key::CURRENT_FRAME_MASK;
	}
	else
	{
		context->keys[key] &= ~Key::CURRENT_FRAME_MASK;
	}
}

void input_register_char_press(Codepoint codepoint)
{
	Context* context = get_context();
	if (context->codepoints_pressed_length < INPUT_CODEPOINT_MAX)
	{
		context->codepoints_pressed[context->codepoints_pressed_length] = codepoint;
		++context->codepoints_pressed_length;
	}
}

bool key_pressed(u32 key)
{
	LGUI_ASSERT(key < Key::MAX, "Key out of bounds");
	u8 value = get_context()->keys[key];
	return !(value & Key::PREV_FRAME_MASK) && (value & Key::CURRENT_FRAME_MASK);
}

bool key_released(u32 key)
{
	LGUI_ASSERT(key < Key::MAX, "Key out of bounds");
	u8 value = get_context()->keys[key];
	return (value & Key::PREV_FRAME_MASK) && !(value & Key::CURRENT_FRAME_MASK);
}

bool key_down(u32 key)
{
	LGUI_ASSERT(key < Key::MAX, "Key out of bounds");
	u8 value = get_context()->keys[key];
	return value & Key::CURRENT_FRAME_MASK;
}

Slice<Codepoint> key_codepoints_pressed()
{
	Context* context = get_context();
	return {context->codepoints_pressed, context->codepoints_pressed_length};
}

RetainedData* get_retained_data(ID id)
{
	Context* context = get_context();

	Panel* panel = get_current_panel();

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

void DrawBuffer::allocate()
{
	Context* context = get_context();

	// TODO: Replace with growing solution? Especially for index buffer
	// TODO: Growing solution required for u32 indices

	const usize vertex_size = sizeof(f32) * 2 + sizeof(f32) * 2 + sizeof(u32);
	const usize vertex_floats = 5;
	const usize index_size = sizeof(DrawIndex);
	const usize short_max = 1 << 16;
	vertex_buffer_length = short_max * vertex_floats;
	index_buffer_length = short_max;

	vertex_buffer_top = 0;
	index_buffer_top = 0;

	vertex_buffer = (f32*)context->arena.allocate(short_max * vertex_size);
	index_buffer = (DrawIndex*)context->arena.allocate(index_buffer_length * index_size);
}

ID box_generate_id()
{
	// Just some random number so you don't interfere with other simple ids
	const i32 layout_starting_id = 472502; 
	Box* box = get_box();
	box->counter += 1;
	return get_id(layout_starting_id + box->counter);
}

void Box::append_child(Box* box)
{
	LGUI_LL_APPEND_END(box, prev, next, first_child, last_child);
	box->parent = this;
	++child_count;
}

void Box::begin()
{
}

void Box::end()
{
	end_calculate_size(0);
	end_calculate_size(1);
}

void Box::end_calculate_size(int index)
{
	LGUI_ASSERT(is_size_calculated[index] == false, "Size already calculated");

	f32& calc_size = index ? calculated_size.y : calculated_size.x;

	is_size_calculated[index] = true;

	switch (size[index].type)
	{
	case SizeType_Px:
	{
		calc_size = size[index].value;
		if (parent)
		{
			parent->add_used_size(index, calc_size);
			parent->add_static_size(index, calc_size);
		}
	} break;
	case SizeType_Remainder:
	case SizeType_Percent:
	{
		// Impossible, add to list
		Panel* panel = get_current_panel();
		next_unknown_size[index] = panel->last_unknown_pc[index];
		panel->last_unknown_pc[index] = this;

		is_size_calculated[index] = false;
	} break;
	case SizeType_Fit:
	{
		if (known_size_child_count[index] == child_count)
		{
			calc_size = index ? (used_size.y + padding.y * 2.f) : (used_size.x + padding.x * 2.f);
			if (parent)
			{
				parent->add_used_size(index, calc_size);
				parent->add_static_size(index, calc_size);
			}
		}
		else
		{
			if (parent) 
				parent->add_static_size(index, index ? (static_size.y + padding.y * 2.f) : (static_size.x + padding.x * 2.f));

			Panel* panel = get_current_panel();
			if (panel->last_unknown_fit[index])
			{
				panel->last_unknown_fit[index]->next_unknown_size[index] = this;
				panel->last_unknown_fit[index] = this;
			}
			else
			{
				panel->first_unknown_fit[index] = this;
				panel->last_unknown_fit[index] = this;
			}

			is_size_calculated[index] = false;
		}
	} break;
	}
}

void Box::add_used_size(int index, f32 pixel_size)
{
	known_size_child_count[index] += 1;

	if (index == 0)
	{
		if (flags & BoxFlag_IsHorizontal)
		{
			used_size.x += pixel_size;
		}
		else
		{
			used_size.x = LGUI_MAX(used_size.x, pixel_size);
		}
	}
	else // (index == 1)
	{
		if (flags & BoxFlag_IsHorizontal)
		{
			used_size.y = LGUI_MAX(used_size.y, pixel_size);
		}
		else
		{
			used_size.y += pixel_size;
		}
	}
}

void Box::add_static_size(int index, f32 pixel_size)
{
	if (index == 0)
	{
		if (flags & BoxFlag_IsHorizontal)
		{
			static_size.x += pixel_size;
		}
		else
		{
			static_size.x = LGUI_MAX(static_size.x, pixel_size);
		}
	}
	else // (index == 1)
	{
		if (flags & BoxFlag_IsHorizontal)
		{
			static_size.y = LGUI_MAX(static_size.y, pixel_size);
		}
		else
		{
			static_size.y += pixel_size;
		}
	}
}

void Box::post_calculate_percent(int index)
{
	LGUI_ASSERT(size[index].type == SizeType_Percent || size[index].type == SizeType_Remainder, "Box in percent calculation list, but is not percent type");
	LGUI_ASSERT(parent, "Percentage size requires parent");

	f32& calc_size = index ? calculated_size.y : calculated_size.x;

	if (size[index].type == SizeType_Percent)
	{
		if (parent->size[index].type == SizeType_Fit)
		{
			calc_size = (index ? parent->static_size.y : parent->static_size.x) * size[index].value;
		}
		else
		{
			calc_size = (index ? 
				(parent->calculated_size.y - parent->padding.y * 2.f) : 
				(parent->calculated_size.x - parent->padding.x * 2.f)) 
				* size[index].value;
		}
	}
	else // Remainder
	{
		if (parent->size[index].type == SizeType_Fit)
		{
			calc_size = 0.f;
		}
		else
		{
			calc_size = (index ? 
				(parent->calculated_size.y - parent->static_size.y - parent->padding.y * 2.f) : 
				(parent->calculated_size.x - parent->static_size.x - parent->padding.x * 2.f)) 
				* size[index].value;
			calc_size = LGUI_MAX(calc_size, 0.f);
		}
	}

	parent->add_used_size(index, calc_size);
}

void Box::post_calculate_fit(int index)
{
	LGUI_ASSERT(size[index].type == SizeType_Fit, "Box in fit calculation list, but is not fit type");
	LGUI_ASSERT(known_size_child_count[index] == child_count, "At this point all children must have been calculated");

	f32& calc_size = index ? calculated_size.y : calculated_size.x;
	calc_size = index ? (used_size.y + padding.y * 2.f) : (used_size.x + padding.x * 2.f);

	if (parent) parent->add_used_size(index, calc_size);
}

Box* _allocate_box(ID id)
{
	Context* context = get_context();
	Panel* panel = get_current_panel();
	
	LGUI_ASSERT(panel->box_lookup[0] && panel->box_lookup[1], "Box lookup arrays not initialized");
	Box** lookup_new = panel->box_lookup[context->current_frame % 2];
	Box** lookup_old = panel->box_lookup[(context->current_frame - 1) % 2];

	Box* new_box = context->temp_arena->allocate_one<Box>();

	// Find box from previous frame
	Box* old_box = nullptr;
	for (Box* it = lookup_old[id % BOX_TABLE_SIZE]; it; it = it->hash_next)
	//for (Box* it = lookup_old[id & (BOX_TABLE_SIZE - 1)]; it; it = it->hash_next)
	{
		if (it->id == id)
		{
			old_box = it;
			break;
		}
	}

	if (old_box)
	{
		*new_box = *old_box;
		new_box->next = nullptr;
		new_box->prev = nullptr;
		new_box->parent = nullptr;
		new_box->first_child = nullptr;
		new_box->last_child = nullptr;
		new_box->child_count = 0;
		new_box->known_size_child_count[0] = 0;
		new_box->known_size_child_count[1] = 0;
		new_box->prev_used_size = new_box->used_size;
		new_box->used_size = {};
		new_box->static_size = {};
		new_box->prev_calculated_position = new_box->calculated_position;
		new_box->counter = 0;
		new_box->is_size_calculated[0] = false;
		new_box->is_size_calculated[1] = false;
		new_box->next_unknown_size[0] = nullptr;
		new_box->next_unknown_size[1] = nullptr;
		// Keep calculated position/size so the user can reuse it
	}
	else
	{
		// All other fields are already null
		new_box->id = id;
		new_box->h_align = -1;
		new_box->v_align = -1;
	}

	// Insert box in new frame
	u32 index = id % BOX_TABLE_SIZE;
	new_box->hash_next = lookup_new[index];
	lookup_new[index] = new_box;

	return new_box;
}

void _init_box(Box* box, Size2 size, u32 flags)
{
	box->size[0] = size.x;
	box->size[1] = size.y;
	
	box->flags = flags;
}

Box* make_box(ID id, Size2 size, u32 flags)
{
	Box* box = _allocate_box(id);

	_init_box(box, size, flags);
	
	Box* parent = get_box();
	if (parent)
	{
		parent->append_child(box);
		box->begin();
		box->end();
	}

	return box;
}

Box* make_box(const char* name_id, Size2 size, u32 flags)
{
	return make_box(get_id(name_id), size, flags);
}

void push_box(Box* box)
{
	Context* context = get_context();

	push_id_raw(box->id);
	
	if (context->box_stack_top > 0 && !(box->flags & BoxFlag_IsRoot))
	{
		Box* parent = context->box_stack[context->box_stack_top - 1];
		parent->append_child(box);
	}

	box->begin();

	LGUI_ASSERT(context->box_stack_top < BOX_STACK_SIZE, "Box stack out of bounds");
	context->box_stack[context->box_stack_top] = box;
	++context->box_stack_top;
}

Box* push_box(ID id, Size2 size, u32 flags)
{
	Box* box = _allocate_box(id);
	_init_box(box, size, flags);

	push_box(box);

	return box;
}

Box* push_box(const char* name_id, Size2 size, u32 flags)
{
	return push_box(get_id(name_id), size, flags);
}

Box* pop_box()
{
	Context* context = get_context();

	LGUI_ASSERT(context->box_stack_top > 0, "Box stack is already empty");
	Box* pop = context->box_stack[context->box_stack_top - 1];
	--context->box_stack_top;
	
	pop->end();

	pop_id();

	return pop;
}

Box* get_box()
{
	Context* context = get_context();
	return context->box_stack_top > 0 ? context->box_stack[context->box_stack_top - 1] : nullptr;
}

Box* layout_horizontal(i8 h_align, i8 v_align, Size2 size, u32 flags)
{
	Box* push = push_box(box_generate_id(), size, flags | BoxFlag_IsHorizontal);
	push->h_align = h_align;
	push->v_align = v_align;

	return push;
}

Box* layout_vertical(i8 h_align, i8 v_align, Size2 size, u32 flags)
{
	Box* push = push_box(box_generate_id(), size, flags);
	push->h_align = h_align;
	push->v_align = v_align;

	return push;
}

void layout_end()
{
	pop_box();
}

void Box::set_rectangle(Color color)
{
	flags |= BoxFlag_DrawRectangle;
	this->color = color;
	this->outline_color = {};
	this->outline_size = 0;
}

void Box::set_rectangle(Color color, Color outline_color, f32 outline_size)
{
	flags |= BoxFlag_DrawRectangle;
	this->color = color;
	this->outline_color = outline_color;
	this->outline_size = outline_size;
}

void Box::set_circle(Color color)
{
	flags |= BoxFlag_DrawCircle;
	this->color = color;
	this->outline_color = {};
	this->outline_size = 0;
}

void Box::set_circle(Color color, Color outline_color, f32 outline_size)
{
	flags |= BoxFlag_DrawCircle;
	this->color = color;
	this->outline_color = outline_color;
	this->outline_size = outline_size;
}

void Box::set_draw_hook(void* ud, DrawHook hook)
{
	LGUI_ASSERT(hook, "Hook cannot be null");
	flags |= BoxFlag_DrawHook;
	this->draw_hook = hook;
	this->draw_user_data = ud;
}

void Box::set_draw_hook(DrawHook hook)
{
	LGUI_ASSERT(hook, "Hook cannot be null");
	flags |= BoxFlag_DrawHook;
	this->draw_hook = hook;
	this->draw_user_data = nullptr;
}

static void _draw_box(Painter& painter, Box* box, const Rect* clip_rect)
{
	// Position must already be known here

	// Optimization avoiding get_clip_rect
	Rect pass_clip_rect;

	Rect rect = {{box->calculated_position + box->padding}, {box->calculated_position + box->calculated_size - box->padding}};

	//debug_rect(rect, "lol", {1, 1, 0, 1});

	if (box->flags & BoxFlag_AnyDrawFlags && clip_rect->overlap(rect))
	{
		if (box->flags & BoxFlag_DrawRectangle)
		{
			if (box->outline_size > 0.f)
			{
				v2 outline = { box->outline_size, box->outline_size };
				painter.draw_rectangle(box->calculated_position, box->calculated_size, box->outline_color);
				painter.draw_rectangle(box->calculated_position + outline, box->calculated_size - outline * 2.f, box->color);
			}
			else
			{
				painter.draw_rectangle(box->calculated_position, box->calculated_size, box->color);
			}
		}
		if (box->flags & BoxFlag_DrawCircle)
		{
			v2 size = { box->calculated_size.x, box->calculated_size.x };
			if (box->outline_size > 0.f)
			{
				painter.draw_circle(box->calculated_position + size / 2.f, box->calculated_size.x / 2.f, 1.f, box->outline_color);
				painter.draw_circle(box->calculated_position + size / 2.f, box->calculated_size.x / 2.f - box->outline_size, 1.f, box->color);
			}
			else
			{
				painter.draw_circle(box->calculated_position + size / 2.f, box->calculated_size.x / 2.f, 1.f, box->color);
			}
		}
		if (box->flags & BoxFlag_DrawHook)
		{
			LGUI_ASSERT(box->draw_hook, "No draw hook provided");
			box->draw_hook(box, painter, rect);
		}
		if (box->flags & BoxFlag_DrawText)
		{
			LGUI_ASSERT(box->font, "Box wants to render text but has not font");
			v2 text_size = {box->font->text_width(box->text, box->text_length, 0.f), box->font->height};
			Rect r = rect.align_size(text_size, box->h_align, box->v_align);
			painter.draw_text(box->font, box->text, box->text_length, r.top_left, 0.f, box->text_color);
		}
	}
	if (box->flags & BoxFlag_Clip)
	{
		painter.push_clip_rect(rect);
		pass_clip_rect = painter.get_clip_rect();
		clip_rect = &pass_clip_rect;

		//debug_rect(*clip_rect, "clip", { 1, 0, 0, 1 });
	}

	// Render children
	if (box->first_child)
	{
		Rect children = rect.align_size(box->used_size, box->h_align, box->v_align);
		v2 pos = children.top_left + box->offset;
		f32 w = children.width();
		f32 h = children.height();
		bool horizontal = box->flags & BoxFlag_IsHorizontal;
		int v_align = box->v_align;
		int h_align = box->h_align;

		if (horizontal)
		{
			for (Box* it = box->first_child; it; it = it->next)
			{
				// Calculate child position
				it->calculated_position = pos;

				if (v_align == 0)
				{
					it->calculated_position.y += h / 2.f - it->calculated_size.y / 2.f;
				}
				else if (v_align == 1)
				{
					it->calculated_position.y += h - it->calculated_size.y;
				}

				_draw_box(painter, it, clip_rect);

				pos.x += it->calculated_size.x;
			}
		}
		else
		{
			for (Box* it = box->first_child; it; it = it->next)
			{
				// Calculate child position
				it->calculated_position = pos;

				if (h_align == 0)
				{
					it->calculated_position.x += w / 2.f - it->calculated_size.x / 2.f;
				}
				else if (h_align == 1)
				{
					it->calculated_position.x += w - it->calculated_size.x;
				}

				_draw_box(painter, it, clip_rect);

				pos.y += it->calculated_size.y;
			}
		}
	}

	if (box->flags & BoxFlag_Clip)
	{
		painter.pop_clip_rect();
	}

	// Handle Y scroll
	if (box->flags & BoxFlag_ScrollY)
	{
		f32 scroll_bar_width = 10.f;
		Color scroll_bar_color = {0.6f, 0.6f, 0.6f, 0.7f};
		f32 mouse_scroll_speed = 25.f;

		v2 top_right = box->calculated_position;
		top_right.x += box->calculated_size.x;

		f32 axis_size = box->calculated_size.y;
		f32 used_size = box->used_size.y;
		f32 area_size = rect.height();

		if (used_size > area_size)
		{
			// The size of the scroll bar is based on the size of the view compared to the size of the content
			// If the total content is 200 pixels high, but the view is 100 pixels high, the scroll bar will be 50 pixels high
			f32 scroll_bar_size = (area_size / used_size) * axis_size;
			f32 scroll_bar_offset = (-box->offset.y / (used_size - area_size)) * (axis_size - scroll_bar_size);
			Rect scroll_rect = Rect::from_pos_size(
				{top_right.x - scroll_bar_width, top_right.y + scroll_bar_offset}, 
				{scroll_bar_width, scroll_bar_size}
			);
			
			// Scroll bar dragging
			InputResult input = handle_element_input(scroll_rect, box->id + 1, true);
			if (input.dragging)
			{
				f32 value = input.drag_delta.y;
				box->offset.y -= (value / (axis_size - scroll_bar_size)) * (used_size - area_size);
			}

			// Mouse wheel input
			v2 mouse_wheel = mouse_scroll();
			bool any_shift = key_down(Key::LeftShift) || key_down(Key::RightShift);
			if (mouse_wheel.y != 0.f && !any_shift && is_mouse_overlapping(rect))
			{
				f32 value = -mouse_wheel.y * mouse_scroll_speed;
				box->offset.y -= (value / (axis_size - scroll_bar_size)) * (used_size - area_size);
			}

			// Clip
			if (box->offset.y < -(used_size - area_size))
			{
				box->offset.y = -(used_size - area_size);
			}
			if (box->offset.y > 0.f)
			{
				box->offset.y = 0.f;
			}

			// Rendering
			painter.draw_rectangle(scroll_rect, scroll_bar_color);
		}
		else
		{
			box->offset.y = 0.f;
		}
	}

	// Handle X scroll
	if (box->flags & BoxFlag_ScrollX)
	{
		f32 scroll_bar_height = 10.f;
		Color scroll_bar_color = {0.6f, 0.6f, 0.6f, 0.7f};
		f32 mouse_scroll_speed = 25.f;

		v2 bottom_left = box->calculated_position;
		bottom_left.y += box->calculated_size.y;

		f32 axis_size = box->calculated_size.x;
		f32 used_size = box->used_size.x;
		f32 area_size = rect.width();

		if (used_size > area_size)
		{
			// The size of the scroll bar is based on the size of the view compared to the size of the content
			// If the total content is 200 pixels high, but the view is 100 pixels high, the scroll bar will be 50 pixels high
			f32 scroll_bar_size = (area_size / used_size) * axis_size;
			f32 scroll_bar_offset = (-box->offset.x / (used_size - area_size)) * (axis_size - scroll_bar_size);
			Rect scroll_rect = Rect::from_pos_size(
				{bottom_left.x + scroll_bar_offset, bottom_left.y - scroll_bar_height},
				{scroll_bar_size, scroll_bar_height}
			);
			
			// Scroll bar dragging
			InputResult input = handle_element_input(scroll_rect, box->id + 2, true);
			if (input.dragging)
			{
				f32 value = input.drag_delta.x;
				box->offset.x -= (value / (axis_size - scroll_bar_size)) * (used_size - area_size);
			}

			// Mouse wheel input
			v2 mouse_wheel = mouse_scroll();
			bool any_shift = key_down(Key::LeftShift) || key_down(Key::RightShift);
			if (mouse_wheel.y != 0.f && any_shift && is_mouse_overlapping(rect))
			{
				f32 value = -mouse_wheel.y * mouse_scroll_speed;
				box->offset.x -= (value / (axis_size - scroll_bar_size)) * (used_size - area_size);
			}

			// Clip
			if (box->offset.x < -(used_size - area_size))
			{
				box->offset.x = -(used_size - area_size);
			}
			if (box->offset.x > 0.f)
			{
				box->offset.x = 0.f;
			}

			// Rendering
			painter.draw_rectangle(scroll_rect, scroll_bar_color);
		}
		else
		{
			box->offset.x = 0.f;
		}
	}

	//debug_rect(rect, "lol", {1, 1, 0, 1});
}

static void _draw_boxes(Painter& painter, Box* root, v2 start_pos)
{
	root->calculated_position = start_pos;
	Rect clip_rect = painter.get_clip_rect();
	_draw_box(painter, root, &clip_rect);
}

void debug_menu()
{
	Context* context = get_context();

	if (begin_panel("Debug Window", Rect::from_pos_size({}, {400, 300}), 0))
	{
		const int buffer_size = 64;
		char buffer[buffer_size]{};

		/*
		snprintf(buffer, buffer_size, "hover_id = %d", context->hover_id);
		text(buffer);
		snprintf(buffer, buffer_size, "active_id = %d", context->active_id);
		text(buffer);
		snprintf(buffer, buffer_size, "overlap_id = %d", context->overlap_id);
		text(buffer);
		snprintf(buffer, buffer_size, "overlap_panel = %p", context->overlap_panel);
		text(buffer);

		separator();
		*/


		/*
#ifdef LGUI_DEBUG_INFO
		LGUI_LINE_LAYOUT(-1)
		{
			checkbox("show_layout_rects", &d_show_layout_rects);
			text("Show layout rectangles");
		}

		LGUI_LINE_LAYOUT(-1)
		{
			checkbox("show layout_allocations", &d_show_layout_allocations);
			text("Show layout allocations");
		}
#endif
	*/

		end_panel();
	}
}

void debug_rect(Rect rect, const char* text, Color color)
{
	static f32 text_offset = 0.f;
	static u32 frame_updated = 0;
	Context* context = get_context();
	if (frame_updated != context->current_frame)
	{
		frame_updated = context->current_frame;
		text_offset = 0.f;
	}

	Painter& painter = get_painter();
	//painter.draw_rectangle(rect, color);
	painter.draw_rectangle_outline(rect, 2.f, color);

	if (rect.overlap(mouse_pos()))
	{
		Color text_color = color;
		color.a = 1.f;
		painter.draw_text(get_style().default_font, text, mouse_pos() + v2{3.f, 5.f + text_offset}, 0.f, text_color);
		text_offset += get_style().default_font->height + 2.f;
	}
}

}
