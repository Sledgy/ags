//=============================================================================
//
// Adventure Game Studio (AGS)
//
// Copyright (C) 1999-2011 Chris Jones and 2011-20xx others
// The full list of copyright holders can be found in the Copyright.txt
// file, which is part of this source code distribution.
//
// The AGS source code is provided under the Artistic License 2.0.
// A copy of this license can be found in the file License.txt and at
// http://www.opensource.org/licenses/artistic-license-2.0.php
//
//=============================================================================

#include <stdio.h>
#include "ac/draw.h"
#include "ac/drawingsurface.h"
#include "ac/common.h"
#include "ac/charactercache.h"
#include "ac/display.h"
#include "ac/game.h"
#include "ac/gamesetupstruct.h"
#include "ac/gamestate.h"
#include "ac/global_translation.h"
#include "ac/objectcache.h"
#include "ac/roomobject.h"
#include "ac/roomstatus.h"
#include "ac/string.h"
#include "debug/debug_log.h"
#include "font/fonts.h"
#include "gui/guimain.h"
#include "ac/spritecache.h"
#include "script/runtimescriptvalue.h"
#include "gfx/gfx_util.h"

using AGS::Common::Bitmap;
namespace BitmapHelper = AGS::Common::BitmapHelper;

extern GameSetupStruct game;
extern GameState play;
extern RoomStatus*croom;
extern RoomObject*objs;
extern CharacterCache *charcache;
extern ObjectCache objcache[MAX_INIT_SPR];
extern GUIMain*guis;
extern SpriteCache spriteset;
extern int spritewidth[MAX_SPRITES],spriteheight[MAX_SPRITES];
extern Bitmap *dynamicallyCreatedSurfaces[MAX_DYNAMIC_SURFACES];

extern int current_screen_resolution_multiplier;

// ** SCRIPT DRAWINGSURFACE OBJECT

void DrawingSurface_Release(ScriptDrawingSurface* sds)
{
    if (sds->roomBackgroundNumber >= 0)
    {
        if (sds->modified)
        {
            if (sds->roomBackgroundNumber == play.bg_frame)
            {
                invalidate_screen();
                mark_current_background_dirty();
            }
            play.raw_modified[sds->roomBackgroundNumber] = 1;
        }

        sds->roomBackgroundNumber = -1;
    }
    if (sds->dynamicSpriteNumber >= 0)
    {
        if (sds->modified)
        {
            int tt;
            // force a refresh of any cached object or character images
            if (croom != NULL) 
            {
                for (tt = 0; tt < croom->numobj; tt++) 
                {
                    if (objs[tt].num == sds->dynamicSpriteNumber)
                        objcache[tt].sppic = -31999;
                }
            }
            for (tt = 0; tt < game.numcharacters; tt++) 
            {
                if (charcache[tt].sppic == sds->dynamicSpriteNumber)
                    charcache[tt].sppic = -31999;
            }
            for (tt = 0; tt < game.numgui; tt++) 
            {
                if ((guis[tt].bgpic == sds->dynamicSpriteNumber) &&
                    (guis[tt].on == 1))
                {
                    guis_need_update = 1;
                    break;
                }
            }
        }

        sds->dynamicSpriteNumber = -1;
    }
    if (sds->dynamicSurfaceNumber >= 0)
    {
        delete dynamicallyCreatedSurfaces[sds->dynamicSurfaceNumber];
        dynamicallyCreatedSurfaces[sds->dynamicSurfaceNumber] = NULL;
        sds->dynamicSurfaceNumber = -1;
    }
    sds->modified = 0;
}

void ScriptDrawingSurface::MultiplyCoordinates(int *xcoord, int *ycoord)
{
    if (this->highResCoordinates)
    {
        if (current_screen_resolution_multiplier == 1) 
        {
            // using high-res co-ordinates but game running at low-res
            xcoord[0] /= 2;
            ycoord[0] /= 2;
        }
    }
    else
    {
        if (current_screen_resolution_multiplier > 1) 
        {
            // using low-res co-ordinates but game running at high-res
            xcoord[0] *= 2;
            ycoord[0] *= 2;
        }
    }
}

