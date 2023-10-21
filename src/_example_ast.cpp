#include <functional>
#include <iostream>
#include "basic.hpp"
#include "lag_gui.hpp"
#include "main.h"
#include "raylib.h"
#include "rlgl.h"
#include <stdio.h>
#include <vector>
#include <initializer_list>

#define GREY(f) {f, f, f, 1.f}
#define COLOR_T(r, g, b, t) {(r) * (t), (g) * (t), (b) * (t), 1.f}

#if _MSC_VER
#define FORCE_INLINE __forceinline
#else
#define FORCE_INLINE
#endif

using v2 = lgui::v2;
using Rect = lgui::Rect;
using f32 = lgui::f32;
using Rect = lgui::Rect;

extern lgui::Font* g_font;



const size_t AST_STRING_SIZE = 32;
struct AstString {
	size_t length;
	char buffer[AST_STRING_SIZE];

	void insert(size_t pos, char c)
	{
		LGUI_ASSERT(pos <= length, "Invalid insertion index");
		size_t copy_size = length - pos;
		if (copy_size > 0)
		{
			size_t size = LGUI_MIN(AST_STRING_SIZE - pos - 1, copy_size);
			memmove(&buffer[pos + 1], &buffer[pos], size);
		}
		buffer[pos] = c;
		++length;
	}

	void erase(size_t pos)
	{
		LGUI_ASSERT(pos <= length, "Invalid insertion index");
		size_t copy_size = length - pos;
		if (copy_size > 0)
		{
			size_t size = LGUI_MIN(AST_STRING_SIZE - pos - 1, copy_size);
			memmove(&buffer[pos], &buffer[pos + 1], size);
		}
		--length;
	}

	bool operator==(const char* other) const
	{
		size_t len = strlen(other);
		if (len != length) return false;
		if (len == 0) return true;
		return memcmp(buffer, other, len) == 0;
	}
};

struct AstNode;
void unlink_node(AstNode* node);

enum AstNodeType {
	AstNodeType_None = 0,
	AstNodeType_Empty, // Empty line or list entry
	AstNodeType_EmptyStatement,
	AstNodeType_Identifier,
	AstNodeType_Number,
	AstNodeType_ExprEmtpy,
	AstNodeType_ExprToken, // + - = ( { [ ) } ]
	AstNodeType_TypeExpr,
	AstNodeType_TypeExprEmpty,
	AstNodeType_FunctionDecl,
	AstNodeType_ParamList,
	AstNodeType_StructDecl,
	AstNodeType_NameAndType, // name : type
	AstNodeType_Expr,
	AstNodeType_Block, // Block of statements
	AstNodeType_TopLevel, // For declarations
	AstNodeType_,
	AstNodeType_VarDecl,
	AstNodeType_IfStatement,
	AstNodeType_ElseStatement,

	AstNodeType_MAX,
};

struct AstBoxElement {
	bool is_child_node;
	int index;
};

const int AST_NODE_LAYOUT_MAX = 8;
struct AstNodeBox {
	bool initialized;
	AstBoxElement layout[AST_NODE_LAYOUT_MAX];
	int layout_count;
};

AstNodeBox create_layout(std::initializer_list<AstBoxElement> list)
{
	LGUI_ASSERT(list.size() <= AST_NODE_LAYOUT_MAX, "Too many layout elements, increase array size");

	AstNodeBox ret{};
	ret.initialized = true;
	for (auto it : list)
	{
		ret.layout[ret.layout_count] = it;
		++ret.layout_count;
	}
	return ret;
}

struct AstStringRef {
	size_t length;
	char* buffer;
};

struct AstNode {
	AstNode* next;
	AstNode* prev;
	AstNode* first_child;
	AstNode* last_child;
	AstNode* parent;

	AstNodeType type;
	// Type of element inserted when user creates a new list entry
	// Only applicable for lists
	AstNodeType list_element_type;
	bool list;
	bool leaf; // Leaf nodes have editable text
	bool vertical;

	// TODO: Add some field to determine if the link is a local
	AstNode* link; // To take the text and type info from
	// For child links
	AstNode* first_linked;
	AstNode* last_linked;
	AstNode* next_linked;
	AstNode* prev_linked;
	AstString text;

	const AstString& get_text()
	{
		if (link) return link->text;
		return text;
	}

	// Only call this if you are going to edit the text
	void pre_edit_text()
	{
		if (link)
		{
			text = link->text;
			//memcpy(text.buffer, link->text.buffer, AST_STRING_SIZE);
			unlink_node(this);
		}
	}

	bool list_empty() const
	{
		return list && first_child == nullptr;
	}
};

struct AstPoint {
	AstNode* node;
	int offset; // Text offset
};

struct Cursor {
	// Possible cursor positions:
	// - Any CursorPosition node, which will be
	//   1. In the text of an identifier or literal (number, string)
	//   2. In an _emtpy_ list
	//   3. On specific spots in a node
	AstPoint pos;

	bool selecting;
	AstPoint selection_start_pos;
	// Selection between selection_start_point and pos where both points have the same parent
	AstPoint selection_min;
	AstPoint selection_max;
};

