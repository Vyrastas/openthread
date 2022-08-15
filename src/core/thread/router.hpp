/*
 *  Copyright (c) 2016, The OpenThread Authors.
 *  All rights reserved.
 *
 *  Redistribution and use in source and binary forms, with or without
 *  modification, are permitted provided that the following conditions are met:
 *  1. Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 *  2. Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *  3. Neither the name of the copyright holder nor the
 *     names of its contributors may be used to endorse or promote products
 *     derived from this software without specific prior written permission.
 *
 *  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 *  AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 *  IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 *  ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 *  LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 *  CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 *  SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 *  INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 *  CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 *  ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 *  POSSIBILITY OF SUCH DAMAGE.
 */

/**
 * @file
 *   This file includes definitions a `Router` node.
 */

#ifndef ROUTER_HPP_
#define ROUTER_HPP_

#include "openthread-core-config.h"

#include <openthread/thread_ftd.h>

#include "thread/child.hpp"
#include "thread/csl_tx_scheduler.hpp"
#include "thread/neighbor.hpp"

namespace ot {

/**
 * This class represents a Thread Router
 *
 */
class Router : public Neighbor
{
public:
    /**
     * This class represents diagnostic information for a Thread Router.
     *
     */
    class Info : public otRouterInfo, public Clearable<Info>
    {
    public:
        /**
         * This method sets the `Info` instance from a given `Router`.
         *
         * @param[in] aRouter   A router.
         *
         */
        void SetFrom(const Router &aRouter);
    };

    /**
     * This method initializes the `Router` object.
     *
     * @param[in] aInstance  A reference to OpenThread instance.
     *
     */
    void Init(Instance &aInstance)
    {
        Neighbor::Init(aInstance);
#if OPENTHREAD_CONFIG_MAC_CSL_RECEIVER_ENABLE
        SetCslClockAccuracy(kCslWorstCrystalPpm);
        SetCslUncertainty(kCslWorstUncertainty);
#endif
    }

    /**
     * This method clears the router entry.
     *
     */
    void Clear(void);

    /**
     * This method gets the router ID of the next hop to this router.
     *
     * @returns The router ID of the next hop to this router.
     *
     */
    uint8_t GetNextHop(void) const { return mNextHop; }

    /**
     * This method sets the router ID of the next hop to this router.
     *
     * @param[in]  aRouterId  The router ID of the next hop to this router.
     *
     */
    void SetNextHop(uint8_t aRouterId) { mNextHop = aRouterId; }

    /**
     * This method gets the link quality out value for this router.
     *
     * @returns The link quality out value for this router.
     *
     */
    LinkQuality GetLinkQualityOut(void) const { return static_cast<LinkQuality>(mLinkQualityOut); }

    /**
     * This method sets the link quality out value for this router.
     *
     * @param[in]  aLinkQuality  The link quality out value for this router.
     *
     */
    void SetLinkQualityOut(LinkQuality aLinkQuality) { mLinkQualityOut = aLinkQuality; }

    /**
     * This method get the route cost to this router.
     *
     * @returns The route cost to this router.
     *
     */
    uint8_t GetCost(void) const { return mCost; }

    /**
     * This method sets the router cost to this router.
     *
     * @param[in]  aCost  The router cost to this router.
     *
     */
    void SetCost(uint8_t aCost) { mCost = aCost; }

#if OPENTHREAD_CONFIG_MAC_CSL_RECEIVER_ENABLE
    /**
     * This method get the CSL clock accuracy of this router.
     *
     * @returns The CSL clock accuracy of this router.
     *
     */
    uint8_t GetCslClockAccuracy(void) const { return mCslClockAccuracy; }

    /**
     * This method sets the CSL clock accuracy of this router.
     *
     * @param[in]  aCslClockAccuracy  The CSL clock accuracy of this router.
     *
     */
    void SetCslClockAccuracy(uint8_t aCslClockAccuracy) { mCslClockAccuracy = aCslClockAccuracy; }

    /**
     * This method get the CSL clock uncertainty of this router.
     *
     * @returns The CSL clock uncertainty of this router.
     *
     */
    uint8_t GetCslUncertainty(void) const { return mCslUncertainty; }

    /**
     * This method sets the CSL clock uncertainty of this router.
     *
     * @param[in]  aCslUncertainty  The CSL clock uncertainty of this router.
     *
     */
    void SetCslUncertainty(uint8_t aCslUncertainty) { mCslUncertainty = aCslUncertainty; }
#endif

private:
    uint8_t mNextHop;            ///< The next hop towards this router
    uint8_t mLinkQualityOut : 2; ///< The link quality out for this router

#if OPENTHREAD_CONFIG_MLE_LONG_ROUTES_ENABLE
    uint8_t mCost; ///< The cost to this router via neighbor router
#else
    uint8_t mCost : 4; ///< The cost to this router via neighbor router
#endif
#if OPENTHREAD_CONFIG_MAC_CSL_RECEIVER_ENABLE
    uint8_t mCslClockAccuracy; ///< Crystal accuracy, in units of ± ppm.
    uint8_t mCslUncertainty;   ///< Scheduling uncertainty, in units of 10 us.
#endif
};

DefineCoreType(otRouterInfo, Router::Info);

} // namespace ot

#endif // ROUTER_HPP_