void ScriptDrawingSurface::MultiplyThickness(int *valueToAdjust)
{
    if (this->highResCoordinates)
    {
        if (current_screen_resolution_multiplier == 1) 
        {
            valueToAdjust[0] /= 2;
            if (valueToAdjust[0] < 1)
                valueToAdjust[0] = 1;
        }
    }
    else
    {
        if (current_screen_resolution_multiplier > 1) 
        {
            valueToAdjust[0] *= 2;
        }
    }
}

// convert actual co-ordinate back to what the script is expecting
void ScriptDrawingSurface::UnMultiplyThickness(int *valueToAdjust)
{
    if (this->highResCoordinates)
    {
        if (current_screen_resolution_multiplier == 1) 
        {
            valueToAdjust[0] *= 2;
        }
    }
    else
    {
        if (current_screen_resolution_multiplier > 1) 
        {
            valueToAdjust[0] /= 2;
            if (valueToAdjust[0] < 1)
                valueToAdjust[0] = 1;
        }
    }
}

ScriptDrawingSurface* DrawingSurface_CreateCopy(ScriptDrawingSurface *sds)
{
    Bitmap *sourceBitmap = sds->GetBitmapSurface();

    for (int i = 0; i < MAX_DYNAMIC_SURFACES; i++)
    {
        if (dynamicallyCreatedSurfaces[i] == NULL)
        {
            dynamicallyCreatedSurfaces[i] = BitmapHelper::CreateBitmapCopy(sourceBitmap);
            ScriptDrawingSurface *newSurface = new ScriptDrawingSurface();
            newSurface->dynamicSurfaceNumber = i;
            newSurface->hasAlphaChannel = sds->hasAlphaChannel;
            ccRegisterManagedObject(newSurface, newSurface);
            return newSurface;
        }
    }

    quit("!DrawingSurface.CreateCopy: too many copied surfaces created");
    return NULL;
}

void DrawingSurface_DrawSurface(ScriptDrawingSurface* target, ScriptDrawingSurface* source, int translev) {
    if ((translev < 0) || (translev > 99))
        quit("!DrawingSurface.DrawSurface: invalid parameter (transparency must be 0-99)");

    Bitmap *ds = target->StartDrawing();
    Bitmap *surfaceToDraw = source->GetBitmapSurface();

    if (surfaceToDraw == target->GetBitmapSurface())
        quit("!DrawingSurface.DrawSurface: cannot draw surface onto itself");

    if (translev == 0) {
        // just draw it over the top, no transparency
        ds->Blit(surfaceToDraw, 0, 0, 0, 0, surfaceToDraw->GetWidth(), surfaceToDraw->GetHeight());
        target->FinishedDrawing();
        return;
    }

    if (surfaceToDraw->GetColorDepth() <= 8)
        quit("!DrawingSurface.DrawSurface: 256-colour surfaces cannot be drawn transparently");

    // Draw it transparently
    int trans_mode = ((100-translev) * 25) / 10;
    AGS::Engine::GfxUtil::DrawSpriteWithTransparency(ds, surfaceToDraw, 0, 0, trans_mode);
    target->FinishedDrawing();
}

