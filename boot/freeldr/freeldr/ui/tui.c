/*
 *  FreeLoader
 *  Copyright (C) 1998-2003  Brian Palmer  <brianp@sginet.com>
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
 *  You should have received a copy of the GNU General Public License along
 *  with this program; if not, write to the Free Software Foundation, Inc.,
 *  51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#include <freeldr.h>

#ifndef _M_ARM
PVOID TextVideoBuffer = NULL;
#endif

/* GENERIC TUI UTILS *********************************************************/

/*
 * TuiPrintf()
 * Prints formatted text to the screen.
 */
INT
TuiPrintf(
    _In_ PCSTR Format, ...)
{
    INT i;
    INT Length;
    va_list ap;
    CHAR Buffer[512];

    va_start(ap, Format);
    Length = _vsnprintf(Buffer, sizeof(Buffer), Format, ap);
    va_end(ap);

    if (Length == -1)
        Length = (INT)sizeof(Buffer);

    for (i = 0; i < Length; i++)
    {
        MachConsPutChar(Buffer[i]);
    }

    return Length;
}

/*
 * DrawText()
 * Displays a string on a single screen line.
 * This function assumes coordinates are zero-based.
 */
VOID
TuiDrawText(
    _In_ ULONG X,
    _In_ ULONG Y,
    _In_ PCSTR Text,
    _In_ UCHAR Attr)
{
    TuiDrawText2(X, Y, 0 /*(ULONG)strlen(Text)*/, Text, Attr);
}

/*
 * DrawText2()
 * Displays a string on a single screen line.
 * This function assumes coordinates are zero-based.
 * MaxNumChars is the maximum number of characters to display.
 * If MaxNumChars == 0, then display the whole string.
 */
VOID
TuiDrawText2(
    _In_ ULONG X,
    _In_ ULONG Y,
    _In_opt_ ULONG MaxNumChars,
    _In_reads_or_z_(MaxNumChars) PCSTR Text,
    _In_ UCHAR Attr)
{
#ifndef _M_ARM
    PUCHAR ScreenMemory = (PUCHAR)TextVideoBuffer;
#endif
    ULONG i, j;

    /* Don't display anything if we are out of the screen */
    if ((X >= UiScreenWidth) || (Y >= UiScreenHeight))
        return;

    /* Draw the text, not exceeding the width */
    for (i = X, j = 0; Text[j] && i < UiScreenWidth && (MaxNumChars > 0 ? j < MaxNumChars : TRUE); i++, j++)
    {
#ifndef _M_ARM
        ScreenMemory[((Y*2)*UiScreenWidth)+(i*2)]   = (UCHAR)Text[j];
        ScreenMemory[((Y*2)*UiScreenWidth)+(i*2)+1] = Attr;
#else
        /* Write the character */
        MachVideoPutChar(Text[j], Attr, i, Y);
#endif
    }
}

VOID
TuiDrawCenteredText(
    _In_ ULONG Left,
    _In_ ULONG Top,
    _In_ ULONG Right,
    _In_ ULONG Bottom,
    _In_ PCSTR TextString,
    _In_ UCHAR Attr)
{
    SIZE_T TextLength;
    SIZE_T Index, LastIndex;
    ULONG  LineBreakCount;
    ULONG  BoxWidth, BoxHeight;
    ULONG  RealLeft, RealTop;
    ULONG  X, Y;
    CHAR   Temp[2];

    /* Query text length */
    TextLength = strlen(TextString);

    /* Count the new lines and the box width */
    LineBreakCount = 0;
    BoxWidth = 0;
    LastIndex = 0;
    for (Index = 0; Index < TextLength; Index++)
    {
        /* Scan for new lines */
        if (TextString[Index] == '\n')
        {
            /* Remember the new line */
            LastIndex = Index;
            LineBreakCount++;
        }
        else
        {
            /* Check for new larger box width */
            if ((Index - LastIndex) > BoxWidth)
            {
                /* Update it */
                BoxWidth = (ULONG)(Index - LastIndex);
            }
        }
    }

    /* Base the box height on the number of lines */
    BoxHeight = LineBreakCount + 1;

    /*
     * Create the centered coordinates.
     * Here, the Left/Top/Right/Bottom rectangle is a hint, around
     * which we center the "real" text rectangle RealLeft/RealTop.
     */
    RealLeft = (Left + Right - BoxWidth + 1) / 2;
    RealTop  = (Top + Bottom - BoxHeight + 1) / 2;

    /* Now go for a second scan */
    LastIndex = 0;
    for (Index = 0; Index < TextLength; Index++)
    {
        /* Look for new lines again */
        if (TextString[Index] == '\n')
        {
            /* Update where the text should start */
            RealTop++;
            LastIndex = 0;
        }
        else
        {
            /* We've got a line of text to print, do it */
            X = (ULONG)(RealLeft + LastIndex);
            Y = RealTop;
            LastIndex++;
            Temp[0] = TextString[Index];
            Temp[1] = 0;
            TuiDrawText(X, Y, Temp, Attr);
        }
    }
}

