/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/*
 * This file is part of the LibreOffice project.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 *
 * This file incorporates work covered by the following license notice:
 *
 *   Licensed to the Apache Software Foundation (ASF) under one or more
 *   contributor license agreements. See the NOTICE file distributed
 *   with this work for additional information regarding copyright
 *   ownership. The ASF licenses this file to you under the Apache
 *   License, Version 2.0 (the "License"); you may not use this file
 *   except in compliance with the License. You may obtain a copy of
 *   the License at http://www.apache.org/licenses/LICENSE-2.0 .
 */

#pragma once

#include <cairo.h>

#include <vcl/dllapi.h>
#include <vcl/region.hxx>
#include <vcl/salgtype.hxx>

#include <com/sun/star/drawing/LineCap.hpp>

#include <basegfx/range/b2drange.hxx>
#include <basegfx/range/b2irange.hxx>
#include <basegfx/vector/b2ivector.hxx>
#include <basegfx/matrix/b2dhommatrix.hxx>
#include <basegfx/polygon/b2dpolypolygon.hxx>
#include <basegfx/polygon/b2dpolygon.hxx>

#include <tools/color.hxx>

//Using formats that match cairo's formats. For android we patch cairo,
//which is internal in that case, to swap the rgb components so that
//cairo then matches the OpenGL GL_RGBA format so we can use it there
//where we don't have GL_BGRA support.
// SVP_24BIT_FORMAT is used to store 24-bit images in 3-byte pixels to conserve memory.
#if defined(ANDROID) && !HAVE_FEATURE_ANDROID_LOK
#define SVP_24BIT_FORMAT (ScanlineFormat::N24BitTcRgb | ScanlineFormat::TopDown)
#define SVP_CAIRO_FORMAT (ScanlineFormat::N32BitTcRgba | ScanlineFormat::TopDown)
#define SVP_CAIRO_BLUE 1
#define SVP_CAIRO_GREEN 2
#define SVP_CAIRO_RED 0
#define SVP_CAIRO_ALPHA 3
#elif defined OSL_BIGENDIAN
#define SVP_24BIT_FORMAT (ScanlineFormat::N24BitTcRgb | ScanlineFormat::TopDown)
#define SVP_CAIRO_FORMAT (ScanlineFormat::N32BitTcArgb | ScanlineFormat::TopDown)
#define SVP_CAIRO_BLUE 3
#define SVP_CAIRO_GREEN 2
#define SVP_CAIRO_RED 1
#define SVP_CAIRO_ALPHA 0
#else
#define SVP_24BIT_FORMAT (ScanlineFormat::N24BitTcBgr | ScanlineFormat::TopDown)
#define SVP_CAIRO_FORMAT (ScanlineFormat::N32BitTcBgra | ScanlineFormat::TopDown)
#define SVP_CAIRO_BLUE 0
#define SVP_CAIRO_GREEN 1
#define SVP_CAIRO_RED 2
#define SVP_CAIRO_ALPHA 3
#endif

enum class PaintMode
{
    Over,
    Xor
};

typedef struct _cairo cairo_t;
typedef struct _cairo_surface cairo_surface_t;
typedef struct _cairo_user_data_key cairo_user_data_key_t;

VCL_DLLPUBLIC void dl_cairo_surface_set_device_scale(cairo_surface_t* surface, double x_scale,
                                                     double y_scale);
VCL_DLLPUBLIC void dl_cairo_surface_get_device_scale(cairo_surface_t* surface, double* x_scale,
                                                     double* y_scale);

VCL_DLLPUBLIC basegfx::B2DRange getClippedFillDamage(cairo_t* cr);
VCL_DLLPUBLIC basegfx::B2DRange getFillDamage(cairo_t* cr);
VCL_DLLPUBLIC basegfx::B2DRange getClipBox(cairo_t* cr);
VCL_DLLPUBLIC basegfx::B2DRange getClippedStrokeDamage(cairo_t* cr);
VCL_DLLPUBLIC basegfx::B2DRange getStrokeDamage(cairo_t* cr);

