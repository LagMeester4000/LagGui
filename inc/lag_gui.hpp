#pragma once
#include "basic.hpp"
#include "lag_gui.hpp"

// TEMP
#include "stb_truetype.h"
#include "raylib.h"

namespace lgui {

struct Rect {
	v2 top_left;
	v2 bottom_right;

	v2 size() const { return {LGUI_ABS(bottom_right.x - top_left.x), LGUI_ABS(bottom_right.y - top_left.y)}; }
	f32 width() const { return LGUI_ABS(bottom_right.x - top_left.x); }
	f32 height() const { return LGUI_ABS(bottom_right.y - top_left.y); }
	f32 area() const { return LGUI_ABS(bottom_right.x - top_left.x) * LGUI_ABS(bottom_right.y - top_left.y); }
	static Rect from_pos_size(v2 pos, v2 size) { return {pos, pos + size}; }
	static Rect from_2_pos(v2 pos1, v2 pos2) { return {v2_min(pos1, pos2), v2_max(pos1, pos2)}; }
	// Returns a rect of overlapping region of the two rects (order does not matter)
	Rect clip(const Rect& other) const
	{
		Rect ret = {v2_max(top_left, other.top_left), v2_min(bottom_right, other.bottom_right)};
		ret.bottom_right.x = ret.bottom_right.x < ret.top_left.x ? ret.top_left.x : ret.bottom_right.x;
		ret.bottom_right.y = ret.bottom_right.y < ret.top_left.y ? ret.top_left.y : ret.bottom_right.y;
		return ret;
	}

	v2 top_right() const { return {bottom_right.x, top_left.y}; }
	v2 bottom_left() const { return {top_left.x, bottom_right.y}; }

	// Returns a rect that is centered within this rect
	Rect center_size(v2 size) const
	{
		v2 pos = (top_left + bottom_right) / 2.f - size / 2.f;
		return {pos, pos + size};
	}
	v2 center() const { return (top_left + bottom_right) / 2.f; }

	// Create a rect of given size within this one, align it as specified
	// horizontal/vertical values: -1 = min, 0 = center, 1 = max
	// align_size(x, -1, -1) = top-left, align_size(x, 0, 1) = bottom-center
	Rect align_size(v2 size, i8 horizontal, i8 vertical) const
	{
		v2 pos{};

		switch (horizontal)
		{
		case -1: pos.x = top_left.x; break;
		case 0: pos.x = top_left.x + (bottom_right.x - top_left.x) / 2.f - size.x / 2.f; break;
		case 1: pos.x = bottom_right.x - size.x; break;
		default: LGUI_TRAP("Invalid horizontal value used");
		}

		switch (vertical)
		{
		case -1: pos.y = top_left.y; break;
		case 0: pos.y = top_left.y + (bottom_right.y - top_left.y) / 2.f - size.y / 2.f; break;
		case 1: pos.y = bottom_right.y - size.y; break;
		default: LGUI_TRAP("Invalid vertical value used");
		}

		return {pos, pos + size};
	}


	bool overlap(v2 point) const
	{
		return (point.x >= top_left.x && point.x < bottom_right.x &&
				point.y >= top_left.y && point.y < bottom_right.y);
	}
	bool overlap(const Rect& other) const
	{
		return (top_left.x < other.bottom_right.x &&
				other.top_left.x < bottom_right.x &&
				top_left.y < other.bottom_right.y &&
				other.top_left.y < bottom_right.y);
	}

	void move(v2 movement)
	{
		top_left += movement;
		bottom_right += movement;
	}

	Rect pad(f32 amount) const
	{
		v2 s = size();
		v2 p = {
			amount * 2.f > s.x ? s.x / 2.f : amount,
			amount * 2.f > s.y ? s.y / 2.f : amount
		};
		return {top_left + p, bottom_right - p};
	}
	Rect pad(v2 amount) const
	{
		v2 s = size();
		v2 p = {
			amount.x * 2.f > s.x ? s.x / 2.f : amount.x,
			amount.y * 2.f > s.y ? s.y / 2.f : amount.y
		};
		return {top_left + p, bottom_right - p};
	}