/* FULL TUI THEME ************************************************************/

#ifndef _M_ARM

#define TAG_TUI_SCREENBUFFER 'SiuT'
#define TAG_TUI_PALETTE      'PiuT'

extern UCHAR MachDefaultTextColor;

BOOLEAN TuiInitialize(VOID)
{
    MachVideoHideShowTextCursor(FALSE);
    MachVideoSetTextCursorPosition(0, 0);
    MachVideoClearScreen(ATTR(COLOR_GRAY, COLOR_BLACK));

    TextVideoBuffer = VideoAllocateOffScreenBuffer();
    if (TextVideoBuffer == NULL)
    {
        return FALSE;
    }

    return TRUE;
}

VOID TuiUnInitialize(VOID)
{
    /* Do nothing if already uninitialized */
    if (!TextVideoBuffer)
        return;

    if (UiUseSpecialEffects)
    {
        TuiFadeOut();
    }
    else
    {
        MachVideoSetDisplayMode(NULL, FALSE);
    }

    VideoFreeOffScreenBuffer();
    TextVideoBuffer = NULL;

    MachVideoClearScreen(ATTR(COLOR_GRAY, COLOR_BLACK));
    MachVideoSetTextCursorPosition(0, 0);
    MachVideoHideShowTextCursor(TRUE);
}

VOID TuiDrawBackdrop(VOID)
{
    //
    // Fill in the background (excluding title box & status bar)
    //
    TuiFillArea(0,
            TUI_TITLE_BOX_CHAR_HEIGHT,
            UiScreenWidth - 1,
            UiScreenHeight - 2,
            UiBackdropFillStyle,
            ATTR(UiBackdropFgColor, UiBackdropBgColor));

    //
    // Draw the title box
    //
    TuiDrawBox(0,
            0,
            UiScreenWidth - 1,
            TUI_TITLE_BOX_CHAR_HEIGHT - 1,
            D_VERT,
            D_HORZ,
            TRUE,
            FALSE,
            ATTR(UiTitleBoxFgColor, UiTitleBoxBgColor));

    //
    // Draw version text
    //
    TuiDrawText(2,
            1,
            FrLdrVersionString,
            ATTR(UiTitleBoxFgColor, UiTitleBoxBgColor));

    //
    // Draw copyright
    //
    TuiDrawText(2,
            2,
            BY_AUTHOR,
            ATTR(UiTitleBoxFgColor, UiTitleBoxBgColor));
    TuiDrawText(2,
            3,
            AUTHOR_EMAIL,
            ATTR(UiTitleBoxFgColor, UiTitleBoxBgColor));

    //
    // Draw help text
    //
    TuiDrawText(UiScreenWidth - 16, 3, /*"F1 for Help"*/"F8 for Options", ATTR(UiTitleBoxFgColor, UiTitleBoxBgColor));

    //
    // Draw title text
    //
    TuiDrawText((UiScreenWidth - (ULONG)strlen(UiTitleBoxTitleText)) / 2,
            2,
            UiTitleBoxTitleText,
            ATTR(UiTitleBoxFgColor, UiTitleBoxBgColor));

    //
    // Update the date & time
    //
    TuiUpdateDateTime();

    VideoCopyOffScreenBufferToVRAM();
}

/*
 * FillArea()
 * This function assumes coordinates are zero-based
 */
VOID TuiFillArea(ULONG Left, ULONG Top, ULONG Right, ULONG Bottom, CHAR FillChar, UCHAR Attr /* Color Attributes */)
{
    PUCHAR ScreenMemory = (PUCHAR)TextVideoBuffer;
    ULONG  i, j;

    /* Clip the area to the screen */
    if ((Left >= UiScreenWidth) || (Top >= UiScreenHeight))
    {
        return;
    }
    if (Right >= UiScreenWidth)
        Right = UiScreenWidth - 1;
    if (Bottom >= UiScreenHeight)
        Bottom = UiScreenHeight - 1;

    /* Loop through each line and column and fill it in */
    for (i = Top; i <= Bottom; ++i)
    {
        for (j = Left; j <= Right; ++j)
        {
            ScreenMemory[((i*2)*UiScreenWidth)+(j*2)] = (UCHAR)FillChar;
            ScreenMemory[((i*2)*UiScreenWidth)+(j*2)+1] = Attr;
        }
    }
}