struct Suggestion {
	int command;
	AstString text;
};

struct Editor {
	lgui::Arena arena;
	lgui::Arena temp_arena_arr[2];
	lgui::Arena* temp_arena;
	int arena_swap;

	Cursor cursor;

	AstNode* root;
	AstNode* first_free_node;

	f32 tab_size;

	int line;
	v2 draw_cursor_start;
	v2 draw_cursor;
	int tab_count;
	Rect window_rect;
	v2 char_size;

	AstNodeBox layouts[AstNodeType_MAX];
};

AstNode* allocate_node(Editor* editor)
{
	if (editor->first_free_node)
	{
		AstNode* ret = editor->first_free_node;
		editor->first_free_node = ret->next;
		memset(ret, 0, sizeof(AstNode));
		return ret;
	}

	return editor->arena.allocate_one<AstNode>();
}

void free_node(Editor* editor, AstNode* node)
{
	node->next = editor->first_free_node;
	editor->first_free_node = node;
}

void remove_node(AstNode* node)
{
	LGUI_LL_REMOVE(node, prev, next, node->parent->first_child, node->parent->last_child);
	node->parent = nullptr;
	node->next = nullptr;
	node->prev = nullptr;
}

void add_child(AstNode* parent, AstNode* child)
{
	LGUI_LL_APPEND_END(child, prev, next, parent->first_child, parent->last_child);
	child->parent = parent;
}

void link_node(AstNode* node, AstNode* link)
{
	LGUI_ASSERT(!node->link, "Node is already linked");

	LGUI_LL_APPEND_END(node, prev_linked, next_linked, link->first_linked, link->last_linked);
	node->link = link;
}

void unlink_node(AstNode* node)
{
	LGUI_ASSERT(node->link, "Node is not linked");

	LGUI_LL_REMOVE(node, prev_linked, next_linked, node->link->first_linked, node->link->last_linked);
	node->prev_linked = nullptr;
	node->next_linked = nullptr;
	node->link = nullptr;
}

void add_node_after(AstNode* node, AstNode* new_node)
{
	new_node->prev = node;
	new_node->next = node->next;
	new_node->parent = node->parent;
	if (node->next)
	{
		node->next->prev = new_node;
	}
	node->next = new_node;
	if (node->parent->last_child == node)
	{
		node->parent->last_child = new_node;
	}
}

void add_node_before(AstNode* node, AstNode* new_node)
{
	new_node->next = node;
	new_node->prev = node->prev;
	new_node->parent = node->parent;
	if (node->prev)
	{
		node->prev->next = new_node;
	}
	node->prev = new_node;
	if (node->parent->first_child == node)
	{
		node->parent->first_child = new_node;
	}
}

Editor* make_editor()
{
	const int mbs = 10;
	lgui::Arena arena = lgui::Arena::from_memory(malloc(LGUI_MB(mbs)), LGUI_MB(mbs));
	Editor* ret = arena.allocate_one<Editor>();
	ret->arena = arena;
	ret->temp_arena_arr[0] = lgui::Arena::from_memory(malloc(LGUI_MB(mbs)), LGUI_MB(mbs));
	ret->temp_arena_arr[1] = lgui::Arena::from_memory(malloc(LGUI_MB(mbs)), LGUI_MB(mbs));
	ret->temp_arena = &ret->temp_arena_arr[0];

	ret->root = ret->arena.allocate_one<AstNode>();
	ret->root->type = AstNodeType_TopLevel;
	ret->root->list = true;
	ret->root->vertical = true;

	ret->tab_size = 40;

	return ret;
}

void add_layout(Editor* editor, AstNodeType node, std::initializer_list<AstBoxElement> list)
{
	editor->layouts[node] = create_layout(list);
}

FORCE_INLINE
f32 ast_text_width(Editor* editor, size_t characters)
{
	return (f32)characters * editor->char_size.x;
}

FORCE_INLINE
void draw_text_simple(Editor* editor, lgui::Context* context, const char* text, size_t length, lgui::Color color = {1, 1, 1, 1})
{
	f32 text_width = ast_text_width(editor, length);

	if (editor->window_rect.overlap(editor->draw_cursor))
	{
		v2 pos = editor->draw_cursor;
		lgui::Painter& painter = lgui::get_current_panel()->get_painter();
		painter.draw_text(g_font, text, length, pos, 0, color);
	}

	editor->draw_cursor.x += text_width;
}

FORCE_INLINE
Rect draw_text(Editor* editor, lgui::Context* context, const char* text, lgui::Color color = {1, 1, 1, 1})
{
	v2 pos = editor->draw_cursor;
	f32 text_width = ast_text_width(editor, strlen(text));
	Rect ret = Rect::from_pos_size(pos, {text_width, editor->char_size.y});
	/*Rect ret = {
		pos,
		{pos.x + text_width, pos.y + editor->char_size.y}
	};*/

	if (ret.overlap(editor->window_rect))
	{
		lgui::Painter& painter = lgui::get_current_panel()->get_painter();
		painter.draw_text(g_font, text, pos, 0, color);
	}

	editor->draw_cursor.x += text_width;
	return ret;
}