	Rect cut_top(f32 amount)
	{
		f32 old = top_left.y;
		top_left.y = LGUI_MIN(top_left.y + amount, bottom_right.y);
		return Rect{{top_left.x, old}, {bottom_right.x, top_left.y}};
	}
	Rect cut_bottom(f32 amount)
	{
		f32 old = bottom_right.y;
		bottom_right.y = LGUI_MAX(bottom_right.y - amount, top_left.y);
		return Rect{{top_left.x, bottom_right.y}, {bottom_right.x, old}};
	}
	Rect cut_left(f32 amount)
	{
		f32 old = top_left.x;
		top_left.x = LGUI_MIN(top_left.x + amount, bottom_right.x);
		return Rect{{old, top_left.y}, {top_left.x, bottom_right.y}};
	}
	Rect cut_right(f32 amount)
	{
		f32 old = bottom_right.x;
		bottom_right.x = LGUI_MAX(bottom_right.x - amount, top_left.x);
		return Rect{{bottom_right.x, top_left.y}, {old, bottom_right.y}};
	}
	// Same as cut methods, but without mutating
	Rect get_top(f32 amount) const
	{
		return Rect{{top_left.x, top_left.y}, {bottom_right.x, LGUI_MIN(top_left.y + amount, bottom_right.y)}};
	}
	Rect get_bottom(f32 amount) const
	{
		return Rect{{top_left.x, LGUI_MAX(bottom_right.y - amount, top_left.y)}, {bottom_right.x, bottom_right.y}};
	}
	Rect get_left(f32 amount) const
	{
		return Rect{{top_left.x, top_left.y}, {LGUI_MIN(top_left.x + amount, bottom_right.x), bottom_right.y}};
	}
	Rect get_right(f32 amount) const
	{
		return Rect{{LGUI_MAX(bottom_right.x - amount, top_left.x), top_left.y}, {bottom_right.x, bottom_right.y}};
	}
};

using ID = u32;

struct RetainedData {
	RetainedData* next;
	RetainedData* prev;
	ID id;

	// Animation
	f32 hover_t;
	//f32 goal_hover_t;
	f32 active_t;
	//f32 goal_active_t;

	// State
	bool open;
	v2 pos;
	v2 scroll;
	Rect rect;
	v2 value_v2;
	i32 value_int;
	i32 value_int2;

	void update_t_linear(bool hover, bool active, f32 duration = 0.25f);
	void update_t_towards(bool hover, bool active, f32 rate = 10.f);
};

struct Color {
	f32 r, g, b, a;
};

using TextureID = u32;

struct DrawCommand {
	DrawCommand* next;
	DrawCommand* prev;

	Rect clip_rect;
	usize vertex_start;
	usize vertex_end;
	usize index_start;
	usize index_end;
	TextureID texture_id;

	// Needed to adjust clip rect retroactively
	u32 layout_depth;
	bool is_layout;
};

// A point in the painters drawing sequence
struct DrawCommandPoint {
	DrawCommand* command;
	usize vertex_pos;
	usize index_pos;
};

struct Context;

const u64 DRAW_INDEX_MAX = 1 << 16;
using DrawIndex = u16;

struct DrawBuffer {
	f32* vertex_buffer;
	usize vertex_buffer_length;
	usize vertex_buffer_top;
	DrawIndex* index_buffer;
	usize index_buffer_length;
	usize index_buffer_top;

	void allocate();
};

using Codepoint = i32;

struct Glyph {
	//Codepoint codepoint;
	v2 pos;
	v2 size;
	v2 uv1;
	v2 uv2;
	f32 advance_x;
};

struct Font {
	Font* next;

	const char* name;
	u32 name_hash;
	f32 height;

	Slice<Glyph> glyphs;

	const Glyph& get_glyph(Codepoint codepoint) const;
	f32 text_width(const char* text, f32 spacing) const;
	f32 text_width(const char* text, usize text_length, f32 spacing) const;
	// Returns index of first character that doesn't fit (aka the length of the new string)
	usize find_text_width_fit(const char* text, usize length, f32 spacing, f32 max_width) const;
};

struct Icon {
	Icon* next;

	u32 name_hash;
	u32 width;
	u32 height;
	byte* pixels; // Set to nullptr once used
	v2 uv1;
	v2 uv2;
};

struct Atlas {
	TextureID texture_id;
	Texture2D texture_obj;
	u32 width;
	u32 height;

	Font* first_font;
	Font* last_font;

	Icon* first_icon;
	Icon* last_icon;
	u32 icon_count;