void DrawingSurface_DrawImage(ScriptDrawingSurface* sds, int xx, int yy, int slot, int trans, int width, int height)
{
    if ((slot < 0) || (slot >= MAX_SPRITES) || (spriteset[slot] == NULL))
        quit("!DrawingSurface.DrawImage: invalid sprite slot number specified");

    if ((trans < 0) || (trans > 100))
        quit("!DrawingSurface.DrawImage: invalid transparency setting");

    // 100% transparency, don't draw anything
    if (trans == 100)
        return;

    Bitmap *sourcePic = spriteset[slot];
    bool needToFreeBitmap = false;

    if (width != SCR_NO_VALUE)
    {
        // Resize specified

        if ((width < 1) || (height < 1))
            return;

        sds->MultiplyCoordinates(&width, &height);

        // resize the sprite to the requested size
        Bitmap *newPic = BitmapHelper::CreateBitmap(width, height, sourcePic->GetColorDepth());

        newPic->StretchBlt(sourcePic,
            RectWH(0, 0, spritewidth[slot], spriteheight[slot]),
            RectWH(0, 0, width, height));

        sourcePic = newPic;
        needToFreeBitmap = true;
        update_polled_stuff_if_runtime();
    }

    Bitmap *ds = sds->StartDrawing();
    sds->MultiplyCoordinates(&xx, &yy);

    if (sourcePic->GetColorDepth() != ds->GetColorDepth()) {
        debug_log("RawDrawImage: Sprite %d colour depth %d-bit not same as background depth %d-bit", slot, spriteset[slot]->GetColorDepth(), ds->GetColorDepth());
    }

    int trans_mode = 0;
    if (trans > 0)
    {
        trans_mode = ((100 - trans) * 255) / 100;
    }

    draw_sprite_support_alpha(ds, xx, yy, sourcePic, slot, trans_mode);

    sds->FinishedDrawing();

    if (needToFreeBitmap)
        delete sourcePic;
}


void DrawingSurface_SetDrawingColor(ScriptDrawingSurface *sds, int newColour) 
{
    sds->currentColourScript = newColour;
    // StartDrawing to set up ds to set the colour at the appropriate
    // depth for the background
    Bitmap *ds = sds->StartDrawing();
    if (newColour == SCR_COLOR_TRANSPARENT)
    {
        sds->currentColour = ds->GetMaskColor();
    }
    else
    {
        sds->currentColour = ds->GetCompatibleColor(newColour);
    }
    sds->FinishedDrawingReadOnly();
}

int DrawingSurface_GetDrawingColor(ScriptDrawingSurface *sds)
{
    return sds->currentColourScript;
}

void DrawingSurface_SetUseHighResCoordinates(ScriptDrawingSurface *sds, int highRes) 
{
    sds->highResCoordinates = (highRes) ? 1 : 0;
}

int DrawingSurface_GetUseHighResCoordinates(ScriptDrawingSurface *sds) 
{
    return sds->highResCoordinates;
}

int DrawingSurface_GetHeight(ScriptDrawingSurface *sds) 
{
    Bitmap *ds = sds->StartDrawing();
    int height = ds->GetHeight();
    sds->FinishedDrawingReadOnly();
    sds->UnMultiplyThickness(&height);
    return height;
}

int DrawingSurface_GetWidth(ScriptDrawingSurface *sds) 
{
    Bitmap *ds = sds->StartDrawing();
    int width = ds->GetWidth();
    sds->FinishedDrawingReadOnly();
    sds->UnMultiplyThickness(&width);
    return width;
}

void DrawingSurface_Clear(ScriptDrawingSurface *sds, int colour)
{
    Bitmap *ds = sds->StartDrawing();
    int allegroColor;
    if ((colour == -SCR_NO_VALUE) || (colour == SCR_COLOR_TRANSPARENT))
    {
        allegroColor = ds->GetMaskColor();
    }
    else
    {
        allegroColor = ds->GetCompatibleColor(colour);
    }
    ds->Fill(allegroColor);
    sds->FinishedDrawing();
}

void DrawingSurface_DrawCircle(ScriptDrawingSurface *sds, int x, int y, int radius)
{
    sds->MultiplyCoordinates(&x, &y);
    sds->MultiplyThickness(&radius);

    Bitmap *ds = sds->StartDrawing();
    ds->FillCircle(Circle(x, y, radius), sds->currentColour);
    sds->FinishedDrawing();
}