FORCE_INLINE
Rect draw_text(Editor* editor, lgui::Context* context, const AstString& text, lgui::Color color = {1, 1, 1, 1})
{
	v2 pos = editor->draw_cursor;
	f32 text_width = ast_text_width(editor, text.length);
	Rect ret = Rect::from_pos_size(pos, {text_width, editor->char_size.y});
	/*Rect ret = {
		pos,
		{pos.x + text_width, pos.y + editor->char_size.y}
	};*/

	if (ret.overlap(editor->window_rect))
	{
		lgui::Painter& painter = lgui::get_current_panel()->get_painter();
		painter.draw_text(g_font, text.buffer, text.length, pos, 0, color);
	}

	editor->draw_cursor.x += text_width;
	return ret;
}

void new_line(Editor* editor)
{
	++editor->line;
	editor->draw_cursor.y += g_font->height + 1;
	editor->draw_cursor.x = editor->draw_cursor_start.x + ((float)editor->tab_count) * editor->tab_size;
}

void reset_line(Editor* editor)
{
	editor->draw_cursor.x = editor->draw_cursor_start.x + ((float)editor->tab_count) * editor->tab_size;
}

AstNode* make_node_prefab(Editor* editor, AstNodeType type)
{
	AstNode* ret = allocate_node(editor);
	ret->type = type;

	switch (type)
	{
	case AstNodeType_Block:
	{
		ret->list = true;
		ret->list_element_type = AstNodeType_EmptyStatement;
		ret->vertical = true;
		ret->text.buffer[0] = '{';
		ret->text.buffer[1] = '}';
		ret->text.buffer[2] = ';';
	} break;
	case AstNodeType_ParamList:
	{
		ret->list = true;
		ret->list_element_type = AstNodeType_NameAndType;
		ret->text.buffer[0] = '(';
		ret->text.buffer[1] = ')';
		ret->text.buffer[2] = ',';
	} break;
	case AstNodeType_FunctionDecl:
	{
		add_child(ret, make_node_prefab(editor, AstNodeType_Identifier));
		add_child(ret, make_node_prefab(editor, AstNodeType_ParamList));
		add_child(ret, make_node_prefab(editor, AstNodeType_TypeExpr));
		add_child(ret, make_node_prefab(editor, AstNodeType_Block));
	} break;
	case AstNodeType_VarDecl:
	{
		add_child(ret, make_node_prefab(editor, AstNodeType_Identifier));
		add_child(ret, make_node_prefab(editor, AstNodeType_TypeExpr));
		add_child(ret, make_node_prefab(editor, AstNodeType_Expr));
	} break;
	case AstNodeType_IfStatement:
	{
		add_child(ret, make_node_prefab(editor, AstNodeType_Expr));
		add_child(ret, make_node_prefab(editor, AstNodeType_Block));
	} break;
	case AstNodeType_ElseStatement:
	{
		// After the "else" keyword
		add_child(ret, make_node_prefab(editor, AstNodeType_Block));
	} break;
	case AstNodeType_Expr:
	{
		ret->list = true;
		ret->list_element_type = AstNodeType_ExprEmtpy;
		// Temp
		ret->text.buffer[0] = '(';
		ret->text.buffer[1] = ')';
		ret->text.buffer[2] = ' ';
	} break;
	case AstNodeType_TypeExpr:
	{
		ret->list = true;
		ret->list_element_type = AstNodeType_TypeExprEmpty;
		// Temp
		ret->text.buffer[0] = '<';
		ret->text.buffer[1] = '>';
		ret->text.buffer[2] = ' ';
	} break;
	case AstNodeType_NameAndType:
	{
		add_child(ret, make_node_prefab(editor, AstNodeType_Identifier));
		add_child(ret, make_node_prefab(editor, AstNodeType_TypeExpr));
	} break;

	case AstNodeType_EmptyStatement:
	case AstNodeType_ExprEmtpy:
	case AstNodeType_TypeExprEmpty:
	case AstNodeType_ExprToken:
	case AstNodeType_Identifier:
	case AstNodeType_Number:
	{
		ret->leaf = true;
	} break;
	}

	return ret;
}

// TODO: Can this be removed now that first_point exists?
// Find the first point that the cursor can land on in a given node
// To be used when a new node is inserted
bool node_first_point(AstNode* node, AstPoint* point)
{
	if (node->list)
	{
		if (node->list_empty())
		{
			point->node = node;
			point->offset = 0;
			return true;
		}

		for (AstNode* it = node->first_child; it; it = it->next)
		{
			if (node_first_point(it, point))
			{
				return true;
			}
		}
	}
	else if (node->leaf)
	{
		point->node = node;
		point->offset = 0;
		return true;
	}
	else // Specific cases
	{
		switch (node->type)
		{
		case AstNodeType_NameAndType:
		{
			for (AstNode* it = node->first_child; it; it = it->next)
			{
				if (node_first_point(it, point))
				{
					return true;
				}
			}
		} break;

		// This is almost always true
		default:
		{
			point->node = node;
			point->offset = 0;
			return true;
		} break;
		}
	}

	return false;
}