	bool is_built() const
	{
		return width != 0 && height != 0;
	}

	Font* add_font(const char* filename, f32 pixel_height);
	Icon* add_icon(const char* name, byte* pixels_rgba, u32 width, u32 height);
	bool build();
};

struct Style {
	Font* default_font;

	f32 line_padding; // Line padding on top of font size
	f32 default_layout_spacing;

	Color window_background;
	Color window_outline;
	Color window_title_background;
	Color window_title_color;

	Color button_background;
	Color button_background_hover;
	Color button_background_down;
	f32 button_padding;

	Color checkbox_outline;

	f32 line_height() const { return default_font->height + line_padding * 2.f; }
};

enum LayoutFlags {
	LayoutFlag_IsHorizontal = 1 << 0,
	LayoutFlag_Reverse = 1 << 1,
	LayoutFlag_FixedH = 1 << 3, // Fixed horizontal size
	LayoutFlag_FixedV = 1 << 4, // Fixed vertical size
	LayoutFlag_Static = 1 << 5,

	LayoutFlag_ScrollX = 1 << 6,
	LayoutFlag_ScrollY = 1 << 7,
	LayoutFlag_Clip = 1 << 8,
	LayoutFlag_DrawBackground = 1 << 9,
};

struct Layout {
	// Required if the layout has retained state
	// Retained data stores the size of the layout, which is needed to correctly calculate widget positions for input
	ID id;
	RetainedData* retained_data;

	u32 flags;
	// Counter for generating IDs
	i32 counter;

	// The space between elements on the axis
	f32 spacing;

	// TODO: Implement
	v2 padding;

	// Alignment of child elements within the layout
	// -1 = min, 0 = center, 1 = max
	i8 h_align;
	i8 v_align;

	// The position that the layout starts at
	// If the layout is reverse, this value will be at the opposite side of the rect
	v2 start;
	// The offset added to every allocated widget position (used for scrolling)
	v2 offset;
	// Cursor position on the axis (horizontal or vertical)
	f32 cursor;
	// Growing value to calculate the size on the cross axis
	f32 cross_axis_max;

	v2 max_size;
	// The size used when creating the layout
	v2 original_size;

	// The drawcommand state when this layout started
	// Used to iterate over draw command and move elements
	DrawCommandPoint draw_command_point;

	// Background
	bool background_has_outline;
	f32 background_outline_size;
	Color background_colors[8];
	DrawCommandPoint background_draw_point;

	Rect allocate(v2 size);

	Rect get_cursor_rect();
	// Pushes the cursor of this layout to match the
	void end_child_layout(Layout* child);

	// Get the full-size rectangle of the layout, including the cursor
	// If the layout is fixed on an axis then it will have the provided size
	v2 get_full_size();

	// Get the full-size rectangle of the layout, including the cursor
	Rect get_stretched_rect();
	v2 get_stretched_size();


	// Get a rect that includes all the allocated element rects
	Rect get_used_rect();

	v2 get_scroll_pos();
	void set_scroll_pos(v2 scroll_pos);
};

struct Panel;

const usize MAX_CLIP_RECT = 16;

enum TriangleStripMode {
	TriangleStripMode_None,
	TriangleStripMode_Strip,
	TriangleStripMode_Convex,
};

struct ClipRect {
	Rect original_rect;
	// Clip rect that immediately gets clipped by the previous clip rect
	Rect input_clip_rect;
	bool is_layout; // Otherwise, the layout_id refers to the parent
	u32 layout_depth;
};

struct Painter {
	DrawCommand* first_command;
	DrawCommand* last_command;
	DrawCommand* current_command;

	u32 frame_last_updated;

	ClipRect clip_rect_stack[MAX_CLIP_RECT];
	u32 clip_rect_stack_top;

	// Call on begin_panel
	void _start_painter();
	// Call when returning to a previous panel
	void _restart_painter();
	void _push_command();

	void push_clip_rect(Rect rect);
	void _push_layout_clip_rect(Rect rect, u32 layout_depth);
	void pop_clip_rect();
	Rect get_clip_rect();
	ClipRect& _get_internal_clip_rect();

	DrawCommandPoint _get_draw_command_point();
	void _move_vertices_in_range(DrawCommandPoint p1, DrawCommandPoint p2, v2 movement);
	void _adjust_clip_rect_in_range(DrawCommandPoint p1, DrawCommandPoint p2, v2 movement, bool replace, u32 layout_depth, Rect new_clip);

