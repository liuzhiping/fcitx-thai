/***************************************************************************
 *   Copyright (C) YEAR~YEAR by Your Name                                  *
 *   your-email@address.com                                                *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 *   This program is distributed in the hope that it will be useful,       *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this program; if not, write to the                         *
 *   Free Software Foundation, Inc.,                                       *
 *   51 Franklin St, Fifth Floor, Boston, MA 02110-1301, USA.              *
 ***************************************************************************/

#include <thai/thcell.h>
#include <thai/thinp.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <fcitx/ime.h>
#include <fcitx-config/fcitx-config.h>
#include <fcitx-config/xdg.h>
#include <fcitx-utils/log.h>
#include <fcitx-utils/utils.h>
#include <fcitx-utils/utf8.h>
#include <fcitx/instance.h>
#include <fcitx/context.h>
#include <fcitx/module.h>
#include <fcitx/hook.h>
#include <libintl.h>

#include "config.h"
#include "thai-internal.h"

static void* FcitxThaiCreate(FcitxInstance* instance);
static void FcitxThaiDestroy(void* arg);
static void FcitxThaiReloadConfig(void* arg);
boolean FcitxThaiInit(void* arg); /**< FcitxThaiInit */
void  FcitxThaiResetIM(void* arg); /**< FcitxThaiResetIM */
INPUT_RETURN_VALUE FcitxThaiDoInput(void* arg, FcitxKeySym sym, unsigned int state); /**< FcitxThaiDoInput */
INPUT_RETURN_VALUE FcitxThaiGetCandWords(void* arg); /**< FcitxThaiGetCandWords */
void FcitxThaiSave(void* arg); /**< FcitxThaiSave */

CONFIG_DEFINE_LOAD_AND_SAVE(Thai, FcitxThaiConfig, "fcitx-thai")
DECLARE_ADDFUNCTIONS(Thai)

FCITX_DEFINE_PLUGIN(fcitx_thai, imclass2, FcitxIMClass2) = {
    .Create = FcitxThaiCreate,
    .Destroy = FcitxThaiDestroy,
    .ReloadConfig = FcitxThaiReloadConfig
};

static void*
FcitxThaiCreate(FcitxInstance* instance)
{
    FcitxThai* thai = fcitx_utils_new(FcitxThai);
    bindtextdomain("fcitx-thai", LOCALEDIR);
    thai->owner = instance;

    if (!ThaiLoadConfig(&thai->config)) {
        free(thai);
        return NULL;
    }

    FcitxIMIFace iface;
    memset(&iface, 0, sizeof(FcitxIMIFace));
    iface.DoInput = FcitxThaiDoInput;
    iface.GetCandWords = FcitxThaiGetCandWords;
    iface.Init = FcitxThaiInit;
    iface.Save = FcitxThaiSave;
    iface.ResetIM = FcitxThaiResetIM;

    FcitxInstanceRegisterIMv2(
        instance,
        thai,
        "thai",
        _("Thai"),
        "thai",
        iface,
        10,
        "th"
    );

    FcitxThaiAddFunctions(instance);
    return thai;
}

static void
FcitxThaiDestroy(void* arg)
{
    FcitxThai* thai = (FcitxThai*)arg;
    free(thai);
}

static void
FcitxThaiReloadConfig(void* arg)
{
    FcitxThai* thai = (FcitxThai*)arg;
    ThaiLoadConfig(&thai->config);
}

boolean FcitxThaiInit(void* arg)
{
    return true;
}