void DrawingSurface_DrawRectangle(ScriptDrawingSurface *sds, int x1, int y1, int x2, int y2)
{
    sds->MultiplyCoordinates(&x1, &y1);
    sds->MultiplyCoordinates(&x2, &y2);

    Bitmap *ds = sds->StartDrawing();
    ds->FillRect(Rect(x1,y1,x2,y2), sds->currentColour);
    sds->FinishedDrawing();
}

void DrawingSurface_DrawTriangle(ScriptDrawingSurface *sds, int x1, int y1, int x2, int y2, int x3, int y3)
{
    sds->MultiplyCoordinates(&x1, &y1);
    sds->MultiplyCoordinates(&x2, &y2);
    sds->MultiplyCoordinates(&x3, &y3);

    Bitmap *ds = sds->StartDrawing();
    ds->DrawTriangle(Triangle(x1,y1,x2,y2,x3,y3), sds->currentColour);
    sds->FinishedDrawing();
}

void DrawingSurface_DrawString(ScriptDrawingSurface *sds, int xx, int yy, int font, const char* texx, ...)
{
    char displbuf[STD_BUFFER_SIZE];
    va_list ap;
    va_start(ap,texx);
    vsprintf(displbuf, get_translation(texx), ap);
    va_end(ap);
    
    sds->MultiplyCoordinates(&xx, &yy);
    Bitmap *ds = sds->StartDrawing();
    // don't use wtextcolor because it will do a 16->32 conversion
    color_t text_color = sds->currentColour;
    if ((ds->GetColorDepth() <= 8) && (play.raw_color > 255)) {
        text_color = ds->GetCompatibleColor(1);
        debug_log ("RawPrint: Attempted to use hi-color on 256-col background");
    }
    wouttext_outline(ds, xx, yy, font, text_color, displbuf);
    sds->FinishedDrawing();
}

void DrawingSurface_DrawStringWrapped(ScriptDrawingSurface *sds, int xx, int yy, int wid, int font, int alignment, const char *msg) {
    int texthit = wgetfontheight(font);
    sds->MultiplyCoordinates(&xx, &yy);
    sds->MultiplyThickness(&wid);

    break_up_text_into_lines(wid, font, (char*)msg);

    Bitmap *ds = sds->StartDrawing();
    color_t text_color = sds->currentColour;

    for (int i = 0; i < numlines; i++)
    {
        int drawAtX = xx;

        if (alignment == SCALIGN_CENTRE)
        {
            drawAtX = xx + ((wid / 2) - wgettextwidth(lines[i], font) / 2);
        }
        else if (alignment == SCALIGN_RIGHT)
        {
            drawAtX = (xx + wid) - wgettextwidth(lines[i], font);
        }

        wouttext_outline(ds, drawAtX, yy + texthit*i, font, text_color, lines[i]);
    }

    sds->FinishedDrawing();
}

void DrawingSurface_DrawMessageWrapped(ScriptDrawingSurface *sds, int xx, int yy, int wid, int font, int msgm)
{
    char displbuf[3000];
    get_message_text(msgm, displbuf);
    // it's probably too late but check anyway
    if (strlen(displbuf) > 2899)
        quit("!RawPrintMessageWrapped: message too long");

    DrawingSurface_DrawStringWrapped(sds, xx, yy, wid, font, SCALIGN_LEFT, displbuf);
}

void DrawingSurface_DrawLine(ScriptDrawingSurface *sds, int fromx, int fromy, int tox, int toy, int thickness) {
    sds->MultiplyCoordinates(&fromx, &fromy);
    sds->MultiplyCoordinates(&tox, &toy);
    sds->MultiplyThickness(&thickness);
    int ii,jj,xx,yy;
    Bitmap *ds = sds->StartDrawing();
    // draw several lines to simulate the thickness
    color_t draw_color = sds->currentColour;
    for (ii = 0; ii < thickness; ii++) 
    {
        xx = (ii - (thickness / 2));
        for (jj = 0; jj < thickness; jj++)
        {
            yy = (jj - (thickness / 2));
            ds->DrawLine (Line(fromx + xx, fromy + yy, tox + xx, toy + yy), draw_color);
        }
    }
    sds->FinishedDrawing();
}

