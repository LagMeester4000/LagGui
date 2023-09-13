#pragma once
#include "basic.hpp"
#include "lag_gui.hpp"

namespace lgui {

struct Rect {
	v2 top_left;
	v2 bottom_right;

	v2 size() { return {LGUI_ABS(bottom_right.x - top_left.x), LGUI_ABS(bottom_right.y - top_left.y)}; }
	static Rect from_pos_size(v2 pos, v2 size) { return {pos, pos + size}; }
};

using ID = u32;

struct RetainedData {
	RetainedData* next;
	RetainedData* prev;
	ID id;

	// Animation
	f32 hover_t;
	f32 goal_hover_t;
	f32 active_t;
	f32 goal_active_t;

	// State
	bool open;
};

/*
Docking Explained

When you dock a panel into another panel, it creates a new panel that is the parent of both initial panels.
When docked child panels are removed and there is one panel left, that one panel will become the new root... or should it???
*/

enum DockDirection {
	DockDirection_Right,
	DockDirection_Down,
};

using PanelFlag = u32;
enum {
	PanelFlag_TitleBar = 1 << 0, // Show title bar
	PanelFlag_CanMove = 1 << 1, // Can move the panel by draggin the title
	PanelFlag_CanResize = 1 << 2, // Show resize control in bottom right
	PanelFlag_AutoResizeHorizontal = 1 << 3,
	PanelFlag_AutoResizeVertical = 1 << 4,

	PanelFlag_DrawBackground = 1 << 5,
	PanelFlag_ClipContent = 1 << 6, // Add a clip rect before rendering the content

	PanelFlag_Animate = 1 << 7, // Animate t values

	PanelFlag_DockedTitleBar = 1 << 8, // Show docked title bar (a tab), even when there are no other docked windows
	PanelFlag_CanDock = 1 << 9, // Panel that can dock into other dock panels, and other panels can dock into it
	PanelFlag_NoDockHorizontal = 1 << 10,
	PanelFlag_NoDockVertical = 1 << 11,
	PanelFlag_NoDockTab = 1 << 12,
	PanelFlag_DockRoot = 1 << 13, // The root dock that holds other windows
	PanelFlag_DockReplaceWhenOne = 1 << 14, // Replace this panel when it has one docked child left
};

const usize RETAINED_TABLE_SIZE = 16;

struct Panel {
	PanelFlag flags;
	ID id;

	Rect rect;
	Rect content;

	// Temp layouting
	v2 draw_pos;

	// Depth
	Panel* order_next; // Towards the screen
	Panel* order_prev;

	// Docking as a child
	Panel* dock_next;
	Panel* dock_prev;
	Panel* dock_parent;
	f32 dock_size;

	// Docking as a parent
	Panel* dock_first_child;
	Panel* dock_last_child;
	DockDirection dock_direction;

	RetainedData* lookup[RETAINED_TABLE_SIZE];
};

const usize ID_STACK_SIZE = 32;
const usize PANEL_STACK_SIZE = 32;

struct Context {
	Panel* first_depth_panel;
	Panel* last_depth_panel;

	ID id_stack[ID_STACK_SIZE];
	u32 id_stack_top;
	ID prev_id; // ID of previous drawn element

	Panel* panel_stack[PANEL_STACK_SIZE];
	u32 panel_stack_top;

	Panel* overlap_panel;
	ID overlap_id;
	ID hover_id;
	ID active_id;

	Arena arena;
	Panel* first_free_panel;
	RetainedData* first_free_retained_data;
};

// Core

Context* create_context();
void begin_frame();
void end_frame();

ID get_id(Context* context, const char* string);
ID get_id(Context* context, i32 i);
void push_id(Context* context, const char* string);
void push_id(Context* context, i32 i);
void pop_id(Context* context);

Panel* get_panel(Context* context, ID id);
Panel* get_current_panel(Context* context);
bool begin_panel(Context* context, const char* name, Rect rect, PanelFlag flags);
void end_panel(Context* context);

// Gets the retained data, or create it if it doesn't exist
RetainedData* get_retained_data(Context* context);

v2 layout_next(Context* context);



// Rendering

// TEMP
struct Color {
	f32 r, g, b, a;
};

void set_clip_rect(v2 start, v2 end);
void reset_clip_rect();

void draw_triangle(v2 p1, v2 p2, v2 p3, Color color);
void draw_triangle(v2 p1, v2 p2, v2 p3, Color c1, Color c2, Color c3);

void draw_rectangle(v2 pos, v2 size, Color color);



// Builder code

bool button(Context* context, const char* name);
void text(Context* context, const char* text);

}
