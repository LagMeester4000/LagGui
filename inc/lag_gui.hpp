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
	f32 area() const { return LGUI_ABS(bottom_right.x - top_left.x) * LGUI_ABS(bottom_right.y - top_left.y); }
	static Rect from_pos_size(v2 pos, v2 size) { return {pos, pos + size}; }
	// Returns a rect of overlapping region of the two rects (order does not matter)
	Rect clip(const Rect& other) const
	{
		return Rect{v2_max(top_left, other.top_left), v2_min(bottom_right, other.bottom_right)};
	}

	v2 top_right() const { return {bottom_right.x, top_left.y}; }
	v2 bottom_left() const { return {top_left.x, bottom_right.y}; }

	// Returns a rect that is centered within this rect
	Rect center_size(v2 size) const
	{
		v2 pos = (top_left + bottom_right) / 2.f - size / 2.f;
		return {pos, size};
	}
	v2 center() const { return (top_left + bottom_right) / 2.f; }

	bool overlap(v2 point) const
	{
		return (point.x >= top_left.x && point.x < bottom_right.x &&
				point.y >= top_left.y && point.y < bottom_right.y);
	}
	bool overlap(const Rect& other) const
	{
		return clip(other).area() > 0.f;
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

	void update_t_linear(bool hover, bool active, f32 dt, f32 duration = 0.25f);
	void update_t_towards(bool hover, bool active, f32 dt, f32 rate = 10.f);
};

// TEMP
struct Color {
	f32 r, g, b, a;
};

using TextureID = u32;

struct DrawCommand {
	DrawCommand* next;
	DrawCommand* prev;

	Rect clip_rect;
	u32 vertex_start;
	u32 vertex_end;
	u32 index_start;
	u32 index_end;
	TextureID texture_id;
};

struct Context;

struct DrawBuffer {
	f32* vertex_buffer;
	usize vertex_buffer_length;
	usize vertex_buffer_top;
	u16* index_buffer;
	usize index_buffer_length;
	usize index_buffer_top;

	void allocate(Context* context);
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
	//stbtt_fontinfo font_info;

	Slice<Glyph> glyphs;

	const Glyph& get_glyph(Codepoint codepoint) const;
	f32 text_width(const char* text, f32 spacing) const;
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

	Font* add_font(Context* context, const char* filename, f32 pixel_height);
	Icon* add_icon(Context* context, const char* name, byte* pixels_rgba, u32 width, u32 height);
	bool build(Context* context);
};

struct Style {
	Font* default_font;

	f32 line_padding; // Line padding on top of font size

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

struct Panel;

const usize MAX_CLIP_RECT = 16;

enum TriangleStripMode {
	TriangleStripMode_None,
	TriangleStripMode_Strip,
	TriangleStripMode_Convex,
};

struct Painter {
	Panel* panel;
	DrawCommand* first_command;
	DrawCommand* last_command;
	DrawCommand current_command;

	Rect clip_rect_stack[MAX_CLIP_RECT];
	u32 clip_rect_stack_top;

	// Call on begin_panel
	void _start_painter(Context* context);
	// Call when returning to a previous panel
	void _restart_painter(Context* context);
	void _push_command(Context* context);

	void push_clip_rect(Context* context, Rect rect);
	void pop_clip_rect(Context* context);
	Rect get_clip_rect();

	//void draw_triangle();
	void draw_rectangle(Context* context, v2 pos, v2 size, Color color, v2 uv1, v2 uv2);
	void draw_rectangle(Context* context, v2 pos, v2 size, Color color);
	void draw_rectangle(Context* context, Rect rect, Color color);

	TriangleStripMode triangle_strip_mode;
	// Count of vertices
	i8 triangle_strip_counter;
	u32 triangle_strip_indices[2];

	// Triangle strips
	void begin_triangle_strip();
	void end_triangle_strip();
	void add_strip_triangle(Context* context, v2 pos, Color color, v2 uv);
	void add_strip_triangle(Context* context, v2 pos, Color color);

	// Some way to render convex shapes (circles and such)
	void begin_convex_strip();
	void end_convex_strip();

	void draw_text(Context* context, Font* font, const char* text, v2 pos, f32 spacing, Color color);

	void draw_circle(Context* context, v2 pos, f32 size, f32 t, Color color);
};

void rl_render(Context* context);


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
	u32 frame_last_updated;
	bool open;
	f32 open_anim;

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

	// Retained data of contained elements
	RetainedData* retained_data_lookup[RETAINED_TABLE_SIZE];

	// Rendering
	Painter painter;
};

const usize ID_STACK_SIZE = 32;
const usize PANEL_STACK_SIZE = 32;
const usize STYLE_STACK_SIZE = 32;

struct Context {
	Panel* first_depth_panel;
	Panel* last_depth_panel;

	ID id_stack[ID_STACK_SIZE];
	u32 id_stack_top;
	ID prev_id; // ID of previous drawn element

	Panel* panel_stack[PANEL_STACK_SIZE];
	u32 panel_stack_top;

	// Input
	Panel* overlap_panel;
	ID overlap_id;
	ID hover_id;
	ID active_id;
	v2 mouse_pressed_pos[3];
	MouseState mouse_states[2];
	bool mouse_dragging;

	u32 current_frame;

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

	// Temp clip rectangle
	Rect clip_rect_stack[MAX_CLIP_RECT];
	u32 clip_rect_stack_top;

	// Style
	Style style_stack[STYLE_STACK_SIZE];
	u32 style_stack_top;

	// Atlas
	Atlas atlas;
};

// Core

Context* create_context();
void begin_frame(Context* context);
void end_frame(Context* context);

ID get_id(Context* context, const char* string);
ID get_id(Context* context, i32 i);
void push_id(Context* context, const char* string);
void push_id(Context* context, i32 i);
void pop_id(Context* context);

void push_style(Context* context, const Style& style);
void pop_style(Context* context);
const Style& get_style(Context* context);
void set_default_style(Context* context, const Style& style);

Panel* get_panel(Context* context, ID id);
Panel* get_current_panel(Context* context);
bool begin_panel(Context* context, const char* name, Rect rect, PanelFlag flags);
void end_panel(Context* context);
void move_panel_to_front(Context* context, Panel* panel);


// Gets the retained data, or create it if it doesn't exist
RetainedData* get_retained_data(Context* context, ID id);

v2 layout_next(Context* context);

InputResult handle_element_input(Context* context, Rect rect, ID id, bool enable_drag = false);
bool mouse_pressed(Context* context, int button = 0);
bool mouse_released(Context* context, int button = 0);
bool mouse_down(Context* context, int button = 0);
v2 mouse_pos(Context* context);



// Rendering
// Early rendering API, to be replaced with Painter

void set_clip_rect(v2 start, v2 end);
void reset_clip_rect();
void push_clip_rect(Context* context, Rect rect);
void pop_clip_rect(Context* context);

void draw_triangle(v2 p1, v2 p2, v2 p3, Color color);
void draw_triangle(v2 p1, v2 p2, v2 p3, Color c1, Color c2, Color c3);

void draw_rectangle(v2 pos, v2 size, Color color);



// Builder code

InputResult button(Context* context, const char* name);
InputResult checkbox(Context* context, const char* name, bool* value);
void text(Context* context, const char* text);

}