/*
 * DrawShadow()
 * This function assumes coordinates are zero-based
 */
VOID TuiDrawShadow(ULONG Left, ULONG Top, ULONG Right, ULONG Bottom)
{
    PUCHAR ScreenMemory = (PUCHAR)TextVideoBuffer;
    ULONG  Idx;

    /* Shade the bottom of the area */
    if (Bottom < (UiScreenHeight - 1))
    {
        if (UiScreenHeight < 34)
            Idx = Left + 2;
        else
            Idx = Left + 1;

        for (; Idx <= Right; ++Idx)
        {
            ScreenMemory[(((Bottom+1)*2)*UiScreenWidth)+(Idx*2)+1] = ATTR(COLOR_GRAY, COLOR_BLACK);
        }
    }

    /* Shade the right of the area */
    if (Right < (UiScreenWidth - 1))
    {
        for (Idx=Top+1; Idx<=Bottom; Idx++)
        {
            ScreenMemory[((Idx*2)*UiScreenWidth)+((Right+1)*2)+1] = ATTR(COLOR_GRAY, COLOR_BLACK);
        }
    }
    if (UiScreenHeight < 34)
    {
        if ((Right + 1) < (UiScreenWidth - 1))
        {
            for (Idx=Top+1; Idx<=Bottom; Idx++)
            {
                ScreenMemory[((Idx*2)*UiScreenWidth)+((Right+2)*2)+1] = ATTR(COLOR_GRAY, COLOR_BLACK);
            }
        }
    }

    /* Shade the bottom right corner */
    if ((Right < (UiScreenWidth - 1)) && (Bottom < (UiScreenHeight - 1)))
    {
        ScreenMemory[(((Bottom+1)*2)*UiScreenWidth)+((Right+1)*2)+1] = ATTR(COLOR_GRAY, COLOR_BLACK);
    }
    if (UiScreenHeight < 34)
    {
        if (((Right + 1) < (UiScreenWidth - 1)) && (Bottom < (UiScreenHeight - 1)))
        {
            ScreenMemory[(((Bottom+1)*2)*UiScreenWidth)+((Right+2)*2)+1] = ATTR(COLOR_GRAY, COLOR_BLACK);
        }
    }
}

/*
 * DrawBox()
 * This function assumes coordinates are zero-based
 */
VOID TuiDrawBox(ULONG Left, ULONG Top, ULONG Right, ULONG Bottom, UCHAR VertStyle, UCHAR HorzStyle, BOOLEAN Fill, BOOLEAN Shadow, UCHAR Attr)
{
    UCHAR    ULCorner, URCorner, LLCorner, LRCorner;

    // Calculate the corner values
    if (HorzStyle == HORZ)
    {
        if (VertStyle == VERT)
        {
            ULCorner = UL;
            URCorner = UR;
            LLCorner = LL;
            LRCorner = LR;
        }
        else // VertStyle == D_VERT
        {
            ULCorner = VD_UL;
            URCorner = VD_UR;
            LLCorner = VD_LL;
            LRCorner = VD_LR;
        }
    }
    else // HorzStyle == D_HORZ
    {
        if (VertStyle == VERT)
        {
            ULCorner = HD_UL;
            URCorner = HD_UR;
            LLCorner = HD_LL;
            LRCorner = HD_LR;
        }
        else // VertStyle == D_VERT
        {
            ULCorner = D_UL;
            URCorner = D_UR;
            LLCorner = D_LL;
            LRCorner = D_LR;
        }
    }

    // Fill in box background
    if (Fill)
    {
        TuiFillArea(Left, Top, Right, Bottom, ' ', Attr);
    }

    // Fill in corners
    TuiFillArea(Left, Top, Left, Top, ULCorner, Attr);
    TuiFillArea(Right, Top, Right, Top, URCorner, Attr);
    TuiFillArea(Left, Bottom, Left, Bottom, LLCorner, Attr);
    TuiFillArea(Right, Bottom, Right, Bottom, LRCorner, Attr);

    // Fill in left line
    TuiFillArea(Left, Top+1, Left, Bottom-1, VertStyle, Attr);
    // Fill in top line
    TuiFillArea(Left+1, Top, Right-1, Top, HorzStyle, Attr);
    // Fill in right line
    TuiFillArea(Right, Top+1, Right, Bottom-1, VertStyle, Attr);
    // Fill in bottom line
    TuiFillArea(Left+1, Bottom, Right-1, Bottom, HorzStyle, Attr);

    // Draw the shadow
    if (Shadow)
    {
        TuiDrawShadow(Left, Top, Right, Bottom);
    }
}