// Get the index of this child node in its parent
// Returns -1 on error
int get_child_index(AstNode* node)
{
	LGUI_ASSERT(node->parent, "Node has no parent");
	int i = 0;
	for (AstNode* it = node->parent->first_child; it; it = it->next)
	{
		if (it == node)
		{
			return i;
		}
		++i;
	}
	return -1;
}

// Can return nullptr
AstNode* get_child_from_index(AstNode* node, int index)
{
	AstNode* child = node->first_child;
	for (int i = 0; i < index; ++i)
	{
		if (!child->next)
		{
			return nullptr;
		}
		child = child->next;
	}
	return child;
}

AstPoint last_point(Editor* editor, AstNode* node)
{
	if (node->leaf)
	{
		return {node, (int)node->get_text().length};
	}
	else if (node->list)
	{
		if (node->list_empty())
		{
			return {node, 0};
		}
		else
		{
			// Assume normal order
			return last_point(editor, node->last_child);
		}
	}
	else
	{
		// Specific types that have a layout
		const AstNodeBox& layout = editor->layouts[node->type];
		LGUI_ASSERT(layout.initialized && layout.layout_count > 0, "Need layout but it is not initialized");
		const auto& l = layout.layout[layout.layout_count - 1];
		if (l.is_child_node)
		{
			AstNode* child = get_child_from_index(node, l.index);
			LGUI_ASSERT(child, "Required child from layout node but it doesn't exist");
			return last_point(editor, child);
		}
		else
		{
			return {node, l.index};
		}
	}
}

AstPoint first_point(Editor* editor, AstNode* node)
{
	if (node->leaf)
	{
		return {node, 0};
	}
	else if (node->list)
	{
		if (node->list_empty())
		{
			return {node, 0};
		}
		else
		{
			// Assume normal order
			// TODO: Go back if there is no selectable point in the first child, try the next child
			return first_point(editor, node->first_child);
		}
	}
	else
	{
		// Specific types that have a layout
		const AstNodeBox& layout = editor->layouts[node->type];
		LGUI_ASSERT(layout.initialized && layout.layout_count > 0, "Need layout but it is not initialized");
		const auto& l = layout.layout[0];
		if (l.is_child_node)
		{
			AstNode* child = get_child_from_index(node, l.index);
			LGUI_ASSERT(child, "Required child from layout node but it doesn't exist");
			return first_point(editor, child);
		}
		else
		{
			return {node, l.index};
		}
	}
}

AstPoint _next_point(Editor* editor, AstPoint point, AstNode* node)
{
	if (node->leaf)
	{
		if (point.offset < point.node->get_text().length)
		{
			return AstPoint{point.node, point.offset + 1};
		}
		else
		{
			if (!node->parent) return {nullptr, 0};
			return _next_point(editor, point, point.node->parent);
		}
	}
	else if (node->list)
	{
		if (node == point.node)
		{
			if (!node->parent) return {nullptr, 0};
			return _next_point(editor, point, point.node->parent);
		}

		// Assume normal order, other orders can be implemented later
		if (point.node->next)
		{
			//return AstPoint{point.node->next, 0};
			AstPoint ret = first_point(editor, point.node->next);
			if (ret.node)
			{
				return ret;
			}
		}

		if (!node->parent) return {nullptr, 0};
		return _next_point(editor, {node, 0}, node->parent);
	}
	else
	{
		// Specific types that have a layout
		const AstNodeBox& layout = editor->layouts[node->type];
		LGUI_ASSERT(layout.initialized, "Need layout but it is not initialized");

		bool do_check_offset = (node == point.node);

		// Check child node layouts
		int point_index = -1;
		if (node != point.node)
		{
			point_index = get_child_index(point.node);
			LGUI_ASSERT(point_index != -1, "Failed to find child index");
		}

		int i = 0;
		for (; i < layout.layout_count; ++i)
		{
			auto& it = layout.layout[i];
			if ((!it.is_child_node && do_check_offset && it.index == point.offset) ||
				(it.is_child_node && it.index == point_index))
			{
				break;
			}
		}

		if (i + 1 >= layout.layout_count)
		{
			if (!node->parent) return {nullptr, 0};
			return _next_point(editor, {node, 0}, node->parent);
		}
		else
		{
			auto& l = layout.layout[i + 1];
			if (l.is_child_node)
			{
				AstNode* child = get_child_from_index(node, l.index);
				return first_point(editor, child);
			}
			else
			{
				return {node, l.index};
			}
		}
	}

	return {nullptr, 0};
}

AstPoint next_point(Editor* editor, AstPoint point)
{
	if (point.node == nullptr || !point.node->parent)
	{
		return point;
	}
	AstPoint ret = _next_point(editor, point, point.node);
	if (ret.node)
	{
		return ret;
	}
	return point;
}

