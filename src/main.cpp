#include <iostream>
#include "lag_gui.hpp"
#include "raylib.h"
#include "rlgl.h"
#include <vector>

#define GREY(f) {f, f, f, 1.f}
#define COLOR_T(r, g, b, t) {(r) * (t), (g) * (t), (b) * (t), 1.f}

using v2 = lgui::v2;
using Rect = lgui::Rect;
using f32 = lgui::f32;
using Rect = lgui::Rect;

lgui::Font* g_font = 0;


/*
// Idea for easier 2d area implementations
struct AreaHelper {
	Rect screen_rect;

	// Camera
	v2 pos;
	v2 scale;

	// The size of a unit in pixels when scale = 1
	v2 cell_size;

	v2 local_to_screen(v2 local_pos) const;
	Rect local_to_screen(v2 local_pos, v2 size) const;

	v2 screen_to_local(v2 screen_pos) const;
	Rect screen_to_local(v2 screen_pos, v2 size) const;

	v2 screen_cell_size(v2 cell_size) const;

	v2 loop_offset(v2 cell_size) const;
	v2 loop_stride(v2 cell_size) const;

	// Iterator
	AreaIterator begin()
	{
		return ...;
	}

	AreaIterator end()
	{
		AreaIterator ret{};
		ret.done = true;
		return ret;
	}
};

// I think it's important to also add an iterator
struct AreaIterator {
	// Internal
	bool done;
	bool is_horizontal;
	v2 start_pos_pixels;
	f32 end_pos_pixels;
	int start_index;
	f32 offset_pixels; // Offset for start_pos (negative number)
	f32 cell_size_pixels; // Advance value

	// Iterator functions
	bool operator==(const AreaIterator& other) const { return done == other.done; }
	// ++ operator blah blah

	// Interface
	int cell_index; // Very useful for things like alternating background colors, or piano keys in piano roll
	f32 position; // Position along its axis
	f32 size; // Size along its axis (vertical or horizontal)
};
*/


struct Note {
	int tone;
	float x;
	float width;
	int id;
};

struct NoteArea {
	v2 pos;
	v2 scale;
	std::vector<Note> notes;
	int id_counter;

	v2 mouse_on_press;
	Note note_on_press;
};

