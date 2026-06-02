#pragma once

#include "gui/HmiGlyphTheme.h"

namespace WaterTest::GuiGlyph
{
    inline HmiGlyphTheme makeHmiGlyphTheme(
        const QColor &shadow,
        const QColor &panel,
        const QColor &body,
        const QColor &border,
        const QColor &borderWeak,
        const QColor &cyan,
        const QColor &text,
        const QColor &textDim,
        const QColor &textMuted,
        const QColor &ink,
        const QColor &green,
        const QColor &red,
        const QColor &orange,
        const QColor &purple,
        const QColor &metalDark,
        const QColor &metalMid,
        const QColor &water)
    {
        return HmiGlyphTheme{
            shadow,
            panel,
            body,
            border,
            borderWeak,
            cyan,
            text,
            textDim,
            textMuted,
            ink,
            green,
            red,
            orange,
            purple,
            metalDark,
            metalMid,
            water,
        };
    }
}