VOID TuiDrawStatusText(PCSTR StatusText)
{
    SIZE_T    i;

    TuiDrawText(0, UiScreenHeight-1, " ", ATTR(UiStatusBarFgColor, UiStatusBarBgColor));
    TuiDrawText(1, UiScreenHeight-1, StatusText, ATTR(UiStatusBarFgColor, UiStatusBarBgColor));

    for (i=strlen(StatusText)+1; i<UiScreenWidth; i++)
    {
        TuiDrawText((ULONG)i, UiScreenHeight-1, " ", ATTR(UiStatusBarFgColor, UiStatusBarBgColor));
    }

    VideoCopyOffScreenBufferToVRAM();
}

VOID TuiUpdateDateTime(VOID)
{
    TIMEINFO*    TimeInfo;
    char    DateString[40];
    CHAR    TimeString[40];
    CHAR    TempString[20];
    BOOLEAN    PMHour = FALSE;

    /* Don't draw the time if this has been disabled */
    if (!UiDrawTime) return;

    TimeInfo = ArcGetTime();
    if (TimeInfo->Year < 1 || 9999 < TimeInfo->Year ||
        TimeInfo->Month < 1 || 12 < TimeInfo->Month ||
        TimeInfo->Day < 1 || 31 < TimeInfo->Day ||
        23 < TimeInfo->Hour ||
        59 < TimeInfo->Minute ||
        59 < TimeInfo->Second)
    {
        /* This happens on QEmu sometimes. We just skip updating */
        return;
    }
    // Get the month name
    strcpy(DateString, UiMonthNames[TimeInfo->Month - 1]);
    // Get the day
    _itoa(TimeInfo->Day, TempString, 10);
    // Get the day postfix
    if (1 == TimeInfo->Day || 21 == TimeInfo->Day || 31 == TimeInfo->Day)
    {
        strcat(TempString, "st");
    }
    else if (2 == TimeInfo->Day || 22 == TimeInfo->Day)
    {
        strcat(TempString, "nd");
    }
    else if (3 == TimeInfo->Day || 23 == TimeInfo->Day)
    {
        strcat(TempString, "rd");
    }
    else
    {
        strcat(TempString, "th");
    }

    // Add the day to the date
    strcat(DateString, TempString);
    strcat(DateString, " ");

    // Get the year and add it to the date
    _itoa(TimeInfo->Year, TempString, 10);
    strcat(DateString, TempString);

    // Draw the date
    TuiDrawText(UiScreenWidth-(ULONG)strlen(DateString)-2, 1, DateString, ATTR(UiTitleBoxFgColor, UiTitleBoxBgColor));

    // Get the hour and change from 24-hour mode to 12-hour
    if (TimeInfo->Hour > 12)
    {
        TimeInfo->Hour -= 12;
        PMHour = TRUE;
    }
    if (TimeInfo->Hour == 0)
    {
        TimeInfo->Hour = 12;
    }
    _itoa(TimeInfo->Hour, TempString, 10);
    strcpy(TimeString, "    ");
    strcat(TimeString, TempString);
    strcat(TimeString, ":");
    _itoa(TimeInfo->Minute, TempString, 10);
    if (TimeInfo->Minute < 10)
    {
        strcat(TimeString, "0");
    }
    strcat(TimeString, TempString);
    strcat(TimeString, ":");
    _itoa(TimeInfo->Second, TempString, 10);
    if (TimeInfo->Second < 10)
    {
        strcat(TimeString, "0");
    }
    strcat(TimeString, TempString);
    if (PMHour)
    {
        strcat(TimeString, " PM");
    }
    else
    {
        strcat(TimeString, " AM");
    }

    // Draw the time
    TuiDrawText(UiScreenWidth-(ULONG)strlen(TimeString)-2, 2, TimeString, ATTR(UiTitleBoxFgColor, UiTitleBoxBgColor));
}

VOID TuiSaveScreen(PUCHAR Buffer)
{
    PUCHAR    ScreenMemory = (PUCHAR)TextVideoBuffer;
    ULONG    i;

    for (i=0; i < (UiScreenWidth * UiScreenHeight * 2); i++)
    {
        Buffer[i] = ScreenMemory[i];
    }
}

VOID TuiRestoreScreen(PUCHAR Buffer)
{
    PUCHAR    ScreenMemory = (PUCHAR)TextVideoBuffer;
    ULONG    i;

    for (i=0; i < (UiScreenWidth * UiScreenHeight * 2); i++)
    {
        ScreenMemory[i] = Buffer[i];
    }
}

