/*
 *   
 *
 * Copyright  1990-2007 Sun Microsystems, Inc. All Rights Reserved.
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER
 * 
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License version
 * 2 only, as published by the Free Software Foundation.
 * 
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * General Public License version 2 for more details (a copy is
 * included at /legal/license.txt).
 * 
 * You should have received a copy of the GNU General Public License
 * version 2 along with this work; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
 * 02110-1301 USA
 * 
 * Please contact Sun Microsystems, Inc., 4150 Network Circle, Santa
 * Clara, CA 95054 or visit www.sun.com if you need additional
 * information or have any questions.
 */

#include <kni.h>
#include <midp_logging.h>
#include <lfjport_export.h>

#include <gxj_putpixel.h>
#include <gxj_screen_buffer.h>

#include "SDL.h"
#include "midp_constants_data.h"

/**
 * @file
 * Additional porting API for Java Widgets based port of abstract
 * command manager.
 */

#define SDL_FULLWIDTH   FULLWIDTH
#define SDL_FULLHEIGHT  FULLHEIGHT
void InitGP2XKeys(void);
int  InitAudioSubsystem(void) { return 0; }
void FinalizeAudioSubsystem(void) {}

SDL_Surface     *Native_SDL_Screen, *Native_SDL_HScreen, *Native_SDL_VScreen;
static jboolean  Native_SDL_ScreenOrientation;
static jboolean  Native_SDL_Fullscreen;
static int       OriginalOrientation, OriginalWidth, OriginalHeight;

/**
 *
 * Generic Screen Buffer required by putpixel library.
 *
 */
gxj_screen_buffer gxj_system_screen_buffer;

/**
 * Initializes the lfjport_ui_ native resources.
 *
 * @return <tt>0</tt> upon successful initialization, or
 *         <tt>other value</tt> otherwise
 */
int lfjport_ui_init() {
    if (SDL_Init(SDL_INIT_JOYSTICK|SDL_INIT_VIDEO) != 0) 
        return(-1);

    SDL_ShowCursor(SDL_DISABLE);
    InitAudioSubsystem();
    if (SDL_NumJoysticks() > 0) SDL_JoystickOpen(0);
    OriginalOrientation = 0;
    if (getenv("J2ME_GP2X_REVERSE") != NULL) OriginalOrientation = 1;
    OriginalWidth = OriginalOrientation ? SDL_FULLHEIGHT : SDL_FULLWIDTH;
    OriginalHeight = OriginalOrientation ? SDL_FULLWIDTH : SDL_FULLHEIGHT;
    InitGP2XKeys();
    Native_SDL_Screen = SDL_SetVideoMode(SDL_FULLWIDTH, SDL_FULLHEIGHT, 16, 0);
    if (Native_SDL_Screen == NULL) return(-2);
    Native_SDL_HScreen = SDL_CreateRGBSurface(SDL_SWSURFACE, OriginalWidth, OriginalHeight, 16, 0x0000F800, 0x000007E0, 0x0000001F, 0x00000000);
    if (Native_SDL_HScreen == NULL) return(-3);
    Native_SDL_VScreen = SDL_CreateRGBSurface(SDL_SWSURFACE, OriginalHeight, OriginalWidth, 16, 0x0000F800, 0x000007E0, 0x0000001F, 0x00000000);
    if (Native_SDL_VScreen == NULL) return(-4);
    SDL_LockSurface(Native_SDL_HScreen);
    SDL_LockSurface(Native_SDL_VScreen);
    gxj_system_screen_buffer.width = OriginalWidth;
    gxj_system_screen_buffer.height = OriginalHeight;
    gxj_system_screen_buffer.alphaData = NULL;
    gxj_system_screen_buffer.pixelData = Native_SDL_HScreen->pixels;
    Native_SDL_ScreenOrientation = KNI_FALSE;
    Native_SDL_Fullscreen = KNI_FALSE;
    atexit(SDL_Quit);

    return 0;
}

/**
 * Finalize the lfjport_ui_ native resources.
 */
void lfjport_ui_finalize() {
    FinalizeAudioSubsystem();
    SDL_Quit();
}