AstPoint _prev_point(Editor* editor, AstPoint point, AstNode* node)
{
	if (node->leaf)
	{
		if (point.offset > 0)
		{
			return AstPoint{point.node, point.offset - 1};
		}
		else
		{
			if (!node->parent) return {nullptr, 0};
			return _prev_point(editor, point, point.node->parent);
		}
	}
	else if (node->list)
	{
		if (node == point.node)
		{
			if (!node->parent) return {nullptr, 0};
			return _prev_point(editor, point, point.node->parent);
		}

		// Assume normal order, other orders can be implemented later
		if (point.node->prev)
		{
			AstPoint ret = last_point(editor, point.node->prev);
			if (ret.node)
			{
				return ret;
			}
		}

		if (!node->parent) return {nullptr, 0};
		return _prev_point(editor, {node, 0}, node->parent);
	}
	else
	{
		// Specific types that have a layout
		const AstNodeBox& layout = editor->layouts[node->type];
		LGUI_ASSERT(layout.initialized && layout.layout_count > 0, "Need layout but it is not initialized");

		bool do_check_offset = (node == point.node);

		// Check child node layouts
		int point_index = -1;
		if (node != point.node)
		{
			point_index = get_child_index(point.node);
			LGUI_ASSERT(point_index != -1, "Failed to find child index");
		}

		int i = layout.layout_count - 1;
		for (; i >= 0; --i)
		{
			auto& it = layout.layout[i];
			if ((!it.is_child_node && do_check_offset && it.index == point.offset) ||
				(it.is_child_node && it.index == point_index))
			{
				break;
			}
		}

		if (i - 1 < 0)
		{
			if (!node->parent) return {nullptr, 0};
			return _prev_point(editor, {node, 0}, node->parent);
		}
		else
		{
			auto& l = layout.layout[i - 1];
			if (l.is_child_node)
			{
				AstNode* child = get_child_from_index(node, l.index);
				return last_point(editor, child);
			}
			else
			{
				return {node, l.index};
			}
		}
	}

	return {nullptr, 0};
}

AstPoint prev_point(Editor* editor, AstPoint point)
{
	if (point.node == nullptr || !point.node->parent)
	{
		return point;
	}
	AstPoint ret = _prev_point(editor, point, point.node);
	if (ret.node)
	{
		return ret;
	}
	return point;
}

const lgui::Color color_keyword = {1.f, 0.2f, 0.2f, 1.f};

void draw_cursor(Editor* editor, lgui::Context* context, v2 pos, bool horizontal = false)
{
	lgui::Color color = lgui::Color{0.3f, 1.0f, 0.3f, 0.8f};
	lgui::Painter& painter = lgui::get_current_panel()->get_painter();
	f32 thick = 2;
	f32 horizontal_size = 25;

	if (horizontal)
	{
		painter.draw_rectangle(Rect::from_pos_size(pos - v2{0, thick / 2.f}, {horizontal_size, thick}), color);
	}
	else // Vertical
	{
		painter.draw_rectangle(Rect::from_pos_size(pos - v2{thick / 2.f, 0}, {thick, editor->char_size.y}), color);
	}
}

// For nodes that can have their text editor (identifier, number, string)
void update_node_text_edit(Editor* editor, lgui::Context* context, AstNode* node)
{
	if (context && editor->window_rect.overlap(editor->draw_cursor))
	{
		Rect rect = draw_text(editor, context, node->get_text());
		lgui::InputResult input = lgui::handle_element_input(rect, lgui::get_id(node), false);
		if (input.clicked)
		{
			// TODO: Select specific character index
			editor->cursor.pos = {node, 0};
		}

		if (editor->cursor.pos.node == node)
		{
			draw_cursor(editor, context, rect.top_left + v2{(f32)editor->cursor.pos.offset * editor->char_size.x, 0});
		}
	}
}

// Call this while updating a node to register a possible cursor position (replacement of AstNodeType_CursorPosition node)
void update_cursor_position(Editor* editor, lgui::Context* context, AstNode* node, int index)
{
	if (context && editor->window_rect.overlap(editor->draw_cursor))
	{
		if (editor->cursor.pos.node == node && editor->cursor.pos.offset == index)
		{
			draw_cursor(editor, context, editor->draw_cursor, node->list && node->vertical);
		}

		v2 size = editor->char_size;
		Rect c = Rect::from_pos_size(editor->draw_cursor - v2{size.x / 2.f, 0.f}, size);
		if (lgui::handle_element_input(c, lgui::get_id(node)).clicked)
		{
			editor->cursor.pos = {node, 0};
		}
	}
}