void area_test(NoteArea& area)
{
	lgui::Context* context = lgui::get_context();

	//if (lgui::begin_window("Area", Rect::from_pos_size({}, {500, 500}), 0))
	{
		lgui::draw_hook(lgui::pc(1.f, 1.f), &area, [](lgui::Box* box, lgui::Painter& painter, Rect rect) 
		{
			NoteArea& area = *((NoteArea*)box->draw_user_data);

			v2 base_cell_size = v2{100, 10};
			v2 cell_size = base_cell_size * area.scale;
			v2 pos = area.pos * cell_size;
			v2 pos_screen_offset = v2{fmodf(pos.x / cell_size.x, 1.f), fmodf(pos.y / cell_size.y, 1.f)} * cell_size;

			float width_snap = 0.125f;

			// Style
			lgui::Color line_color = {0, 0, 0, 1};
			lgui::Color note_outline_color =  COLOR_T(1.f, 0.2f, 0.2f, 0.7f);
			lgui::Color note_top_color =  COLOR_T(1.f, 0.2f, 0.2f, 1.f);
			lgui::Color note_bottom_color =  COLOR_T(1.f, 0.2f, 0.2f, 0.7f);


			// Draw lines
			for (float x = -pos_screen_offset.x; x < rect.width(); x += cell_size.x)
			{
				float screen_x = rect.top_left.x + x;
				Rect line = Rect::from_pos_size({ screen_x, rect.top_left.y }, { 1, rect.height() });
				painter.draw_rectangle(line, line_color);
			}
			for (float y = -pos_screen_offset.y; y < rect.height(); y += cell_size.y)
			{
				float screen_y = rect.top_left.y + y;
				Rect line = Rect::from_pos_size({ rect.top_left.x, screen_y }, { rect.width(), 1 });
				painter.draw_rectangle(line, line_color);
			}


			lgui::InputResult rect_response = lgui::handle_element_input(rect, lgui::get_id("background"), true);
			if (rect_response.dragging)
			{
				area.pos -= rect_response.drag_delta / cell_size;
				area.pos.x = LGUI_MAX(area.pos.x, 0.f);
				area.pos.y = LGUI_MAX(area.pos.y, 0.f);
			}

			
			v2 scroll = lgui::mouse_scroll();
			bool hover = lgui::is_mouse_overlapping(rect);
			bool shift = lgui::key_down(lgui::Key::LeftShift) || lgui::key_down(lgui::Key::LeftShift);
			if (scroll.y < 0.f && hover && shift)
			{
				area.scale.x -= 0.2f;
				area.scale.x = LGUI_MAX(area.scale.x, 0.1f);
			}
			if (scroll.y > 0.f && hover && shift)
			{
				area.scale.x += 0.2f;
			}
			if (scroll.y < 0.f && hover && !shift)
			{
				area.scale.y -= 0.2f;
				area.scale.y = LGUI_MAX(area.scale.y, 0.1f);
			}
			if (scroll.y > 0.f && hover && !shift)
			{
				area.scale.y += 0.2f;
			}

			for (size_t i = 0; i < area.notes.size(); ++i)
			{
				auto& it = area.notes[i];

				Rect note_rect = Rect::from_pos_size(rect.top_left - pos + cell_size * v2{it.x, (float)it.tone},
					v2{it.width * cell_size.x, cell_size.y});
				note_rect.cut_bottom(-1);

				painter.draw_rectangle(note_rect, note_outline_color);
				painter.draw_rectangle_gradient(note_rect.pad(1), note_top_color, note_top_color, note_bottom_color, note_bottom_color);

				lgui::push_id(it.id);

				Rect right_side = note_rect.cut_right(5);
				lgui::InputResult input_main = lgui::handle_element_input(note_rect, lgui::get_id("main"), true);
				lgui::InputResult input_right = lgui::handle_element_input(right_side, lgui::get_id("right"), true);

				if (input_main.pressed)
				{
					area.note_on_press = it;
					area.mouse_on_press = (lgui::mouse_pos() - rect.top_left + pos) / cell_size;
				}
				if (input_main.dragging)
				{
					float x_offset = area.note_on_press.x - area.mouse_on_press.x;
					v2 mouse_local = (lgui::mouse_pos() - rect.top_left + pos) / cell_size;
					float mouse_x = mouse_local.x + x_offset;
					it.x = mouse_x - fmodf(mouse_x, width_snap);
					it.tone = (int)mouse_local.y;
				}
				if (input_right.dragging)
				{
					v2 mouse_local = (lgui::mouse_pos() - rect.top_left + pos) / cell_size;
					it.width = mouse_local.x - fmodf(mouse_local.x, width_snap) - it.x;
					it.width = LGUI_MAX(it.width, 0.05f);
				}
				if (input_main.clicked || input_right.clicked)
				{
					// Must be done last so it doesn't invalidate the pointer
					area.notes.erase(area.notes.begin() + i);
					--i;
				}

				lgui::pop_id();
			}

			if (rect_response.clicked)
			{
				v2 mouse_local = (lgui::mouse_pos() - rect.top_left + pos) / cell_size;
				Note note{};
				note.x = mouse_local.x;
				note.width = 0.25f;
				note.tone = (int)mouse_local.y;
				++area.id_counter;
				note.id = area.id_counter;
				area.notes.push_back(note);
			}

		});

		//lgui::end_window();
	}
}

struct LayoutTest {
	int h_align;
	int v_align;
	bool horizontal;
	bool full_width;
	bool enable;
	int layout_count;
};

