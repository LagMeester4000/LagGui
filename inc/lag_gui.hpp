#pragma once
#include "basic.hpp"

// TEMP
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
	f32 window_content_padding;

	Color window_background;
	Color window_outline;
	Color window_title_background;
	Color window_title_color;

	Color button_background;
	Color button_background_hover;
	Color button_background_down;
	Color button_text;
	f32 button_padding;

	Color radio_button_background;
	Color radio_button_background_hover;
	Color radio_button_inside;
	Color radio_button_outline;
	f32 radio_button_outline_size;

	Color checkbox_outline;
	Color checkbox_background;
	Color checkbox_inside;
	f32 checkbox_outline_size;

	Color separator;
	f32 separator_size;
	f32 separator_spacing;

	f32 line_height() const { return default_font->height + line_padding * 2.f; }
};

struct Box;
struct Painter;

using DrawHook = void(*)(Box* box, Painter& painter, Rect rect);

enum SizeTypes {
	SizeType_Px,
	SizeType_Percent, // Percentage of parent
	SizeType_Remainder, // Percentage of remaining size in parent (size - static_size)
	SizeType_Fit, // Size of children
};

struct Size {
	u32 type;
	f32 value;
};

struct Size2 {
	Size x;
	Size y;
};

inline Size px(f32 v) { return {SizeType_Px, v}; }
inline Size2 px(f32 x, f32 y) { return {{SizeType_Px, x}, {SizeType_Px, y}}; }
inline Size pc(f32 v) { return {SizeType_Percent, v}; }
inline Size2 pc(f32 x, f32 y) { return {{SizeType_Percent, x}, {SizeType_Percent, y}}; }
inline Size fit() { return {SizeType_Fit, 1.f}; }
inline Size rem(f32 v) { return {SizeType_Remainder, v}; }

enum BoxFlags {
	BoxFlag_IsHorizontal = 1 << 0,
	BoxFlag_Reverse = 1 << 1,
	BoxFlag_FixedH = 1 << 3, // Fixed horizontal size
	BoxFlag_FixedV = 1 << 4, // Fixed vertical size
	BoxFlag_Static = 1 << 5,
	BoxFlag_IsRoot = 1 << 6,

	BoxFlag_ScrollX = 1 << 7,
	BoxFlag_ScrollY = 1 << 8,
	BoxFlag_Clip = 1 << 9,

	// Animate position relative to parent
	BoxFlag_AnimateX = 1 << 10,
	BoxFlag_AnimateY = 1 << 11,

	// Draw flags
	BoxFlag_DrawRectangle = 1 << 12, // Will draw a full-size rectangle in the box
	BoxFlag_DrawCircle = 1 << 13,
	BoxFlag_DrawText = 1 << 14, // Will draw text in in the aligned position within the box
	BoxFlag_DrawHook = 1 << 15,

	// Combined flags
	BoxFlag_AnyDrawFlags = BoxFlag_DrawRectangle | BoxFlag_DrawCircle | BoxFlag_DrawText | BoxFlag_DrawHook,
};

struct Box {
	Box* parent;
	Box* next;
	Box* prev;
	Box* first_child;
	Box* last_child;

	Box* next_unknown_size[2];

	Box* hash_next;

	ID id;
	u32 flags;
	// Counter for generating IDs
	i32 counter;

	// The space between elements on the axis
	f32 spacing;
	v2 padding;

	// Alignment of child elements within the layout
	// -1 = min, 0 = center, 1 = max
	i8 h_align;
	i8 v_align;

	Size size[2];

	// Final size of the layout
	v2 calculated_size;
	bool is_size_calculated[2];

	// Added to by child elements
	// + on axis, max() on cross-axis
	v2 used_size;
	v2 prev_used_size;
	// Combined size of all static (pixel) size elements
	v2 static_size;

	u32 child_count;
	// For each axis, how many of the children were able to calculate their size
	u32 known_size_child_count[2];

	// Final position of the box
	v2 calculated_position;
	v2 prev_calculated_position;

	// Value added to the final position of the children of this box
	v2 offset;

	DrawHook draw_hook;
	void* draw_user_data;

	// Styling
	Color color;
	Color outline_color;
	Color text_color;
	f32 outline_size;
	const char* text;
	usize text_length;
	Font* font;

	// Animation
	f32 hover_t;
	f32 active_t;

	// Adds the child box to the tree
	void append_child(Box* box);

	void begin();

	// Calculates parent (this) box size, if it fails it adds it to the unknown size list
	void end();

