/*
 * File      : dc_blend.c
 * This file is part of RT-Thread GUI
 * COPYRIGHT (C) 2006 - 2013, RT-Thread Development Team
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
 *
 * Change Logs:
 * Date           Author       Notes
 * 2013-10-04     Bernard      porting SDL software render to RT-Thread GUI
 */

/*
  Simple DirectMedia Layer
  Copyright (C) 1997-2013 Sam Lantinga <slouken@libsdl.org>

  This software is provided 'as-is', without any express or implied
  warranty.  In no event will the authors be held liable for any damages
  arising from the use of this software.

  Permission is granted to anyone to use this software for any purpose,
  including commercial applications, and to alter it and redistribute it
  freely, subject to the following restrictions:

  1. The origin of this software must not be misrepresented; you must not
     claim that you wrote the original software. If you use this software
     in a product, an acknowledgment in the product documentation would be
     appreciated but is not required.
  2. Altered source versions must be plainly marked as such, and must not be
     misrepresented as being the original software.
  3. This notice may not be removed or altered from any source distribution.
*/
#include <rtgui/dc.h>
#include <rtgui/dc_draw.h>
#include <rtgui/color.h>
#include <string.h>

#include <math.h>

#define hw_driver               (rtgui_graphic_driver_get_default())
#define _int_swap(x, y)         do {x ^= y; y ^= x; x ^= y;} while (0)

rt_inline rt_uint8_t _dc_get_bits_per_pixel(struct rtgui_dc* dc)
{
	rt_uint8_t bits_per_pixel = 0;

	if (dc->type == RTGUI_DC_HW || dc->type == RTGUI_DC_CLIENT)
		bits_per_pixel = hw_driver->bits_per_pixel;
	else if (dc->type == RTGUI_DC_BUFFER)
	{
		struct rtgui_dc_buffer *buffer = (struct rtgui_dc_buffer*)dc;
		
		bits_per_pixel = rtgui_color_get_bits(buffer->pixel_format);
	}

	return bits_per_pixel;
}

rt_inline rt_uint16_t _dc_get_pitch(struct rtgui_dc* dc)
{
	rt_uint16_t pitch = 0;

	if (dc->type == RTGUI_DC_HW || dc->type == RTGUI_DC_CLIENT)
		pitch = hw_driver->pitch;
	else if (dc->type == RTGUI_DC_BUFFER)
	{
		struct rtgui_dc_buffer *dc_buffer;

		dc_buffer = (struct rtgui_dc_buffer*)dc;
		pitch = dc_buffer->pitch;
	}

	return pitch;
}

rt_inline rt_uint8_t* _dc_get_pixel(struct rtgui_dc* dc, int x, int y)
{
	rt_uint8_t *pixel = RT_NULL;

	if ((dc->type == RTGUI_DC_HW) || (dc->type == RTGUI_DC_CLIENT))
	{
		pixel = (rt_uint8_t*)(hw_driver->framebuffer);
		RT_ASSERT(pixel != RT_NULL);

		pixel = pixel + y * hw_driver->pitch + x * (_UI_BITBYTES(hw_driver->bits_per_pixel));
	}
	else if (dc->type == RTGUI_DC_BUFFER)
	{
		struct rtgui_dc_buffer *dc_buffer;

		dc_buffer = (struct rtgui_dc_buffer*)dc;

		pixel = dc_buffer->pixel + y * dc_buffer->pitch + 
			x * rtgui_color_get_bpp(dc_buffer->pixel_format);
	}

	return pixel;
}

/* Use the Cohen-Sutherland algorithm for line clipping */
#define CODE_BOTTOM 1
#define CODE_TOP    2
#define CODE_LEFT   4
#define CODE_RIGHT  8

static int ComputeOutCode(const rtgui_rect_t * rect, int x, int y)
{
    int code = 0;
    if (y < rect->y1) 
	{
        code |= CODE_TOP;
    } 
	else if (y > rect->y2) 
	{
        code |= CODE_BOTTOM;
    }

	if (x < rect->x1) 
	{
        code |= CODE_LEFT;
    } 
	else if (x > rect->x2) 
	{
        code |= CODE_RIGHT;
    }

	return code;
}

static rt_bool_t _intersect_rect_line(const rtgui_rect_t* rect, int *X1, int *Y1, int *X2,
                         int *Y2)
{
    int x = 0;
    int y = 0;
    int x1, y1;
    int x2, y2;
    int rectx1;
    int recty1;
    int rectx2;
    int recty2;
    int outcode1, outcode2;

    x1 = *X1;
    y1 = *Y1;
    x2 = *X2;
    y2 = *Y2;
    rectx1 = rect->x1;
    recty1 = rect->y1;
    rectx2 = rect->x2 - 1;
    recty2 = rect->y2 - 1;

    /* Check to see if entire line is inside rect */
    if (x1 >= rectx1 && x1 <= rectx2 && x2 >= rectx1 && x2 <= rectx2 &&
        y1 >= recty1 && y1 <= recty2 && y2 >= recty1 && y2 <= recty2) {
        return RT_TRUE;
    }

    /* Check to see if entire line is to one side of rect */
    if ((x1 < rectx1 && x2 < rectx1) || (x1 > rectx2 && x2 > rectx2) ||
        (y1 < recty1 && y2 < recty1) || (y1 > recty2 && y2 > recty2)) {
        return RT_FALSE;
    }

    if (y1 == y2) {
        /* Horizontal line, easy to clip */
        if (x1 < rectx1) {
            *X1 = rectx1;
        } else if (x1 > rectx2) {
            *X1 = rectx2;
        }
        if (x2 < rectx1) {
            *X2 = rectx1;
        } else if (x2 > rectx2) {
            *X2 = rectx2;
        }
        return RT_TRUE;
    }

    if (x1 == x2) {
        /* Vertical line, easy to clip */
        if (y1 < recty1) {
            *Y1 = recty1;
        } else if (y1 > recty2) {
            *Y1 = recty2;
        }
        if (y2 < recty1) {
            *Y2 = recty1;
        } else if (y2 > recty2) {
            *Y2 = recty2;
        }
        return RT_TRUE;
    }

    /* More complicated Cohen-Sutherland algorithm */
    outcode1 = ComputeOutCode(rect, x1, y1);
    outcode2 = ComputeOutCode(rect, x2, y2);
    while (outcode1 || outcode2) {
        if (outcode1 & outcode2) {
            return RT_FALSE;
        }

        if (outcode1) {
            if (outcode1 & CODE_TOP) {
                y = recty1;
                x = x1 + ((x2 - x1) * (y - y1)) / (y2 - y1);
            } else if (outcode1 & CODE_BOTTOM) {
                y = recty2;
                x = x1 + ((x2 - x1) * (y - y1)) / (y2 - y1);
            } else if (outcode1 & CODE_LEFT) {
                x = rectx1;
                y = y1 + ((y2 - y1) * (x - x1)) / (x2 - x1);
            } else if (outcode1 & CODE_RIGHT) {
                x = rectx2;
                y = y1 + ((y2 - y1) * (x - x1)) / (x2 - x1);
            }
            x1 = x;
            y1 = y;
            outcode1 = ComputeOutCode(rect, x, y);
        } else {
            if (outcode2 & CODE_TOP) {
                y = recty1;
                x = x1 + ((x2 - x1) * (y - y1)) / (y2 - y1);
            } else if (outcode2 & CODE_BOTTOM) {
                y = recty2;
                x = x1 + ((x2 - x1) * (y - y1)) / (y2 - y1);
            } else if (outcode2 & CODE_LEFT) {
                x = rectx1;
                y = y1 + ((y2 - y1) * (x - x1)) / (x2 - x1);
            } else if (outcode2 & CODE_RIGHT) {
                x = rectx2;
                y = y1 + ((y2 - y1) * (x - x1)) / (x2 - x1);
            }
            x2 = x;
            y2 = y;
            outcode2 = ComputeOutCode(rect, x, y);
        }
    }

    *X1 = x1;
    *Y1 = y1;
    *X2 = x2;
    *Y2 = y2;

    return RT_TRUE;
}

static void
_dc_draw_line1(struct rtgui_dc * dst, int x1, int y1, int x2, int y2, rtgui_color_t color,
              rt_bool_t draw_end)
{
    if (y1 == y2) {
        int length;
        int pitch = _dc_get_pitch(dst);
        rt_uint8_t *pixel;
        if (x1 <= x2) {
            pixel = (rt_uint8_t *)_dc_get_pixel(dst, x1, y1);
            length = draw_end ? (x2-x1+1) : (x2-x1);
        } else {
            pixel = (rt_uint8_t *)_dc_get_pixel(dst, x2, y1);
            if (!draw_end) {
                ++pixel;
            }
            length = draw_end ? (x1-x2+1) : (x1-x2);
        }
        rt_memset(pixel, color, length);
    } else if (x1 == x2) {
        VLINE(rt_uint8_t, DRAW_FASTSETPIXEL1, draw_end);
    } else if (ABS(x1 - x2) == ABS(y1 - y2)) {
        DLINE(rt_uint8_t, DRAW_FASTSETPIXEL1, draw_end);
    } else {
        BLINE(x1, y1, x2, y2, DRAW_FASTSETPIXELXY1, draw_end);
    }
}