void layout_test(LayoutTest& test)
{
	if (lgui::begin_window("Layout Test", Rect::from_pos_size({110, 100}, {400, 400}), 0))
	{
		LGUI_H_LAYOUT(-1, 0)
		{
			lgui::text("Horizontal alignment: ");
			lgui::radio_button("align1", -1, &test.h_align);
			lgui::radio_button("align2", 0, &test.h_align);
			lgui::radio_button("align3", 1, &test.h_align);
		}

		LGUI_H_LAYOUT(-1, 0)
		{
			lgui::text("Vertical alignment: ");
			LGUI_V_LAYOUT(0, 0)
			{
				lgui::radio_button("valign1", -1, &test.v_align);
				lgui::radio_button("valign2", 0, &test.v_align);
				lgui::radio_button("valign3", 1, &test.v_align);
			}
		}

		LGUI_H_LAYOUT(-1, 0)
		{
			lgui::checkbox("horizontal", &test.horizontal);
			lgui::text("Horizontal");
		}

		LGUI_H_LAYOUT(-1, 0)
		{
			lgui::checkbox("full_width", &test.full_width);
			lgui::text("Full width (percentage)");
		}

		LGUI_H_LAYOUT(-1, 0)
		{
			lgui::checkbox("enable", &test.enable);
			lgui::text("Enable");
		}

		LGUI_H_LAYOUT(-1, 0)
		{
			if (lgui::button("-").clicked)
			{
				test.layout_count -= 1;
				test.layout_count = LGUI_MAX(test.layout_count, 0);
			}
			lgui::spacer(2.f);
			if (lgui::button("+").clicked)
			{
				test.layout_count += 1;
			}
			lgui::spacer(2.f);
			lgui::text("Count");
		}

		//lgui::text("SPAM SPAM SPAM SPAM SPAM SPAM SPAM SPAM SPAM SPAM SPAM SPAM SPAM SPAM SPAM SPAM SPAM");

		lgui::separator();

		bool empty_bool{};
		int emtpy_int{};

		if (test.enable)
		{
			//lgui::layout_vertical(-1, -1);

			lgui::Size size_x = lgui::fit();
			if (test.full_width)
			{
				size_x = lgui::pc(1.f);
			}

			if (test.horizontal)
			{
				//lgui::layout_horizontal(test.h_align, test.v_align, {size_x, lgui::px(150.f)});
				lgui::layout_horizontal(test.h_align, test.v_align, {size_x, lgui::rem(1.f)});
			}
			else
			{
				//lgui::layout_vertical(test.h_align, test.v_align, {size_x, lgui::px(150.f)});
				lgui::layout_vertical(test.h_align, test.v_align, {size_x, lgui::rem(1.f)});
			}
			lgui::get_box()->set_rectangle({1, 1, 0, 1});

			for (int i = 0; i < test.layout_count + 1; ++i)
			{
				lgui::push_id(i);

				lgui::button("some text");
				lgui::radio_button("radio", 0, &emtpy_int);
				lgui::checkbox("check", &empty_bool);

				lgui::pop_id();
			}

			if (test.horizontal)
			{
				lgui::Box* box = lgui::make_box("boox", {lgui::px(20.f), lgui::pc(1.f)}, 0);
				box->set_rectangle({ 1, 0, 0, 1 });
			}
			else
			{
				lgui::Box* box = lgui::make_box("boox", {lgui::pc(1.f), lgui::px(20.f)}, 0);
				box->set_rectangle({ 1, 0, 0, 1 });
			}


			lgui::layout_end();

			//lgui::text("lalalalalalalal");
			//lgui::layout_end();
		}

		lgui::end_window();
	}
}

struct WidgetTest {
	bool check;
	int radio;
	char string[16];
};