static void VideoCopyRotate(unsigned short *Buffer, unsigned short *Video)
{ int x, y, bufinc, rowinc;
  Buffer = &Buffer[(SDL_FULLWIDTH-1)*SDL_FULLHEIGHT];
  bufinc = SDL_FULLHEIGHT;
  rowinc = ((SDL_FULLWIDTH-1)*SDL_FULLHEIGHT)+1;
  for(y=0; y<SDL_FULLHEIGHT; y++, Buffer+=rowinc+bufinc)
     for(x=0; x<SDL_FULLWIDTH; x++, Video++, Buffer-=bufinc)
          *Video = *Buffer;
}

/**
 * Bridge function to request a repaint 
 * of the area specified.
 *
 * @param x1 top-left x coordinate of the area to refresh
 * @param y1 top-left y coordinate of the area to refresh
 * @param x2 bottom-right x coordinate of the area to refresh
 * @param y2 bottom-right y coordinate of the area to refresh
 */
void lfjport_refresh(int hardwareId, int x1, int y1, int x2, int y2)
{ unsigned short *Video, *Buffer;
  SDL_UnlockSurface(Native_SDL_HScreen);
  SDL_UnlockSurface(Native_SDL_VScreen);
  if (Native_SDL_ScreenOrientation) 
     { if (OriginalOrientation == 1) SDL_BlitSurface(Native_SDL_VScreen, NULL, Native_SDL_Screen, NULL);
       else { SDL_LockSurface(Native_SDL_Screen);
              Video = (unsigned short *)Native_SDL_Screen->pixels;
              Buffer = (unsigned short *)Native_SDL_VScreen->pixels;
              VideoCopyRotate(Buffer, Video);
              SDL_UnlockSurface(Native_SDL_Screen);
            }
     }
  else { if (OriginalOrientation == 0) SDL_BlitSurface(Native_SDL_HScreen, NULL, Native_SDL_Screen, NULL);
         else { SDL_LockSurface(Native_SDL_Screen);
                Video = (unsigned short *)Native_SDL_Screen->pixels;
                Buffer = (unsigned short *)Native_SDL_HScreen->pixels;
                VideoCopyRotate(Buffer, Video);
                SDL_UnlockSurface(Native_SDL_Screen);
              }
       }
  SDL_UpdateRect(Native_SDL_Screen, 0,0,0,0);
  SDL_LockSurface(Native_SDL_HScreen);
  SDL_LockSurface(Native_SDL_VScreen);

    /* Suppress unused parameter warnings */
    (void)hardwareId;
    (void)x1;
    (void)y1;
    (void)x2;
    (void)y2;

}

/**
 * Porting API function to update scroll bar.
 *
 * @param scrollPosition current scroll position
 * @param scrollProportion maximum scroll position
 * @return status of this call
 */
int lfjport_set_vertical_scroll(int scrollPosition, int scrollProportion)
{
    REPORT_CALL_TRACE2(LC_HIGHUI, "LF:STUB:lfjport_ui_setVerticalScroll(%3d, %3d)\n",
                       scrollPosition, scrollProportion);

    /* Suppress unused parameter warnings */
    (void)scrollPosition;
    (void)scrollProportion;

    return 0;
}


/**
 * Turn on or off the full screen mode
 *
 * @param mode true for full screen mode
 *             false for normal
 */
void lfjport_set_fullscreen_mode(int hardwareId, jboolean mode) {
    Native_SDL_Fullscreen = mode;

    /* Suppress unused parameter warnings */
    (void)hardwareId;

}

/**
 * Resets native resources when foreground is gained by a new display.
 */
void lfjport_gained_foreground(int hardwareId) {
    REPORT_CALL_TRACE(LC_HIGHUI, "LF:STUB:gainedForeground()\n");
    (void)hardwareId;
}

/**
 * Change screen orientation flag
 */
jboolean lfjport_reverse_orientation(int hardwareId) {
    (void)hardwareId;

  Native_SDL_ScreenOrientation = !Native_SDL_ScreenOrientation;
  gxj_system_screen_buffer.pixelData = Native_SDL_ScreenOrientation ? Native_SDL_VScreen->pixels : Native_SDL_HScreen->pixels;
  gxj_system_screen_buffer.width = Native_SDL_ScreenOrientation ? OriginalHeight : OriginalWidth;
  gxj_system_screen_buffer.height = Native_SDL_ScreenOrientation ? OriginalWidth : OriginalHeight;
  return Native_SDL_ScreenOrientation;
}