VCL_DLLPUBLIC void add_polygon_path(cairo_t* cr, const basegfx::B2DPolyPolygon& rPolyPolygon,
                                    const basegfx::B2DHomMatrix& rObjectToDevice, bool bPixelSnap);

VCL_DLLPUBLIC size_t AddPolygonToPath(cairo_t* cr, const basegfx::B2DPolygon& rPolygon,
                                      const basegfx::B2DHomMatrix& rObjectToDevice, bool bPixelSnap,
                                      bool bPixelSnapHairline);

VCL_DLLPUBLIC basegfx::B2DPoint impPixelSnap(const basegfx::B2DPolygon& rPolygon,
                                             const basegfx::B2DHomMatrix& rObjectToDevice,
                                             basegfx::B2DHomMatrix& rObjectToDeviceInv,
                                             sal_uInt32 nIndex);

typedef void (*damageHandler)(void* handle, sal_Int32 nExtentsX, sal_Int32 nExtentsY,
                              sal_Int32 nExtentsWidth, sal_Int32 nExtentsHeight);

struct VCL_DLLPUBLIC DamageHandler
{
    void* handle;
    damageHandler damaged;
};

struct VCL_DLLPUBLIC CairoCommon
{
    cairo_surface_t* m_pSurface;
    basegfx::B2IVector m_aFrameSize;
    vcl::Region m_aClipRegion;
    Color m_aLineColor;
    Color m_aFillColor;
    PaintMode m_ePaintMode;
    double m_fScale;

    CairoCommon()
        : m_pSurface(nullptr)
        , m_aLineColor(Color(0x00, 0x00, 0x00))
        , m_aFillColor(Color(0xFF, 0xFF, 0XFF))
        , m_ePaintMode(PaintMode::Over)
        , m_fScale(1.0)
    {
    }

    cairo_surface_t* getSurface() const { return m_pSurface; }

    cairo_t* getCairoContext(bool bXorModeAllowed, bool bAntiAlias) const;
    void releaseCairoContext(cairo_t* cr, bool bXorModeAllowed,
                             const basegfx::B2DRange& rExtents) const;

    cairo_t* createTmpCompatibleCairoContext() const;
    void applyColor(cairo_t* cr, Color rColor, double fTransparency = 0.0);

    void clipRegion(cairo_t* cr);
    static void clipRegion(cairo_t* cr, const vcl::Region& rClipRegion);

    static cairo_user_data_key_t* getDamageKey();

    // need this static version of ::drawPolyLine for usage from
    // vcl/unx/generic/gdi/salgdi.cxx. It gets wrapped by
    // ::drawPolyLine with some added parameters (see there)
    static bool drawPolyLine(cairo_t* cr, basegfx::B2DRange* pExtents, const Color& rLineColor,
                             bool bAntiAlias, const basegfx::B2DHomMatrix& rObjectToDevice,
                             const basegfx::B2DPolygon& rPolyLine, double fTransparency,
                             double fLineWidth,
                             const std::vector<double>* pStroke, // MM01
                             basegfx::B2DLineJoin eLineJoin, css::drawing::LineCap eLineCap,
                             double fMiterMinimumAngle, bool bPixelSnapHairline);

    void copyWithOperator(const SalTwoRect& rTR, cairo_surface_t* source, cairo_operator_t eOp,
                          bool bAntiAlias);

    void copySource(const SalTwoRect& rTR, cairo_surface_t* source, bool bAntiAlias);

    static basegfx::B2DRange renderSource(cairo_t* cr, const SalTwoRect& rTR,
                                          cairo_surface_t* source);

    void copyBitsCairo(const SalTwoRect& rTR, cairo_surface_t* pSourceSurface, bool bAntiAlias);

    void invert(const basegfx::B2DPolygon& rPoly, SalInvert nFlags, bool bAntiAlias);
};

/* vim:set shiftwidth=4 softtabstop=4 expandtab: */
