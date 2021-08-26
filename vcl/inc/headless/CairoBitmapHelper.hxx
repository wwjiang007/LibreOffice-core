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
#include <headless/svpbmp.hxx>
#include <headless/CairoCommon.hxx>

cairo_format_t getCairoFormat(const BitmapBuffer& rBuffer)
{
    cairo_format_t nFormat;
#ifdef HAVE_CAIRO_FORMAT_RGB24_888
    assert(rBuffer.mnBitCount == 32 || rBuffer.mnBitCount == 24 || rBuffer.mnBitCount == 1);
#else
    assert(rBuffer.mnBitCount == 32 || rBuffer.mnBitCount == 1);
#endif

    if (rBuffer.mnBitCount == 32)
        nFormat = CAIRO_FORMAT_ARGB32;
#ifdef HAVE_CAIRO_FORMAT_RGB24_888
    else if (rBuffer.mnBitCount == 24)
        nFormat = CAIRO_FORMAT_RGB24_888;
#endif
    else
        nFormat = CAIRO_FORMAT_A1;
    return nFormat;
}

void Toggle1BitTransparency(const BitmapBuffer& rBuf)
{
    assert(rBuf.maPalette.GetBestIndex(BitmapColor(COL_BLACK)) == 0);
    // TODO: make upper layers use standard alpha
    if (getCairoFormat(rBuf) == CAIRO_FORMAT_A1)
    {
        const int nImageSize = rBuf.mnHeight * rBuf.mnScanlineSize;
        unsigned char* pDst = rBuf.mpBits;
        for (int i = nImageSize; --i >= 0; ++pDst)
            *pDst = ~*pDst;
    }
}

std::unique_ptr<BitmapBuffer> FastConvert24BitRgbTo32BitCairo(const BitmapBuffer* pSrc)
{
    if (pSrc == nullptr)
        return nullptr;

    assert(pSrc->mnFormat == SVP_24BIT_FORMAT);
    const tools::Long nWidth = pSrc->mnWidth;
    const tools::Long nHeight = pSrc->mnHeight;
    std::unique_ptr<BitmapBuffer> pDst(new BitmapBuffer);
    pDst->mnFormat = (ScanlineFormat::N32BitTcArgb | ScanlineFormat::TopDown);
    pDst->mnWidth = nWidth;
    pDst->mnHeight = nHeight;
    pDst->mnBitCount = 32;
    pDst->maColorMask = pSrc->maColorMask;
    pDst->maPalette = pSrc->maPalette;

    tools::Long nScanlineBase;
    const bool bFail = o3tl::checked_multiply<tools::Long>(pDst->mnBitCount, nWidth, nScanlineBase);
    if (bFail)
    {
        SAL_WARN("vcl.gdi", "checked multiply failed");
        pDst->mpBits = nullptr;
        return nullptr;
    }

    pDst->mnScanlineSize = AlignedWidth4Bytes(nScanlineBase);
    if (pDst->mnScanlineSize < nScanlineBase / 8)
    {
        SAL_WARN("vcl.gdi", "scanline calculation wraparound");
        pDst->mpBits = nullptr;
        return nullptr;
    }

    try
    {
        pDst->mpBits = new sal_uInt8[pDst->mnScanlineSize * nHeight];
    }
    catch (const std::bad_alloc&)
    {
        // memory exception, clean up
        pDst->mpBits = nullptr;
        return nullptr;
    }

    for (tools::Long y = 0; y < nHeight; ++y)
    {
        sal_uInt8* pS = pSrc->mpBits + y * pSrc->mnScanlineSize;
        sal_uInt8* pD = pDst->mpBits + y * pDst->mnScanlineSize;
        for (tools::Long x = 0; x < nWidth; ++x)
        {
#if defined(ANDROID) && !HAVE_FEATURE_ANDROID_LOK
            static_assert((SVP_CAIRO_FORMAT & ~ScanlineFormat::TopDown)
                              == ScanlineFormat::N32BitTcRgba,
                          "Expected SVP_CAIRO_FORMAT set to N32BitTcBgra");
            static_assert((SVP_24BIT_FORMAT & ~ScanlineFormat::TopDown)
                              == ScanlineFormat::N24BitTcRgb,
                          "Expected SVP_24BIT_FORMAT set to N24BitTcRgb");
            pD[0] = pS[0];
            pD[1] = pS[1];
            pD[2] = pS[2];
            pD[3] = 0xff; // Alpha
#elif defined OSL_BIGENDIAN
            static_assert((SVP_CAIRO_FORMAT & ~ScanlineFormat::TopDown)
                              == ScanlineFormat::N32BitTcArgb,
                          "Expected SVP_CAIRO_FORMAT set to N32BitTcBgra");
            static_assert((SVP_24BIT_FORMAT & ~ScanlineFormat::TopDown)
                              == ScanlineFormat::N24BitTcRgb,
                          "Expected SVP_24BIT_FORMAT set to N24BitTcRgb");
            pD[0] = 0xff; // Alpha
            pD[1] = pS[0];
            pD[2] = pS[1];
            pD[3] = pS[2];
#else
            static_assert((SVP_CAIRO_FORMAT & ~ScanlineFormat::TopDown)
                              == ScanlineFormat::N32BitTcBgra,
                          "Expected SVP_CAIRO_FORMAT set to N32BitTcBgra");
            static_assert((SVP_24BIT_FORMAT & ~ScanlineFormat::TopDown)
                              == ScanlineFormat::N24BitTcBgr,
                          "Expected SVP_24BIT_FORMAT set to N24BitTcBgr");
            pD[0] = pS[0];
            pD[1] = pS[1];
            pD[2] = pS[2];
            pD[3] = 0xff; // Alpha
#endif

            pS += 3;
            pD += 4;
        }
    }

    return pDst;
}