/**
 * Handle clamshell event
 */
void lfjport_handle_clamshell_event() {
    // not implemented
}

/**
 * Change screen orientation flag
 */
jboolean lfjport_get_reverse_orientation(int hardwareId) {
    (void)hardwareId;
    return Native_SDL_ScreenOrientation;        
}

/**
 * Return screen width
 */
int lfjport_get_screen_width(int hardwareId) {
    (void)hardwareId;

    return Native_SDL_ScreenOrientation ? OriginalHeight : OriginalWidth;
}

/**
 * Return screen height
 */
int lfjport_get_screen_height(int hardwareId) {
    (void)hardwareId;

    return Native_SDL_ScreenOrientation ? OriginalWidth : OriginalHeight;
}

/**
 * Flushes the offscreen buffer directly to the device screen.
 * The size of the buffer flushed is defined by offscreen buffer width
 * and passed in height. 
 * Offscreen_buffer must be aligned to the top-left of the screen and
 * its width must be the same as the device screen width.
 * @param graphics The Graphics handle associated with the screen.
 * @param offscreen_buffer The ImageData handle associated with 
 *                         the offscreen buffer to be flushed
 * @param h The height to be flushed
 * @return KNI_TRUE if direct_flush was successful, KNI_FALSE - otherwise
 */
jboolean lfjport_direct_flush(int hardwareId, const java_graphics *g, 
		  	      const java_imagedata *offscreen_buffer, int h) {
    // FIXME: this looks important!
    REPORT_CALL_TRACE4(LC_HIGHUI, "LF:STUB:lfjport_direct_flush(%d, %p, %p, %d)\n",
                       hardwareId, g, offscreen_buffer, h);

    *(int*)0 = 123;

  (void)hardwareId;
  (void)g;
  (void)offscreen_buffer;
  (void)h;
  return KNI_FALSE;
}

/**
 * Check if native softbutton is supported on platform
 * 
 * @return KNI_TRUE if native softbutton is supported, KNI_FALSE - otherwise
 */
jboolean lfjport_is_native_softbutton_layer_supported() {
    return KNI_FALSE;
}

/**
 * Request platform to draw a label in the soft button layer.
 * 
 * @param label Label to draw (UTF16)
 * @param len Length of the lable (0 will cause removal of current label)
 * @param index Index of the soft button in the soft button bar.
 */
void lfjport_set_softbutton_label_on_native_layer (unsigned short *label, 
                                                 int len, 
                                                 int index) {
    (void)label;
    (void)len;
    (void)index;
    // Not implemented
}

/**
 * get currently enabled hardware display id
 */
int lfjport_get_current_hardwareId() {
    return  0;  // just one display  is supported
}

/** 
 * Get display device name by id
 */
char * lfjport_get_display_name(int hardwareId) {
    (void)hardwareId;
    // Not implemented
    return 0;
}


/**
 * Check if the display device is primary
 */
jboolean lfjport_is_display_primary(int hardwareId) {
    (void)hardwareId;
    // Not implemented
    return KNI_TRUE;
}
/**
 * Check if the display device is build-in
 */
jboolean lfjport_is_display_buildin(int hardwareId) {
    (void)hardwareId;
    // Not implemented
    return KNI_TRUE;
}
/**
 * Check if the display device supports pointer events
 */
jboolean lfjport_is_display_pen_supported(int hardwareId) {
    (void)hardwareId;
    // Not implemented
    return KNI_FALSE;
}
/**
 * Check if the display device supports pointer motion  events
 */
jboolean lfjport_is_display_pen_motion_supported(int hardwareId) {
    (void)hardwareId;
    // Not implemented
    return KNI_FALSE;
}
/**
 * Get display device capabilities
 */
int lfjport_get_display_capabilities(int hardwareId) {
    (void)hardwareId;
    return 0xffff;
}


static jint display_device_ids[] = {0};
  
jint* lfjport_get_display_device_ids(jint* n) {  
    *n = 1; 
    return display_device_ids;
}

/**
 * Notify the display device state has been changed
 */
void lfjport_display_device_state_changed(int hardwareId, int state) {
  (void)hardwareId;
  (void)state;
}