void update_node(Editor* editor, lgui::Context* context, AstNode* node)
{
	if (node->list)
	{
		bool is_root = node->type == AstNodeType_TopLevel;
		f32 tab_size = editor->tab_size;

		if (!is_root)
		{
			if (node->text.buffer[0])
			{
				draw_text_simple(editor, context, &node->text.buffer[0], 1);
			}

			if (node->vertical)
			{
				++editor->tab_count;
				new_line(editor);
			}
		}

		if (node->list_empty())
		{
			update_cursor_position(editor, context, node, 0);
		}

		// Update child nodes
		for (AstNode* it = node->first_child; it; it = it->next)
		{
			update_node(editor, context, it);

			// Separation character
			if (node->text.buffer[2] && (node->vertical || it->next))
			{
				draw_text_simple(editor, context, &node->text.buffer[2], 1);
			}

			if (node->vertical)
			{
				new_line(editor);
			}
		}

		if (!is_root)
		{
			if (node->vertical)
			{
				--editor->tab_count;
				reset_line(editor);
			}

			if (node->text.buffer[1])
			{
				draw_text_simple(editor, context, &node->text.buffer[1], 1);
			}
		}
	}
	else
	{
		switch (node->type)
		{
		case AstNodeType_VarDecl:
		{
			AstNode* name = node->first_child;
			AstNode* type = name->next;
			AstNode* expr = type->next;

			update_cursor_position(editor, context, node, 0);

			Rect var_rect = draw_text(editor, context, "var ", color_keyword);

			update_node(editor, context, name);

			// TODO: Draw greyed out _type_ text when hovered
			//Rect type_rect;
			//Rect expr_rect;
			if (type->list_empty())
			{
				//type_rect = draw_text(editor, context, " :");
				draw_text_simple(editor, context, " :", 2);
				update_node(editor, context, type);
				//expr_rect = draw_text(editor, context, "= ");
				draw_text_simple(editor, context, "= ", 2);
			}
			else
			{
				//type_rect = draw_text(editor, context, " : ");
				draw_text_simple(editor, context, " : ", 3);
				update_node(editor, context, type);
				//expr_rect = draw_text(editor, context, " = ");
				draw_text_simple(editor, context, " = ", 3);
			}

			//lgui::InputResult input_type = lgui::handle_element_input(context, type_rect, lgui::get_id(context, 1));
			//lgui::InputResult input_expr = lgui::handle_element_input(context, expr_rect, lgui::get_id(context, 2));

			update_node(editor, context, expr);
		} break;
		case AstNodeType_IfStatement:
		{
			AstNode* expr = node->first_child;
			AstNode* block = expr->next;
			update_cursor_position(editor, context, node,  0);
			//Rect if_rect = draw_text(editor, context, "if ", color_keyword);
			draw_text_simple(editor, context, "if ", 3, color_keyword);
			update_node(editor, context, expr);
			draw_text_simple(editor, context, " ", 1);
			update_node(editor, context, block);
			update_cursor_position(editor, context, node,  1);
		} break;
		case AstNodeType_ElseStatement:
		{
			AstNode* block = node->first_child;
			update_cursor_position(editor, context, node,  0);
			Rect else_rect = draw_text(editor, context, "else ", color_keyword);
			update_cursor_position(editor, context, node,  1);
			update_node(editor, context, block);
			update_cursor_position(editor, context, node,  2);
		} break;
		case AstNodeType_NameAndType:
		{
			AstNode* name = node->first_child;
			AstNode* type = name->next;
			update_node(editor, context, name);
			Rect if_rect = draw_text(editor, context, " : ", color_keyword);
			update_node(editor, context, type);
		} break;
		case AstNodeType_FunctionDecl:
		{
			AstNode* name = node->first_child;
			AstNode* param_list = name->next;
			AstNode* return_type = param_list->next;
			AstNode* block = return_type->next;

			update_cursor_position(editor, context, node,  0);
			draw_text(editor, context, "func ", color_keyword);
			update_node(editor, context, name);
			update_node(editor, context, param_list);
			draw_text(editor, context, " -> ");
			update_node(editor, context, return_type);
			new_line(editor);
			update_node(editor, context, block);
			update_cursor_position(editor, context, node,  1);
		} break;
		case AstNodeType_StructDecl:
		{

		} break;

		case AstNodeType_Identifier:
		case AstNodeType_Number:
		case AstNodeType_EmptyStatement:
		case AstNodeType_ExprEmtpy:
		case AstNodeType_TypeExprEmpty:
		case AstNodeType_ExprToken:
		{
			update_node_text_edit(editor, context, node);
		} break;

		}
	}
}

bool is_valid_identifier(int codepoint, bool is_first_char)
{
	if (is_first_char)
	{
		return (codepoint >= 'a' && codepoint <= 'z') ||
			(codepoint >= 'A' && codepoint <= 'Z') ||
			(codepoint == '_');
	}
	else
	{
		return (codepoint >= 'a' && codepoint <= 'z') ||
			(codepoint >= 'A' && codepoint <= 'Z') ||
			(codepoint >= '0' && codepoint <= '9') ||
			(codepoint == '_');
	}
}

bool is_valid_number(int codepoint)
{
	return (codepoint >= '0' && codepoint <= '9');
}

// Returns the closest parent that is in a list
// Can return the passed node
// Can return null
AstNode* node_find_first_parent_in_list(AstNode* node)
{
	for (AstNode* it = node; it; it = it->parent)
	{
		if (it->parent && it->parent->list)
		{
			return it;
		}
	}
	return nullptr;
}