void widget_test(WidgetTest& test)
{
	if (lgui::begin_window("Widget Test", Rect::from_pos_size({200.f, 200.f}, {400.f, 650.f}), 0))
	{
		LGUI_H_LAYOUT(0, 0, {lgui::pc(1.f), lgui::fit()})
		{
			lgui::checkbox("check", &test.check);
			lgui::spacer(2.f);
			lgui::text("Checkbox");
		}

		lgui::separator();
		LGUI_V_LAYOUT(0, 0, {lgui::pc(1.f), lgui::fit()})
		{
			lgui::text("Radio buttons");
			LGUI_H_LAYOUT(0, 0)
			{
				lgui::radio_button("radio1", 1, &test.radio);
				lgui::spacer(2.f);
				lgui::radio_button("radio2", 2, &test.radio);
				lgui::spacer(2.f);
				lgui::radio_button("radio3", 3, &test.radio);
			}
		}

		lgui::separator();
		LGUI_H_LAYOUT(0, 0, {lgui::pc(1.f), lgui::fit()})
		{
			if (lgui::button("Button!").clicked)
			{
				printf("Clicked!\n");
			}
		}

		lgui::separator();
		LGUI_H_LAYOUT(0, 0, {lgui::pc(1.f), lgui::fit()})
		{
			lgui::text("Scroll areas (layouts)");
		}
		lgui::spacer(4.f);
		LGUI_H_LAYOUT(-1, -1, {lgui::pc(1.f), lgui::px(200.f)})
		{
			lgui::u32 flags = lgui::BoxFlag_Clip | lgui::BoxFlag_ScrollY;

			{
				lgui::Box* layout = lgui::layout_vertical(-1, -1, { lgui::rem(0.5f), lgui::pc(1.f) }, flags);
				layout->set_rectangle({0.2f, 0.2f, 0.2f, 1.f});
				layout->padding = {2.f, 2.f};

				char buffer[16];
				for (int i = 0; i < 20; ++i)
				{
					snprintf(buffer, 16, "text %d", i);
					lgui::text(buffer);
				}

				lgui::layout_end();
			}

			lgui::separator();

			{
				lgui::Box* layout = lgui::layout_vertical(-1, -1, { lgui::rem(0.5f), lgui::pc(1.f) }, flags);
				layout->set_rectangle({0.2f, 0.2f, 0.2f, 1.f});
				layout->padding = {2.f, 2.f};

				char buffer[16];
				for (int i = 0; i < 20; ++i)
				{
					snprintf(buffer, 16, "text %d", i);
					lgui::text(buffer);
				}

				lgui::layout_end();
			}
		}
		
		lgui::separator();
		if (lgui::begin_fancy_collapse_header("Collapsible header"))
		{
			lgui::text("With animation");
			lgui::text("1");
			lgui::text("2");
			lgui::text("3");
			lgui::text("4");
			lgui::end_fancy_collapse_header();
		}

		lgui::separator();
		if (lgui::begin_tree_node("Tree node"))
		{
			if (lgui::begin_tree_node("With animation"))
			{
				lgui::text("Hello :)");
				if (lgui::begin_tree_node("More content"))
				{
					lgui::text("Hi");
					lgui::end_tree_node();
				}
				lgui::end_tree_node();
			}
			if (lgui::begin_tree_node("Another"))
			{
				lgui::text("Goodbye");
				lgui::end_tree_node();
			}
			lgui::end_tree_node();
		}

		lgui::end_window();
	}
}