	// at_end indicates if the node has ended (or has no (more) children)
	void end_calculate_size(int index);
	void add_used_size(int index, f32 pixel_size);
	void add_static_size(int index, f32 pixel_size);

	// Calculate size after the initial build phase
	// Asserts if size cannot be calculated (which probably means the unknown size list is incorrect, or there is a bug)
	void post_calculate_percent(int index);
	void post_calculate_fit(int index);

	Rect prev_rect() { return Rect::from_pos_size(calculated_position, calculated_size); }

	void set_rectangle(Color color);
	void set_rectangle(Color color, Color outline_color, f32 outline_size);
	void set_circle(Color color);
	void set_circle(Color color, Color outline_color, f32 outline_size);
	void set_draw_hook(DrawHook hook);
	void set_draw_hook(void* ud, DrawHook hook);
};

struct Panel;

const usize MAX_CLIP_RECT = 16;

enum TriangleStripMode {
	TriangleStripMode_None,
	TriangleStripMode_Strip,
	TriangleStripMode_Convex,
};

struct Painter {
	DrawCommand* first_command;
	DrawCommand* last_command;
	DrawCommand* current_command;

	u32 frame_last_updated;

	Rect clip_rect_stack[MAX_CLIP_RECT];
	u32 clip_rect_stack_top;

	// Call on begin_panel
	void _start_painter();
	// Call when returning to a previous panel
	void _restart_painter();
	void _push_command();

	void push_clip_rect(Rect rect);
	void pop_clip_rect();
	Rect get_clip_rect();

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
};

void rl_render();

// Same key mapping as Raylib and GLFW
// Lifted from Raylib
struct Key {
	enum {
		Null = 0, // Key: NULL, used for no key pressed
		// Alphanumeric keys
		Apostrophe = 39, // Key: '

		Comma = 44, // Key: ,
		Minus = 45, // Key: -
		Period = 46, // Key: .
		Slash = 47, // Key: /
		Zero = 48, // Key: 0
		One = 49, // Key: 1
		Two = 50, // Key: 2
		Three = 51, // Key: 3
		Four = 52, // Key: 4
		Five = 53, // Key: 5
		Six = 54, // Key: 6
		Seven = 55, // Key: 7
		Eight = 56, // Key: 8
		Nine = 57, // Key: 9
		Semicolon = 59, // Key: ;
		Equals = 61, // Key: =
		A = 65, // Key: A | a
		B = 66, // Key: B | b
		C = 67, // Key: C | c
		D = 68, // Key: D | d
		E = 69, // Key: E | e
		F = 70, // Key: F | f
		G = 71, // Key: G | g
		H = 72, // Key: H | h
		I = 73, // Key: I | i
		J = 74, // Key: J | j
		K = 75, // Key: K | k
		L = 76, // Key: L | l
		M = 77, // Key: M | m
		N = 78, // Key: N | n
		O = 79, // Key: O | o
		P = 80, // Key: P | p
		Q = 81, // Key: Q | q
		R = 82, // Key: R | r
		S = 83, // Key: S | s
		T = 84, // Key: T | t
		U = 85, // Key: U | u
		V = 86, // Key: V | v
		W = 87, // Key: W | w
		X = 88, // Key: X | x
		Y = 89, // Key: Y | y
		Z = 90, // Key: Z | z
		LeftBracket = 91, // Key: [
		Backslash = 92, // Key: '\'
		RightBracket = 93, // Key: ]
		Tilde = 96, // Key: `
		// Function keys
		Space = 32, // Key: Space
		Escape = 256, // Key: Esc
		Enter = 257, // Key: Enter
		Tab = 258, // Key: Tab
		Backspace = 259, // Key: Backspace
		Insert = 260, // Key: Ins
		Delete = 261, // Key: Del
		Right = 262, // Key: Cursor right
		Left = 263, // Key: Cursor left
		Down = 264, // Key: Cursor down
		Up = 265, // Key: Cursor up
		PageUp = 266, // Key: Page up
		PageDown = 267, // Key: Page down
		Home = 268, // Key: Home
		End = 269, // Key: End
		CapsLock = 280, // Key: Caps lock
		ScrollLock = 281, // Key: Scroll down
		NumLock = 282, // Key: Num lock
		PrintScreen = 283, // Key: Print screen
		Pause = 284, // Key: Pause
		F1 = 290, // Key: F1
		F2 = 291, // Key: F2
		F3 = 292, // Key: F3
		F4 = 293, // Key: F4
		F5 = 294, // Key: F5
		F6 = 295, // Key: F6
		F7 = 296, // Key: F7
		F8 = 297, // Key: F8
		F9 = 298, // Key: F9
		F10 = 299, // Key: F10
		F11 = 300, // Key: F11
		F12 = 301, // Key: F12
		LeftShift = 340, // Key: Shift left
		LeftControl = 341, // Key: Control left
		LeftAlt = 342, // Key: Alt left
		LeftSuper = 343, // Key: Super left
		RightShift = 344, // Key: Shift right
		RightControl = 345, // Key: Control right
		RightAlt = 346, // Key: Alt right
		RightSuper = 347, // Key: Super right
		KBMenu = 348, // Key: KB menu
		// Keypad keys
		Keypad0 = 320, // Key: Keypad 0
		Keypad1 = 321, // Key: Keypad 1
		Keypad2 = 322, // Key: Keypad 2
		Keypad3 = 323, // Key: Keypad 3
		Keypad4 = 324, // Key: Keypad 4
		Keypad5 = 325, // Key: Keypad 5
		Keypad6 = 326, // Key: Keypad 6
		Keypad7 = 327, // Key: Keypad 7
		Keypad8 = 328, // Key: Keypad 8
		Keypad9 = 329, // Key: Keypad 9
		KeypadDecimal = 330, // Key: Keypad .
		KeypadDivide = 331, // Key: Keypad /
		KeypadMultiply = 332, // Key: Keypad *
		KeypadSubtract = 333, // Key: Keypad -
		KeypadAdd = 334, // Key: Keypad +
		KeypadEnter = 335, // Key: Keypad Enter
		KeypadEquals = 336, // Key: Keypad =
		// Android key buttons
		Back = 4, // Key: Android back button
		Menu = 82, // Key: Android menu button
		VolumeUp = 24, // Key: Android volume up button
		VolumeDown = 25, // Key: Android volume down button
		MAX = 350,