static boolean
is_context_lost_key(FcitxKeySym keyval)
{
    return ((keyval & 0xFF00) == 0xFF00) &&
           (keyval == FcitxKey_BackSpace ||
            keyval == FcitxKey_Tab ||
            keyval == FcitxKey_Linefeed ||
            keyval == FcitxKey_Clear ||
            keyval == FcitxKey_Return ||
            keyval == FcitxKey_Pause ||
            keyval == FcitxKey_Scroll_Lock ||
            keyval == FcitxKey_Sys_Req ||
            keyval == FcitxKey_Escape ||
            keyval == FcitxKey_Delete ||
            /* IsCursorkey */
            (FcitxKey_Home <= keyval && keyval <= FcitxKey_Begin) ||
            /* IsKeypadKey, non-chars only */
            (FcitxKey_KP_Space <= keyval && keyval <= FcitxKey_KP_Delete) ||
            /* IsMiscFunctionKey */
            (FcitxKey_Select <= keyval && keyval <= FcitxKey_Break) ||
            /* IsFunctionKey */
            (FcitxKey_F1 <= keyval && keyval <= FcitxKey_F35));
}

static boolean
is_context_intact_key(FcitxKeySym keyval)
{
    return (((keyval & 0xFF00) == 0xFF00) &&
            ( /* IsModifierKey */
                (FcitxKey_Shift_L <= keyval && keyval <= FcitxKey_Hyper_R) ||
                (keyval == FcitxKey_Mode_switch) ||
                (keyval == FcitxKey_Num_Lock))) ||
           (((keyval & 0xFE00) == 0xFE00) &&
            (FcitxKey_ISO_Lock <= keyval && keyval <= FcitxKey_ISO_Last_Group_Lock));
}

/* TIS 820-2538 Keyboard Layout */
static const char tis_qwerty_map[] = {
    0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
    0x08, 0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f,
    0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17,
    0x18, 0x19, 0x1a, 0x1b, 0x1c, 0x1d, 0x1e, 0x1f,
    0x20, 0xe5, 0x2e, 0xf2, 0xf3, 0xf4, 0xee, 0xa7,
    0xf6, 0xf7, 0xf5, 0xf9, 0xc1, 0xa2, 0xe3, 0xbd,
    0xa8, 0xdf, 0x2f, 0x2d, 0xc0, 0xb6, 0xd8, 0xd6,
    0xa4, 0xb5, 0xab, 0xc7, 0xb2, 0xaa, 0xcc, 0xc6,
    0xf1, 0xc4, 0xda, 0xa9, 0xaf, 0xae, 0xe2, 0xac,
    0xe7, 0xb3, 0xeb, 0xc9, 0xc8, 0x3f, 0xec, 0xcf,
    0xad, 0xf0, 0xb1, 0xa6, 0xb8, 0xea, 0xce, 0x22,
    0x29, 0xed, 0x28, 0xba, 0xa3, 0xc5, 0xd9, 0xf8,
    0xef, 0xbf, 0xd4, 0xe1, 0xa1, 0xd3, 0xb4, 0xe0,
    0xe9, 0xc3, 0xe8, 0xd2, 0xca, 0xb7, 0xd7, 0xb9,
    0xc2, 0xe6, 0xbe, 0xcb, 0xd0, 0xd5, 0xcd, 0xe4,
    0xbb, 0xd1, 0xbc, 0xb0, 0xa5, 0x2c, 0xfb, 0x7f
};

/* Pattachote Layout */
static const char pattachote_qwerty_map[] = {
    0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
    0x08, 0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f,
    0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17,
    0x18, 0x19, 0x1a, 0x1b, 0x1c, 0x1d, 0x1e, 0x1f,
    0x20, 0x2b, 0xb1, 0x2f, 0x2c, 0x3f, 0x5f, 0xa2,
    0x28, 0x29, 0x2e, 0x25, 0xd2, 0xf1, 0xa8, 0xbe,
    0xf0, 0x3d, 0xf2, 0xf3, 0xf4, 0xf5, 0xd9, 0xf7,
    0xf8, 0xf9, 0xa6, 0xe4, 0xbf, 0xf6, 0xa9, 0xcc,
    0x22, 0xeb, 0xda, 0xb0, 0xd3, 0xe6, 0xb3, 0xec,
    0xd7, 0xab, 0xbc, 0xaa, 0xe2, 0xce, 0xc8, 0xb6,
    0xb2, 0xea, 0xad, 0xb8, 0xc9, 0xbd, 0xc0, 0xc4,
    0xaf, 0xd6, 0xae, 0xe3, 0xe5, 0xac, 0xd8, 0x2d,
    0x5f, 0xe9, 0xd4, 0xc5, 0xa7, 0xc2, 0xa1, 0xd1,
    0xd5, 0xc1, 0xd2, 0xb9, 0xe0, 0xca, 0xa4, 0xc7,
    0xe1, 0xe7, 0xcd, 0xb7, 0xc3, 0xb4, 0xcb, 0xb5,
    0xbb, 0xe8, 0xba, 0xcf, 0xed, 0xc6, 0xdf, 0x7f
};

