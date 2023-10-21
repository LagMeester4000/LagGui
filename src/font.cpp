#include "basic.hpp"
#include "lag_gui.hpp"
#include "crc32.hpp"
#include "raylib.h"
#include "rlgl.h"
#include <corecrt_math.h>
#define STB_RECT_PACK_IMPLEMENTATION
#define STBRP_STATIC
#include "stb_rect_pack.h"
#define STB_TRUETYPE_IMPLEMENTATION
//#define STBTT_STATIC
#include "stb_truetype.h"

namespace lgui {

const auto character_start = 0x0020;
//const auto character_end= 0x00FF;
const auto character_end= 0x00FF;

void try_packing(Arena* tmp_arena)
{
	auto nodes = tmp_arena->allocate_array<stbrp_node>(512);
	stbrp_context packer{};
	stbrp_init_target(&packer, 512, 512, nodes.ptr, (int)nodes.length);

	//stbrp_pack_rects(&packer, );
}

Font* Atlas::add_font(const char* filename, f32 pixel_height)
{
	Context* context = get_context();

	if (does_file_exist(filename))
	{
		Font* ret = context->arena.allocate_one<Font>();
		ret->name = copy_string(&context->arena, filename);
		ret->name_hash = xcrc32((const byte*)ret->name, (int)strlen(ret->name), 0);
		ret->height = pixel_height;

		if (!context->atlas.first_font)
		{
			context->atlas.first_font = ret;
			context->atlas.last_font = ret;
		}
		else
		{
			context->atlas.last_font->next = ret;
			context->atlas.last_font = ret;
		}

		return ret;
	}

	return nullptr;
}

Icon* Atlas::add_icon(const char* name, byte* pixels_rgba, u32 width, u32 height)
{

	return nullptr;
}

static usize font_count(Atlas* atlas)
{
	usize ret = 0;
	for (Font* font = atlas->first_font; font; font = font->next)
	{
		++ret;
	}

	return ret;
}

static usize rect_count(Atlas* atlas)
{
	auto character_count = character_end - character_start;

	usize ret = 0;
	for (Font* font = atlas->first_font; font; font = font->next)
	{
		ret += character_count;
	}

	return ret;
}

struct TempFont {
	Font* font;
	stbtt_fontinfo font_info;
	f32 scale;
};

bool Atlas::build()
{
	Context* context = get_context();

	auto character_count = character_end - character_start;

	auto nodes = context->temp_arena->allocate_array<stbrp_node>(512);
	stbrp_context packer{};
	stbrp_init_target(&packer, 512, 511, nodes.ptr, (int)nodes.length);

	usize rects_count = rect_count(&context->atlas);
	Slice<stbrp_rect> rects = context->temp_arena->allocate_array<stbrp_rect>(rects_count);
	usize rects_top = 0;

	for (Font* font = context->atlas.first_font; font; font = font->next)
	{
		Slice<byte> file = read_file(context->temp_arena, font->name);
		if (!file.ptr)
		{
			continue;
		}

		TempFont* temp_font = context->temp_arena->allocate_one<TempFont>();
		temp_font->font = font;
		stbtt_InitFont(&temp_font->font_info, file.ptr, 0);

		f32 scale = stbtt_ScaleForPixelHeight(&temp_font->font_info, font->height);
		temp_font->scale = scale;

		for (Codepoint codepoint = character_start; codepoint < character_end; ++codepoint)
		{
			int x0, y0, x1, y1;
			stbtt_GetCodepointBitmapBox(&temp_font->font_info, codepoint, scale, scale, &x0, &y0, &x1, &y1);

			stbrp_rect push{};
			push.w = x1 - x0;
			push.h = y1 - y0;
			push.id = codepoint;
			push.user_ptr = temp_font;
			rects[rects_top] = push;
			++rects_top;
		}
	}

	if (stbrp_pack_rects(&packer, rects.ptr, (int)rects.length) == 0)
	{
		return false;
	}

	// Create the font texture and glyph data

	// Add extra height for one white pixel
	Slice<byte> texture = context->arena.allocate_array<byte>(packer.width * (packer.height + 1));
	u32 texture_width = packer.width;
	u32 texture_height = packer.height + 1;

	for (Font* font = context->atlas.first_font; font; font = font->next)
	{
		font->glyphs = context->arena.allocate_array<Glyph>(character_count);
	}

	for (usize i = 0; i < rects.length; ++i)
	{
		auto& rect = rects[i];
		TempFont* font = (TempFont*)rect.user_ptr;

		Glyph glyph{};
		glyph.uv1 = {(f32)rect.x / (f32)texture_width, (f32)rect.y / (f32)texture_height};
		glyph.uv2 = {(f32)(rect.x + rect.w) / (f32)texture_width, (f32)(rect.y + rect.h) / (f32)texture_height};

		Codepoint codepoint = rect.id;
		int x0, y0, x1, y1, ascent;
		stbtt_GetCodepointBitmapBox(&font->font_info, codepoint, font->scale, font->scale, &x0, &y0, &x1, &y1);
		stbtt_GetFontVMetrics(&font->font_info, &ascent, nullptr, nullptr);

		int glyph_index = stbtt_FindGlyphIndex(&font->font_info, codepoint);
		int advance_width;
		stbtt_GetGlyphHMetrics(&font->font_info, glyph_index, &advance_width, nullptr);
		glyph.advance_x = ((f32)advance_width) * font->scale;

		glyph.pos = {(f32)x0, (f32)y0 + floorf(ascent * font->scale)};
		glyph.size = {(f32)(x1 - x0), (f32)(y1 - y0)};

		font->font->glyphs[rect.id - character_start] = glyph;

		stbtt_MakeCodepointBitmap(
			&font->font_info,
			texture.ptr + (texture_width * rect.y + rect.x) * 1,
			x1 - x0,
			y1 - y0,
			texture_width * 1,
			font->scale,
			font->scale,
			codepoint
		);
	}

	// Write white pixel
	texture[texture.length - 1] = 255;

	// TODO: Replace with non-raylib solution
	{
		// Convert to rgba
		Slice<byte> texture_rgba = context->temp_arena->allocate_array<byte>(texture_width * texture_height * 4);
		for (usize i = 0; i < texture_width * texture_height; ++i)
		{
			usize ind = i * 4;
			texture_rgba[ind + 0] = 255;
			texture_rgba[ind + 1] = 255;
			texture_rgba[ind + 2] = 255;
			texture_rgba[ind + 3] = texture[i];
		}

		context->atlas.texture_id = rlLoadTexture((void*)texture_rgba.ptr, texture_width, texture_height, PIXELFORMAT_UNCOMPRESSED_R8G8B8A8, 1);

		Image im{};
		im.data = (void*)texture_rgba.ptr;
		im.format = PIXELFORMAT_UNCOMPRESSED_R8G8B8A8;
		im.width = 512;
		im.height = 512;
		im.mipmaps = 1;
		context->atlas.texture_obj = LoadTextureFromImage(im);
	}

	return true;
}

const Glyph& Font::get_glyph(Codepoint codepoint) const
{
	auto ind = codepoint - character_start;
	LGUI_ASSERT(codepoint >= character_start && codepoint <= character_end, "Out of bounds");
	return glyphs[ind];
}

f32 Font::text_width(const char* text, f32 spacing) const
{
	f32 x_off = 0.f;
	usize len = strlen(text);
	for (usize i = 0; i < len; ++i)
	{
		Codepoint codepoint = text[i];
		const Glyph& glyph = get_glyph(codepoint);
		x_off += glyph.advance_x;
	}
	return floorf(x_off);
}

f32 Font::text_width(const char* text, usize text_length, f32 spacing) const
{
	f32 x_off = 0.f;
	for (usize i = 0; i < text_length; ++i)
	{
		Codepoint codepoint = text[i];
		const Glyph& glyph = get_glyph(codepoint);
		x_off += glyph.advance_x;
	}
	return floorf(x_off);
}

usize Font::find_text_width_fit(const char* text, usize text_length, f32 spacing, f32 max_width) const
{
	f32 x_off = 0.f;
	usize i = 0;
	for (; i < text_length && x_off <= max_width; ++i)
	{
		Codepoint codepoint = text[i];
		const Glyph& glyph = get_glyph(codepoint);
		x_off += glyph.advance_x;
	}
	return i;
}

}