		CURRENT_FRAME_MASK = 1 << 1,
		PREV_FRAME_MASK = 1 << 0,
	};
};

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
	v2 scroll_wheel;
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
	PanelFlag_NoTitleBar = 1 << 0,
	PanelFlag_NoBackground = 1 << 1,
	PanelFlag_NoMove = 1 << 2, // Cannot move the panel by dragging the title
	PanelFlag_CanResize = 1 << 3, // Show resize control in bottom right
	PanelFlag_AutoResizeHorizontal = 1 << 4, // Otherwise, use a scroll bar
	PanelFlag_AutoResizeVertical = 1 << 5, // Otherwise, use a scroll bar
	PanelFlag_AlwaysResetRect = 1 << 6,
	PanelFlag_AlwaysBackground = 1 << 7, // Can't move this panel to the background

	PanelFlag_NoClipContent = 1 << 8, // Add a clip rect before rendering the content

	PanelFlag_DockedTitleBar = 1 << 9, // Show docked title bar (a tab), even when there are no other docked windows
	PanelFlag_CanDock = 1 << 10, // Panel that can dock into other dock panels, and other panels can dock into it
	PanelFlag_NoDockHorizontal = 1 << 11,
	PanelFlag_NoDockVertical = 1 << 12,
	PanelFlag_NoDockTab = 1 << 13,
	PanelFlag_DockRoot = 1 << 14, // The root dock that holds other windows
	PanelFlag_DockReplaceWhenOne = 1 << 15, // Replace this panel when it has one docked child left
};

const usize RETAINED_TABLE_SIZE = 16;
const usize BOX_TABLE_SIZE = 64;
const usize PANEL_NAME_SIZE = 16;

struct Panel {
	PanelFlag flags;
	ID id;
	u32 frame_last_updated;
	bool open;

	Rect rect;

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

	// Boxs from previous and current frame
	// Swap between these two every frame
	Box** box_lookup[2];

	Box* first_unknown_fit[2];
	Box* last_unknown_fit[2];
	// Pc is a reverse singly linked list
	Box* last_unknown_pc[2];

	Box* root_box;

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
const usize BOX_STACK_SIZE = 32;
const usize PANEL_MAP_SIZE = 32;
const usize INPUT_CODEPOINT_MAX = 8;

struct Context {
	// List of root panels sorted by depth/render order
	Panel* first_depth_panel;
	Panel* last_depth_panel;

	Panel* popup_panel;

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
	// Cleared every other frame (swaps between them every frame
	Arena* temp_arena;
	Arena temp_arena_arr[2];