// Check if all child lists and leafs are empty
bool is_node_emtpy(AstNode* node)
{
	if (node->list_empty() || (node->leaf && node->get_text().length == 0))
	{
		return true;
	}
	else if (node->list && !node->list_empty())
	{
		return false;
	}

	if (!node->link && !node->leaf)
	{
		for (AstNode* it = node->first_child; it; it = it->next)
		{
			if (!is_node_emtpy(it))
			{
				return false;
			}
		}
		return true;
	}

	return false;
}

// Also true when node is parent
bool node_has_parent(AstNode* node, AstNode* parent)
{
	for (AstNode* it = node; it; it = it->parent)
	{
		if (it == parent)
		{
			return true;
		}
	}
	return false;
}

// Returns wether it coud perform the complete operation
// Reasons it fails: needs to move to previous node, but there is no CursorPosition node
bool input_backspace(Editor* editor)
{
	AstPoint& pos = editor->cursor.pos;

	auto delete_move_left = [&]() {
		// Check if node is emtpy and then delete it
		AstNode* to_delete = node_find_first_parent_in_list(pos.node);
		bool did_delete = false;
		AstNode* to_delete_parent = nullptr;
		if (to_delete)
		{
			if (is_node_emtpy(to_delete))
			{
				to_delete_parent = to_delete->parent;

				// Delete node
				remove_node(to_delete);
				//unlink_node(to_delete);

				did_delete = true;
			}
		}

		if (to_delete_parent && to_delete_parent->list_empty())
		{
			// Return cursor to empty list
			pos.node = to_delete_parent;
			pos.offset = 0;
		}
		else
		{
			pos = prev_point(editor, pos);
		}

		return true;
	};

	// TODO: Better node deletion behavior: select the current node. If backspace is pressed with a selection, delete the selection

	if (pos.node)
	{
		if (pos.node->leaf)
		{
			if (pos.offset == 0)
			{
				if (!delete_move_left()) return false;
			}
			else
			{
				pos.node->pre_edit_text();
				auto& text = pos.node->text;
				text.erase(pos.offset);
				--pos.offset;

				if (pos.offset == 0)
				{
					//if (!delete_move_left()) return false;
				}
			}
		}
		else
		{
			if (!delete_move_left()) return false;
		}
	}

	return true;
}

void process_input_character(Editor* editor, int codepoint)
{
	AstPoint& pos = editor->cursor.pos;
	if (!pos.node)
	{
		return;
	}

	if (pos.node->leaf)
	{
		if (is_valid_identifier(codepoint, false))
		{
			pos.node->pre_edit_text();
			pos.node->text.insert(pos.offset, (char)codepoint);
			++pos.offset;
		}
	}
	else if (pos.node->list)
	{
		LGUI_ASSERT(pos.node->list_empty(), "Selected list must be empty");
		LGUI_ASSERT(pos.node->list_element_type != 0, "List must not be empty");

		if (is_valid_identifier(codepoint, false))
		{
			AstNode* new_node = make_node_prefab(editor, pos.node->list_element_type);
			add_child(pos.node, new_node);

			AstPoint first_point_ = first_point(editor, new_node);
			if (first_point_.node)
			{
				pos = first_point_;
				if (new_node->leaf)
				{
					pos.node->pre_edit_text();
					pos.node->text.insert(pos.offset, (char)codepoint);
					++pos.offset;
				}
			}
		}
	}
}

AstNode* find_first_parent_in_vertical_list(AstNode* node)
{
	for (AstNode* it = node; it; it = it->parent)
	{
		if (it->parent && it->parent->list && it->parent->vertical)
		{
			return it;
		}
	}
	return nullptr;
}

// Can return nullptr
AstNode* find_next_enter_line(Editor* editor, AstNode* node)
{
	if ((node->list_empty() && node->vertical) ||
		(node->parent && node->parent->list && node->parent->vertical))
	{
		return node;
	}

	// This is a bit lazy, replace it with the layout solution

	if (node->first_child)
	{
		AstNode* ret = find_next_enter_line(editor, node->first_child);
		if (ret)
		{
			return ret;
		}
	}

	for (AstNode* it = node->next; it; it = it->next)
	{
		AstNode* ret = find_next_enter_line(editor, it);
		if (ret)
		{
			return ret;
		}
	}

	if (node->parent && node->parent->next)
	{
		return find_next_enter_line(editor, node->parent->next);
	}

	return nullptr;
}