/* Ketmanee Layout (default) */
static const char ketmanee_qwerty_map[] = {
    0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
    0x08, 0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f,
    0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17,
    0x18, 0x19, 0x1a, 0x1b, 0x1c, 0x1d, 0x1e, 0x1f,
    0x20, 0x2b, 0x2e, 0xf2, 0xf3, 0xf4, 0xdf, 0xa7,
    0xf6, 0xf7, 0xf5, 0xf9, 0xc1, 0xa2, 0xe3, 0xbd,
    0xa8, 0xe5, 0x2f, 0x2d, 0xc0, 0xb6, 0xd8, 0xd6,
    0xa4, 0xb5, 0xab, 0xc7, 0xb2, 0xaa, 0xcc, 0xc6,
    0xf1, 0xc4, 0xda, 0xa9, 0xaf, 0xae, 0xe2, 0xac,
    0xe7, 0xb3, 0xeb, 0xc9, 0xc8, 0x3f, 0xec, 0xcf,
    0xad, 0xf0, 0xb1, 0xa6, 0xb8, 0xea, 0xce, 0x22,
    0x29, 0xed, 0x28, 0xba, 0xa3, 0xc5, 0xd9, 0xf8,
    0x5f, 0xbf, 0xd4, 0xe1, 0xa1, 0xd3, 0xb4, 0xe0,
    0xe9, 0xc3, 0xe8, 0xd2, 0xca, 0xb7, 0xd7, 0xb9,
    0xc2, 0xe6, 0xbe, 0xcb, 0xd0, 0xd5, 0xcd, 0xe4,
    0xbb, 0xd1, 0xbc, 0xb0, 0xa5, 0x2c, 0x25, 0x7f
};

static const char* thai_qwerty_map[] = {
    ketmanee_qwerty_map,
    pattachote_qwerty_map,
    tis_qwerty_map
};

unsigned char
thai_map_qwerty(ThaiKBMap map, unsigned char c)
{
    if (map > THAI_KB_TIS820_2538) {
        map = THAI_KB_TIS820_2538;
    }

    return thai_qwerty_map[map][c];
}

static tischar_t
keyval_to_tis(ThaiKBMap thai_kb_map, guint keyval)
{
    if (FcitxKey_space <= keyval && keyval <= FcitxKey_asciitilde)
        return thai_map_qwerty(thai_kb_map, keyval);

    if (FcitxKey_Thai_kokai <= keyval && keyval <= FcitxKey_Thai_lekkao)
        return (tischar_t)(keyval - FcitxKey_Thai_kokai) + 0xa1;

    if (0x01000e01 <= keyval && keyval <= 0x01000e5f)
        return (tischar_t)(keyval - 0x01000e01) + 0xa1;

    return 0;
}

static void
FcitxThaiForgetPrevChars(FcitxThai* thai)
{
    memset(thai->char_buff, 0, FALLBACK_BUFF_SIZE);
    thai->buff_tail = 0;
}