	//void draw_triangle();
	void draw_rectangle(v2 pos, v2 size, Color color, v2 uv1, v2 uv2);
	void draw_rectangle(v2 pos, v2 size, Color color);
	void draw_rectangle(Rect rect, Color color);
	void draw_rectangle_gradient(v2 pos, v2 size, Color c1, Color c2, Color c3, Color c4);
	void draw_rectangle_gradient(Rect rect, Color c1, Color c2, Color c3, Color c4);
	void draw_rectangle_outline(v2 pos, v2 size, f32 thickness, Color color);
	void draw_rectangle_outline(Rect rect, f32 thickness, Color color);

	TriangleStripMode triangle_strip_mode;
	// Count of vertices
	i8 triangle_strip_counter;
	u32 triangle_strip_indices[2];

	// Triangle strips
	void begin_triangle_strip();
	void end_triangle_strip();
	void add_strip_triangle(v2 pos, Color color, v2 uv);
	void add_strip_triangle(v2 pos, Color color);

	// Some way to render convex shapes (circles and such)
	void begin_convex_strip();
	void end_convex_strip();

	// Returns the width of the rendered text
	f32 draw_text(Font* font, const char* text, v2 pos, f32 spacing, Color color);
	// Returns the width of the rendered text
	f32 draw_text(Font* font, const char* text, usize text_length, v2 pos, f32 spacing, Color color);
	// Will attempt to draw text in the center of the provided rectangle, if it doesn't fit, it replaces text
	//   with dots (...) to still make it fit
	// Returns true when original text fits, returns false when it doesn't
	bool draw_text_fit(Font* font, const char* text, Rect rect, f32 spacing, Color color, i8 h_align = 0, i8 v_align = 0);

	void draw_circle(v2 pos, f32 size, f32 t, Color color);

	void draw_round_corner(v2 pos, v2 size, bool is_right, bool is_bottom, Color color, bool start_end_strip = true, bool reverse = false);
	// corner_size = [top_left, top_right, bottom_left, bottom_right]
	void draw_rounded_rectangle(v2 pos, v2 size, f32 corner_size[4], Color color);
	void draw_rounded_rectangle(Rect rect, f32 corner_size[4], Color color);

	// Retroactive drawing
	DrawCommandPoint retroactive_allocate_rectangle(bool has_outline);
	// Requires collor array of size 4, or 8 if it has an outline
	void retroactive_draw_rectangle(DrawCommandPoint point, bool has_outline, v2 pos, v2 size, Color* colors, f32 outline_size = 0.f);
};

void rl_render();


// Returned by
struct InputResult {
	bool pressed;
	bool released;
	bool clicked;
	bool hover;
	bool down;
	// True when a value is changed
	bool changed;
	bool dragging;
	// Mouse position when the dragging started
	v2 drag_start;
	v2 drag_delta;
};

const f32 MIN_DRAG_DISTANCE = 2.f;

struct MouseState {
	v2 pos;
	bool buttons[3];
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

struct Dock {
	Rect rect;
	u32 last_frame_registered; // Last frame this dock was added to the root dock list

	// Root list
	Dock* next_root;

	DockDirection dock_direction;
	f32 split_pos; // Split offset in pixels
	Dock* child_docks[2];
	Dock* parent_dock;
	Panel* root_panel;

	// The windows within this dock
	Panel* first_tab;
	Panel* last_tab;
	Panel* selected_tab;

	bool is_root() const { return parent_dock == nullptr; }
	// Root can also be leaf
	bool is_leaf() const { return first_tab != nullptr; }
};

enum DockEntry {
	DockEntry_Into,
	DockEntry_Right,
	DockEntry_Left,
	DockEntry_Top,
	DockEntry_Bottom,
};

enum DockCommandType {
	DockCommandType_DockInto,
	DockCommandType_Remove,
	DockCommandType_SelectTab,
};

// Docking command executed at the end of the frame
// Allocated in temporary storage
struct DockCommand {
	DockCommand* next;
	DockCommandType type;
	DockEntry entry;
	Panel* panel1;
	Panel* panel2;
};

using PanelFlag = u32;
enum {
	PanelFlag_TitleBar = 1 << 0, // Show title bar
	PanelFlag_MenuBar = 1 << 1, // Show menu bar
	PanelFlag_CanMove = 1 << 2, // Can move the panel by draggin the title
	PanelFlag_CanResize = 1 << 3, // Show resize control in bottom right
	PanelFlag_AutoResizeHorizontal = 1 << 4,
	PanelFlag_AutoResizeVertical = 1 << 5,