static void
_dc_draw_line2(struct rtgui_dc * dst, int x1, int y1, int x2, int y2, rtgui_color_t c,
              rt_bool_t draw_end)
{
	rt_uint16_t color;

	if (rtgui_dc_get_pixel_format(dst) == RTGRAPHIC_PIXEL_FORMAT_RGB565)
		color = rtgui_color_to_565(c);
	else 
		color = rtgui_color_to_565p(c);

    if (y1 == y2) {
        HLINE(rt_uint16_t, DRAW_FASTSETPIXEL2, draw_end);
    } else if (x1 == x2) {
        VLINE(rt_uint16_t, DRAW_FASTSETPIXEL2, draw_end);
    } else if (ABS(x1 - x2) == ABS(y1 - y2)) {
        DLINE(rt_uint16_t, DRAW_FASTSETPIXEL2, draw_end);
    } else {
        rt_uint8_t _r, _g, _b, _a;

		_r = RTGUI_RGB_R(c);
		_g = RTGUI_RGB_G(c);
		_b = RTGUI_RGB_B(c);
		_a = RTGUI_RGB_A(c);
		
		if (rtgui_dc_get_pixel_format(dst) == RTGRAPHIC_PIXEL_FORMAT_RGB565)
		{
            AALINE(x1, y1, x2, y2,
                   DRAW_FASTSETPIXELXY2, DRAW_SETPIXELXY_BLEND_RGB565,
                   draw_end);
		}
		else if (rtgui_dc_get_pixel_format(dst) == RTGRAPHIC_PIXEL_FORMAT_BGR565)
		{
			AALINE(x1, y1, x2, y2,
				DRAW_FASTSETPIXELXY2, DRAW_SETPIXELXY_BLEND_BGR565,
				draw_end);
        }
    }
}

static void
_dc_draw_line4(struct rtgui_dc * dst, int x1, int y1, int x2, int y2, rtgui_color_t color,
              rt_bool_t draw_end)
{
    if (y1 == y2) {
        HLINE(rt_uint32_t, DRAW_FASTSETPIXEL4, draw_end);
    } else if (x1 == x2) {
        VLINE(rt_uint32_t, DRAW_FASTSETPIXEL4, draw_end);
    } else if (ABS(x1 - x2) == ABS(y1 - y2)) {
        DLINE(rt_uint32_t, DRAW_FASTSETPIXEL4, draw_end);
    } else {
        rt_uint8_t _r, _g, _b, _a;
		_r = RTGUI_RGB_R(color);
		_g = RTGUI_RGB_G(color);
		_b = RTGUI_RGB_B(color);
		_a = RTGUI_RGB_A(color);

		if (rtgui_dc_get_pixel_format(dst) == RTGRAPHIC_PIXEL_FORMAT_RGB888)
		{
            AALINE(x1, y1, x2, y2,
				DRAW_FASTSETPIXELXY4, DRAW_SETPIXELXY_BLEND_RGB888,
				draw_end);
        }
		else if (rtgui_dc_get_pixel_format(dst) == RTGRAPHIC_PIXEL_FORMAT_ARGB888)
		{
            AALINE(x1, y1, x2, y2,
				DRAW_FASTSETPIXELXY4, DRAW_SETPIXELXY_BLEND_ARGB8888,
				draw_end);
		}
    }
}

typedef void (*DrawLineFunc) (struct rtgui_dc * dst,
                              int x1, int y1, int x2, int y2,
                              rtgui_color_t color, rt_bool_t draw_end);

static DrawLineFunc
_dc_calc_draw_line_func(int bpp)
{
	switch (bpp) {
    case 1:
        return _dc_draw_line1;
    case 2:
        return _dc_draw_line2;
    case 4:
        return _dc_draw_line4;
    }

	return NULL;
}

void rtgui_dc_draw_aa_line(struct rtgui_dc * dst, int x1, int y1, int x2, int y2)
{
	int bpp;
    DrawLineFunc func;
	rtgui_color_t color;
	rtgui_widget_t *owner;

	RT_ASSERT(dst != RT_NULL);
	if (!rtgui_dc_get_visible(dst)) return;
	/* we do not support pixel DC */
	if (_dc_get_pixel(dst, 0, 0) == RT_NULL) return ; 

	color = rtgui_dc_get_gc(dst)->foreground;
	bpp = _dc_get_bits_per_pixel(dst); if (bpp < 8) return;

    func = _dc_calc_draw_line_func(bpp/8);
    if (!func)
	{
        rt_kprintf("dc_draw_line(): Unsupported pixel format\n");
		return;
    }
	
	/* perform clip */
	if (dst->type == RTGUI_DC_CLIENT)
	{
		/* get owner */
		owner = RTGUI_CONTAINER_OF(dst, struct rtgui_widget, dc_type);
		
		x1 = x1 + owner->extent.x1;
		x2 = x2 + owner->extent.x1;
		y1 = y1 + owner->extent.y1;
		y2 = y2 + owner->extent.y1;
		if (y1 > y2) _int_swap(y1, y2);
		if (x1 > x2) _int_swap(x1, x2);

		if (owner->clip.data == RT_NULL)
		{
			rtgui_rect_t *prect;

			/* no clip */
			prect = &(owner->clip.extents);

			/* calculate line intersect */
			if (_intersect_rect_line(prect, &x1, &y1, &x2, &y2) == RT_FALSE) return ;

			/* draw line */
			func(dst, x1, y1, x2, y2, color, RT_FALSE);
		}
		else
		{
			register rt_base_t index;

			for (index = 0; index < rtgui_region_num_rects(&(owner->clip)); index ++)
	    	{
		        rtgui_rect_t *prect;
		        int draw_x1, draw_x2;
		        int draw_y1, draw_y2;

		        prect = ((rtgui_rect_t *)(owner->clip.data + index + 1));
		        draw_x1 = x1; draw_x2 = x2;
		        draw_y1 = y1; draw_y2 = y2;

				/* calculate line intersect */
				if (_intersect_rect_line(prect, &draw_x1, &draw_y1, &draw_x2, &draw_y2) == RT_FALSE) continue;

		        /* draw line */
				func(dst, draw_x1, draw_y1, draw_x2, draw_y2, color, RT_FALSE);
		    }
		}
	}
	else 
	{
		if (dst->type == RTGUI_DC_HW)
		{
			struct rtgui_dc_hw *dc_hw = (struct rtgui_dc_hw*)dst;

			owner = dc_hw->owner;
			x1 = x1 + owner->extent.x1;
			x2 = x2 + owner->extent.x1;
			y1 = y1 + owner->extent.y1;
			y2 = y2 + owner->extent.y1;
			if (y1 > y2) _int_swap(y1, y2);
			if (x1 > x2) _int_swap(x1, x2);
		}

		func(dst, x1, y1, x2, y2, color, RT_FALSE);
	}
}

void rtgui_dc_draw_aa_lines(struct rtgui_dc * dst, const struct rtgui_point * points, int count)
{
    int i;
    int x1, y1;
    int x2, y2;
    rt_bool_t draw_end;
    DrawLineFunc func;
	int bpp;
	rtgui_color_t color;
	rtgui_widget_t *owner = RT_NULL;

	RT_ASSERT(dst);
	if (!rtgui_dc_get_visible(dst)) return;
	/* we do not support pixel DC */
	if (_dc_get_pixel(dst, 0, 0) == RT_NULL) return ; 

	color = rtgui_dc_get_gc(dst)->foreground;
	bpp = _dc_get_bits_per_pixel(dst); if (bpp < 8) return;

    func = _dc_calc_draw_line_func(bpp/8);
    if (!func) {
        rt_kprintf("dc_draw_lines(): Unsupported pixel format\n");
		return;
    }

	/* perform cliping */
	if (dst->type == RTGUI_DC_CLIENT)
	{
		/* get owner */
		owner = RTGUI_CONTAINER_OF(dst, struct rtgui_widget, dc_type);
	}
	else if (dst->type == RTGUI_DC_HW)
	{
		struct rtgui_dc_hw* dc_hw = (struct rtgui_dc_hw*)dst;

		owner = dc_hw->owner;
	}
	
    for (i = 1; i < count; ++i) {
        x1 = points[i-1].x;
        y1 = points[i-1].y;
        x2 = points[i].x;
        y2 = points[i].y;

        /* Draw the end if it was clipped */
        draw_end = (x2 != points[i].x || y2 != points[i].y);

		if (dst->type == RTGUI_DC_CLIENT)
		{
			x1 = x1 + owner->extent.x1;
			x2 = x2 + owner->extent.x1;
			y1 = y1 + owner->extent.y1;
			y2 = y2 + owner->extent.y1;
			if (y1 > y2) _int_swap(y1, y2);
			if (x1 > x2) _int_swap(x1, x2);

			if (owner->clip.data == RT_NULL)
			{
				rtgui_rect_t *prect;
		
				/* no clip */
				prect = &(owner->clip.extents);
			
				/* calculate line intersect */
				if (_intersect_rect_line(prect, &x1, &y1, &x2, &y2) == RT_FALSE) return;
		
				/* draw line */
				func(dst, x1, y1, x2, y2, color, draw_end);
			}
			else
			{
				register rt_base_t index;
		
				for (index = 0; index < rtgui_region_num_rects(&(owner->clip)); index ++)
				{
					rtgui_rect_t *prect;
					int draw_x1, draw_x2;
					int draw_y1, draw_y2;
		
					prect = ((rtgui_rect_t *)(owner->clip.data + index + 1));
					draw_x1 = x1; draw_x2 = x2;
					draw_y1 = y1; draw_y2 = y2;
		
					/* calculate line intersect */
					if (_intersect_rect_line(prect, &draw_x1, &draw_y1, &draw_x2, &draw_y2) == RT_FALSE) continue;

					/* draw line */
					func(dst, draw_x1, draw_y1, draw_x2, draw_y2, color, draw_end);
				}
			}
		}
		else
		{
			if (dst->type == RTGUI_DC_HW)
			{
				x1 = x1 + owner->extent.x1;
				x2 = x2 + owner->extent.x1;
				y1 = y1 + owner->extent.y1;
				y2 = y2 + owner->extent.y1;
				if (y1 > y2) _int_swap(y1, y2);
				if (x1 > x2) _int_swap(x1, x2);
			}
			func(dst, x1, y1, x2, y2, color, draw_end);
		}
    }

    if (points[0].x != points[count-1].x || points[0].y != points[count-1].y) 
	{
        rtgui_dc_draw_point(dst, points[count-1].x, points[count-1].y);
    }

    return ;
}