void DrawingSurface_DrawPixel(ScriptDrawingSurface *sds, int x, int y) {
    sds->MultiplyCoordinates(&x, &y);
    int thickness = 1;
    sds->MultiplyThickness(&thickness);
    int ii,jj;
    Bitmap *ds = sds->StartDrawing();
    // draw several pixels to simulate the thickness
    color_t draw_color = sds->currentColour;
    for (ii = 0; ii < thickness; ii++) 
    {
        for (jj = 0; jj < thickness; jj++)
        {
            ds->PutPixel(x + ii, y + jj, draw_color);
        }
    }
    sds->FinishedDrawing();
}

int DrawingSurface_GetPixel(ScriptDrawingSurface *sds, int x, int y) {
    sds->MultiplyCoordinates(&x, &y);
    Bitmap *ds = sds->StartDrawing();
    unsigned int rawPixel = ds->GetPixel(x, y);
    unsigned int maskColor = ds->GetMaskColor();
    int colDepth = ds->GetColorDepth();

    if (rawPixel == maskColor)
    {
        rawPixel = SCR_COLOR_TRANSPARENT;
    }
    else if (colDepth > 8)
    {
        int r = getr_depth(colDepth, rawPixel);
        int ds = getg_depth(colDepth, rawPixel);
        int b = getb_depth(colDepth, rawPixel);

        rawPixel = Game_GetColorFromRGB(r, ds, b);
    }

    sds->FinishedDrawingReadOnly();

    return rawPixel;
}

//=============================================================================
//
// Script API Functions
//
//=============================================================================

#include "debug/out.h"
#include "script/script_api.h"
#include "script/script_runtime.h"

// void (ScriptDrawingSurface *sds, int colour)
RuntimeScriptValue Sc_DrawingSurface_Clear(void *self, const RuntimeScriptValue *params, int32_t param_count)
{
    API_OBJCALL_VOID_PINT(ScriptDrawingSurface, DrawingSurface_Clear);
}

// ScriptDrawingSurface* (ScriptDrawingSurface *sds)
RuntimeScriptValue Sc_DrawingSurface_CreateCopy(void *self, const RuntimeScriptValue *params, int32_t param_count)
{
    API_OBJCALL_OBJAUTO(ScriptDrawingSurface, ScriptDrawingSurface, DrawingSurface_CreateCopy);
}

// void (ScriptDrawingSurface *sds, int x, int y, int radius)
RuntimeScriptValue Sc_DrawingSurface_DrawCircle(void *self, const RuntimeScriptValue *params, int32_t param_count)
{
    API_OBJCALL_VOID_PINT3(ScriptDrawingSurface, DrawingSurface_DrawCircle);
}

// void (ScriptDrawingSurface* sds, int xx, int yy, int slot, int trans, int width, int height)
RuntimeScriptValue Sc_DrawingSurface_DrawImage(void *self, const RuntimeScriptValue *params, int32_t param_count)
{
    API_OBJCALL_VOID_PINT6(ScriptDrawingSurface, DrawingSurface_DrawImage);
}

// void (ScriptDrawingSurface *sds, int fromx, int fromy, int tox, int toy, int thickness)
RuntimeScriptValue Sc_DrawingSurface_DrawLine(void *self, const RuntimeScriptValue *params, int32_t param_count)
{
    API_OBJCALL_VOID_PINT5(ScriptDrawingSurface, DrawingSurface_DrawLine);
}

