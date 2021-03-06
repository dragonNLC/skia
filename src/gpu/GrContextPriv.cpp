/*
 * Copyright 2019 Google Inc.
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "src/gpu/GrContextPriv.h"

#include "include/gpu/GrContextThreadSafeProxy.h"
#include "include/gpu/GrDirectContext.h"
#include "src/gpu/GrAuditTrail.h"
#include "src/gpu/GrContextThreadSafeProxyPriv.h"
#include "src/gpu/GrDrawingManager.h"
#include "src/gpu/GrGpu.h"
#include "src/gpu/GrMemoryPool.h"
#include "src/gpu/GrRenderTargetContext.h"
#include "src/gpu/GrSurfaceContext.h"
#include "src/gpu/GrSurfaceContextPriv.h"
#include "src/gpu/GrTexture.h"
#include "src/gpu/SkGr.h"
#include "src/gpu/effects/GrSkSLFP.h"
#include "src/gpu/effects/generated/GrConfigConversionEffect.h"
#include "src/gpu/text/GrAtlasManager.h"
#include "src/gpu/text/GrTextBlobCache.h"
#include "src/image/SkImage_Base.h"
#include "src/image/SkImage_Gpu.h"

#define ASSERT_OWNED_PROXY(P) \
    SkASSERT(!(P) || !((P)->peekTexture()) || (P)->peekTexture()->getContext() == fContext)
#define ASSERT_SINGLE_OWNER GR_ASSERT_SINGLE_OWNER(fContext->singleOwner())
#define RETURN_VALUE_IF_ABANDONED(value) if (fContext->abandoned()) { return (value); }
#define RETURN_IF_ABANDONED RETURN_VALUE_IF_ABANDONED(void)

sk_sp<const GrCaps> GrContextPriv::refCaps() const {
    return fContext->refCaps();
}

void GrContextPriv::addOnFlushCallbackObject(GrOnFlushCallbackObject* onFlushCBObject) {
    fContext->addOnFlushCallbackObject(onFlushCBObject);
}

GrSemaphoresSubmitted GrContextPriv::flushSurfaces(GrSurfaceProxy* proxies[], int numProxies,
                                                   const GrFlushInfo& info) {
    ASSERT_SINGLE_OWNER
    RETURN_VALUE_IF_ABANDONED(GrSemaphoresSubmitted::kNo)
    GR_CREATE_TRACE_MARKER_CONTEXT("GrContextPriv", "flushSurfaces", fContext);
    SkASSERT(numProxies >= 0);
    SkASSERT(!numProxies || proxies);
    for (int i = 0; i < numProxies; ++i) {
        SkASSERT(proxies[i]);
        ASSERT_OWNED_PROXY(proxies[i]);
    }
    return fContext->drawingManager()->flushSurfaces(
            proxies, numProxies, SkSurface::BackendSurfaceAccess::kNoAccess, info, nullptr);
}

void GrContextPriv::flushSurface(GrSurfaceProxy* proxy) {
    this->flushSurfaces(proxy ? &proxy : nullptr, proxy ? 1 : 0, {});
}

void GrContextPriv::copyRenderTasksFromDDL(sk_sp<const SkDeferredDisplayList> ddl,
                                           GrRenderTargetProxy* newDest) {
    fContext->drawingManager()->copyRenderTasksFromDDL(std::move(ddl), newDest);
}

bool GrContextPriv::compile(const GrProgramDesc& desc, const GrProgramInfo& info) {
    GrGpu* gpu = this->getGpu();
    if (!gpu) {
        return false;
    }

    return gpu->compile(desc, info);
}


//////////////////////////////////////////////////////////////////////////////
#if GR_TEST_UTILS

void GrContextPriv::dumpCacheStats(SkString* out) const {
#if GR_CACHE_STATS
    fContext->fResourceCache->dumpStats(out);
#endif
}

void GrContextPriv::dumpCacheStatsKeyValuePairs(SkTArray<SkString>* keys,
                                                SkTArray<double>* values) const {
#if GR_CACHE_STATS
    fContext->fResourceCache->dumpStatsKeyValuePairs(keys, values);
#endif
}

void GrContextPriv::printCacheStats() const {
    SkString out;
    this->dumpCacheStats(&out);
    SkDebugf("%s", out.c_str());
}

/////////////////////////////////////////////////
void GrContextPriv::resetGpuStats() const {
#if GR_GPU_STATS
    fContext->fGpu->stats()->reset();
#endif
}

void GrContextPriv::dumpGpuStats(SkString* out) const {
#if GR_GPU_STATS
    return fContext->fGpu->stats()->dump(out);
#endif
}

void GrContextPriv::dumpGpuStatsKeyValuePairs(SkTArray<SkString>* keys,
                                              SkTArray<double>* values) const {
#if GR_GPU_STATS
    return fContext->fGpu->stats()->dumpKeyValuePairs(keys, values);
#endif
}

void GrContextPriv::printGpuStats() const {
    SkString out;
    this->dumpGpuStats(&out);
    SkDebugf("%s", out.c_str());
}

/////////////////////////////////////////////////
void GrContextPriv::resetContextStats() const {
#if GR_GPU_STATS
    fContext->stats()->reset();
#endif
}

void GrContextPriv::dumpContextStats(SkString* out) const {
#if GR_GPU_STATS
    return fContext->stats()->dump(out);
#endif
}

void GrContextPriv::dumpContextStatsKeyValuePairs(SkTArray<SkString>* keys,
                                                  SkTArray<double>* values) const {
#if GR_GPU_STATS
    return fContext->stats()->dumpKeyValuePairs(keys, values);
#endif
}

void GrContextPriv::printContextStats() const {
    SkString out;
    this->dumpContextStats(&out);
    SkDebugf("%s", out.c_str());
}

/////////////////////////////////////////////////
sk_sp<SkImage> GrContextPriv::testingOnly_getFontAtlasImage(GrMaskFormat format, unsigned int index) {
    auto atlasManager = this->getAtlasManager();
    if (!atlasManager) {
        return nullptr;
    }

    unsigned int numActiveProxies;
    const GrSurfaceProxyView* views = atlasManager->getViews(format, &numActiveProxies);
    if (index >= numActiveProxies || !views || !views[index].proxy()) {
        return nullptr;
    }

    SkColorType colorType = GrColorTypeToSkColorType(GrMaskFormatToColorType(format));
    SkASSERT(views[index].proxy()->priv().isExact());
    sk_sp<SkImage> image(new SkImage_Gpu(sk_ref_sp(fContext), kNeedNewImageUniqueID,
                                         views[index], colorType, kPremul_SkAlphaType, nullptr));
    return image;
}

void GrContextPriv::testingOnly_purgeAllUnlockedResources() {
    fContext->fResourceCache->purgeAllUnlocked();
}

void GrContextPriv::testingOnly_flushAndRemoveOnFlushCallbackObject(GrOnFlushCallbackObject* cb) {
    fContext->flushAndSubmit();
    fContext->drawingManager()->testingOnly_removeOnFlushCallbackObject(cb);
}
#endif

bool GrContextPriv::validPMUPMConversionExists() {
    ASSERT_SINGLE_OWNER

    // CONTEXT TODO: remove this downcast when this class becomes GrDirectContextPriv
    auto direct = GrAsDirectContext(fContext);
    SkASSERT(direct);

    if (!fContext->fDidTestPMConversions) {
        fContext->fPMUPMConversionsRoundTrip =
                GrConfigConversionEffect::TestForPreservingPMConversions(direct);
        fContext->fDidTestPMConversions = true;
    }

    // The PM<->UPM tests fail or succeed together so we only need to check one.
    return fContext->fPMUPMConversionsRoundTrip;
}

std::unique_ptr<GrFragmentProcessor> GrContextPriv::createPMToUPMEffect(
        std::unique_ptr<GrFragmentProcessor> fp) {
    ASSERT_SINGLE_OWNER
    // We should have already called this->priv().validPMUPMConversionExists() in this case
    SkASSERT(fContext->fDidTestPMConversions);
    // ...and it should have succeeded
    SkASSERT(this->validPMUPMConversionExists());

    return GrConfigConversionEffect::Make(std::move(fp), PMConversion::kToUnpremul);
}

std::unique_ptr<GrFragmentProcessor> GrContextPriv::createUPMToPMEffect(
        std::unique_ptr<GrFragmentProcessor> fp) {
    ASSERT_SINGLE_OWNER
    // We should have already called this->priv().validPMUPMConversionExists() in this case
    SkASSERT(fContext->fDidTestPMConversions);
    // ...and it should have succeeded
    SkASSERT(this->validPMUPMConversionExists());

    return GrConfigConversionEffect::Make(std::move(fp), PMConversion::kToPremul);
}
