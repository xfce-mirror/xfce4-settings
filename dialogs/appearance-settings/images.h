/* $Id$ */
/*
 *  Copyright (c) 2008 Nick Schermer <nick@xfce.org>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU Library General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

static const gchar *rgba_image_none_xpm[] = 
{
    "18 15 1 1",
    " 	c None",
    "                  ",
    "                  ",
    "                  ",
    "                  ",
    "                  ",
    "                  ",
    "                  ",
    "                  ",
    "                  ",
    "                  ",
    "                  ",
    "                  ",
    "                  ",
    "                  ",
    "                  "
};

static const gchar *rgba_image_rgb_xpm[] = 
{
    "18 15 4 1",
    " 	c None",
    ".	c #FF0000",
    "+	c #00FF00",
    "@	c #0000FF",
    ".....+++++@@@@@   ",
    ".....+++++@@@@@   ",
    ".....+++++@@@@@   ",
    ".....+++++@@@@@   ",
    ".....+++++@@@@@   ",
    ".....+++++@@@@@   ",
    ".....+++++@@@@@   ",
    ".....+++++@@@@@   ",
    ".....+++++@@@@@   ",
    ".....+++++@@@@@   ",
    ".....+++++@@@@@   ",
    ".....+++++@@@@@   ",
    ".....+++++@@@@@   ",
    ".....+++++@@@@@   ",
    ".....+++++@@@@@   "
};

static const gchar *rgba_image_bgr_xpm[] = 
{
    "18 15 4 1",
    " 	c None",
    ".	c #0000FF",
    "+	c #00FF00",
    "@	c #FF0000",
    ".....+++++@@@@@   ",
    ".....+++++@@@@@   ",
    ".....+++++@@@@@   ",
    ".....+++++@@@@@   ",
    ".....+++++@@@@@   ",
    ".....+++++@@@@@   ",
    ".....+++++@@@@@   ",
    ".....+++++@@@@@   ",
    ".....+++++@@@@@   ",
    ".....+++++@@@@@   ",
    ".....+++++@@@@@   ",
    ".....+++++@@@@@   ",
    ".....+++++@@@@@   ",
    ".....+++++@@@@@   ",
    ".....+++++@@@@@   "
};

static const gchar *rgba_image_vrgb_xpm[] = 
{
    "18 15 4 1",
    " 	c None",
    ".	c #FF0000",
    "+	c #00FF00",
    "@	c #0000FF",
    "...............   ",
    "...............   ",
    "...............   ",
    "...............   ",
    "...............   ",
    "+++++++++++++++   ",
    "+++++++++++++++   ",
    "+++++++++++++++   ",
    "+++++++++++++++   ",
    "+++++++++++++++   ",
    "@@@@@@@@@@@@@@@   ",
    "@@@@@@@@@@@@@@@   ",
    "@@@@@@@@@@@@@@@   ",
    "@@@@@@@@@@@@@@@   ",
    "@@@@@@@@@@@@@@@   "
};

static const gchar *rgba_image_vbgr_xpm[] = 
{
    "18 15 4 1",
    " 	c None",
    ".	c #0000FF",
    "+	c #00FF00",
    "@	c #FF0000",
    "...............   ",
    "...............   ",
    "...............   ",
    "...............   ",
    "...............   ",
    "+++++++++++++++   ",
    "+++++++++++++++   ",
    "+++++++++++++++   ",
    "+++++++++++++++   ",
    "+++++++++++++++   ",
    "@@@@@@@@@@@@@@@   ",
    "@@@@@@@@@@@@@@@   ",
    "@@@@@@@@@@@@@@@   ",
    "@@@@@@@@@@@@@@@   ",
    "@@@@@@@@@@@@@@@   "
};