// void (ScriptDrawingSurface *sds, int xx, int yy, int wid, int font, int msgm)
RuntimeScriptValue Sc_DrawingSurface_DrawMessageWrapped(void *self, const RuntimeScriptValue *params, int32_t param_count)
{
    API_OBJCALL_VOID_PINT5(ScriptDrawingSurface, DrawingSurface_DrawMessageWrapped);
}

// void (ScriptDrawingSurface *sds, int x, int y)
RuntimeScriptValue Sc_DrawingSurface_DrawPixel(void *self, const RuntimeScriptValue *params, int32_t param_count)
{
    API_OBJCALL_VOID_PINT2(ScriptDrawingSurface, DrawingSurface_DrawPixel);
}

// void (ScriptDrawingSurface *sds, int x1, int y1, int x2, int y2)
RuntimeScriptValue Sc_DrawingSurface_DrawRectangle(void *self, const RuntimeScriptValue *params, int32_t param_count)
{
    API_OBJCALL_VOID_PINT4(ScriptDrawingSurface, DrawingSurface_DrawRectangle);
}

// void (ScriptDrawingSurface *sds, int xx, int yy, int font, const char* texx, ...)
RuntimeScriptValue Sc_DrawingSurface_DrawString(void *self, const RuntimeScriptValue *params, int32_t param_count)
{
    API_OBJCALL_SCRIPT_SPRINTF(DrawingSurface_DrawString, 4);
    DrawingSurface_DrawString((ScriptDrawingSurface*)self, params[0].IValue, params[1].IValue, params[2].IValue, "%s", scsf_buffer);
    return RuntimeScriptValue();
}

// void (ScriptDrawingSurface *sds, int xx, int yy, int wid, int font, int alignment, const char *msg)
RuntimeScriptValue Sc_DrawingSurface_DrawStringWrapped(void *self, const RuntimeScriptValue *params, int32_t param_count)
{
    API_OBJCALL_VOID_PINT5_POBJ(ScriptDrawingSurface, DrawingSurface_DrawStringWrapped, const char);
}

// void (ScriptDrawingSurface* target, ScriptDrawingSurface* source, int translev)
RuntimeScriptValue Sc_DrawingSurface_DrawSurface(void *self, const RuntimeScriptValue *params, int32_t param_count)
{
    API_OBJCALL_VOID_POBJ_PINT(ScriptDrawingSurface, DrawingSurface_DrawSurface, ScriptDrawingSurface);
}

// void (ScriptDrawingSurface *sds, int x1, int y1, int x2, int y2, int x3, int y3)
RuntimeScriptValue Sc_DrawingSurface_DrawTriangle(void *self, const RuntimeScriptValue *params, int32_t param_count)
{
    API_OBJCALL_VOID_PINT6(ScriptDrawingSurface, DrawingSurface_DrawTriangle);
}

// int (ScriptDrawingSurface *sds, int x, int y)
RuntimeScriptValue Sc_DrawingSurface_GetPixel(void *self, const RuntimeScriptValue *params, int32_t param_count)
{
    API_OBJCALL_INT_PINT2(ScriptDrawingSurface, DrawingSurface_GetPixel);
}

// void (ScriptDrawingSurface* sds)
RuntimeScriptValue Sc_DrawingSurface_Release(void *self, const RuntimeScriptValue *params, int32_t param_count)
{
    API_OBJCALL_VOID(ScriptDrawingSurface, DrawingSurface_Release);
}

// int (ScriptDrawingSurface *sds)
RuntimeScriptValue Sc_DrawingSurface_GetDrawingColor(void *self, const RuntimeScriptValue *params, int32_t param_count)
{
    API_OBJCALL_INT(ScriptDrawingSurface, DrawingSurface_GetDrawingColor);
}

// void (ScriptDrawingSurface *sds, int newColour)
RuntimeScriptValue Sc_DrawingSurface_SetDrawingColor(void *self, const RuntimeScriptValue *params, int32_t param_count)
{
    API_OBJCALL_VOID_PINT(ScriptDrawingSurface, DrawingSurface_SetDrawingColor);
}