static int
_dc_blend_point_rgb565(struct rtgui_dc * dst, int x, int y, enum RTGUI_BLENDMODE blendMode, rt_uint8_t r,
                      rt_uint8_t g, rt_uint8_t b, rt_uint8_t a)
{
    unsigned inva = 0xff - a;

    switch (blendMode) {
    case RTGUI_BLENDMODE_BLEND:
        DRAW_SETPIXELXY_BLEND_RGB565(x, y);
        break;
    case RTGUI_BLENDMODE_ADD:
        DRAW_SETPIXELXY_ADD_RGB565(x, y);
        break;
    case RTGUI_BLENDMODE_MOD:
        DRAW_SETPIXELXY_MOD_RGB565(x, y);
        break;
    default:
        DRAW_SETPIXELXY_RGB565(x, y);
        break;
    }
    return 0;
}

static int
_dc_blend_point_bgr565(struct rtgui_dc * dst, int x, int y, enum RTGUI_BLENDMODE blendMode, rt_uint8_t r,
					   rt_uint8_t g, rt_uint8_t b, rt_uint8_t a)
{
	unsigned inva = 0xff - a;

	switch (blendMode) {
	case RTGUI_BLENDMODE_BLEND:
		DRAW_SETPIXELXY_BLEND_BGR565(x, y);
		break;
	case RTGUI_BLENDMODE_ADD:
		DRAW_SETPIXELXY_ADD_BGR565(x, y);
		break;
	case RTGUI_BLENDMODE_MOD:
		DRAW_SETPIXELXY_MOD_BGR565(x, y);
		break;
	default:
		DRAW_SETPIXELXY_BGR565(x, y);
		break;
	}
	return 0;
}

static int
_dc_blend_point_rgb888(struct rtgui_dc * dst, int x, int y, enum RTGUI_BLENDMODE blendMode, rt_uint8_t r,
                      rt_uint8_t g, rt_uint8_t b, rt_uint8_t a)
{
    unsigned inva = 0xff - a;

    switch (blendMode) {
    case RTGUI_BLENDMODE_BLEND:
        DRAW_SETPIXELXY_BLEND_RGB888(x, y);
        break;
    case RTGUI_BLENDMODE_ADD:
        DRAW_SETPIXELXY_ADD_RGB888(x, y);
        break;
    case RTGUI_BLENDMODE_MOD:
        DRAW_SETPIXELXY_MOD_RGB888(x, y);
        break;
    default:
        DRAW_SETPIXELXY_RGB888(x, y);
        break;
    }
    return 0;
}

static int
_dc_blend_point_argb8888(struct rtgui_dc * dst, int x, int y, enum RTGUI_BLENDMODE blendMode,
                        rt_uint8_t r, rt_uint8_t g, rt_uint8_t b, rt_uint8_t a)
{
    unsigned inva = 0xff - a;

    switch (blendMode) {
    case RTGUI_BLENDMODE_BLEND:
        DRAW_SETPIXELXY_BLEND_ARGB8888(x, y);
        break;
    case RTGUI_BLENDMODE_ADD:
        DRAW_SETPIXELXY_ADD_ARGB8888(x, y);
        break;
    case RTGUI_BLENDMODE_MOD:
        DRAW_SETPIXELXY_MOD_ARGB8888(x, y);
        break;
    default:
        DRAW_SETPIXELXY_ARGB8888(x, y);
        break;
    }
    return 0;
}

void 
rtgui_dc_blend_point(struct rtgui_dc * dst, int x, int y, enum RTGUI_BLENDMODE blendMode, rt_uint8_t r,
               rt_uint8_t g, rt_uint8_t b, rt_uint8_t a)
{
	RT_ASSERT(dst != RT_NULL);

	if (!rtgui_dc_get_visible(dst)) return;
	/* we do not support pixel DC */
	if (_dc_get_pixel(dst, 0, 0) == RT_NULL) return ; 

	/* Perform clipping */
	if (dst->type == RTGUI_DC_CLIENT)
	{
		rtgui_widget_t *owner;
		rtgui_rect_t rect;

		/* get owner */
		owner = RTGUI_CONTAINER_OF(dst, struct rtgui_widget, dc_type);

		x = x + owner->extent.x1;
		y = y + owner->extent.y1;

		if (rtgui_region_contains_point(&(owner->clip), x, y, &rect) != RT_EOK)
			return ;
	}

    if (blendMode == RTGUI_BLENDMODE_BLEND || blendMode == RTGUI_BLENDMODE_ADD) {
        r = DRAW_MUL(r, a);
        g = DRAW_MUL(g, a);
        b = DRAW_MUL(b, a);
    }

    switch (rtgui_dc_get_pixel_format(dst)) {
	case RTGRAPHIC_PIXEL_FORMAT_RGB565:
        _dc_blend_point_rgb565(dst, x, y, blendMode, r, g, b, a);
		break;
	case RTGRAPHIC_PIXEL_FORMAT_BGR565:
		_dc_blend_point_bgr565(dst, x, y, blendMode, r, g, b, a);
		break;
    case RTGRAPHIC_PIXEL_FORMAT_RGB888:
        _dc_blend_point_rgb888(dst, x, y, blendMode, r, g, b, a);
		break;
	case RTGRAPHIC_PIXEL_FORMAT_ARGB888:
		_dc_blend_point_argb8888(dst, x, y, blendMode, r, g, b, a);
		break;
    default:
        break;
    }
}

void 
rtgui_dc_blend_points(struct rtgui_dc * dst, const rtgui_point_t * points, int count,
                enum RTGUI_BLENDMODE blendMode, rt_uint8_t r, rt_uint8_t g, rt_uint8_t b, rt_uint8_t a)
{
    int i;
    int x, y;
    int (*func)(struct rtgui_dc * dst, int x, int y,
                enum RTGUI_BLENDMODE blendMode, rt_uint8_t r, rt_uint8_t g, rt_uint8_t b, rt_uint8_t a) = NULL;

	RT_ASSERT(dst != RT_NULL);
	if (!rtgui_dc_get_visible(dst)) return;
	/* we do not support pixel DC */
	if (_dc_get_pixel(dst, 0, 0) == RT_NULL) return; 

    if (blendMode == RTGUI_BLENDMODE_BLEND || blendMode == RTGUI_BLENDMODE_ADD)
	{
        r = DRAW_MUL(r, a);
        g = DRAW_MUL(g, a);
        b = DRAW_MUL(b, a);
    }

    /* FIXME: Does this function pointer slow things down significantly? */
	switch (rtgui_dc_get_pixel_format(dst))
	{
	case RTGRAPHIC_PIXEL_FORMAT_RGB565:
        func = _dc_blend_point_rgb565;
        break;
    case RTGRAPHIC_PIXEL_FORMAT_RGB888:
        func = _dc_blend_point_rgb888;
		break;
	case RTGRAPHIC_PIXEL_FORMAT_ARGB888:
		func = _dc_blend_point_argb8888;
    default:
        break;
    }