VOID TuiMessageBox(PCSTR MessageText)
{
    PVOID    ScreenBuffer;

    // Save the screen contents
    ScreenBuffer = FrLdrTempAlloc(UiScreenWidth * UiScreenHeight * 2,
                                  TAG_TUI_SCREENBUFFER);
    TuiSaveScreen(ScreenBuffer);

    // Display the message box
    TuiMessageBoxCritical(MessageText);

    // Restore the screen contents
    TuiRestoreScreen(ScreenBuffer);
    FrLdrTempFree(ScreenBuffer, TAG_TUI_SCREENBUFFER);
}

VOID TuiMessageBoxCritical(PCSTR MessageText)
{
    int        width = 8;
    unsigned int    height = 1;
    int        curline = 0;
    int        k;
    size_t        i , j;
    int        x1, x2, y1, y2;
    char    temp[260];
    char    key;

    // Find the height
    for (i=0; i<strlen(MessageText); i++)
    {
        if (MessageText[i] == '\n')
            height++;
    }

    // Find the width
    for (i=0,j=0,k=0; i<height; i++)
    {
        while ((MessageText[j] != '\n') && (MessageText[j] != 0))
        {
            j++;
            k++;
        }

        if (k > width)
            width = k;

        k = 0;
        j++;
    }

    // Calculate box area
    x1 = (UiScreenWidth - (width+2))/2;
    x2 = x1 + width + 3;
    y1 = ((UiScreenHeight - height - 2)/2) + 1;
    y2 = y1 + height + 4;

    // Draw the box
    TuiDrawBox(x1, y1, x2, y2, D_VERT, D_HORZ, TRUE, TRUE, ATTR(UiMessageBoxFgColor, UiMessageBoxBgColor));

    // Draw the text
    for (i=0,j=0; i<strlen(MessageText)+1; i++)
    {
        if ((MessageText[i] == '\n') || (MessageText[i] == 0))
        {
            temp[j] = 0;
            j = 0;
            UiDrawText(x1+2, y1+1+curline, temp, ATTR(UiMessageBoxFgColor, UiMessageBoxBgColor));
            curline++;
        }
        else
            temp[j++] = MessageText[i];
    }

    // Draw OK button
    strcpy(temp, "   OK   ");
    UiDrawText(x1+((x2-x1)/2)-3, y2-2, temp, ATTR(COLOR_BLACK, COLOR_GRAY));

    // Draw status text
    UiDrawStatusText("Press ENTER to continue");

    VideoCopyOffScreenBufferToVRAM();

    for (;;)
    {
        if (MachConsKbHit())
        {
            key = MachConsGetCh();
            if (key == KEY_EXTENDED)
                key = MachConsGetCh();

            if ((key == KEY_ENTER) || (key == KEY_SPACE) || (key == KEY_ESC))
                break;
        }

        TuiUpdateDateTime();

        VideoCopyOffScreenBufferToVRAM();

        MachHwIdle();
    }
}

VOID
TuiDrawProgressBarCenter(
    _In_ ULONG Position,
    _In_ ULONG Range,
    _Inout_z_ PSTR ProgressText)
{
    ULONG Left, Top, Right, Bottom, Width, Height;

    /* Build the coordinates and sizes */
    Height = 2;
    Width  = 50; // Allow for 50 "bars"
    Left = (UiScreenWidth - Width) / 2;
    Top  = (UiScreenHeight - Height + 4) / 2;
    Right  = Left + Width - 1;
    Bottom = Top + Height - 1;

    /* Inflate to include the box margins */
    Left -= 2;
    Right += 2;
    Top -= 1;
    Bottom += 1;

    /* Draw the progress bar */
    TuiDrawProgressBar(Left, Top, Right, Bottom, Position, Range, ProgressText);
}