// check for env var that decides for using downscale pattern
const char* pDisableDownScale(getenv("SAL_DISABLE_CAIRO_DOWNSCALE"));
bool bDisableDownScale(nullptr != pDisableDownScale);

class SurfaceHelper
{
private:
    cairo_surface_t* pSurface;
    std::unordered_map<unsigned long long, cairo_surface_t*> maDownscaled;

    SurfaceHelper(const SurfaceHelper&) = delete;
    SurfaceHelper& operator=(const SurfaceHelper&) = delete;

    cairo_surface_t* implCreateOrReuseDownscale(unsigned long nTargetWidth,
                                                unsigned long nTargetHeight)
    {
        const unsigned long nSourceWidth(cairo_image_surface_get_width(pSurface));
        const unsigned long nSourceHeight(cairo_image_surface_get_height(pSurface));

        // zoomed in, need to stretch at paint, no pre-scale useful
        if (nTargetWidth >= nSourceWidth || nTargetHeight >= nSourceHeight)
        {
            return pSurface;
        }

        // calculate downscale factor
        unsigned long nWFactor(1);
        unsigned long nW((nSourceWidth + 1) / 2);
        unsigned long nHFactor(1);
        unsigned long nH((nSourceHeight + 1) / 2);

        while (nW > nTargetWidth && nW > 1)
        {
            nW = (nW + 1) / 2;
            nWFactor *= 2;
        }

        while (nH > nTargetHeight && nH > 1)
        {
            nH = (nH + 1) / 2;
            nHFactor *= 2;
        }

        if (1 == nWFactor && 1 == nHFactor)
        {
            // original size *is* best binary size, use it
            return pSurface;
        }

        // go up one scale again - look for no change
        nW = (1 == nWFactor) ? nTargetWidth : nW * 2;
        nH = (1 == nHFactor) ? nTargetHeight : nH * 2;

        // check if we have a downscaled version of required size
        const unsigned long long key((nW * LONG_MAX) + nH);
        auto isHit(maDownscaled.find(key));

        if (isHit != maDownscaled.end())
        {
            return isHit->second;
        }

        // create new surface in the targeted size
        cairo_surface_t* pSurfaceTarget
            = cairo_surface_create_similar(pSurface, cairo_surface_get_content(pSurface), nW, nH);

        // made a version to scale self first that worked well, but would've
        // been hard to support CAIRO_FORMAT_A1 including bit shifting, so
        // I decided to go with cairo itself - use CAIRO_FILTER_FAST or
        // CAIRO_FILTER_GOOD though. Please modify as needed for
        // performance/quality
        cairo_t* cr = cairo_create(pSurfaceTarget);
        const double fScaleX(static_cast<double>(nW) / static_cast<double>(nSourceWidth));
        const double fScaleY(static_cast<double>(nH) / static_cast<double>(nSourceHeight));
        cairo_scale(cr, fScaleX, fScaleY);
        cairo_set_source_surface(cr, pSurface, 0.0, 0.0);
        cairo_pattern_set_filter(cairo_get_source(cr), CAIRO_FILTER_GOOD);
        cairo_paint(cr);
        cairo_destroy(cr);

        // need to set device_scale for downscale surfaces to get
        // them handled correctly
        cairo_surface_set_device_scale(pSurfaceTarget, fScaleX, fScaleY);

        // add entry to cached entries
        maDownscaled[key] = pSurfaceTarget;

        return pSurfaceTarget;
    }

protected:
    cairo_surface_t* implGetSurface() const { return pSurface; }
    void implSetSurface(cairo_surface_t* pNew) { pSurface = pNew; }