static void
FcitxThaiRememberPrevChars(FcitxThai* thai, tischar_t new_char)
{
    if (FALLBACK_BUFF_SIZE == thai->buff_tail) {
        memmove(thai->char_buff, thai->char_buff + 1, FALLBACK_BUFF_SIZE - 1);
        --thai->buff_tail;
    }
    thai->char_buff[thai->buff_tail++] = new_char;
}

static void
FcitxThaiGetPrevCell(FcitxThai* thai, struct thcell_t* res)
{
    th_init_cell(res);

    if (is_client_support_surrounding(IBUS_ENGINE(libthai_engine))) {
        IBusText* surrounding;
        guint     cursor_pos;
        guint     anchor_pos;
        const gchar* s;
        gchar*    tis_text = NULL;

        ibus_engine_get_surrounding_text(IBUS_ENGINE(libthai_engine),
                                         &surrounding, &cursor_pos, &anchor_pos);
        s = ibus_text_get_text(surrounding);
        cursor_pos = g_utf8_offset_to_pointer(s, cursor_pos) - s;
        while (*s) {
            const gchar* t;

            tis_text = g_convert(s, cursor_pos, "TIS-620", "UTF-8",
                                 NULL, NULL, NULL);
            if (tis_text)
                break;

            t = g_utf8_next_char(s);
            cursor_pos -= (t - s);
            s = t;
        }
        if (tis_text) {
            gint char_index;

            char_index = g_utf8_pointer_to_offset(s, s + cursor_pos);
            th_prev_cell((thchar_t*) tis_text, char_index, res, TRUE);
            g_free(tis_text);
        }
    } else {
        /* retrieve from the fallback buffer */
        th_prev_cell(libthai_engine->char_buff, libthai_engine->buff_tail,
                     res, TRUE);
    }
}

INPUT_RETURN_VALUE FcitxThaiDoInput(void* arg, FcitxKeySym sym, unsigned int state)
{
    FcitxThai* thai = (FcitxThai*) arg;

    struct thcell_t context_cell;
    struct thinpconv_t conv;
    tischar_t new_char;

    if ((state & (FcitxKeyState_Ctrl | FcitxKeyState_Alt))
            || is_context_lost_key(sym)) {
        FcitxThaiForgetPrevChars(thai);
        return IRV_TO_PROCESS;
    }
    if (is_context_intact_key(sym)) {
        return IRV_TO_PROCESS;
    }

    new_char = keyval_to_tis(thai->config.kb_map, sym);
    if (0 == new_char)
        return IRV_TO_PROCESS;

    /* No correction -> just reject or commit */
    if (!thai->config.do_correct) {
        thchar_t prev_char = FcitxThaiGetPrevChar(thai);

        if (!th_isaccept(prev_char, new_char, thai->config.isc_mode))
            goto reject_char;

        return FcitxThaiCommitChars(thai, &new_char, 1);
    }

    FcitxThaiGetPrevCell(thai, &context_cell);
    if (!th_validate_leveled(context_cell, new_char, &conv,
                             thai->config.isc_mode)) {
        goto reject_char;
    }

    if (conv.offset < 0) {
        FcitxInputContext* ic = FcitxInstanceGetCurrentIC(thai->owner);
        /* Can't correct context, just fall back to rejection */
        if (!(ic->contextCaps & CAPACITY_SURROUNDING_TEXT))
            goto reject_char;

        FcitxInstanceDeleteSurroundingText(thai->owner, ic, conv.offset, -conv.offset);
    }
    FcitxThaiForgetPrevChars(thai);
    FcitxThaiRememberPrevChars(thai, new_char);

    return FcitxThaiCommitChars(thai, conv.conv, strlen((char*)conv.conv));

reject_char:
    /* gdk_beep (); */
    return true;
}

INPUT_RETURN_VALUE FcitxThaiGetCandWords(void* arg)
{

}

void FcitxThaiResetIM(void* arg)
{

}

void FcitxThaiSave(void* arg)
{

}


#include "fcitx-thai-addfunctions.h"