	PanelFlag_DrawBackground = 1 << 6,
	PanelFlag_ClipContent = 1 << 7, // Add a clip rect before rendering the content

	PanelFlag_Animate = 1 << 8, // Animate t values

	PanelFlag_DockedTitleBar = 1 << 9, // Show docked title bar (a tab), even when there are no other docked windows
	PanelFlag_CanDock = 1 << 10, // Panel that can dock into other dock panels, and other panels can dock into it
	PanelFlag_NoDockHorizontal = 1 << 11,
	PanelFlag_NoDockVertical = 1 << 12,
	PanelFlag_NoDockTab = 1 << 13,
	PanelFlag_DockRoot = 1 << 14, // The root dock that holds other windows
	PanelFlag_DockReplaceWhenOne = 1 << 15, // Replace this panel when it has one docked child left
};

const usize RETAINED_TABLE_SIZE = 16;
const usize PANEL_NAME_SIZE = 16;

struct Panel {
	PanelFlag flags;
	ID id;
	u32 frame_last_updated;
	bool open;
	f32 open_anim;

	Rect rect;
	Rect content;

	v2 scroll_pos;

	// Temp layouting
	v2 draw_pos;
	v2 start_draw_pos;
	v2 end_draw_pos;

	// Depth
	Panel* order_next; // Towards the screen
	Panel* order_prev;

	// General list
	Panel* hash_next;
	Panel* hash_prev;

	// Docking
	Dock* parent_dock; // Actually the root dock if this is the root panel
	Panel* next_dock_tab;
	Panel* prev_dock_tab;
	Panel* root_dock_panel; // TODO: Can this be removed in favor of parent_dock->root_panel?
	bool is_dock_root;
	bool is_docked() const { return parent_dock; }

	// Retained data of contained elements
	RetainedData* retained_data_lookup[RETAINED_TABLE_SIZE];

	// Rendering
	Painter painter;
	Painter& get_painter() { return is_docked() ? parent_dock->root_panel->painter : painter; }

	char name[PANEL_NAME_SIZE];

	// (If docked) the rectangle of the tab for this panel
	Rect dock_tab_rect;
};

const usize ID_STACK_SIZE = 32;
const usize PANEL_STACK_SIZE = 32;
const usize STYLE_STACK_SIZE = 32;
const usize LAYOUT_STACK_SIZE = 32;
const usize PANEL_MAP_SIZE = 32;

struct Context {
	// List of root panels sorted by depth/render order
	Panel* first_depth_panel;
	Panel* last_depth_panel;

	// Lost of all root docks
	Dock* first_root_dock;
	Dock* last_root_dock;

	DockCommand* first_dock_command;
	DockCommand* last_dock_command;

	ID id_stack[ID_STACK_SIZE];
	u32 id_stack_top;
	ID prev_id; // ID of previous drawn element

	Panel* panel_stack[PANEL_STACK_SIZE];
	u32 panel_stack_top;

	// Input
	Panel* overlap_panel;
	ID overlap_id; // Used to find which element was last hovered at the end of the frame, to set hover_id
	ID hover_id; // Element that the cursor hovers over
	ID active_id; // Element that the mouse was pressed on (used for registering dragging and clicks)
	ID selected_id; // Element selected (text edit)
	v2 mouse_pressed_pos[3];
	MouseState mouse_states[2];
	bool mouse_dragging;

	u32 current_frame;
	f32 delta_time;

	v2 app_window_size;

	// Cleared on shutdown
	Arena arena;
	// Cleared every frame
	Arena temp_arena;

	// Stored in main arena
	Panel* first_free_panel;
	RetainedData* first_free_retained_data;
	DrawCommand* first_free_draw_command;

	DrawBuffer draw_buffer;
	DrawBuffer merge_draw_buffer;

	// Style
	Style style_stack[STYLE_STACK_SIZE];
	u32 style_stack_top;