void update_editor(Editor* editor, lgui::Context* context)
{
	if (lgui::begin_panel("Ast editor", Rect::from_pos_size({ 0, 300 }, { 400, 400 }), 0))
	{
		// Input
		{
			AstPoint& pos = editor->cursor.pos;

			if (IsKeyPressed(KEY_LEFT))
			{
				pos = prev_point(editor, pos);
			}

			if (IsKeyPressed(KEY_RIGHT) || IsKeyPressed(KEY_TAB))
			{
				pos = next_point(editor, pos);
			}

			if (IsKeyPressed(KEY_UP))
			{
			}

			if (IsKeyPressed(KEY_BACKSPACE))
			{
				input_backspace(editor);
			}

			if (IsKeyPressed(KEY_SPACE))
			{
				if (pos.node && pos.node->leaf)
				{
					if (pos.node->type == AstNodeType_EmptyStatement)
					{
						AstNode* empty_statement = pos.node;
						const AstString& text = pos.node->get_text();

						if (text == "if")
						{
							AstNode* new_node = make_node_prefab(editor, AstNodeType_IfStatement);
							add_node_after(empty_statement, new_node);
							remove_node(empty_statement);
							pos = first_point(editor, new_node);
							pos = next_point(editor, pos);
						}
						else if (text == "else")
						{
							AstNode* new_node = make_node_prefab(editor, AstNodeType_ElseStatement);
							add_node_after(empty_statement, new_node);
							remove_node(empty_statement);
							pos = first_point(editor, new_node);
							pos = next_point(editor, pos);
						}
						else if (text == "var")
						{
							AstNode* new_node = make_node_prefab(editor, AstNodeType_VarDecl);
							add_node_after(empty_statement, new_node);
							remove_node(empty_statement);
							pos = first_point(editor, new_node);
							pos = next_point(editor, pos);
						}
						else if (text == "func")
						{
							AstNode* new_node = make_node_prefab(editor, AstNodeType_FunctionDecl);
							add_node_after(empty_statement, new_node);
							remove_node(empty_statement);
							pos = first_point(editor, new_node);
							pos = next_point(editor, pos);
						}
						else if (text == "")
						{
							AstNode* new_node = make_node_prefab(editor, AstNodeType_Expr);
							add_node_after(empty_statement, new_node);
							remove_node(empty_statement);
							pos = first_point(editor, new_node);
						}

					}
				}
			}

			if (IsKeyPressed(KEY_ENTER))
			{
				if (pos.node)
				{
					AstNode* line = find_next_enter_line(editor, pos.node);
					if (line)
					{
						if (line->list_empty() && line->vertical)
						{
							LGUI_ASSERT(line->list_element_type != AstNodeType_None, "List must have an element type");
							AstNode* new_node = make_node_prefab(editor, line->list_element_type);
							add_child(line, new_node);
							pos = first_point(editor, new_node);
						}
						else
						{
							LGUI_ASSERT(line->parent->list_element_type != AstNodeType_None, "List must have an element type");
							AstNode* new_node = make_node_prefab(editor, line->parent->list_element_type);
							add_node_before(line, new_node);
							pos = first_point(editor, new_node);
						}
					}
				}
			}
		}

		while (int codepoint = GetCharPressed())
		{
			process_input_character(editor, codepoint);
		}

		editor->temp_arena = &editor->temp_arena_arr[editor->arena_swap];
		editor->arena_swap = (editor->arena_swap + 1) % 2;
		editor->temp_arena->reset();

		lgui::Panel* panel = lgui::get_current_panel();
		editor->line = 0;
		editor->draw_cursor_start = panel->draw_pos;
		editor->draw_cursor = editor->draw_cursor_start;
		editor->window_rect = panel->content;
		editor->char_size = {
			g_font->get_glyph('0').advance_x,
			g_font->height
		};
		update_node(editor, context, editor->root);

		lgui::end_panel();
	}
}

Editor* g_editor;

void ast_init()
{
	g_editor = make_editor();

	add_layout(g_editor, AstNodeType_IfStatement, {
		{false, 0},
		{true, 0},
		{true, 1},
		{false, 1}
	});
	add_layout(g_editor, AstNodeType_ElseStatement, {
		{false, 0},
		{false, 1},
		{true, 0},
		{false, 2}
	});
	add_layout(g_editor, AstNodeType_VarDecl, {
		{false, 0},
		{true, 0},
		{true, 1},
		{true, 2},
	});
	add_layout(g_editor, AstNodeType_NameAndType, {
		{true, 0},
		{true, 1},
	});
	add_layout(g_editor, AstNodeType_FunctionDecl, {
		{false, 0},
		{true, 0},
		{true, 1},
		{true, 2},
		{true, 3},
		{false, 1},
	});
	add_layout(g_editor, AstNodeType_StructDecl, {
		{false, 0},
		{true, 0},
		{true, 1},
		{false, 1},
	});

	AstNode* block = make_node_prefab(g_editor, AstNodeType_Block);

	for (int i = 0; i < 1; ++i)
	{
		//AstNode* var = make_node_prefab(g_editor, AstNodeType_VarDecl);
		//add_child(block, var);
		AstNode* if_s = make_node_prefab(g_editor, AstNodeType_IfStatement);
		add_child(block, if_s);

		const char* s = "hello";
		for (int j = 0; j < strlen(s); ++j)
		{
			AstNode* e = make_node_prefab(g_editor, AstNodeType_EmptyStatement);
			e->text.buffer[0] = s[j];
			e->text.length = 1;
			add_child(block, e);
		}
	}

	g_editor->root = block;
}

void ast_update(lgui::Context* context)
{
	update_editor(g_editor, context);
}