void app_test()
{
	lgui::Context* context = lgui::get_context();

	Rect full_rect = Rect::from_pos_size({}, context->app_window_size);
	Rect menu_bar = full_rect.cut_top(25.f);
	f32 right_window_size = LGUI_MIN(300.f, full_rect.width() * 0.3f);
	Rect right_window = full_rect.cut_left(right_window_size);
	Rect playlist_window = full_rect.cut_bottom(300.f);
	Rect piano_roll_window = full_rect;

	lgui::u32 bg_flags = lgui::PanelFlag_AlwaysResetRect | lgui::PanelFlag_AlwaysBackground | 
		lgui::PanelFlag_NoTitleBar;
	
	if (lgui::begin_panel("Menu Bar", menu_bar, bg_flags))
	{
		lgui::get_box()->set_rectangle(GREY(0.35f));
		if (lgui::begin_button_menu("  File  "))
		{
			lgui::button("Save");
			lgui::spacer(2.f);
			lgui::button("Load");
			lgui::spacer(2.f);
			lgui::button("Quit");
			lgui::end_button_menu();
		}
		lgui::end_panel();
	}

	if (lgui::begin_window("Right Window", right_window, bg_flags))
	{
		lgui::spacer(2.f);
		if (lgui::layout_horizontal(0, 0, {lgui::pc(1.f), lgui::fit()}))
		{
			lgui::text("My Project");
			lgui::layout_end();
		}

		f32 height = lgui::get_style().line_height();
		lgui::spacer(4.f);
		if (lgui::layout_horizontal(0, 0, {lgui::pc(1.f), lgui::fit()}))
		{
			lgui::button("Play", {lgui::rem(0.5f), lgui::px(height)});
			lgui::spacer(2.f);
			lgui::button("|<", {lgui::rem(0.25f), lgui::px(height)});
			lgui::spacer(2.f);
			lgui::button(">|", {lgui::rem(0.25f), lgui::px(height)});
			lgui::layout_end();
		}

		lgui::spacer(4.f);
		static f32 main_volume = 50.f;
		lgui::slider_value("Volume", {lgui::pc(1.f), lgui::px(height)}, 0.f, 100.f, &main_volume);

		lgui::separator();
		{
			lgui::text("Instrument: Waveform");
			lgui::spacer(4.f);
			static f32 instrument_volume = 50.f;
			lgui::slider_value("Instrument Volume", {lgui::pc(1.f), lgui::px(height)}, 0.f, 100.f, &instrument_volume);
			lgui::spacer(4.f);
			LGUI_H_LAYOUT(-1, 0, {lgui::pc(1.f), lgui::fit()})
			{
				lgui::text("Waveform Type: ");
				lgui::spacer(2.f);
				// TODO: Replace with dropdown
				lgui::button("Square", {lgui::rem(1.f), lgui::px(height)});
			}
		}

		lgui::separator();
		{
			lgui::button("+", {lgui::pc(1.f), lgui::px(height)});
			lgui::spacer(4.f);

			auto prop = [&](const char* name, f32 min, f32 max, f32* value) {
				LGUI_H_LAYOUT(-1, 0, {lgui::pc(1.f), lgui::px(height)})
				{
					lgui::text(name, {lgui::rem(0.5f), lgui::px(height)}, -1, 0);
					lgui::spacer(2.f);
					lgui::slider_value(name, {lgui::rem(0.5f), lgui::px(height)}, min, max, value);
				}
			};

			if (lgui::begin_fancy_collapse_header("Reverb"))
			{
				static f32 mix = 1.f;
				prop("Mix", 0.f, 1.f, &mix);

				lgui::spacer(4.f);
				static f32 room_size = 0.5f;
				prop("Room Size", 0.f, 1.f, &room_size);

				lgui::end_fancy_collapse_header();
			}
			lgui::spacer(4.f);
			if (lgui::begin_fancy_collapse_header("Delay"))
			{
				static f32 mix = 1.f;
				prop("Mix", 0.f, 1.f, &mix);

				lgui::spacer(4.f);
				static f32 delay = 0.5f;
				prop("Delay Time", 0.f, 1.f, &delay);

				lgui::end_fancy_collapse_header();
			}
			lgui::spacer(4.f);
			if (lgui::begin_fancy_collapse_header("Chorus"))
			{
				static f32 mix = 1.f;
				prop("Mix", 0.f, 1.f, &mix);

				lgui::spacer(4.f);
				static f32 stereo = 0.5f;
				prop("Stereo", 0.f, 1.f, &stereo);

				lgui::spacer(4.f);
				static f32 depth = 0.5f;
				prop("Depth", 0.f, 1.f, &depth);

				lgui::end_fancy_collapse_header();
			}
		}

		lgui::end_window();
	}

	if (lgui::begin_window("Playlist", playlist_window, bg_flags))
	{
		lgui::get_box()->set_rectangle(GREY(0.25f));
		f32 button_size = lgui::get_style().line_height();
		for (int y = 0; y < 5; ++y)
		{
			LGUI_H_LAYOUT(-1, -1)
			{
				for (int x = 0; x < 10; ++x)
				{
					char buffer[8];
					snprintf(buffer, 8, "%d", x);
					lgui::button(buffer, lgui::px(button_size, button_size));
					lgui::spacer(3.f);
				}
			}

			lgui::spacer(3.f);
		}

		lgui::end_window();
	}

	if (lgui::begin_window("Piano Roll", piano_roll_window, bg_flags))
	{
		lgui::get_box()->set_rectangle(GREY(0.2f));
		static NoteArea notes = {{}, {1.f, 1.f}};
		area_test(notes);
		lgui::end_window();
	}
}

