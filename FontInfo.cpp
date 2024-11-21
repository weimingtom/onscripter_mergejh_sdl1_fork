/* -*- C++ -*-
 *
 *  FontInfo.cpp - Font information storage class of ONScripter
 *
 *  Copyright (c) 2001-2022 Ogapee. All rights reserved.
 *
 *  ogapee@aqua.dti2.ne.jp
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include "FontInfo.h"
#include <stdio.h>
#include <SDL_ttf.h>

#if defined(PSP)
#include <string.h>
#include <stdlib.h>
extern int psp_power_resume_number;
#endif

static struct FontContainer {
    FontContainer* next;
    int size;
    TTF_Font* font[2];
#if defined(PSP)
    SDL_RWops* rw_ops;
    int power_resume_number;
    char name[256];
#endif

    FontContainer()
    {
        size = 0;
        next = NULL;
        font[0] = font[1] = NULL;
#if defined(PSP)
        rw_ops = NULL;
        power_resume_number = 0;
#endif
    };
} root_font_container;

FontInfo::FontInfo()
{
    ttf_font[0] = ttf_font[1] = NULL;

    color[0] = color[1] = color[2] = 0xff;
    on_color[0] = on_color[1] = on_color[2] = 0xff;
    off_color[0] = off_color[1] = off_color[2] = 0xaa;
    nofile_color[0] = 0x55;
    nofile_color[1] = 0x55;
    nofile_color[2] = 0x99;
    rubyon_flag = false;

    reset(NULL);
}

void FontInfo::reset(Encoding* enc)
{
    this->enc = enc;
    tateyoko_mode = YOKO_MODE;
    clear();

    is_bold = true;
    is_shadow = true;
    is_transparent = true;
    is_newline_accepted = false;

    is_line_space_fixed = false;
}

void* FontInfo::openFont(char* font_file, int ratio1, int ratio2)
{
    int font_size = font_size_xy[1];
    if (enc->getEncoding() != Encoding::CODE_UTF8 &&
        font_size_xy[0] < font_size_xy[1])
        font_size = font_size_xy[0];

    FontContainer* fc = &root_font_container;
    while (fc->next) {
        if (fc->next->size == font_size) break;
        fc = fc->next;
    }
    if (!fc->next) {
        fc->next = new FontContainer();
        fc->next->size = font_size;
        FILE* fp = fopen(font_file, "r");
        if (fp == NULL) return NULL;
        fclose(fp);
#if defined(PSP)
        fc->next->rw_ops = SDL_RWFromFile(font_file, "r");
        fc->next->font[0] = TTF_OpenFontRW(fc->next->rw_ops, SDL_TRUE, font_size * ratio1 / ratio2);
#if (SDL_TTF_MAJOR_VERSION >= 2) && (SDL_TTF_MINOR_VERSION >= 0) && (SDL_TTF_PATCHLEVEL >= 10)
        fc->next->font[1] = TTF_OpenFontRW(fc->next->rw_ops, SDL_TRUE, font_size * ratio1 / ratio2);
        TTF_SetFontOutline(fc->next->font[1], 1);
#endif
        fc->next->power_resume_number = psp_power_resume_number;
        strcpy(fc->next->name, font_file);
#else
        fp = fopen(font_file, "rb");
        fseek(fp, 0, SEEK_END);
        long length = ftell(fp);
        unsigned char* buf = new unsigned char[length]; // not released
        fseek(fp, 0, SEEK_SET);
        fread(buf, 1, length, fp);
        fclose(fp);
        SDL_RWops* src = SDL_RWFromMem(buf, length);
        fc->next->font[0] = TTF_OpenFontRW(src, 1, font_size * ratio1 / ratio2);
#if (SDL_TTF_MAJOR_VERSION >= 2) && (SDL_TTF_MINOR_VERSION >= 0) && (SDL_TTF_PATCHLEVEL >= 10)
        SDL_RWops* src2 = SDL_RWFromMem(buf, length);
        fc->next->font[1] = TTF_OpenFontRW(src2, 1, font_size * ratio1 / ratio2);
        TTF_SetFontOutline(fc->next->font[1], 1);
#endif
#endif
    }
#if defined(PSP)
    else if (fc->next->power_resume_number != psp_power_resume_number) {
        FILE* fp = fopen(fc->next->name, "r");
        fc->next->rw_ops->hidden.stdio.fp = fp;
        fc->next->power_resume_number = psp_power_resume_number;
    }
#endif

    ttf_font[0] = (void*)fc->next->font[0];
    ttf_font[1] = (void*)fc->next->font[1];

    return fc->next->font;
}

void FontInfo::setTateyokoMode(int tateyoko_mode)
{
    this->tateyoko_mode = tateyoko_mode;
    clear();
}

int FontInfo::getTateyokoMode()
{
    return tateyoko_mode;
}

int FontInfo::getRemainingLine()
{
    if (tateyoko_mode == YOKO_MODE)
        return num_xy[1] - xy[1] / 2;
    else
        return num_xy[1] - num_xy[0] + xy[0] / 2 + 1;
}

void FontInfo::toggleStyle(int style)
{
    for (int i = 0; i < 2; i++) {
        if (ttf_font[i] == NULL) continue;
        int old_style = TTF_GetFontStyle((TTF_Font*)ttf_font[i]);
        int new_style = old_style ^ style;
        TTF_SetFontStyle((TTF_Font*)ttf_font[i], new_style);
    }
}

int FontInfo::x(bool use_ruby_offset)
{
    int x = xy[0] * pitch_xy[0] / 2 + top_xy[0] + line_offset_xy[0];
    if (use_ruby_offset && rubyon_flag && tateyoko_mode == TATE_MODE)
        x += font_size_xy[0] - pitch_xy[0];
    return x;
}

int FontInfo::y(bool use_ruby_offset)
{
    int pitch_y = pitch_xy[1];
    if (!is_line_space_fixed &&
        enc->getEncoding() == Encoding::CODE_UTF8 && ttf_font[0])
        pitch_y += TTF_FontLineSkip((TTF_Font*)ttf_font[0]) - font_size_xy[1];
    int y = xy[1] * pitch_y / 2 + top_xy[1] + line_offset_xy[1];
    if (use_ruby_offset && rubyon_flag && tateyoko_mode == YOKO_MODE &&
        enc->getEncoding() == Encoding::CODE_CP932)
        y += pitch_xy[1] - font_size_xy[1];
    return y;
}

void FontInfo::setXY(int x, int y)
{
    if (x != -1) xy[0] = x * 2;
    if (y != -1) xy[1] = y * 2;
}

void FontInfo::clear()
{
    if (tateyoko_mode == YOKO_MODE)
        setXY(0, 0);
    else
        setXY(num_xy[0] - 1, 0);
    line_offset_xy[0] = line_offset_xy[1] = 0;
}

void FontInfo::newLine()
{
    if (tateyoko_mode == YOKO_MODE) {
        xy[0] = 0;
        xy[1] += 2;
    }
    else {
        xy[0] -= 2;
        xy[1] = 0;
    }
    line_offset_xy[0] = line_offset_xy[1] = 0;
}

void FontInfo::setLineArea(const char* buf)
{
    if (enc->getEncoding() == Encoding::CODE_UTF8) {
        int w = 0;
        while (buf[0]) {
            int n = enc->getBytes(buf[0]);
            unsigned short unicode = enc->getUTF16(buf);

            int minx, maxx, miny, maxy, advanced;
            TTF_GlyphMetrics((TTF_Font*)ttf_font[0], unicode,
                             &minx, &maxx, &miny, &maxy, &advanced);

            w += advanced + pitch_xy[tateyoko_mode] - font_size_xy[tateyoko_mode];
            buf += n;
        }
        num_xy[tateyoko_mode] = w * 2 / pitch_xy[tateyoko_mode] + 1;
    }
    else {
        num_xy[tateyoko_mode] = strlen(buf) / 2 + 1;
    }
    num_xy[1 - tateyoko_mode] = 1;
}

bool FontInfo::isEndOfLine(float margin)
{
    if (xy[tateyoko_mode] + margin >= num_xy[tateyoko_mode] * 2) return true;

    return false;
}

bool FontInfo::isLineEmpty()
{
    if (xy[tateyoko_mode] == 0) return true;

    return false;
}

void FontInfo::advanceCharInHankaku(float offset)
{
    xy[tateyoko_mode] += offset;
}

void FontInfo::addLineOffset(int offset)
{
    line_offset_xy[tateyoko_mode] += offset;
}

SDL_Rect FontInfo::calcUpdatedArea(int start_xy[2], int ratio1, int ratio2)
{
    SDL_Rect rect;

    if (tateyoko_mode == YOKO_MODE) {
        int pitch_y = pitch_xy[1];
        if (enc->getEncoding() == Encoding::CODE_UTF8 && ttf_font[0])
            pitch_y += TTF_FontLineSkip((TTF_Font*)ttf_font[0]) - font_size_xy[1];
        if (start_xy[1] == xy[1]) {
            rect.x = top_xy[0] + pitch_xy[0] * start_xy[0] / 2;
            rect.w = pitch_xy[0] * (xy[0] - start_xy[0]) / 2 + 1;
        }
        else {
            rect.x = top_xy[0];
            rect.w = pitch_xy[0] * num_xy[0];
        }
        rect.y = top_xy[1] + start_xy[1] * pitch_y / 2;
        rect.h = pitch_y + pitch_y * (xy[1] - start_xy[1]) / 2;
        if (rubyon_flag && enc->getEncoding() == Encoding::CODE_CP932)
            rect.h += pitch_xy[1] - font_size_xy[1];
    }
    else {
        rect.x = top_xy[0] + pitch_xy[0] * xy[0] / 2;
        rect.w = font_size_xy[0] + pitch_xy[0] * (start_xy[0] - xy[0]) / 2;
        if (rubyon_flag) rect.w += font_size_xy[0] - pitch_xy[0];
        if (start_xy[0] == xy[0]) {
            rect.y = top_xy[1] + pitch_xy[1] * start_xy[1] / 2;
            rect.h = pitch_xy[1] * (xy[1] - start_xy[1]) / 2 + 1;
        }
        else {
            rect.y = top_xy[1];
            rect.h = pitch_xy[1] * num_xy[1];
        }
        num_xy[0] = (xy[0] - start_xy[0]) / 2 + 1;
    }

    return rect;
}

void FontInfo::addShadeArea(SDL_Rect& rect, int dx, int dy, int dw, int dh)
{
    rect.x += dx;
    rect.y += dy;
    rect.w += dw;
    rect.h += dh;
}

int FontInfo::initRuby(FontInfo& body_info, int body_count, int ruby_count)
{
    if ((tateyoko_mode == YOKO_MODE &&
         body_count + body_info.xy[0] / 2 >= body_info.num_xy[0] - 1) ||
        (tateyoko_mode == TATE_MODE &&
         body_count + body_info.xy[1] / 2 > body_info.num_xy[1]))
        body_info.newLine();

    top_xy[0] = body_info.x();
    top_xy[1] = body_info.y();
    pitch_xy[0] = font_size_xy[0];
    pitch_xy[1] = font_size_xy[1];

    int margin = 0;

    if (tateyoko_mode == YOKO_MODE) {
        top_xy[1] -= font_size_xy[1];
        num_xy[0] = ruby_count;
        num_xy[1] = 1;
    }
    else {
        top_xy[0] += body_info.font_size_xy[0];
        num_xy[0] = 1;
        num_xy[1] = ruby_count;
    }

    if (ruby_count * font_size_xy[tateyoko_mode] >= body_count * body_info.pitch_xy[tateyoko_mode]) {
        margin = (ruby_count * font_size_xy[tateyoko_mode] - body_count * body_info.pitch_xy[tateyoko_mode] + 1) / 2;
    }
    else {
        int offset = 0;
        if (ruby_count > 0)
            offset = (body_count * body_info.pitch_xy[tateyoko_mode] - ruby_count * font_size_xy[tateyoko_mode] + ruby_count) / (ruby_count * 2);
        top_xy[tateyoko_mode] += offset;
        pitch_xy[tateyoko_mode] += offset * 2;
    }
    body_info.line_offset_xy[tateyoko_mode] += margin;

    clear();

    return margin;
}