	/* get owner */
	if (dst->type == RTGUI_DC_CLIENT)
	{
		rtgui_widget_t *owner;
		rtgui_rect_t rect;

		owner = RTGUI_CONTAINER_OF(dst, struct rtgui_widget, dc_type);

		for (i = 0; i < count; ++i) 
		{
			x = points[i].x;
			y = points[i].y;

			/* Perform clipping */
			x = x + owner->extent.x1;
			y = y + owner->extent.y1;

			if (rtgui_region_contains_point(&(owner->clip), x, y, &rect) != RT_EOK)
				continue;

			func(dst, x, y, blendMode, r, g, b, a);
		}
	}
	else
	{
		for (i = 0; i < count; ++i) 
		{
			x = points[i].x;
			y = points[i].y;

			func(dst, x, y, blendMode, r, g, b, a);
		}
	}
}

static void
_dc_blend_line_rgb565(struct rtgui_dc * dst, int x1, int y1, int x2, int y2,
                     enum RTGUI_BLENDMODE blendMode, rt_uint8_t _r, rt_uint8_t _g, rt_uint8_t _b, rt_uint8_t _a,
                     rt_bool_t draw_end)
{
    unsigned r, g, b, a, inva;

    if (blendMode == RTGUI_BLENDMODE_BLEND || blendMode == RTGUI_BLENDMODE_ADD) {
        r = DRAW_MUL(_r, _a);
        g = DRAW_MUL(_g, _a);
        b = DRAW_MUL(_b, _a);
        a = _a;
    } else {
        r = _r;
        g = _g;
        b = _b;
        a = _a;
    }
    inva = (a ^ 0xff);

    if (y1 == y2) {
        switch (blendMode) {
        case RTGUI_BLENDMODE_BLEND:
            HLINE(rt_uint16_t, DRAW_SETPIXEL_BLEND_RGB565, draw_end);
            break;
        case RTGUI_BLENDMODE_ADD:
            HLINE(rt_uint16_t, DRAW_SETPIXEL_ADD_RGB565, draw_end);
            break;
        case RTGUI_BLENDMODE_MOD:
            HLINE(rt_uint16_t, DRAW_SETPIXEL_MOD_RGB565, draw_end);
            break;
        default:
            HLINE(rt_uint16_t, DRAW_SETPIXEL_RGB565, draw_end);
            break;
        }
    } else if (x1 == x2) {
        switch (blendMode) {
        case RTGUI_BLENDMODE_BLEND:
            VLINE(rt_uint16_t, DRAW_SETPIXEL_BLEND_RGB565, draw_end);
            break;
        case RTGUI_BLENDMODE_ADD:
            VLINE(rt_uint16_t, DRAW_SETPIXEL_ADD_RGB565, draw_end);
            break;
        case RTGUI_BLENDMODE_MOD:
            VLINE(rt_uint16_t, DRAW_SETPIXEL_MOD_RGB565, draw_end);
            break;
        default:
            VLINE(rt_uint16_t, DRAW_SETPIXEL_RGB565, draw_end);
            break;
        }
    } else if (ABS(x1 - x2) == ABS(y1 - y2)) {
        switch (blendMode) {
        case RTGUI_BLENDMODE_BLEND:
            DLINE(rt_uint16_t, DRAW_SETPIXEL_BLEND_RGB565, draw_end);
            break;
        case RTGUI_BLENDMODE_ADD:
            DLINE(rt_uint16_t, DRAW_SETPIXEL_ADD_RGB565, draw_end);
            break;
        case RTGUI_BLENDMODE_MOD:
            DLINE(rt_uint16_t, DRAW_SETPIXEL_MOD_RGB565, draw_end);
            break;
        default:
            DLINE(rt_uint16_t, DRAW_SETPIXEL_RGB565, draw_end);
            break;
        }
    } else {
        switch (blendMode) {
        case RTGUI_BLENDMODE_BLEND:
            AALINE(x1, y1, x2, y2,
                   DRAW_SETPIXELXY_BLEND_RGB565, DRAW_SETPIXELXY_BLEND_RGB565,
                   draw_end);
            break;
        case RTGUI_BLENDMODE_ADD:
            AALINE(x1, y1, x2, y2,
                   DRAW_SETPIXELXY_ADD_RGB565, DRAW_SETPIXELXY_ADD_RGB565,
                   draw_end);
            break;
        case RTGUI_BLENDMODE_MOD:
            AALINE(x1, y1, x2, y2,
                   DRAW_SETPIXELXY_MOD_RGB565, DRAW_SETPIXELXY_MOD_RGB565,
                   draw_end);
            break;
        default:
            AALINE(x1, y1, x2, y2,
                   DRAW_SETPIXELXY_RGB565, DRAW_SETPIXELXY_BLEND_RGB565,
                   draw_end);
            break;
        }
    }
}

static void
_dc_blend_line_bgr565(struct rtgui_dc * dst, int x1, int y1, int x2, int y2,
                     enum RTGUI_BLENDMODE blendMode, rt_uint8_t _r, rt_uint8_t _g, rt_uint8_t _b, rt_uint8_t _a,
                     rt_bool_t draw_end)
{
    unsigned r, g, b, a, inva;

    if (blendMode == RTGUI_BLENDMODE_BLEND || blendMode == RTGUI_BLENDMODE_ADD) {
        r = DRAW_MUL(_r, _a);
        g = DRAW_MUL(_g, _a);
        b = DRAW_MUL(_b, _a);
        a = _a;
    } else {
        r = _r;
        g = _g;
        b = _b;
        a = _a;
    }
    inva = (a ^ 0xff);

    if (y1 == y2) {
        switch (blendMode) {
        case RTGUI_BLENDMODE_BLEND:
            HLINE(rt_uint16_t, DRAW_SETPIXEL_BLEND_BGR565, draw_end);
            break;
        case RTGUI_BLENDMODE_ADD:
            HLINE(rt_uint16_t, DRAW_SETPIXEL_ADD_BGR565, draw_end);
            break;
        case RTGUI_BLENDMODE_MOD:
            HLINE(rt_uint16_t, DRAW_SETPIXEL_MOD_BGR565, draw_end);
            break;
        default:
            HLINE(rt_uint16_t, DRAW_SETPIXEL_BGR565, draw_end);
            break;
        }
    } else if (x1 == x2) {
        switch (blendMode) {
        case RTGUI_BLENDMODE_BLEND:
            VLINE(rt_uint16_t, DRAW_SETPIXEL_BLEND_BGR565, draw_end);
            break;
        case RTGUI_BLENDMODE_ADD:
            VLINE(rt_uint16_t, DRAW_SETPIXEL_ADD_BGR565, draw_end);
            break;
        case RTGUI_BLENDMODE_MOD:
            VLINE(rt_uint16_t, DRAW_SETPIXEL_MOD_BGR565, draw_end);
            break;
        default:
            VLINE(rt_uint16_t, DRAW_SETPIXEL_BGR565, draw_end);
            break;
        }
    } else if (ABS(x1 - x2) == ABS(y1 - y2)) {
        switch (blendMode) {
        case RTGUI_BLENDMODE_BLEND:
            DLINE(rt_uint16_t, DRAW_SETPIXEL_BLEND_BGR565, draw_end);
            break;
        case RTGUI_BLENDMODE_ADD:
            DLINE(rt_uint16_t, DRAW_SETPIXEL_ADD_BGR565, draw_end);
            break;
        case RTGUI_BLENDMODE_MOD:
            DLINE(rt_uint16_t, DRAW_SETPIXEL_MOD_BGR565, draw_end);
            break;
        default:
            DLINE(rt_uint16_t, DRAW_SETPIXEL_BGR565, draw_end);
            break;
        }
    } else {
        switch (blendMode) {
        case RTGUI_BLENDMODE_BLEND:
            AALINE(x1, y1, x2, y2,
                   DRAW_SETPIXELXY_BLEND_BGR565, DRAW_SETPIXELXY_BLEND_BGR565,
                   draw_end);
            break;
        case RTGUI_BLENDMODE_ADD:
            AALINE(x1, y1, x2, y2,
                   DRAW_SETPIXELXY_ADD_BGR565, DRAW_SETPIXELXY_ADD_BGR565,
                   draw_end);
            break;
        case RTGUI_BLENDMODE_MOD:
            AALINE(x1, y1, x2, y2,
                   DRAW_SETPIXELXY_MOD_BGR565, DRAW_SETPIXELXY_MOD_BGR565,
                   draw_end);
            break;
        default:
            AALINE(x1, y1, x2, y2,
                   DRAW_SETPIXELXY_BGR565, DRAW_SETPIXELXY_BLEND_BGR565,
                   draw_end);
            break;
        }
    }
}