void misc_test()
{
	static int h_count = 0;
	static int v_count = 0;

	if (lgui::begin_window("Growing Window", Rect::from_pos_size({300, 200}, {1, 1}), 
		lgui::PanelFlag_AutoResizeHorizontal | lgui::PanelFlag_AutoResizeVertical))
	{
		LGUI_H_LAYOUT(-1, 0)
		{
			for (int i = 0; i < h_count + 1; ++i)
			{
				lgui::text("Horizontal");
			}
		}
		LGUI_V_LAYOUT(-1, 0)
		{
			for (int i = 0; i < v_count + 1; ++i)
			{
				lgui::text("Vertical");
			}
		}
		lgui::end_window();
	}

	if (lgui::begin_window("Grow", Rect::from_pos_size({300, 200}, {1, 1}), 
		lgui::PanelFlag_AutoResizeHorizontal | lgui::PanelFlag_AutoResizeVertical))
	{
		LGUI_H_LAYOUT(-1, 0)
		{
			if (lgui::button("-").clicked)
			{
				h_count = LGUI_MAX(h_count - 1, 0);
			}
			lgui::spacer(2.f);
			if (lgui::button("+").clicked)
			{
				++h_count;
			}
			lgui::spacer(2.f);

			char buffer[32];
			snprintf(buffer, 32, "Horizontal element count (%d)", h_count);
			lgui::text(buffer);
		}
		lgui::spacer(2.f);
		LGUI_H_LAYOUT(-1, 0)
		{
			if (lgui::button("-").clicked)
			{
				v_count = LGUI_MAX(v_count - 1, 0);
			}
			lgui::spacer(2.f);
			if (lgui::button("+").clicked)
			{
				++v_count;
			}
			lgui::spacer(2.f);

			char buffer[32];
			snprintf(buffer, 32, "Vertical element count (%d)", v_count);
			lgui::text(buffer);
		}
		lgui::end_window();
	}

	if (lgui::begin_window("Scroll Test", Rect::from_pos_size({300, 200}, {150, 200}), 0))
	{
		lgui::text("hello hello hello hello hello hello hello hello hello hello hello hello hello hello hello hello hello");
		for (int i = 0; i < 15; ++i)
		{
			char buff[16];
			snprintf(buff, 16, "hello %d", i);
			lgui::text(buff);
		}
		lgui::end_window();
	}
}

struct ConsoleStr {
	char str[32];
};

static std::vector<ConsoleStr> _fake_console;

void fake_console()
{
	if (lgui::begin_window("Console", v2{150.f, 100.f}))
	{
		for (auto& it : _fake_console)
		{
			lgui::text(it.str);
		}
		lgui::end_window();
	}
}

void print(const char* format, ...)
{
	va_list args;
	va_start(args, format);

	ConsoleStr s{};
	vsnprintf(s.str, 32, format, args);
	_fake_console.push_back(s);

	va_end(args);
}