	// Layout
	Layout layout_stack[LAYOUT_STACK_SIZE];
	u32 layout_stack_top;
	bool layout_next_draw_background;
	bool layout_background_has_outline;
	f32 layout_background_outline_size;
	Color layout_background_colors[8];

	// Panel lookup
	// Maps ID to panel
	Panel* panel_map[PANEL_MAP_SIZE];

	// Atlas
	Atlas atlas;
};



// Core

Context* init();
void deinit();
Context* get_context();
void begin_frame(f32 delta_time);
void end_frame();
void draw_frame();


// To check if any ui is hovered over with the mouse
bool is_anything_hovered();


ID get_id(const char* string);
ID get_id(i32 i);
ID get_id(void* ptr);
ID peek_id();
void push_id(const char* string);
void push_id(i32 i);
void push_id(void* ptr);
void push_id_raw(ID id);
void pop_id();


void push_style(const Style& style);
void pop_style();
const Style& get_style();
void set_default_style(const Style& style);


bool begin_layout(const Layout& layout);
void end_layout();
Rect layout_next(v2 size);
Layout& get_layout();

bool layout_static(ID id, Rect rect, bool horizontal, bool reverse, i8 line_h_align, i8 line_v_align, f32 spacing, v2 padding = {}, u32 flags = 0);
bool layout_unknown(ID id, v2 size, bool horizontal, bool reverse, i8 line_h_align, i8 line_v_align, f32 spacing, v2 padding = {}, u32 flags = 0);
bool layout_horizontal(i8 h_align = -1, i8 v_align = -1, v2 size = {0.f, 0.f}, bool reverse = false, f32 spacing = -1.f, v2 padding = {}, u32 flags = 0);
bool layout_vertical(i8 h_align = -1, i8 v_align = -1, v2 size = {0.f, 0.f}, bool reverse = false, f32 spacing = -1.f, v2 padding = {}, u32 flags = 0);
// Horizontal layout with the height of a line and the width of the layout
bool layout_line(i8 h_align, i8 v_align = 0, bool reverse = false, f32 spacing = -1.f);

// Layout macro helpers (you don't have to use these if you don't want to)
#define LGUI_H_LAYOUT(...) LGUI_DEFER_LOOP(lgui::layout_horizontal(__VA_ARGS__), lgui::end_layout())
#define LGUI_V_LAYOUT(...) LGUI_DEFER_LOOP(lgui::layout_vertical(__VA_ARGS__), lgui::end_layout())
#define LGUI_LINE_LAYOUT(...) LGUI_DEFER_LOOP(lgui::layout_line(__VA_ARGS__), lgui::end_layout())

// Generate an ID based on position in the layout
ID layout_generate_id();
f32 layout_width();
f32 layout_height();

void set_next_layout_background(Color color_in[4], bool outline, Color color_outline[4], f32 outline_size);
void set_next_layout_background(Color color, Color outline_color, f32 outline_size);
void set_next_layout_background(Color color);


Panel* get_panel(ID id);
Panel* get_current_panel();
bool begin_panel(const char* name, Rect rect, PanelFlag flags);
void end_panel();
void move_panel_to_front(Panel* panel);
Painter& get_painter();


// Gets the retained data, or create it if it doesn't exist
RetainedData* get_retained_data(ID id);


InputResult handle_element_input(Rect rect, ID id, bool enable_drag = false, bool ignore_clip = false);
bool mouse_pressed(int button = 0);
bool mouse_released(int button = 0);
bool mouse_down(int button = 0);
v2 mouse_pos();


void select_element(ID id);



// Builder code

InputResult button(const char* name);
InputResult checkbox(const char* name, bool* value);
InputResult radio_button(const char* name, int option, int* selected);
InputResult drag_value(const char* name, f32* value);
bool collapse_header(const char* name);
void text(const char* text, bool wrap = false);
bool input_text(char* buffer, usize buffer_size, bool wrap = false);
void separator();
// Only works in vertical layout
void separator_text(const char* text);
// Emtpy space on the axis of the layout
void spacer(f32 size);
bool begin_fancy_collapse_header(const char* name);
void end_fancy_collapse_header();

void draw_open_triangle(Painter* painter, v2 pos, f32 size, f32 rotation, Color color);



// Debug

void debug_menu();
// Draw rectangle and shows text when hovered
void debug_rect(Rect rect, const char* text, Color color);


}