static void
_dc_blend_line_rgb888(struct rtgui_dc * dst, int x1, int y1, int x2, int y2,
                     enum RTGUI_BLENDMODE blendMode, rt_uint8_t _r, rt_uint8_t _g, rt_uint8_t _b, rt_uint8_t _a,
                     rt_bool_t draw_end)
{
    unsigned r, g, b, a, inva;

    if (blendMode == RTGUI_BLENDMODE_BLEND || blendMode == RTGUI_BLENDMODE_ADD) {
        r = DRAW_MUL(_r, _a);
        g = DRAW_MUL(_g, _a);
        b = DRAW_MUL(_b, _a);
        a = _a;
    } else {
        r = _r;
        g = _g;
        b = _b;
        a = _a;
    }
    inva = (a ^ 0xff);

    if (y1 == y2) {
        switch (blendMode) {
        case RTGUI_BLENDMODE_BLEND:
            HLINE(rt_uint32_t, DRAW_SETPIXEL_BLEND_RGB888, draw_end);
            break;
        case RTGUI_BLENDMODE_ADD:
            HLINE(rt_uint32_t, DRAW_SETPIXEL_ADD_RGB888, draw_end);
            break;
        case RTGUI_BLENDMODE_MOD:
            HLINE(rt_uint32_t, DRAW_SETPIXEL_MOD_RGB888, draw_end);
            break;
        default:
            HLINE(rt_uint32_t, DRAW_SETPIXEL_RGB888, draw_end);
            break;
        }
    } else if (x1 == x2) {
        switch (blendMode) {
        case RTGUI_BLENDMODE_BLEND:
            VLINE(rt_uint32_t, DRAW_SETPIXEL_BLEND_RGB888, draw_end);
            break;
        case RTGUI_BLENDMODE_ADD:
            VLINE(rt_uint32_t, DRAW_SETPIXEL_ADD_RGB888, draw_end);
            break;
        case RTGUI_BLENDMODE_MOD:
            VLINE(rt_uint32_t, DRAW_SETPIXEL_MOD_RGB888, draw_end);
            break;
        default:
            VLINE(rt_uint32_t, DRAW_SETPIXEL_RGB888, draw_end);
            break;
        }
    } else if (ABS(x1 - x2) == ABS(y1 - y2)) {
        switch (blendMode) {
        case RTGUI_BLENDMODE_BLEND:
            DLINE(rt_uint32_t, DRAW_SETPIXEL_BLEND_RGB888, draw_end);
            break;
        case RTGUI_BLENDMODE_ADD:
            DLINE(rt_uint32_t, DRAW_SETPIXEL_ADD_RGB888, draw_end);
            break;
        case RTGUI_BLENDMODE_MOD:
            DLINE(rt_uint32_t, DRAW_SETPIXEL_MOD_RGB888, draw_end);
            break;
        default:
            DLINE(rt_uint32_t, DRAW_SETPIXEL_RGB888, draw_end);
            break;
        }
    } else {
        switch (blendMode) {
        case RTGUI_BLENDMODE_BLEND:
            AALINE(x1, y1, x2, y2,
                   DRAW_SETPIXELXY_BLEND_RGB888, DRAW_SETPIXELXY_BLEND_RGB888,
                   draw_end);
            break;
        case RTGUI_BLENDMODE_ADD:
            AALINE(x1, y1, x2, y2,
                   DRAW_SETPIXELXY_ADD_RGB888, DRAW_SETPIXELXY_ADD_RGB888,
                   draw_end);
            break;
        case RTGUI_BLENDMODE_MOD:
            AALINE(x1, y1, x2, y2,
                   DRAW_SETPIXELXY_MOD_RGB888, DRAW_SETPIXELXY_MOD_RGB888,
                   draw_end);
            break;
        default:
            AALINE(x1, y1, x2, y2,
                   DRAW_SETPIXELXY_RGB888, DRAW_SETPIXELXY_BLEND_RGB888,
                   draw_end);
            break;
        }
    }
}

static void
_dc_blend_line_argb8888(struct rtgui_dc * dst, int x1, int y1, int x2, int y2,
                       enum RTGUI_BLENDMODE blendMode, rt_uint8_t _r, rt_uint8_t _g, rt_uint8_t _b, rt_uint8_t _a,
                       rt_bool_t draw_end)
{
    unsigned r, g, b, a, inva;

    if (blendMode == RTGUI_BLENDMODE_BLEND || blendMode == RTGUI_BLENDMODE_ADD) {
        r = DRAW_MUL(_r, _a);
        g = DRAW_MUL(_g, _a);
        b = DRAW_MUL(_b, _a);
        a = _a;
    } else {
        r = _r;
        g = _g;
        b = _b;
        a = _a;
    }
    inva = (a ^ 0xff);

    if (y1 == y2) {
        switch (blendMode) {
        case RTGUI_BLENDMODE_BLEND:
            HLINE(rt_uint32_t, DRAW_SETPIXEL_BLEND_ARGB8888, draw_end);
            break;
        case RTGUI_BLENDMODE_ADD:
            HLINE(rt_uint32_t, DRAW_SETPIXEL_ADD_ARGB8888, draw_end);
            break;
        case RTGUI_BLENDMODE_MOD:
            HLINE(rt_uint32_t, DRAW_SETPIXEL_MOD_ARGB8888, draw_end);
            break;
        default:
            HLINE(rt_uint32_t, DRAW_SETPIXEL_ARGB8888, draw_end);
            break;
        }
    } else if (x1 == x2) {
        switch (blendMode) {
        case RTGUI_BLENDMODE_BLEND:
            VLINE(rt_uint32_t, DRAW_SETPIXEL_BLEND_ARGB8888, draw_end);
            break;
        case RTGUI_BLENDMODE_ADD:
            VLINE(rt_uint32_t, DRAW_SETPIXEL_ADD_ARGB8888, draw_end);
            break;
        case RTGUI_BLENDMODE_MOD:
            VLINE(rt_uint32_t, DRAW_SETPIXEL_MOD_ARGB8888, draw_end);
            break;
        default:
            VLINE(rt_uint32_t, DRAW_SETPIXEL_ARGB8888, draw_end);
            break;
        }
    } else if (ABS(x1 - x2) == ABS(y1 - y2)) {
        switch (blendMode) {
        case RTGUI_BLENDMODE_BLEND:
            DLINE(rt_uint32_t, DRAW_SETPIXEL_BLEND_ARGB8888, draw_end);
            break;
        case RTGUI_BLENDMODE_ADD:
            DLINE(rt_uint32_t, DRAW_SETPIXEL_ADD_ARGB8888, draw_end);
            break;
        case RTGUI_BLENDMODE_MOD:
            DLINE(rt_uint32_t, DRAW_SETPIXEL_MOD_ARGB8888, draw_end);
            break;
        default:
            DLINE(rt_uint32_t, DRAW_SETPIXEL_ARGB8888, draw_end);
            break;
        }
    } else {
        switch (blendMode) {
        case RTGUI_BLENDMODE_BLEND:
            AALINE(x1, y1, x2, y2,
                   DRAW_SETPIXELXY_BLEND_ARGB8888, DRAW_SETPIXELXY_BLEND_ARGB8888,
                   draw_end);
            break;
        case RTGUI_BLENDMODE_ADD:
            AALINE(x1, y1, x2, y2,
                   DRAW_SETPIXELXY_ADD_ARGB8888, DRAW_SETPIXELXY_ADD_ARGB8888,
                   draw_end);
            break;
        case RTGUI_BLENDMODE_MOD:
            AALINE(x1, y1, x2, y2,
                   DRAW_SETPIXELXY_MOD_ARGB8888, DRAW_SETPIXELXY_MOD_ARGB8888,
                   draw_end);
            break;
        default:
            AALINE(x1, y1, x2, y2,
                   DRAW_SETPIXELXY_ARGB8888, DRAW_SETPIXELXY_BLEND_ARGB8888,
                   draw_end);
            break;
        }
    }
}

typedef void (*BlendLineFunc) (struct rtgui_dc * dst,
                               int x1, int y1, int x2, int y2,
                               enum RTGUI_BLENDMODE blendMode,
                               rt_uint8_t r, rt_uint8_t g, rt_uint8_t b, rt_uint8_t a,
                               rt_bool_t draw_end);

static BlendLineFunc
_dc_calc_blend_line_func(rt_uint8_t pixel_format)
{
	switch (pixel_format)
	{
	case RTGRAPHIC_PIXEL_FORMAT_RGB565:
		return _dc_blend_line_rgb565;
	case RTGRAPHIC_PIXEL_FORMAT_BGR565:
		return _dc_blend_line_bgr565;
	case RTGRAPHIC_PIXEL_FORMAT_RGB888:
		return _dc_blend_line_rgb888;
	case RTGRAPHIC_PIXEL_FORMAT_ARGB888:
		return _dc_blend_line_argb8888;
	}

    return NULL;
}

void 
rtgui_dc_blend_line(struct rtgui_dc * dst, int x1, int y1, int x2, int y2,
              enum RTGUI_BLENDMODE blendMode, rtgui_color_t color)
{
    BlendLineFunc func;
	rt_uint8_t pixel_format = 0xff;
	rt_uint8_t r, g, b, a;
	rtgui_widget_t *owner;

	RT_ASSERT(dst != RT_NULL);
	if (!rtgui_dc_get_visible(dst)) return;
	/* we do not support pixel DC */
	if (_dc_get_pixel(dst, 0, 0) == RT_NULL) return; 