// int (ScriptDrawingSurface *sds)
RuntimeScriptValue Sc_DrawingSurface_GetHeight(void *self, const RuntimeScriptValue *params, int32_t param_count)
{
    API_OBJCALL_INT(ScriptDrawingSurface, DrawingSurface_GetHeight);
}

// int (ScriptDrawingSurface *sds)
RuntimeScriptValue Sc_DrawingSurface_GetUseHighResCoordinates(void *self, const RuntimeScriptValue *params, int32_t param_count)
{
    API_OBJCALL_INT(ScriptDrawingSurface, DrawingSurface_GetUseHighResCoordinates);
}

// void (ScriptDrawingSurface *sds, int highRes)
RuntimeScriptValue Sc_DrawingSurface_SetUseHighResCoordinates(void *self, const RuntimeScriptValue *params, int32_t param_count)
{
    API_OBJCALL_VOID_PINT(ScriptDrawingSurface, DrawingSurface_SetUseHighResCoordinates);
}

// int (ScriptDrawingSurface *sds)
RuntimeScriptValue Sc_DrawingSurface_GetWidth(void *self, const RuntimeScriptValue *params, int32_t param_count)
{
    API_OBJCALL_INT(ScriptDrawingSurface, DrawingSurface_GetWidth);
}

//=============================================================================
//
// Exclusive API for Plugins
//
//=============================================================================

// void (ScriptDrawingSurface *sds, int xx, int yy, int font, const char* texx, ...)
void ScPl_DrawingSurface_DrawString(ScriptDrawingSurface *sds, int xx, int yy, int font, const char* texx, ...)
{
    va_list arg_ptr;
    va_start(arg_ptr, texx);
    const char *scsf_buffer = ScriptVSprintf(ScSfBuffer, 3000, get_translation(texx), arg_ptr);
    va_end(arg_ptr);
    DrawingSurface_DrawString(sds, xx, yy, font, "%s", scsf_buffer);
}

