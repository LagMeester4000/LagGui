#include <functional>
#include <iostream>
#include "basic.hpp"
#include "lag_gui.hpp"
#include "main.h"
#include "raylib.h"
#include "rlgl.h"
#include <stdio.h>
#include <vector>

#define GREY(f) {f, f, f, 1.f}
#define COLOR_T(r, g, b, t) {(r) * (t), (g) * (t), (b) * (t), 1.f}

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
};

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

// Possible cursor position
// Regenerated every time the document is rendered
struct CursorPosition {
	CursorPosition* next;
	CursorPosition* prev;
	AstPoint point;
	int line;
	f32 x_off;
};

struct Cursor {
	// Possible cursor positions:
	// - Any CursorPosition node, which will be
	//   1. In the text of an identifier or literal (number, string)
	//   2. In an _emtpy_ list
	//   3. On specific spots in a node
	CursorPosition* _pos;
	AstPoint pos;

	bool selecting;
	CursorPosition* _selection_start_pos;
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

	CursorPosition* first_cursor_position;
	CursorPosition* last_cursor_position;
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

CursorPosition* make_cursor_position(Editor* editor, AstNode* node, int index)
{
	CursorPosition* ret =  editor->temp_arena->allocate_one<CursorPosition>();
	ret->line = editor->line;
	ret->x_off = editor->draw_cursor.x - editor->draw_cursor_start.x;
	ret->point.node = node;
	ret->point.offset = index;

	LGUI_LL_APPEND_END(ret, prev, next, editor->first_cursor_position, editor->last_cursor_position);

	return ret;
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
	node->prev = new_node;
	if (node->parent->first_child == node)
	{
		node->parent->first_child = new_node;
	}
}

Editor* make_editor()
{
	lgui::Arena arena = lgui::Arena::from_memory(malloc(LGUI_MB(2)), LGUI_MB(2));
	Editor* ret = arena.allocate_one<Editor>();
	ret->arena = arena;
	ret->temp_arena_arr[0] = lgui::Arena::from_memory(malloc(LGUI_MB(2)), LGUI_MB(2));
	ret->temp_arena_arr[1] = lgui::Arena::from_memory(malloc(LGUI_MB(2)), LGUI_MB(2));
	ret->temp_arena = &ret->temp_arena_arr[0];

	ret->root = ret->arena.allocate_one<AstNode>();
	ret->root->type = AstNodeType_TopLevel;
	ret->root->list = true;
	ret->root->vertical = true;

	ret->tab_size = 40;

	return ret;
}

Rect draw_text(Editor* editor, lgui::Context* context, const char* text, lgui::Color color = {1, 1, 1, 1})
{
	v2 pos = editor->draw_cursor;
	f32 text_width = g_font->text_width(text, 0);
	lgui::Painter& painter = lgui::get_current_panel(context)->get_painter();
	painter.draw_text(context, g_font, text, pos, 0, color);
	editor->draw_cursor.x += text_width;
	return Rect::from_pos_size(pos, {text_width, g_font->height});
}

Rect draw_text(Editor* editor, lgui::Context* context, const AstString& text, lgui::Color color = {1, 1, 1, 1})
{
	v2 pos = editor->draw_cursor;
	f32 text_width = g_font->text_width(text.buffer, text.length, 0);
	lgui::Painter& painter = lgui::get_current_panel(context)->get_painter();
	painter.draw_text(context, g_font, text.buffer, text.length, pos, 0, color);
	editor->draw_cursor.x += text_width;
	return Rect::from_pos_size(pos, {text_width, g_font->height});
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
		ret->vertical = true;
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

const lgui::Color color_keyword = {1, 0.2, 0.2, 1};

f32 char_width(Editor* editor)
{
	return g_font->get_glyph('0').advance_x;
}

f32 char_height(Editor* editor)
{
	return g_font->height;
}

void draw_cursor(Editor* editor, lgui::Context* context, v2 pos, bool horizontal = false)
{
	lgui::Color color = lgui::Color{0.3, 1.0, 0.3, 0.8};
	lgui::Painter& painter = lgui::get_current_panel(context)->get_painter();
	f32 thick = 2;
	f32 horizontal_size = 25;

	if (horizontal)
	{
		painter.draw_rectangle(context, Rect::from_pos_size(pos - v2{0, thick / 2.f}, {horizontal_size, thick}), color);
	}
	else // Vertical
	{
		painter.draw_rectangle(context, Rect::from_pos_size(pos - v2{thick / 2.f, 0}, {thick, char_height(editor)}), color);
	}
}

// For nodes that can have their text editor (identifier, number, string)
void update_node_text_edit(Editor* editor, lgui::Context* context, AstNode* node)
{
	CursorPosition* pos = make_cursor_position(editor, node, 0);

	if (editor->cursor.pos.node == node)
	{
		editor->cursor._pos = pos;
		editor->cursor.pos.node = pos->point.node;
	}

	if (context)
	{
		Rect rect = draw_text(editor, context, node->get_text());
		lgui::InputResult input = lgui::handle_element_input(context, rect, lgui::get_id(context, 1), false);
		if (input.clicked)
		{
			// TODO: Select specific character index
			editor->cursor._pos = pos;
			editor->cursor.pos = pos->point;
		}

		if (editor->cursor.pos.node == node)
		{
			draw_cursor(editor, context, rect.top_left + v2{(f32)editor->cursor.pos.offset * char_width(editor), 0});
		}
	}
}

// Call this while updating a node to register a possible cursor position (replacement of AstNodeType_CursorPosition node)
void update_cursor_position(Editor* editor, lgui::Context* context, AstNode* node, int index)
{
	CursorPosition* pos = make_cursor_position(editor, node, index);

	// Assign new pos
	if (editor->cursor.pos.node == node && editor->cursor.pos.offset == index)
	{
		editor->cursor._pos = pos;
		editor->cursor.pos = pos->point;
	}
	if (editor->cursor.selecting && editor->cursor.selection_start_pos.node == node &&
		editor->cursor.selection_start_pos.offset == index)
	{
		editor->cursor._selection_start_pos = pos;
		editor->cursor.selection_start_pos = pos->point;
	}

	if (context)
	{
		if (editor->cursor.pos.node == node && editor->cursor.pos.offset == index)
		{
			draw_cursor(editor, context, editor->draw_cursor, node->list && node->vertical);
		}

		v2 size = {char_width(editor), char_height(editor)};
		Rect c = Rect::from_pos_size(editor->draw_cursor - v2{size.x / 2.f, 0.f}, size);
		if (lgui::handle_element_input(context, c, lgui::get_id(context, node)).clicked)
		{
			editor->cursor._pos = pos;
			editor->cursor.pos = pos->point;
		}
	}
}

void update_node(Editor* editor, lgui::Context* context, AstNode* node)
{
	if (context)
	{
		lgui::push_id(context, node);
	}

	if (node->list)
	{
		bool is_root = node->type == AstNodeType_TopLevel;
		f32 tab_size = editor->tab_size;

		if (!is_root)
		{
			if (node->text.buffer[0])
			{
				char t[2]{};
				t[0] = node->text.buffer[0];
				draw_text(editor, context, t);
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
				char t[2]{};
				t[0] = node->text.buffer[2];
				draw_text(editor, context, t);
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
				char t[2]{};
				t[0] = node->text.buffer[1];
				draw_text(editor, context, t);
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
			Rect type_rect;
			Rect expr_rect;
			if (type->list_empty())
			{
				type_rect = draw_text(editor, context, " :");
				update_node(editor, context, type);
				expr_rect = draw_text(editor, context, "= ");
			}
			else
			{
				type_rect = draw_text(editor, context, " : ");
				update_node(editor, context, type);
				expr_rect = draw_text(editor, context, " = ");
			}

			lgui::InputResult input_type = lgui::handle_element_input(context, type_rect, lgui::get_id(context, 1));
			lgui::InputResult input_expr = lgui::handle_element_input(context, expr_rect, lgui::get_id(context, 2));

			update_node(editor, context, expr);
		} break;
		case AstNodeType_IfStatement:
		{
			AstNode* expr = node->first_child;
			AstNode* block = expr->next;
			update_cursor_position(editor, context, node,  0);
			Rect if_rect = draw_text(editor, context, "if ", color_keyword);
			update_node(editor, context, expr);
			draw_text(editor, context, " ");
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
			new_line(editor);
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

	if (context)
	{
		lgui::pop_id(context);
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

	for (AstNode* it = node->first_child; it; it = it->next)
	{
		if (!is_node_emtpy(it))
		{
			return false;
		}
	}

	return true;
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
	CursorPosition*& _pos = editor->cursor._pos;

	auto delete_move_left = [&]() {
		if (!_pos)
		{
			// We need the cursor position to go backwards
			return false;
		}

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
			_pos = nullptr;
			pos.node = to_delete_parent;
			pos.offset = 0;
		}
		else if (_pos->prev)
		{
			// Return cursor to a previous position outside the removed node
			_pos = _pos->prev; // Should I set this to nullptr?
			if (did_delete)
			{
				while (_pos->prev && node_has_parent(_pos->point.node, to_delete))
				{
					_pos = _pos->prev;
				}
			}
			pos = _pos->point;

			if (pos.node->leaf)
			{
				pos.offset = pos.node->get_text().length;
			}
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
					if (!delete_move_left()) return false;
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

			AstPoint first_point{};
			if (node_first_point(new_node, &first_point))
			{
				pos = first_point;
				if (new_node->leaf)
				{
					pos.node->pre_edit_text();
					pos.node->text.insert(pos.offset, (char)codepoint);
					++pos.offset;
					editor->cursor._pos = nullptr;
				}
			}
		}
	}
}

void update_editor(Editor* editor, lgui::Context* context)
{
	if (lgui::begin_panel(context, "Ast editor", Rect::from_pos_size({ 0, 300 }, { 400, 400 }), 0))
	{
		// Input
		{
			AstPoint& pos = editor->cursor.pos;
			CursorPosition*& _pos = editor->cursor._pos;

			if (IsKeyPressed(KEY_LEFT))
			{
				if (pos.node && pos.node->leaf && pos.offset > 0)
				{
					--pos.offset;
				}
				else if (_pos && _pos->prev)
				{
					_pos = _pos->prev;
					pos = _pos->point;

					if (pos.node->leaf)
					{
						size_t length = pos.node->get_text().length;
						pos.offset = length;
					}
				}
			}

			if (IsKeyPressed(KEY_RIGHT) || IsKeyPressed(KEY_TAB))
			{
				if (pos.node && pos.node->leaf && pos.offset < pos.node->get_text().length)
				{
					++pos.offset;
				}
				else if (_pos && _pos->next)
				{
					_pos = _pos->next;
					pos = _pos->point;

					if (pos.node->leaf)
					{
						pos.offset = 0;
					}
				}
			}

			if (IsKeyPressed(KEY_UP))
			{
			}

			if (IsKeyPressed(KEY_BACKSPACE))
			{
				input_backspace(editor);
			}
		}

		while (int codepoint = GetCharPressed())
		{
			process_input_character(editor, codepoint);
		}

		editor->temp_arena = &editor->temp_arena_arr[editor->arena_swap];
		editor->arena_swap = (editor->arena_swap + 1) % 2;
		editor->temp_arena->reset();

		editor->first_cursor_position = nullptr;
		editor->last_cursor_position = nullptr;
		editor->line = 0;
		editor->draw_cursor_start = lgui::get_current_panel(context)->draw_pos;
		editor->draw_cursor = editor->draw_cursor_start;
		update_node(editor, context, editor->root);

		lgui::end_panel(context);
	}
}

Editor* g_editor;

void ast_init()
{
	g_editor = make_editor();

	AstNode* block = make_node_prefab(g_editor, AstNodeType_Block);
	AstNode* var = make_node_prefab(g_editor, AstNodeType_VarDecl);
	add_child(block, var);
	AstNode* if_s = make_node_prefab(g_editor, AstNodeType_IfStatement);
	add_child(block, if_s);

	g_editor->root = block;
}

void ast_update(lgui::Context* context)
{
	update_editor(g_editor, context);
}