	pixel_format = rtgui_dc_get_pixel_format(dst);
    func = _dc_calc_blend_line_func(pixel_format);	
    if (!func) 
	{
        rt_kprintf("dc_blend_line(): Unsupported pixel format\n");
		return;
    }

	r = RTGUI_RGB_R(color);
	g = RTGUI_RGB_G(color);
	b = RTGUI_RGB_B(color);
	a = RTGUI_RGB_A(color);

	/* perform clip */
	if (dst->type == RTGUI_DC_CLIENT)
	{
		/* get owner */
		owner = RTGUI_CONTAINER_OF(dst, struct rtgui_widget, dc_type);
		
		x1 = x1 + owner->extent.x1;
		x2 = x2 + owner->extent.x1;
		y1 = y1 + owner->extent.y1;
		y2 = y2 + owner->extent.y1;
		if (y1 > y2) _int_swap(y1, y2);
		if (x1 > x2) _int_swap(x1, x2);

		if (owner->clip.data == RT_NULL)
		{
			rtgui_rect_t *prect;

			/* no clip */
			prect = &(owner->clip.extents);
		
			/* calculate line intersect */
			if (_intersect_rect_line(prect, &x1, &y1, &x2, &y2) == RT_FALSE) return ;

			/* draw line */
			func(dst, x1, y1, x2, y2, blendMode, r, g, b, a, RT_TRUE);
		}
		else
		{
			register rt_base_t index;

			for (index = 0; index < rtgui_region_num_rects(&(owner->clip)); index ++)
	    	{
		        rtgui_rect_t *prect;
		        int draw_x1, draw_x2;
		        int draw_y1, draw_y2;

		        prect = ((rtgui_rect_t *)(owner->clip.data + index + 1));
		        draw_x1 = x1; draw_x2 = x2;
		        draw_y1 = y1; draw_y2 = y2;

				/* calculate line intersect */
				if (_intersect_rect_line(prect, &draw_x1, &draw_y1, &draw_x2, &draw_y2) == RT_FALSE) continue;

		        /* draw line */
				func(dst, draw_x1, draw_y1, draw_x2, draw_y2, blendMode, r, g, b, a, RT_TRUE);
		    }
		}
	}
	else
	{
		if (dst->type == RTGUI_DC_HW)
		{
			struct rtgui_dc_hw *dc_hw = (struct rtgui_dc_hw*)dst;

			owner = dc_hw->owner;
			x1 = x1 + owner->extent.x1;
			x2 = x2 + owner->extent.x1;
			y1 = y1 + owner->extent.y1;
			y2 = y2 + owner->extent.y1;
			if (y1 > y2) _int_swap(y1, y2);
			if (x1 > x2) _int_swap(x1, x2);
		}
		
    	func(dst, x1, y1, x2, y2, blendMode, r, g, b, a, RT_TRUE);
	}
}

void
rtgui_dc_blend_lines(struct rtgui_dc * dst, const rtgui_point_t * points, int count,
               enum RTGUI_BLENDMODE blendMode, rtgui_color_t color)
{
    int i;
    int x1, y1;
    int x2, y2;
    rt_bool_t draw_end;
    BlendLineFunc func;
	rt_uint8_t pixel_format = 0xff;
	rt_uint8_t r, g, b, a;
	rtgui_widget_t *owner = RT_NULL;

	RT_ASSERT(dst != RT_NULL);
	if (!rtgui_dc_get_visible(dst)) return;
	/* we do not support pixel DC */
	if (_dc_get_pixel(dst, 0, 0) == RT_NULL) return ; 

	pixel_format = rtgui_dc_get_pixel_format(dst);
    func = _dc_calc_blend_line_func(pixel_format);
    if (!func) {
        rt_kprintf("dc_blend_lines(): Unsupported pixel format\n");
		return ;
    }

	/* Perform clipping */
	if (dst->type == RTGUI_DC_CLIENT)
	{
		/* get owner */
		owner = RTGUI_CONTAINER_OF(dst, struct rtgui_widget, dc_type);
	}
	else if (dst->type == RTGUI_DC_HW)
	{
		struct rtgui_dc_hw* dc_hw = (struct rtgui_dc_hw*)dst;

		owner = dc_hw->owner;
	}
	
	r = RTGUI_RGB_R(color);
	g = RTGUI_RGB_G(color);
	b = RTGUI_RGB_B(color);
	a = RTGUI_RGB_A(color);

    for (i = 1; i < count; ++i)
	{
        x1 = points[i-1].x;
        y1 = points[i-1].y;
        x2 = points[i].x;
        y2 = points[i].y;

		/* Draw the end if it was clipped */
		draw_end = (x2 != points[i].x || y2 != points[i].y);

        /* Perform clipping */
		if (dst->type == RTGUI_DC_CLIENT)
		{
			x1 = x1 + owner->extent.x1;
			x2 = x2 + owner->extent.x1;
			y1 = y1 + owner->extent.y1;
			y2 = y2 + owner->extent.y1;
			if (y1 > y2) _int_swap(y1, y2);
			if (x1 > x2) _int_swap(x1, x2);
		
			if (owner->clip.data == RT_NULL)
			{
				rtgui_rect_t *prect;
		
				/* no clip */
				prect = &(owner->clip.extents);
			
				/* calculate line intersect */
				if (prect->x1 > x2	|| prect->x2 <= x1) return ;
				if (prect->y2 <= y1 || prect->y1 > y2)	return ;
		
				if (prect->y1 > y1) y1 = prect->y1;
				if (prect->y2 < y2) y2 = prect->y2;
				if (prect->x1 > x1) x1 = prect->x1;
				if (prect->x2 < x2) x2 = prect->x2;
						
				/* draw line */
				func(dst, x1, y1, x2, y2, blendMode, r, g, b, a, draw_end);
			}
			else
			{
				register rt_base_t index;
				
				for (index = 0; index < rtgui_region_num_rects(&(owner->clip)); index ++)
				{
					rtgui_rect_t *prect;
					register rt_base_t draw_x1, draw_x2;
					register rt_base_t draw_y1, draw_y2;
		
					prect = ((rtgui_rect_t *)(owner->clip.data + index + 1));
					draw_x1 = x1; draw_x2 = x2;
					draw_y1 = y1; draw_y2 = y2;
		
					/* calculate vline clip */
					if (prect->x1 > x1	|| prect->x2 <= x1) continue;
					if (prect->y2 <= y1 || prect->y1 > y2) continue;
		
					if (prect->y1 > y1) draw_y1 = prect->y1;
					if (prect->y2 < y2) draw_y2 = prect->y2;
					if (prect->x1 > x1) draw_x1 = prect->x1;
					if (prect->x2 < x2) draw_x2 = prect->x2;
		
					/* draw line */
					func(dst, draw_x1, draw_y1, draw_x2, draw_y2, blendMode, r, g, b, a, draw_end);
				}
			}
		}
		else
		{
			if (dst->type == RTGUI_DC_HW)
			{
				x1 = x1 + owner->extent.x1;
				x2 = x2 + owner->extent.x1;
				y1 = y1 + owner->extent.y1;
				y2 = y2 + owner->extent.y1;
				if (y1 > y2) _int_swap(y1, y2);
				if (x1 > x2) _int_swap(x1, x2);
			}
			
			func(dst, x1, y1, x2, y2, blendMode, r, g, b, a, draw_end);
		}
    }

    if (points[0].x != points[count-1].x || points[0].y != points[count-1].y) 
	{
        rtgui_dc_blend_point(dst, points[count-1].x, points[count-1].y,
                       blendMode, r, g, b, a);
    }
}

static void
_dc_blend_fill_rect_rgb565(struct rtgui_dc * dst, const rtgui_rect_t * rect,
                         enum RTGUI_BLENDMODE blendMode, rt_uint8_t r, rt_uint8_t g, rt_uint8_t b, rt_uint8_t a)
{
    unsigned inva = 0xff - a;

    switch (blendMode) {
    case RTGUI_BLENDMODE_BLEND:
        FILLRECT(rt_uint16_t, DRAW_SETPIXEL_BLEND_RGB565);
        break;
    case RTGUI_BLENDMODE_ADD:
        FILLRECT(rt_uint16_t, DRAW_SETPIXEL_ADD_RGB565);
        break;
    case RTGUI_BLENDMODE_MOD:
        FILLRECT(rt_uint16_t, DRAW_SETPIXEL_MOD_RGB565);
        break;
    default:
        FILLRECT(rt_uint16_t, DRAW_SETPIXEL_RGB565);
        break;
    }
}

static void
_dc_blend_fill_rect_bgr565(struct rtgui_dc * dst, const rtgui_rect_t * rect,
                         enum RTGUI_BLENDMODE blendMode, rt_uint8_t r, rt_uint8_t g, rt_uint8_t b, rt_uint8_t a)
{
    unsigned inva = 0xff - a;