void RegisterDrawingSurfaceAPI()
{
    ccAddExternalObjectFunction("DrawingSurface::Clear^1",              Sc_DrawingSurface_Clear);
    ccAddExternalObjectFunction("DrawingSurface::CreateCopy^0",         Sc_DrawingSurface_CreateCopy);
    ccAddExternalObjectFunction("DrawingSurface::DrawCircle^3",         Sc_DrawingSurface_DrawCircle);
    ccAddExternalObjectFunction("DrawingSurface::DrawImage^6",          Sc_DrawingSurface_DrawImage);
    ccAddExternalObjectFunction("DrawingSurface::DrawLine^5",           Sc_DrawingSurface_DrawLine);
    ccAddExternalObjectFunction("DrawingSurface::DrawMessageWrapped^5", Sc_DrawingSurface_DrawMessageWrapped);
    ccAddExternalObjectFunction("DrawingSurface::DrawPixel^2",          Sc_DrawingSurface_DrawPixel);
    ccAddExternalObjectFunction("DrawingSurface::DrawRectangle^4",      Sc_DrawingSurface_DrawRectangle);
    ccAddExternalObjectFunction("DrawingSurface::DrawString^104",       Sc_DrawingSurface_DrawString);
    ccAddExternalObjectFunction("DrawingSurface::DrawStringWrapped^6",  Sc_DrawingSurface_DrawStringWrapped);
    ccAddExternalObjectFunction("DrawingSurface::DrawSurface^2",        Sc_DrawingSurface_DrawSurface);
    ccAddExternalObjectFunction("DrawingSurface::DrawTriangle^6",       Sc_DrawingSurface_DrawTriangle);
    ccAddExternalObjectFunction("DrawingSurface::GetPixel^2",           Sc_DrawingSurface_GetPixel);
    ccAddExternalObjectFunction("DrawingSurface::Release^0",            Sc_DrawingSurface_Release);
    ccAddExternalObjectFunction("DrawingSurface::get_DrawingColor",     Sc_DrawingSurface_GetDrawingColor);
    ccAddExternalObjectFunction("DrawingSurface::set_DrawingColor",     Sc_DrawingSurface_SetDrawingColor);
    ccAddExternalObjectFunction("DrawingSurface::get_Height",           Sc_DrawingSurface_GetHeight);
    ccAddExternalObjectFunction("DrawingSurface::get_UseHighResCoordinates", Sc_DrawingSurface_GetUseHighResCoordinates);
    ccAddExternalObjectFunction("DrawingSurface::set_UseHighResCoordinates", Sc_DrawingSurface_SetUseHighResCoordinates);
    ccAddExternalObjectFunction("DrawingSurface::get_Width",            Sc_DrawingSurface_GetWidth);

    /* ----------------------- Registering unsafe exports for plugins -----------------------*/

    ccAddExternalFunctionForPlugin("DrawingSurface::Clear^1",              (void*)DrawingSurface_Clear);
    ccAddExternalFunctionForPlugin("DrawingSurface::CreateCopy^0",         (void*)DrawingSurface_CreateCopy);
    ccAddExternalFunctionForPlugin("DrawingSurface::DrawCircle^3",         (void*)DrawingSurface_DrawCircle);
    ccAddExternalFunctionForPlugin("DrawingSurface::DrawImage^6",          (void*)DrawingSurface_DrawImage);
    ccAddExternalFunctionForPlugin("DrawingSurface::DrawLine^5",           (void*)DrawingSurface_DrawLine);
    ccAddExternalFunctionForPlugin("DrawingSurface::DrawMessageWrapped^5", (void*)DrawingSurface_DrawMessageWrapped);
    ccAddExternalFunctionForPlugin("DrawingSurface::DrawPixel^2",          (void*)DrawingSurface_DrawPixel);
    ccAddExternalFunctionForPlugin("DrawingSurface::DrawRectangle^4",      (void*)DrawingSurface_DrawRectangle);
    ccAddExternalFunctionForPlugin("DrawingSurface::DrawString^104",       (void*)ScPl_DrawingSurface_DrawString);
    ccAddExternalFunctionForPlugin("DrawingSurface::DrawStringWrapped^6",  (void*)DrawingSurface_DrawStringWrapped);
    ccAddExternalFunctionForPlugin("DrawingSurface::DrawSurface^2",        (void*)DrawingSurface_DrawSurface);
    ccAddExternalFunctionForPlugin("DrawingSurface::DrawTriangle^6",       (void*)DrawingSurface_DrawTriangle);
    ccAddExternalFunctionForPlugin("DrawingSurface::GetPixel^2",           (void*)DrawingSurface_GetPixel);
    ccAddExternalFunctionForPlugin("DrawingSurface::Release^0",            (void*)DrawingSurface_Release);
    ccAddExternalFunctionForPlugin("DrawingSurface::get_DrawingColor",     (void*)DrawingSurface_GetDrawingColor);
    ccAddExternalFunctionForPlugin("DrawingSurface::set_DrawingColor",     (void*)DrawingSurface_SetDrawingColor);
    ccAddExternalFunctionForPlugin("DrawingSurface::get_Height",           (void*)DrawingSurface_GetHeight);
    ccAddExternalFunctionForPlugin("DrawingSurface::get_UseHighResCoordinates", (void*)DrawingSurface_GetUseHighResCoordinates);
    ccAddExternalFunctionForPlugin("DrawingSurface::set_UseHighResCoordinates", (void*)DrawingSurface_SetUseHighResCoordinates);
    ccAddExternalFunctionForPlugin("DrawingSurface::get_Width",            (void*)DrawingSurface_GetWidth);
}