	// Stored in main arena
	Panel* first_free_panel;
	RetainedData* first_free_retained_data;
	DrawCommand* first_free_draw_command;

	DrawBuffer draw_buffer;
	DrawBuffer merge_draw_buffer;

	// Style
	Style style_stack[STYLE_STACK_SIZE];
	u32 style_stack_top;

	// Box
	Box* box_stack[BOX_STACK_SIZE];
	u32 box_stack_top;

	// Panel lookup
	// Maps ID to panel
	Panel* panel_map[PANEL_MAP_SIZE];

	// Atlas
	Atlas atlas;

	// Key input
	Codepoint codepoints_pressed[INPUT_CODEPOINT_MAX];
	usize codepoints_pressed_length;
	u8 keys[Key::MAX];
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


// Only allocate the box
Box* _allocate_box(ID id);
// Allocate box and add it to the parent box
Box* make_box(ID id, Size2 size, u32 flags);
Box* make_box(const char* name_id, Size2 size, u32 flags);
// Used for layouts (any box with children)
void push_box(Box* box);
Box* push_box(ID id, Size2 size, u32 flags);
Box* push_box(const char* name_id, Size2 size, u32 flags);
Box* pop_box();
Box* get_box();

const Size2 DEFAULT_LAYOUT_SIZE = {{SizeType_Fit, 1.f}, {SizeType_Fit, 1.f}};

Box* layout_horizontal(i8 h_align, i8 v_align, Size2 size = DEFAULT_LAYOUT_SIZE, u32 flags = 0);
Box* layout_vertical(i8 h_align, i8 v_align, Size2 size = DEFAULT_LAYOUT_SIZE, u32 flags = 0);
// Does the same thing as pop_box
void layout_end();
#define LGUI_H_LAYOUT(...) LGUI_DEFER_LOOP(lgui::layout_horizontal(__VA_ARGS__), lgui::layout_end())
#define LGUI_V_LAYOUT(...) LGUI_DEFER_LOOP(lgui::layout_vertical(__VA_ARGS__), lgui::layout_end())

// Generate an ID based on position in the layout
ID box_generate_id();


Panel* get_panel(ID id);
Panel* get_current_panel();
bool begin_panel(const char* name, Rect rect, PanelFlag flags);
void end_panel();
void move_panel_to_front(Panel* panel);
Painter& get_painter();

bool begin_window(const char* name, Rect rect, PanelFlag flags);
bool begin_window(const char* name, v2 size, PanelFlag flags = 0);
void end_window();


// Gets the retained data, or create it if it doesn't exist
RetainedData* get_retained_data(ID id);


InputResult handle_element_input(Rect rect, ID id, bool enable_drag = false, bool ignore_clip = false);
bool is_mouse_overlapping(Rect rect);
bool mouse_pressed(int button = 0);
bool mouse_released(int button = 0);
bool mouse_down(int button = 0);
v2 mouse_pos();
v2 mouse_scroll();

void input_register_char_press(Codepoint codepoint);
void input_register_key_press(u32 key);
void input_register_key_release(u32 key);
// Alternative method to register key input
void input_register_key_down(u32 key, bool down);

bool key_pressed(u32 key);
bool key_released(u32 key);
bool key_down(u32 key);
Slice<Codepoint> key_codepoints_pressed();

void select_element(ID id);



// Builder code

InputResult button(const char* name);
InputResult checkbox(const char* name, bool* value);
InputResult radio_button(const char* name, int option, int* selected);
InputResult drag_value(const char* name, f32* value);
bool collapse_header(const char* name);
void text(const char* text, bool static_string = false);
void textf(const char* format, ...);
//bool input_text(char* buffer, usize buffer_size, bool wrap = false);
void separator();
// Emtpy space on the axis of the layout
void spacer(f32 size);
// Inserts a 0 width or height element but with given size
void min_size(f32 size);
Box* draw_hook(Size2 size, void* ud, DrawHook hook);
Box* draw_hook(Size2 size, DrawHook hook);

bool begin_fancy_collapse_header(const char* name);
void end_fancy_collapse_header();

bool begin_tab_bar(const char* name);
void end_tab_bar();
bool do_tab(const char* name);

bool begin_tree_node(const char* name);
void end_tree_node();


void draw_open_triangle(Painter* painter, v2 pos, f32 size, f32 rotation, Color color);



// Debug

void debug_menu();
// Draw rectangle and shows text when hovered
void debug_rect(Rect rect, const char* text, Color color);


}