VOID
TuiDrawProgressBar(
    _In_ ULONG Left,
    _In_ ULONG Top,
    _In_ ULONG Right,
    _In_ ULONG Bottom,
    _In_ ULONG Position,
    _In_ ULONG Range,
    _Inout_z_ PSTR ProgressText)
{
    ULONG ProgressBarWidth, i;

    /* Draw the box */
    TuiDrawBox(Left, Top, Right, Bottom, VERT, HORZ, TRUE, TRUE, ATTR(UiMenuFgColor, UiMenuBgColor));

    /* Exclude the box margins */
    Left += 2;
    Right -= 2;
    Top += 1;
    Bottom -= 1;

    /* Calculate the width of the bar proper */
    ProgressBarWidth = Right - Left + 1;

    /* Clip the position */
    if (Position > Range)
        Position = Range;

    /* First make sure the progress bar text fits */
    UiTruncateStringEllipsis(ProgressText, ProgressBarWidth);

    /* Draw the "Loading..." text */
    TuiDrawCenteredText(Left, Top, Right, Bottom - 1,
                        ProgressText, ATTR(UiTextColor, UiMenuBgColor));

    /* Draw the percent complete -- Use the fill character */
    for (i = 0; i < (Position * ProgressBarWidth) / Range; i++)
    {
        TuiDrawText(Left + i, Bottom,
                    "\xDB", ATTR(UiTextColor, UiMenuBgColor));
    }
    /* Fill the remaining with shadow blanks */
    TuiFillArea(Left + i, Bottom, Right, Bottom,
                '\xB2', ATTR(UiTextColor, UiMenuBgColor));

#ifndef _M_ARM
    TuiUpdateDateTime();
    VideoCopyOffScreenBufferToVRAM();
#endif
}

UCHAR TuiTextToColor(PCSTR ColorText)
{
    static const struct
    {
        PCSTR ColorName;
        UCHAR ColorValue;
    } Colors[] =
    {
        {"Black"  , COLOR_BLACK  },
        {"Blue"   , COLOR_BLUE   },
        {"Green"  , COLOR_GREEN  },
        {"Cyan"   , COLOR_CYAN   },
        {"Red"    , COLOR_RED    },
        {"Magenta", COLOR_MAGENTA},
        {"Brown"  , COLOR_BROWN  },
        {"Gray"   , COLOR_GRAY   },
        {"DarkGray"    , COLOR_DARKGRAY    },
        {"LightBlue"   , COLOR_LIGHTBLUE   },
        {"LightGreen"  , COLOR_LIGHTGREEN  },
        {"LightCyan"   , COLOR_LIGHTCYAN   },
        {"LightRed"    , COLOR_LIGHTRED    },
        {"LightMagenta", COLOR_LIGHTMAGENTA},
        {"Yellow"      , COLOR_YELLOW      },
        {"White"       , COLOR_WHITE       },
    };
    ULONG i;

    if (_stricmp(ColorText, "Default") == 0)
        return MachDefaultTextColor;

    for (i = 0; i < sizeof(Colors)/sizeof(Colors[0]); ++i)
    {
        if (_stricmp(ColorText, Colors[i].ColorName) == 0)
            return Colors[i].ColorValue;
    }

    return COLOR_BLACK;
}

UCHAR TuiTextToFillStyle(PCSTR FillStyleText)
{
    static const struct
    {
        PCSTR FillStyleName;
        UCHAR FillStyleValue;
    } FillStyles[] =
    {
        {"Light" , LIGHT_FILL },
        {"Medium", MEDIUM_FILL},
        {"Dark"  , DARK_FILL  },
    };
    ULONG i;

    for (i = 0; i < sizeof(FillStyles)/sizeof(FillStyles[0]); ++i)
    {
        if (_stricmp(FillStyleText, FillStyles[i].FillStyleName) == 0)
            return FillStyles[i].FillStyleValue;
    }

    return LIGHT_FILL;
}

VOID TuiFadeInBackdrop(VOID)
{
    PPALETTE_ENTRY TuiFadePalette = NULL;

    if (UiUseSpecialEffects && ! MachVideoIsPaletteFixed())
    {
        TuiFadePalette = (PPALETTE_ENTRY)FrLdrTempAlloc(sizeof(PALETTE_ENTRY) * 64,
                                                        TAG_TUI_PALETTE);

        if (TuiFadePalette != NULL)
        {
            VideoSavePaletteState(TuiFadePalette, 64);
            VideoSetAllColorsToBlack(64);
        }
    }

    // Draw the backdrop and title box
    TuiDrawBackdrop();

    if (UiUseSpecialEffects && ! MachVideoIsPaletteFixed() && TuiFadePalette != NULL)
    {
        VideoFadeIn(TuiFadePalette, 64);
        FrLdrTempFree(TuiFadePalette, TAG_TUI_PALETTE);
    }
}

VOID TuiFadeOut(VOID)
{
    PPALETTE_ENTRY TuiFadePalette = NULL;

    if (UiUseSpecialEffects && ! MachVideoIsPaletteFixed())
    {
        TuiFadePalette = (PPALETTE_ENTRY)FrLdrTempAlloc(sizeof(PALETTE_ENTRY) * 64,
                                                        TAG_TUI_PALETTE);

        if (TuiFadePalette != NULL)
        {
            VideoSavePaletteState(TuiFadePalette, 64);
        }
    }

    if (UiUseSpecialEffects && ! MachVideoIsPaletteFixed() && TuiFadePalette != NULL)
    {
        VideoFadeOut(64);
    }

    MachVideoSetDisplayMode(NULL, FALSE);

    if (UiUseSpecialEffects && ! MachVideoIsPaletteFixed() && TuiFadePalette != NULL)
    {
        VideoRestorePaletteState(TuiFadePalette, 64);
        FrLdrTempFree(TuiFadePalette, TAG_TUI_PALETTE);
    }

}