    switch (blendMode) {
    case RTGUI_BLENDMODE_BLEND:
        FILLRECT(rt_uint16_t, DRAW_SETPIXEL_BLEND_RGB565);
        break;
    case RTGUI_BLENDMODE_ADD:
        FILLRECT(rt_uint16_t, DRAW_SETPIXEL_ADD_RGB565);
        break;
    case RTGUI_BLENDMODE_MOD:
        FILLRECT(rt_uint16_t, DRAW_SETPIXEL_MOD_RGB565);
        break;
    default:
        FILLRECT(rt_uint16_t, DRAW_SETPIXEL_RGB565);
        break;
    }
}

static void
_dc_blend_fill_rect_rgb888(struct rtgui_dc * dst, const rtgui_rect_t * rect,
                         enum RTGUI_BLENDMODE blendMode, rt_uint8_t r, rt_uint8_t g, rt_uint8_t b, rt_uint8_t a)
{
    unsigned inva = 0xff - a;

    switch (blendMode) {
    case RTGUI_BLENDMODE_BLEND:
        FILLRECT(rt_uint32_t, DRAW_SETPIXEL_BLEND_RGB888);
        break;
    case RTGUI_BLENDMODE_ADD:
        FILLRECT(rt_uint32_t, DRAW_SETPIXEL_ADD_RGB888);
        break;
    case RTGUI_BLENDMODE_MOD:
        FILLRECT(rt_uint32_t, DRAW_SETPIXEL_MOD_RGB888);
        break;
    default:
        FILLRECT(rt_uint32_t, DRAW_SETPIXEL_RGB888);
        break;
    }
}

static void
_dc_blend_fill_rect_argb8888(struct rtgui_dc * dst, const rtgui_rect_t * rect,
                           enum RTGUI_BLENDMODE blendMode, rt_uint8_t r, rt_uint8_t g, rt_uint8_t b, rt_uint8_t a)
{
    unsigned inva = 0xff - a;

    switch (blendMode) {
    case RTGUI_BLENDMODE_BLEND:
        FILLRECT(rt_uint32_t, DRAW_SETPIXEL_BLEND_ARGB8888);
        break;
    case RTGUI_BLENDMODE_ADD:
        FILLRECT(rt_uint32_t, DRAW_SETPIXEL_ADD_ARGB8888);
        break;
    case RTGUI_BLENDMODE_MOD:
        FILLRECT(rt_uint32_t, DRAW_SETPIXEL_MOD_ARGB8888);
        break;
    default:
        FILLRECT(rt_uint32_t, DRAW_SETPIXEL_ARGB8888);
        break;
    }
}

typedef void (*BlendFillFunc)(struct rtgui_dc * dst, const rtgui_rect_t * rect,
			enum RTGUI_BLENDMODE blendMode, rt_uint8_t r, rt_uint8_t g, rt_uint8_t b, rt_uint8_t a);

void 
rtgui_dc_blend_fill_rect(struct rtgui_dc* dst, const rtgui_rect_t *rect,
                  enum RTGUI_BLENDMODE blendMode, rtgui_color_t color)
{
    unsigned r, g, b, a;
	BlendFillFunc func = RT_NULL;

	RT_ASSERT(dst != RT_NULL);

	if (!rtgui_dc_get_visible(dst)) return;
    /* This function doesn't work on surfaces < 8 bpp */
    if (_dc_get_bits_per_pixel(dst) < 8) {
        rt_kprintf("dc_blend_fill_rect(): Unsupported pixel format\n");
		return ;
    }

	r = RTGUI_RGB_R(color);
	g = RTGUI_RGB_G(color);
	b = RTGUI_RGB_B(color);
	a = RTGUI_RGB_A(color);

    if (blendMode == RTGUI_BLENDMODE_BLEND || blendMode == RTGUI_BLENDMODE_ADD) {
        r = DRAW_MUL(r, a);
        g = DRAW_MUL(g, a);
        b = DRAW_MUL(b, a);
    }

    switch (rtgui_dc_get_pixel_format(dst)) 
	{
    case RTGRAPHIC_PIXEL_FORMAT_RGB565:
        func = _dc_blend_fill_rect_rgb565;
		break;
	case RTGRAPHIC_PIXEL_FORMAT_BGR565:
        func = _dc_blend_fill_rect_bgr565;
		break;
	case RTGRAPHIC_PIXEL_FORMAT_RGB888:
		func = _dc_blend_fill_rect_rgb888;
		break;
	case RTGRAPHIC_PIXEL_FORMAT_ARGB888:
		func = _dc_blend_fill_rect_argb8888;
		break;
    default:
        break;
    }
	if (func == RT_NULL)
	{
        rt_kprintf("dc_blend_fill_rect(): Unsupported pixel format\n");
		return ;
	}

	if (dst->type == RTGUI_DC_CLIENT)
	{
	    register rt_base_t index;
	    rtgui_widget_t *owner;
		rtgui_rect_t draw_rect;
	
	    /* get owner */
	    owner = RTGUI_CONTAINER_OF(dst, struct rtgui_widget, dc_type);

		if (owner->clip.data == RT_NULL)
		{
	        rtgui_rect_t *prect;
	        prect = &(owner->clip.extents);

			/* convert logic to device */
			draw_rect = *rect;
			rtgui_rect_moveto(&draw_rect,owner->extent.x1, owner->extent.y1);
			
	        /* calculate rect intersect */
	        if (prect->y1 > draw_rect.y2  || prect->y2 <= draw_rect.y1) return ;
	        if (prect->x2 <= draw_rect.x1 || prect->x1 > draw_rect.x2 ) return ;
			rtgui_rect_intersect(prect, &draw_rect);

			func(dst, &draw_rect, blendMode, r, g, b, a);
	    }
	    else
		{
			for (index = 0; index < rtgui_region_num_rects(&(owner->clip)); index ++)
	        {
	            rtgui_rect_t *prect;

	            prect = ((rtgui_rect_t *)(owner->clip.data + index + 1));

				draw_rect = *rect;
				rtgui_rect_moveto(&draw_rect,owner->extent.x1, owner->extent.y1);

				/* calculate rect intersect */
				if (prect->y1 > draw_rect.y2  || prect->y2 <= draw_rect.y1) continue;
				if (prect->x2 <= draw_rect.x1 || prect->x1 > draw_rect.x2 ) continue;
				rtgui_rect_intersect(prect, &draw_rect);

				func(dst, &draw_rect, blendMode, r, g, b, a);
	        }
	    }
	}
	else
	{
		func(dst, rect, blendMode, r, g, b, a);
	}
}

void 
rtgui_dc_blend_fill_rects(struct rtgui_dc * dst, const rtgui_rect_t *rects, int count,
                   enum RTGUI_BLENDMODE blendMode, rtgui_color_t color)
{
    int i;
    rtgui_rect_t rect;
	BlendFillFunc func = RT_NULL;
	rt_uint8_t r, g, b, a;
	rtgui_widget_t *owner = RT_NULL;

	RT_ASSERT(dst != RT_NULL);

	if (!rtgui_dc_get_visible(dst)) return;
    /* This function doesn't work on surfaces < 8 bpp */
    if (_dc_get_bits_per_pixel(dst)< 8) {
        rt_kprintf("dc_blend_fill_rects(): Unsupported pixel format\n");
		return;
    }

	r = RTGUI_RGB_R(color);
	g = RTGUI_RGB_G(color);
	b = RTGUI_RGB_B(color);
	a = RTGUI_RGB_A(color);

	if (blendMode == RTGUI_BLENDMODE_BLEND || blendMode == RTGUI_BLENDMODE_ADD) {
        r = DRAW_MUL(r, a);
        g = DRAW_MUL(g, a);
        b = DRAW_MUL(b, a);
    }

    switch (rtgui_dc_get_pixel_format(dst)) 
	{
    case RTGRAPHIC_PIXEL_FORMAT_RGB565:
        func = _dc_blend_fill_rect_rgb565;
		break;
	case RTGRAPHIC_PIXEL_FORMAT_BGR565:
        func = _dc_blend_fill_rect_bgr565;
		break;
	case RTGRAPHIC_PIXEL_FORMAT_RGB888:
		func = _dc_blend_fill_rect_rgb888;
		break;
	case RTGRAPHIC_PIXEL_FORMAT_ARGB888:
		func = _dc_blend_fill_rect_argb8888;
		break;
    default:
        break;
    }
	if (func == RT_NULL)
	{
        rt_kprintf("dc_blend_fill_rects(): Unsupported pixel format\n");
		return;
	}
	
	if (dst->type == RTGUI_DC_CLIENT)
	{
		/* get owner */
		owner = RTGUI_CONTAINER_OF(dst, struct rtgui_widget, dc_type);
	}
	