void presentation()
{
#if 0
	if (lgui::begin_window("My Window", v2{100.f, 100.f}))
	{
		if (lgui::button("Click me!").clicked)
		{
			printf("Click\n");
		}
		lgui::end_window();
	}
#endif

#if 0
	static int value = 0;
	if (lgui::begin_window("My Window", v2{200.f, 150.f}))
	{
		lgui::textf("Radio buttons (value = %d)", value);
		lgui::radio_button("Button 1", 1, &value);
		lgui::radio_button("Button 2", 2, &value);
		lgui::radio_button("Button 3", 3, &value);
		lgui::end_window();
	}
#endif

#if 0
	static int value = 0;
	if (lgui::begin_window("My Window", v2{120.f, 100.f}))
	{
		lgui::textf("value = %d", value);
		if (lgui::button("Increase value").clicked)
		{
			value += 1;
		}
		lgui::textf("value = %d", value);
		lgui::end_window();
	}
#endif

#if 0
	if (lgui::begin_window("My Window", v2{100.f, 100.f}))
	{
		{
			lgui::push_id(1);
			if (lgui::button("Button").clicked)
			{
				print("Result 1");
			}
			lgui::pop_id();
		}
		lgui::spacer(2.f);
		{
			lgui::push_id(2);
			if (lgui::button("Button").clicked)
			{
				print("Result 2");
			}
			lgui::pop_id();
		}
		lgui::end_window();
	}
#endif

#if 0
	if (lgui::begin_window("My Window", v2{100.f, 100.f}))
	{
		if (lgui::button("Button##1").clicked)
		{
			print("Result 1");
		}

		lgui::spacer(2.f);
		if (lgui::button("Button##2").clicked)
		{
			print("Result 2");
		}

		lgui::end_window();
	}

	if (lgui::begin_window("ID Tree", v2{200.f, 200.f}))
	{
		if (lgui::begin_tree_node("\"My Window\""))
		{
			if (lgui::begin_tree_node("1"))
			{
				lgui::text("\"Button\"");
				lgui::end_tree_node();
			}
			if (lgui::begin_tree_node("2"))
			{
				lgui::text("\"Button\"");
				lgui::end_tree_node();
			}
			lgui::end_tree_node();
		}

		lgui::end_window();
	}
#endif

#if 0
	if (lgui::begin_window("My Window", v2{1.f, 1.f},
			lgui::PanelFlag_AutoResizeHorizontal | lgui::PanelFlag_AutoResizeVertical |
			lgui::PanelFlag_NoTitleBar))
	{
		lgui::text("a");
		lgui::end_window();
	}
#endif

#if 0
	static int vert_count = 1;
	if (lgui::begin_window("Painter", v2{200.f, 300.f}))
	{
		LGUI_V_LAYOUT(0, -1, {lgui::pc(1.f), lgui::fit()})
		{
			LGUI_H_LAYOUT(-1, -1)
			{
				if (lgui::button("-").clicked)
				{
					vert_count -= 1;
					vert_count = LGUI_MAX(vert_count, 1);
				}
				lgui::spacer(2.f);
				if (lgui::button("+").clicked)
				{
					vert_count += 1;
				}
				lgui::spacer(2.f);
				lgui::textf("Vertex count (%d)", vert_count);
			}
			lgui::separator();

			lgui::draw_hook(lgui::px(150.f, 150.f), [](lgui::Box* box, lgui::Painter& painter, Rect rect)
			{
				painter.draw_circle(rect.center(), 15.f, 180.f, {1, 0, 0, 1});
			});
		}
		lgui::end_window();
	}
#endif

#if 0
	static int element_count = 0;
	if (lgui::begin_window("Layout inc", v2{100, 100}, 0))
	{
		LGUI_H_LAYOUT(-1, -1)
		{
			if (lgui::button("-").clicked)
			{
				element_count -= 1;
				element_count = LGUI_MAX(element_count, 0);
			}
			lgui::spacer(2.f);
			if (lgui::button("+").clicked)
			{
				element_count += 1;
			}
			lgui::spacer(2.f);
			lgui::textf("Element count (%d)", element_count);
		}
		lgui::separator();

		lgui::end_window();
	}


	if (lgui::begin_window("My Window", v2{200.f, 200.f}))
	{
		LGUI_V_LAYOUT(-1, -1, {lgui::pc(1.f), lgui::fit()})
		{

			for (int i = 0; i < element_count; ++i)
			{
				lgui::push_id(i);
				lgui::button("Button");
				lgui::pop_id();
				lgui::spacer(4.f);
			}

			{
				lgui::Box* box = lgui::make_box("cursor", lgui::px(4, 4), 0);
				box->set_rectangle({1, 0, 0, 1});
			}
		}
		lgui::end_window();
	}
#endif

	if (lgui::begin_window("My Window", v2{250.f, 80.f}))
	{
		if (lgui::layout_horizontal(-1, -1, {lgui::pc(1.f), lgui::fit()}))
		{
			lgui::button("Button 1");
			lgui::spacer(2.f);
			lgui::button("Button 2");
			lgui::spacer(2.f);
			lgui::button("Button 3");
			lgui::layout_end();
		}
		lgui::end_window();
	}

}