    bool isTrivial() const
    {
        constexpr unsigned long nMinimalSquareSizeToBuffer(64 * 64);
        const unsigned long nSourceWidth(cairo_image_surface_get_width(pSurface));
        const unsigned long nSourceHeight(cairo_image_surface_get_height(pSurface));

        return nSourceWidth * nSourceHeight < nMinimalSquareSizeToBuffer;
    }

public:
    explicit SurfaceHelper()
        : pSurface(nullptr)
        , maDownscaled()
    {
    }
    ~SurfaceHelper()
    {
        cairo_surface_destroy(pSurface);
        for (auto& candidate : maDownscaled)
        {
            cairo_surface_destroy(candidate.second);
        }
    }
    cairo_surface_t* getSurface(unsigned long nTargetWidth = 0,
                                unsigned long nTargetHeight = 0) const
    {
        if (bDisableDownScale || 0 == nTargetWidth || 0 == nTargetHeight || !pSurface
            || isTrivial())
        {
            // caller asks for original or disabled or trivial (smaller then a minimal square size)
            // also excludes zero cases for width/height after this point if need to prescale
            return pSurface;
        }

        return const_cast<SurfaceHelper*>(this)->implCreateOrReuseDownscale(nTargetWidth,
                                                                            nTargetHeight);
    }
};

sal_Int64 estimateUsageInBytesForSurfaceHelper(const SurfaceHelper* pHelper)
{
    sal_Int64 nRetval(0);

    if (nullptr != pHelper)
    {
        cairo_surface_t* pSurface(pHelper->getSurface());

        if (pSurface)
        {
            const tools::Long nStride(cairo_image_surface_get_stride(pSurface));
            const tools::Long nHeight(cairo_image_surface_get_height(pSurface));

            nRetval = nStride * nHeight;

            // if we do downscale, size will grow by 1/4 + 1/16 + 1/32 + ...,
            // rough estimation just multiplies by 1.25, should be good enough
            // for estimation of buffer survival time
            if (!bDisableDownScale)
            {
                nRetval = (nRetval * 5) / 4;
            }
        }
    }

    return nRetval;
}

class BitmapHelper : public SurfaceHelper
{
private:
#ifdef HAVE_CAIRO_FORMAT_RGB24_888
    const bool m_bForceARGB32;
#endif
    SvpSalBitmap aTmpBmp;

public:
    explicit BitmapHelper(const SalBitmap& rSourceBitmap, const bool bForceARGB32 = false)
        : SurfaceHelper()
        ,
#ifdef HAVE_CAIRO_FORMAT_RGB24_888
        m_bForceARGB32(bForceARGB32)
        ,
#endif
        aTmpBmp()
    {
        const SvpSalBitmap& rSrcBmp = static_cast<const SvpSalBitmap&>(rSourceBitmap);
#ifdef HAVE_CAIRO_FORMAT_RGB24_888
        if ((rSrcBmp.GetBitCount() != 32 && rSrcBmp.GetBitCount() != 24) || bForceARGB32)
#else
        (void)bForceARGB32;
        if (rSrcBmp.GetBitCount() != 32)
#endif
        {
            //big stupid copy here
            const BitmapBuffer* pSrc = rSrcBmp.GetBuffer();
            const SalTwoRect aTwoRect
                = { 0, 0, pSrc->mnWidth, pSrc->mnHeight, 0, 0, pSrc->mnWidth, pSrc->mnHeight };
            std::unique_ptr<BitmapBuffer> pTmp
                = (pSrc->mnFormat == SVP_24BIT_FORMAT
                       ? FastConvert24BitRgbTo32BitCairo(pSrc)
                       : StretchAndConvert(*pSrc, aTwoRect, SVP_CAIRO_FORMAT));
            aTmpBmp.Create(std::move(pTmp));

            assert(aTmpBmp.GetBitCount() == 32);
            implSetSurface(SvpSalGraphics::createCairoSurface(aTmpBmp.GetBuffer()));
        }
        else { implSetSurface(SvpSalGraphics::createCairoSurface(rSrcBmp.GetBuffer())); }
    }
    void mark_dirty() { cairo_surface_mark_dirty(implGetSurface()); }
    unsigned char* getBits(sal_Int32& rStride)
    {
        cairo_surface_flush(implGetSurface());

        unsigned char* mask_data = cairo_image_surface_get_data(implGetSurface());

        const cairo_format_t nFormat = cairo_image_surface_get_format(implGetSurface());
#ifdef HAVE_CAIRO_FORMAT_RGB24_888
        if (!m_bForceARGB32)
            assert(nFormat == CAIRO_FORMAT_RGB24_888 && "Expected RGB24_888 image");
        else
#endif
        {
            assert(nFormat == CAIRO_FORMAT_ARGB32
                   && "need to implement CAIRO_FORMAT_A1 after all here");
        }

        rStride = cairo_format_stride_for_width(nFormat,
                                                cairo_image_surface_get_width(implGetSurface()));

        return mask_data;
    }
};

/* vim:set shiftwidth=4 softtabstop=4 expandtab: */