    for (i = 0; i < count; ++i) 
	{
		rect = rects[i];

		if (dst->type == RTGUI_DC_CLIENT)
		{
		    register rt_base_t index;
			rtgui_rect_t draw_rect;

			if (owner->clip.data == RT_NULL)
			{
		        rtgui_rect_t *prect;
		        prect = &(owner->clip.extents);

				/* convert logic to device */
				draw_rect = rect;
				rtgui_rect_moveto(&draw_rect,owner->extent.x1, owner->extent.y1);
				
		        /* calculate rect intersect */
		        if (prect->y1 > draw_rect.y2  || prect->y2 <= draw_rect.y1) return ;
		        if (prect->x2 <= draw_rect.x1 || prect->x1 > draw_rect.x2 ) return ;
				rtgui_rect_intersect(prect, &draw_rect);

				func(dst, &draw_rect, blendMode, r, g, b, a);
		    }
		    else
			{
				for (index = 0; index < rtgui_region_num_rects(&(owner->clip)); index ++)
		        {
		            rtgui_rect_t *prect;

		            prect = ((rtgui_rect_t *)(owner->clip.data + index + 1));

					draw_rect = rect;
					rtgui_rect_moveto(&draw_rect,owner->extent.x1, owner->extent.y1);

					/* calculate rect intersect */
					if (prect->y1 > draw_rect.y2  || prect->y2 <= draw_rect.y1) continue;
					if (prect->x2 <= draw_rect.x1 || prect->x1 > draw_rect.x2 ) continue;
					rtgui_rect_intersect(prect, &draw_rect);

					func(dst, &draw_rect, blendMode, r, g, b, a);
		        }
		    }
		}
		else
		{
			func(dst, &rect, blendMode, r, g, b, a);
		}		
    }
}

/* Windows targets do not have lrint, so provide a local inline version */
#if defined(_MSC_VER)
/* Detect 64bit and use intrinsic version */
#ifdef _M_X64
#include <emmintrin.h>
static __inline long 
lrint(float f) 
{
	return _mm_cvtss_si32(_mm_load_ss(&f));
}
#elif defined(_M_IX86)
__inline long int
lrint (double flt)
{	
	int intgr;
	_asm
	{
		fld flt
		fistp intgr
	};
	return intgr;
}
#elif defined(_M_ARM)
#include <armintr.h>
#pragma warning(push)
#pragma warning(disable: 4716)
__declspec(naked) long int
lrint (double flt)
{
	__emit(0xEC410B10); // fmdrr  d0, r0, r1
	__emit(0xEEBD0B40); // ftosid s0, d0
	__emit(0xEE100A10); // fmrs   r0, s0
	__emit(0xE12FFF1E); // bx     lr
}
#pragma warning(pop)
#else
#error lrint needed for MSVC on non X86/AMD64/ARM targets.
#endif
#endif

rt_inline void _draw_pixel_weight(struct rtgui_dc * dc, rt_int16_t x, rt_int16_t y,
							   rt_uint8_t r, rt_uint8_t g, rt_uint8_t b, rt_uint8_t a, rt_uint32_t weight)
{
	/*
	* Modify Alpha by weight 
	*/
	rt_uint32_t ax = a;
	ax = ((ax * weight) >> 8);
	if (ax > 255) {
		a = 255;
	} else {
		a = (rt_uint8_t)(ax & 0x000000ff);
	}

	rtgui_dc_blend_point(dc, x, y, RTGUI_BLENDMODE_BLEND, r, g, b, a);
}

void rtgui_dc_draw_aa_ellipse(struct rtgui_dc *dc, rt_int16_t  x, rt_int16_t y, rt_int16_t rx, rt_int16_t ry)
{
	int i;
	int a2, b2, ds, dt, dxt, t, s, d;
	rt_int16_t xp, yp, xs, ys, od, dyt, xx, yy, xc2, yc2;
	float cp;
	double sab;
	rt_uint8_t weight, iweight;
	rt_uint8_t r, g, b, a;

	/* Sanity check radii */
	if ((rx < 0) || (ry < 0)) return ;

	/*
	* Special case for rx=0 - draw a vline 
	*/
	if (rx == 0) {
		rtgui_dc_draw_vline(dc, x, y - ry, y + ry);
		return;
	}
	/*
	* Special case for ry=0 - draw an hline 
	*/
	if (ry == 0) {
		rtgui_dc_draw_hline(dc, x - rx, x + rx, y);
		return;
	}

	/* Variable setup */
	r = RTGUI_RGB_R(RTGUI_DC_FC(dc));
	g = RTGUI_RGB_G(RTGUI_DC_FC(dc));
	b = RTGUI_RGB_B(RTGUI_DC_FC(dc));
	a = RTGUI_RGB_A(RTGUI_DC_FC(dc));

	a2 = rx * rx;
	b2 = ry * ry;

	ds = 2 * a2;
	dt = 2 * b2;

	xc2 = 2 * x;
	yc2 = 2 * y;
 
	sab = sqrt((double)(a2 + b2));
	od = (rt_int16_t)lrint(sab*0.01) + 1; /* introduce some overdraw */
	dxt = (rt_int16_t)lrint((double)a2 / sab) + od;

	t = 0;
	s = -2 * a2 * ry;
	d = 0;

	xp = x;
	yp = y - ry;

	/* Draw */

	/* "End points" */
	rtgui_dc_blend_point(dc, xp, yp, RTGUI_BLENDMODE_NONE, r, g, b, a);
	rtgui_dc_blend_point(dc, xc2 - xp, yp, RTGUI_BLENDMODE_NONE, r, g, b, a);
	rtgui_dc_blend_point(dc, xp, yc2 - yp, RTGUI_BLENDMODE_NONE, r, g, b, a);
	rtgui_dc_blend_point(dc, xc2 - xp, yc2 - yp, RTGUI_BLENDMODE_NONE, r, g, b, a);

	for (i = 1; i <= dxt; i++) 
	{
		xp--;
		d += t - b2;

		if (d >= 0)
			ys = yp - 1;
		else if ((d - s - a2) > 0) 
		{
			if ((2 * d - s - a2) >= 0)
				ys = yp + 1;
			else {
				ys = yp;
				yp++;
				d -= s + a2;
				s += ds;
			}
		} else {
			yp++;
			ys = yp + 1;
			d -= s + a2;
			s += ds;
		}

		t -= dt;

		/* Calculate alpha */
		if (s != 0) {
			cp = (float) abs(d) / (float) abs(s);
			if (cp > 1.0) {
				cp = 1.0;
			}
		} else {
			cp = 1.0;
		}

		/* Calculate weights */
		weight = (rt_uint8_t) (cp * 255);
		iweight = 255 - weight;

		/* Upper half */
		xx = xc2 - xp;
		_draw_pixel_weight(dc, xp, yp, r, g, b, a, iweight);
		_draw_pixel_weight(dc, xx, yp, r, g, b, a, iweight);

		_draw_pixel_weight(dc, xp, ys, r, g, b, a, weight);
		_draw_pixel_weight(dc, xx, ys, r, g, b, a, weight);

		/* Lower half */
		yy = yc2 - yp;
		_draw_pixel_weight(dc, xp, yy, r, g, b, a, iweight);
		_draw_pixel_weight(dc, xx, yy, r, g, b, a, iweight);

		yy = yc2 - ys;
		_draw_pixel_weight(dc, xp, yy, r, g, b, a, weight);
		_draw_pixel_weight(dc, xx, yy, r, g, b, a, weight);
	}

	/* Replaces original approximation code dyt = abs(yp - yc); */
	dyt = (rt_int16_t)lrint((double)b2 / sab ) + od;
	for (i = 1; i <= dyt; i++) 
	{
		yp++;
		d -= s + a2;

		if (d <= 0)
			xs = xp + 1;
		else if ((d + t - b2) < 0)
		{
			if ((2 * d + t - b2) <= 0)
				xs = xp - 1;
			else {
				xs = xp;
				xp--;
				d += t - b2;
				t -= dt;
			}
		} else {
			xp--;
			xs = xp - 1;
			d += t - b2;
			t -= dt;
		}

		s += ds;

		/* Calculate alpha */
		if (t != 0) {
			cp = (float) abs(d) / (float) abs(t);
			if (cp > 1.0) {
				cp = 1.0;
			}
		} else {
			cp = 1.0;
		}

		/* Calculate weight */
		weight = (rt_uint8_t) (cp * 255);
		iweight = 255 - weight;

		/* Left half */
		xx = xc2 - xp;
		yy = yc2 - yp;
		_draw_pixel_weight(dc, xp, yp, r, g, b, a, iweight);
		_draw_pixel_weight(dc, xx, yp, r, g, b, a, iweight);

		_draw_pixel_weight(dc, xp, yy, r, g, b, a, iweight);
		_draw_pixel_weight(dc, xx, yy, r, g, b, a, iweight);

		/* Right half */
		xx = xc2 - xs;
		_draw_pixel_weight(dc, xs, yp, r, g, b, a, weight);
		_draw_pixel_weight(dc, xx, yp, r, g, b, a, weight);

		_draw_pixel_weight(dc, xs, yy, r, g, b, a, weight);
		_draw_pixel_weight(dc, xx, yy, r, g, b, a, weight);
	}
}

void rtgui_dc_draw_aa_circle(struct rtgui_dc *dc, rt_int16_t x, rt_int16_t y, rt_int16_t r)
{
	rtgui_dc_draw_aa_ellipse(dc, x, y, r, r);
}