int main()
{
	const int screenWidth = 800;
	const int screenHeight = 800;
	bool limit_framerate = true;

	InitWindow(screenWidth, screenHeight, "LGUI Example");

	if (limit_framerate)
	{
		SetWindowState(FLAG_MSAA_4X_HINT | FLAG_WINDOW_RESIZABLE | FLAG_VSYNC_HINT);
		//SetTargetFPS(60);
	}
	else
	{
		SetWindowState(FLAG_MSAA_4X_HINT | FLAG_WINDOW_RESIZABLE);
	}

	lgui::Context* context = lgui::init(16);
	context->app_window_size = {(f32)screenWidth, (f32)screenHeight};

	lgui::Font* font = context->atlas.add_font("resources/fonts/montserrat/Montserrat-Regular.ttf", 18);
	lgui::Font* mono_font = context->atlas.add_font("resources/fonts/ubuntu/UbuntuMono-R.ttf", 18);
	context->atlas.build();
	g_font = mono_font;

	lgui::Style style{};
	style.default_font = font;
	style.line_padding = 4.f;
	style.window_content_padding = 4.f;
	style.window_outline = GREY(0.15f);
	style.window_background = GREY(0.3f);
	style.window_title_background = GREY(0.2f);
	style.window_title_color = GREY(0.9f);
	style.button_background = COLOR_T(0.4f, 0.4f, 0.9f, 1.f);
	style.button_background_hover = COLOR_T(0.4f, 0.4f, 0.9f, 0.8f);
	style.button_background_down = COLOR_T(0.4f, 0.4f, 0.9f, 0.5f);
	style.button_text = {1.f, 1.f, 1.f, 1.f};
	style.radio_button_background = COLOR_T(0.4f, 0.4f, 0.9f, 1.f);
	style.radio_button_background_hover = COLOR_T(0.4f, 0.4f, 0.9f, 0.8f);
	style.radio_button_inside = {0.8f, 0.8f, 0.8f, 1.f};
	style.radio_button_outline = COLOR_T(0.4f, 0.4f, 0.9f, 0.7f);
	style.radio_button_outline_size = 1.f;
	style.button_padding = 2.f;
	style.checkbox_outline = COLOR_T(0.6f, 0.6f, 1.0f, 1.f);
	style.separator = GREY(0.5f);
	style.separator_size = 2.f;
	style.separator_spacing = 20.f;
	lgui::push_style(style);

	NoteArea area{};
	area.scale = {1, 1};

	LayoutTest layout_t{};
	WidgetTest widget_t{};

	while (!WindowShouldClose())
	{
		if (IsKeyPressed(KEY_F11))
		{
			ToggleFullscreen();
		}

		BeginDrawing();
		ClearBackground(SKYBLUE);

		rlDisableBackfaceCulling();

		// Enable for frame by frame
		//if (IsKeyPressed(KEY_ENTER) || IsKeyDown(KEY_RIGHT_SHIFT))
		{
			while (int codepoint = GetCharPressed())
			{
				lgui::input_register_char_press(codepoint);
			}
			for (int i = 0; i < lgui::Key::MAX; ++i)
			{
				lgui::input_register_key_down((lgui::u32)i, IsKeyDown(i));
			}

			lgui::begin_frame(GetFrameTime());
			context->app_window_size = {(f32)GetScreenWidth(), (f32)GetScreenHeight()};

			layout_test(layout_t);
			widget_test(widget_t);
			//area_test(area);
			misc_test();
			app_test();
			
			presentation();
			fake_console();


			lgui::end_frame();


#if 0
			// Draw font atlas
			rlBegin(RL_TRIANGLES);
			rlColor4ub(255, 255, 255, 255);
			rlEnableTexture(context->atlas.texture_id);
			rlSetTexture(context->atlas.texture_id);
			f32 w = 512.f;
			rlTexCoord2f(0, 0);
			rlVertex2f(0, 0);
			rlTexCoord2f(1, 0);
			rlVertex2f(w, 0);
			rlTexCoord2f(0, 1);
			rlVertex2f(0, w);

			rlTexCoord2f(1, 0);
			rlVertex2f(w, 0);
			rlTexCoord2f(0, 1);
			rlVertex2f(0, w);
			rlTexCoord2f(1, 1);
			rlVertex2f(w, w);
			rlEnd();
#endif

		}

		lgui::draw_frame();
		DrawFPS(1, 1);
		EndDrawing();
	}

	lgui::deinit();

	CloseWindow();

	return 0;
}