BOOLEAN TuiEditBox(PCSTR MessageText, PCHAR EditTextBuffer, ULONG Length)
{
    INT        width = 8;
    ULONG    height = 1;
    INT        curline = 0;
    INT        k;
    size_t    i , j;
    INT        x1, x2, y1, y2;
    CHAR    temp[260];
    CHAR    key;
    BOOLEAN    Extended;
    INT        EditBoxLine;
    ULONG    EditBoxStartX, EditBoxEndX;
    INT        EditBoxCursorX;
    ULONG    EditBoxTextLength, EditBoxTextPosition;
    INT        EditBoxTextDisplayIndex;
    BOOLEAN    ReturnCode;
    PVOID    ScreenBuffer;

    // Save the screen contents
    ScreenBuffer = FrLdrTempAlloc(UiScreenWidth * UiScreenHeight * 2,
                                  TAG_TUI_SCREENBUFFER);
    TuiSaveScreen(ScreenBuffer);

    // Find the height
    for (i=0; i<strlen(MessageText); i++)
    {
        if (MessageText[i] == '\n')
            height++;
    }

    // Find the width
    for (i=0,j=0,k=0; i<height; i++)
    {
        while ((MessageText[j] != '\n') && (MessageText[j] != 0))
        {
            j++;
            k++;
        }

        if (k > width)
            width = k;

        k = 0;
        j++;
    }

    // Calculate box area
    x1 = (UiScreenWidth - (width+2))/2;
    x2 = x1 + width + 3;
    y1 = ((UiScreenHeight - height - 2)/2) + 1;
    y2 = y1 + height + 4;

    // Draw the box
    TuiDrawBox(x1, y1, x2, y2, D_VERT, D_HORZ, TRUE, TRUE, ATTR(UiMessageBoxFgColor, UiMessageBoxBgColor));

    // Draw the text
    for (i=0,j=0; i<strlen(MessageText)+1; i++)
    {
        if ((MessageText[i] == '\n') || (MessageText[i] == 0))
        {
            temp[j] = 0;
            j = 0;
            UiDrawText(x1+2, y1+1+curline, temp, ATTR(UiMessageBoxFgColor, UiMessageBoxBgColor));
            curline++;
        }
        else
            temp[j++] = MessageText[i];
    }

    EditBoxTextLength = (ULONG)strlen(EditTextBuffer);
    EditBoxTextLength = min(EditBoxTextLength, Length - 1);
    EditBoxTextPosition = 0;
    EditBoxLine = y2 - 2;
    EditBoxStartX = x1 + 3;
    EditBoxEndX = x2 - 3;

    // Draw the edit box background and the text
    UiFillArea(EditBoxStartX, EditBoxLine, EditBoxEndX, EditBoxLine, ' ', ATTR(UiEditBoxTextColor, UiEditBoxBgColor));
    UiDrawText2(EditBoxStartX, EditBoxLine, EditBoxEndX - EditBoxStartX + 1, EditTextBuffer, ATTR(UiEditBoxTextColor, UiEditBoxBgColor));

    // Show the cursor
    EditBoxCursorX = EditBoxStartX;
    MachVideoSetTextCursorPosition(EditBoxCursorX, EditBoxLine);
    MachVideoHideShowTextCursor(TRUE);

    // Draw status text
    UiDrawStatusText("Press ENTER to continue, or ESC to cancel");

    VideoCopyOffScreenBufferToVRAM();

    //
    // Enter the text. Please keep in mind that the default input mode
    // of the edit boxes is in insertion mode, that is, you can insert
    // text without erasing the existing one.
    //
    for (;;)
    {
        if (MachConsKbHit())
        {
            Extended = FALSE;
            key = MachConsGetCh();
            if (key == KEY_EXTENDED)
            {
                Extended = TRUE;
                key = MachConsGetCh();
            }

            if (key == KEY_ENTER)
            {
                ReturnCode = TRUE;
                break;
            }
            else if (key == KEY_ESC)
            {
                ReturnCode = FALSE;
                break;
            }
            else if (key == KEY_BACKSPACE) // Remove a character
            {
                if ( (EditBoxTextLength > 0) && (EditBoxTextPosition > 0) &&
                     (EditBoxTextPosition <= EditBoxTextLength) )
                {
                    EditBoxTextPosition--;
                    memmove(EditTextBuffer + EditBoxTextPosition,
                            EditTextBuffer + EditBoxTextPosition + 1,
                            EditBoxTextLength - EditBoxTextPosition);
                    EditBoxTextLength--;
                    EditTextBuffer[EditBoxTextLength] = 0;
                }
                else
                {
                    MachBeep();
                }
            }
            else if (Extended && key == KEY_DELETE) // Remove a character
            {
                if ( (EditBoxTextLength > 0) &&
                     (EditBoxTextPosition < EditBoxTextLength) )
                {
                    memmove(EditTextBuffer + EditBoxTextPosition,
                            EditTextBuffer + EditBoxTextPosition + 1,
                            EditBoxTextLength - EditBoxTextPosition);
                    EditBoxTextLength--;
                    EditTextBuffer[EditBoxTextLength] = 0;
                }
                else
                {
                    MachBeep();
                }
            }
            else if (Extended && key == KEY_HOME) // Go to the start of the buffer
            {
                EditBoxTextPosition = 0;
            }
            else if (Extended && key == KEY_END) // Go to the end of the buffer
            {
                EditBoxTextPosition = EditBoxTextLength;
            }
            else if (Extended && key == KEY_RIGHT) // Go right
            {
                if (EditBoxTextPosition < EditBoxTextLength)
                    EditBoxTextPosition++;
                else
                    MachBeep();
            }
            else if (Extended && key == KEY_LEFT) // Go left
            {
                if (EditBoxTextPosition > 0)
                    EditBoxTextPosition--;
                else
                    MachBeep();
            }
            else if (!Extended) // Add this key to the buffer
            {
                if ( (EditBoxTextLength   < Length - 1) &&
                     (EditBoxTextPosition < Length - 1) )
                {
                    memmove(EditTextBuffer + EditBoxTextPosition + 1,
                            EditTextBuffer + EditBoxTextPosition,
                            EditBoxTextLength - EditBoxTextPosition);
                    EditTextBuffer[EditBoxTextPosition] = key;
                    EditBoxTextPosition++;
                    EditBoxTextLength++;
                    EditTextBuffer[EditBoxTextLength] = 0;
                }
                else
                {
                    MachBeep();
                }
            }
            else
            {
                MachBeep();
            }
        }

        // Draw the edit box background
        UiFillArea(EditBoxStartX, EditBoxLine, EditBoxEndX, EditBoxLine, ' ', ATTR(UiEditBoxTextColor, UiEditBoxBgColor));

        // Fill the text in
        if (EditBoxTextPosition > (EditBoxEndX - EditBoxStartX))
        {
            EditBoxTextDisplayIndex = EditBoxTextPosition - (EditBoxEndX - EditBoxStartX);
            EditBoxCursorX = EditBoxEndX;
        }
        else
        {
            EditBoxTextDisplayIndex = 0;
            EditBoxCursorX = EditBoxStartX + EditBoxTextPosition;
        }
        UiDrawText2(EditBoxStartX, EditBoxLine, EditBoxEndX - EditBoxStartX + 1, &EditTextBuffer[EditBoxTextDisplayIndex], ATTR(UiEditBoxTextColor, UiEditBoxBgColor));

        // Move the cursor
        MachVideoSetTextCursorPosition(EditBoxCursorX, EditBoxLine);

        TuiUpdateDateTime();

        VideoCopyOffScreenBufferToVRAM();

        MachHwIdle();
    }

    // Hide the cursor again
    MachVideoHideShowTextCursor(FALSE);

    // Restore the screen contents
    TuiRestoreScreen(ScreenBuffer);
    FrLdrTempFree(ScreenBuffer, TAG_TUI_SCREENBUFFER);

    return ReturnCode;
}

const UIVTBL TuiVtbl =
{
    TuiInitialize,
    TuiUnInitialize,
    TuiDrawBackdrop,
    TuiFillArea,
    TuiDrawShadow,
    TuiDrawBox,
    TuiDrawText,
    TuiDrawText2,
    TuiDrawCenteredText,
    TuiDrawStatusText,
    TuiUpdateDateTime,
    TuiMessageBox,
    TuiMessageBoxCritical,
    TuiDrawProgressBarCenter,
    TuiDrawProgressBar,
    TuiEditBox,
    TuiTextToColor,
    TuiTextToFillStyle,
    TuiFadeInBackdrop,
    TuiFadeOut,
    TuiDisplayMenu,
    TuiDrawMenu,
};

#endif // _M_ARM
