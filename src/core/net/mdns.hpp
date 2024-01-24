/*
 *  Copyright (c) 2024, The OpenThread Authors.
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

#ifndef MULTICAST_DNS_HPP_
#define MULTICAST_DNS_HPP_

#include "openthread-core-config.h"

#if OPENTHREAD_CONFIG_MULTICAST_DNS_ENABLE

#include <openthread/mdns.h>
#include <openthread/platform/mdns_socket.h>

#include "common/as_core_type.hpp"
#include "common/clearable.hpp"
#include "common/debug.hpp"
#include "common/equatable.hpp"
#include "common/error.hpp"
#include "common/heap_allocatable.hpp"
#include "common/heap_array.hpp"
#include "common/heap_data.hpp"
#include "common/heap_string.hpp"
#include "common/linked_list.hpp"
#include "common/owned_ptr.hpp"
#include "common/owning_list.hpp"
#include "common/retain_ptr.hpp"
#include "common/timer.hpp"
#include "crypto/sha256.hpp"
#include "net/dns_types.hpp"

/**
 * @file
 *   This file includes definitions for the Multicast DNS per RFC 6762.
 */

namespace ot {
namespace Dns {
namespace Multicast {

extern "C" void otPlatMdnsHandleReceive(otInstance                  *aInstance,
                                        otMessage                   *aMessage,
                                        bool                         aIsUnicast,
                                        const otPlatMdnsAddressInfo *aAddress);

/**
 * Implements Multicast DNS (mDNS) core.
 *
 */
class Core : public InstanceLocator, private NonCopyable
{
    friend void otPlatMdnsHandleReceive(otInstance                  *aInstance,
                                        otMessage                   *aMessage,
                                        bool                         aIsUnicast,
                                        const otPlatMdnsAddressInfo *aAddress);

public:
    /**
     * Initializes a `Core` instance.
     *
     * @param[in] aInstance  The OpenThread instance.
     *
     */
    explicit Core(Instance &aInstance);

    typedef otMdnsRequestId        RequestId;        ///< A request Identifier.
    typedef otMdnsRegisterCallback RegisterCallback; ///< Registration callback.
    typedef otMdnsConflictCallback ConflictCallback; ///< Conflict callback.
    typedef otMdnsHost             HostInfo;         ///< Host information.
    typedef otMdnsService          ServiceInfo;      ///< Service information.
    typedef otMdnsKey              KeyInfo;          ///< Key information.

    /**
     * Represents an address info.
     *
     */
    class AddressInfo : public otPlatMdnsAddressInfo, public Clearable<AddressInfo>, public Equatable<AddressInfo>
    {
    public:
        /**
         * Initializes the `AddressInfo` clearing all the fields.
         *
         */
        AddressInfo(void) { Clear(); }

        /**
         * Gets the IPv6 address.
         *
         * @returns the IPv6 address.
         *
         */
        const Ip6::Address &GetAddress(void) const { return AsCoreType(&mAddress); }
    };

    /**
     * Enables or disables the mDNS module.
     *
     * mDNS module should be enabled before registration any host, service, or key entries. Disabling mDNS will
     * immediately stop all operations and any communication (multicast or unicast tx) and remove any previously
     * registered entries without sending any "goodbye" announcements or invoking their callback.
     *
     * @param[in] aEnabled   Whether to enable or disable.
     *
     */
    void SetEnabled(bool aEnabled);

    /**
     * Indicates whether or not mDNS module is enabled.
     *
     * @retval TRUE   The mDNS module is enabled.
     * @retval FALSE  The mDNS module is disabled.
     *
     */
    bool IsEnabled(void) const { return mIsEnabled; }

    /**
     * Sets whether mDNS module is allowed to send questions requesting unicast responses referred to as "QU" questions.
     *
     * The "QU" question request unicast response in contrast to "QM" questions which request multicast responses.
     * When allowed, the first probe will be sent as a "QU" question.
     *
     * This can be used to address platform limitation where platform cannot accept unicast response received on mDNS
     * port.
     *
     * @param[in] aAllow        Indicates whether or not to allow "QU" questions.
     *
     */
    void SetQuestionUnicastAllowed(bool aAllow) { mIsQuestionUnicastAllowed = aAllow; }

    /**
     * Indicates whether mDNS module is allowed to send "QU" questions requesting unicast response.
     *
     * @retval TRUE  The mDNS module is allowed to send "QU" questions.
     * @retval FALSE The mDNS module is not allowed to send "QU" questions.
     *
     */
    bool IsQuestionUnicastAllowed(void) const { return mIsQuestionUnicastAllowed; }

    /**
     * Sets the conflict callback.
     *
     * @param[in] aCallback  The conflict callback. Can be `nullptr` is not needed.
     *
     */
    void SetConflictCallback(ConflictCallback aCallback) { mConflictCallback = aCallback; }

    /**
     * Registers or updates a host.
     *
     * The fields in @p aHost follow these rules:
     *
     * - The `mHostName` field specifies the host name to register (e.g., "myhost"). MUST not contain the domain name.
     * - The `mAddresses` is array of IPv6 addresses to register with the host. `mNumAddresses` provides the number of
     *   entries in `mAddresses` array.
     * - The `mAddresses` array can be empty with zero `mNumAddresses`. In this case, mDNS will treat it as if host is
     *   unregistered and stop advertising any addresses for this the host name.
     * - The `mTtl` specifies the TTL if non-zero. If zero, the mDNS core will choose a default TTL to use.
     *
     * This method can be called again for the same `mHostName` to update a previously registered host entry, for
     * example, to change the list of addresses of the host. In this case, the mDNS module will send "goodbye"
     * announcements for any previously registered and now removed addresses and announce any newly added addresses.
     *
     * The outcome of the registration request is reported back by invoking the provided @p aCallback with
     * @p aRequestId as its input and one of the following `aError` inputs:
     *
     * - `kErrorNone`       indicates registration was successful
     * - `kErrorDuplicated` indicates a name conflict, i.e., the name is already claimed by another mDNS responder.
     *
     * For caller convenience, the OpenThread mDNS module guarantees that the callback will be invoked after this
     * method returns, even in cases of immediate registration success. The @p aCallback can be `nullptr` if caller
     * does not want to be notified of the outcome.
     *
     * @param[in] aHost         Information about the host to register.
     * @param[in] aRequestId    The ID associated with this request.
     * @param[in] aCallback     The callback function pointer to report the outcome (can be `nullptr` if not needed).
     *
     * @retval kErrorNone          Successfully started registration. @p aCallback will report the outcome.
     * @retval kErrorInvalidState  mDNS module is not enabled.
     *
     */
    Error RegisterHost(const HostInfo &aHostInfo, RequestId aRequestId, RegisterCallback aCallback);

    /**
     * Unregisters a host.
     *
     * The fields in @p aHost follow these rules:
     *
     * - The `mHostName` field specifies the host name to unregister (e.g., "myhost"). MUST not contain the domain name.
     * - The rest of the fields in @p aHost structure are ignored in an `UnregisterHost()` call.
     *
     * If there is no previously registered host with the same name, no action is performed.
     *
     * If there is a previously registered host with the same name, the mDNS module will send "goodbye" announcement
     * for all previously advertised address records.
     *
     * @param[in] aHost         Information about the host to unregister.
     *
     * @retval kErrorNone           Successfully unregistered host.
     * @retval kErrorInvalidState   mDNS module is not enabled.
     *
     */
    Error UnregisterHost(const HostInfo &aHostInfo);

    /**
     * Registers or updates a service.
     *
     * The fields in @p aService follow these rules:
     *
     * - The `mServiceInstance` specifies the service instance label. It is treated as a single DNS label. It may
     *   contain dot `.` character which is allowed in a service instance label.
     * - The `mServiceType` specifies the service type (e.g., "_tst._udp"). It is treated as multiple dot `.` separated
     *   labels. It MUST not contain the domain name.
     * - The `mHostName` field specifies the host name of the service. MUST not contain the domain name.
     * - The `mSubTypeLabels` is an array of strings representing sub-types associated with the service. Each array
     *   entry is a sub-type label. The `mSubTypeLabels can be `nullptr` if there are no sub-types. Otherwise, the
     *   array length is specified by `mSubTypeLabelsLength`.
     * - The `mTxtData` and `mTxtDataLength` specify the encoded TXT data. The `mTxtData` can be `nullptr` or
     *   `mTxtDataLength` can be zero to specify an empty TXT data. In this case mDNS module will use a single zero
     *   byte `[ 0 ]` as empty TXT data.
     * - The `mPort`, `mWeight`, and `mPriority` specify the service's parameters (as specified in DNS SRV record).
     * - The `mTtl` specifies the TTL if non-zero. If zero, the mDNS module will use default TTL for service entry.
     *
     * This method can be called again for the same `mServiceInstance` and `mServiceType` to update a previously
     * registered service entry, for example, to change the sub-types list or update any parameter such as port, weight,
     * priority, TTL, or host name. The mDNS module will send announcements for any changed info, e.g., will send
     * "goodbye" announcements for any removed sub-types and announce any newly added sub-types.
     *
     * Regarding the invocation of the @p aCallback, this method behaves in the same way as described in
     * `RegisterHost()`.
     *
     * @param[in] aService      Information about the service to register.
     * @param[in] aRequestId    The ID associated with this request.
     * @param[in] aCallback     The callback function pointer to report the outcome (can be `nullptr` if not needed).
     *
     * @retval kErrorNone           Successfully started registration. @p aCallback will report the outcome.
     * @retval kErrorInvalidState   mDNS module is not enabled.
     *
     */
    Error RegisterService(const ServiceInfo &aServiceInfo, RequestId aRequestId, RegisterCallback aCallback);

    /**
     * Unregisters a service.
     *
     * The fields in @p aService follow these rules:

     * - The `mServiceInstance` specifies the service instance label. It is treated as a single DNS label. It may
     *   contain dot `.` character which is allowed in a service instance label.
     * - The `mServiceType` specifies the service type (e.g., "_tst._udp"). It is treated as multiple dot `.` separated
     *   labels. It MUST not contain the domain name.
     * - The rest of the fields in @p aService structure are ignored in  a`otMdnsUnregisterService()` call.
     *
     * If there is no previously registered service with the same name, no action is performed.
     *
     * If there is a previously registered service with the same name, the mDNS module will send "goodbye"
     * announcements for all related records.
     *
     * @param[in] aService      Information about the service to unregister.
     *
     * @retval kErrorNone            Successfully unregistered service.
     * @retval kErrorInvalidState    mDNS module is not enabled.
     *
     */
    Error UnregisterService(const ServiceInfo &aServiceInfo);

    /**
     * Registers or updates a key record.
     *
     * The fields in @p aKey follow these rules:
     *
     * - If the key is associated with a host entry, `mName` specifies the host name & `mServcieType` MUST be `nullptr`.
     * - If the key is associated with a service entry, `mName` specifies the service instance label (always treated as
     *   a single label) and `mServiceType` specifies the service type (e.g. "_tst._udp"). In this case the DNS name
     *   for key record is `<mName>.<mServiceTye>`.
     * - The `mKeyData` field contains the key record's data with `mKeyDataLength` as its length in byes.
     * - The `mTtl` specifies the TTL if non-zero. If zero, the mDNS module will use default TTL for the key entry.
     *
     * This method can be called again for the same name to updated a previously registered key entry, for example,
     * to change the key data or TTL.
     *
     * Regarding the invocation of the @p aCallback, this method behaves in the same way as described in
     * `RegisterHost()`.
     *
     * @param[in] aHost         Information about the key record to register.
     * @param[in] aRequestId    The ID associated with this request.
     * @param[in] aCallback     The callback function pointer to report the outcome (can be `nullptr` if not needed).
     *
     * @retval kErrorNone            Successfully started registration. @p aCallback will report the outcome.
     * @retval kErrorInvalidState    mDNS module is not enabled.
     *
     */
    Error RegisterKey(const KeyInfo &aKeyInfo, RequestId aRequestId, RegisterCallback aCallback);

    /**
     * Unregisters a key record on mDNS.
     *
     * The fields in @p aKey follow these rules:
     *
     * - If the key is associated with a host entry, `mName` specifies the host name & `mServcieType` MUST be `nullptr`.
     * - If the key is associated with a service entry, `mName` specifies the service instance label (always treated as
     *   a single label) and `mServiceType` specifies the service type (e.g. "_tst._udp"). In this case the DNS name
     *   for key record is `<mName>.<mServiceTye>`.
     * - The rest of the fields in @p aKey structure are ignored in  a`otMdnsUnregisterKey()` call.
     *
     * If there is no previously registered key with the same name, no action is performed.
     *
     * If there is a previously registered key with the same name, the mDNS module will send "goodbye" announcements
     * for the key record.
     *
     * @param[in] aKey          Information about the key to unregister.
     *
     * @retval kErrorNone            Successfully unregistered key
     * @retval kErrorInvalidState    mDNS module is not enabled.
     *
     */
    Error UnregisterKey(const KeyInfo &aKeyInfo);

    /**
     * Sets the max size threshold for mDNS messages.
     *
     * This method is mainly intended for testing. The max size threshold is used to break larger messages.
     *
     * @param[in] aMaxSize  The max message size threshold.
     *
     */
    void SetMaxMessageSize(uint16_t aMaxSize) { mMaxMessageSize = aMaxSize; }

private:
    // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

    static constexpr uint16_t kUdpPort = 5353;

    static constexpr bool kDefaultQuAllowed = OPENTHREAD_CONFIG_MULTICAST_DNS_DEFAULT_QUESTION_UNICAST_ALLOWED;

    static constexpr uint32_t kMaxMessageSize = 1200;

    static constexpr uint8_t  kNumberOfProbes   = 3;
    static constexpr uint32_t kInitalProbeDelay = 20;  // In msec
    static constexpr uint32_t kProbeWaitTime    = 250; // In msec

    static constexpr uint8_t  kNumberOfAnnounces = 3;
    static constexpr uint32_t kAnnounceInterval  = 1000; // In msec - time between first two announces

    static constexpr uint32_t kUnspecifiedTtl = 0;
    static constexpr uint32_t kDefaultTtl     = 120;
    static constexpr uint32_t kDefaultKeyTtl  = kDefaultTtl;
    static constexpr uint32_t kNsecTtl        = 4500;
    static constexpr uint32_t kServicesPtrTtl = 4500;

    static constexpr uint16_t kClassQuestionUnicastFlag = (1U << 15);
    static constexpr uint16_t kClassCacheFlushFlag      = (1U << 15);
    static constexpr uint16_t kClassMask                = (0x7fff);

    static constexpr uint16_t kUnspecifiedOffset = 0;

    static constexpr uint8_t kNumSections = 4;

    // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

    enum Section : uint8_t
    {
        kQuestionSection,
        kAnswerSection,
        kAuthoritySection,
        kAdditionalDataSection,
    };

    enum AppendOutcome : uint8_t
    {
        kAppendedFullNameAsCompressed,
        kAppendedLabels,
    };

    // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
    // Forward declarations

    class Context;
    class TxMessage;
    class RxMessage;
    class ServiceEntry;
    class ServiceType;

    // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

    struct EmptyChecker
    {
        // Used in `Matches()` to find empty entries (with no record) to remove and free.
    };

    // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

    struct ExpireChecker
    {
        // Used in `Matches()` to find expired entries in a list.

        explicit ExpireChecker(TimeMilli aNow) { mNow = aNow; }

        TimeMilli mNow;
    };

    // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

    class Callback : public Clearable<Callback>
    {
    public:
        Callback(void) { Clear(); }
        Callback(RequestId aRequestId, RegisterCallback aCallback);

        bool IsEmpty(void) const { return (mCallback == nullptr); }
        void InvokeAndClear(Instance &aInstance, Error aError);

    private:
        RequestId        mRequestId;
        RegisterCallback mCallback;
    };

    // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

    class RecordCounts : public Clearable<RecordCounts>
    {
    public:
        RecordCounts(void) { Clear(); }

        uint16_t GetFor(Section aSection) const { return mCounts[aSection]; }
        void     Increment(Section aSection) { mCounts[aSection]++; }
        void     ReadFrom(const Header &aHeader);
        void     WriteTo(Header &aHeader) const;
        bool     IsEmpty(void) const;

    private:
        uint16_t mCounts[kNumSections];
    };

    // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

    struct AnswerInfo
    {
        uint16_t  mQuestionRrType;
        TimeMilli mAnswerTime;
        bool      mIsProbe;
        bool      mUnicastResponse;
    };

    // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

    class AddressArray : public Heap::Array<Ip6::Address>
    {
    public:
        bool Matches(const Ip6::Address *aAddresses, uint16_t aNumAddresses) const;
        void SetFrom(const Ip6::Address *aAddresses, uint16_t aNumAddresses);
    };

    // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

    class FireTime
    {
    public:
        FireTime(void) { ClearFireTime(); }
        void      ClearFireTime(void) { mHasFireTime = false; }
        bool      HasFireTime(void) const { return mHasFireTime; }
        TimeMilli GetFireTime(void) const { return mFireTime; }
        void      SetFireTime(TimeMilli aFireTime);

    protected:
        void ScheduleFireTimeOn(TimerMilli &aTimer);

    private:
        TimeMilli mFireTime;
        bool      mHasFireTime;
    };

    // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

    class RecordInfo : public Clearable<RecordInfo>, private NonCopyable
    {
    public:
        // Keeps track of record state and timings.

        RecordInfo(void) { Clear(); }

        bool     IsPresent(void) const { return mIsPresent; }
        uint32_t GetTtl(void) const { return mTtl; }

        template <typename UintType> void UpdateProperty(UintType &aProperty, UintType aValue);
        void UpdateProperty(AddressArray &aAddrProperty, const Ip6::Address *aAddrs, uint16_t aNumAddrs);
        void UpdateProperty(Heap::String &aStringProperty, const char *aString);
        void UpdateProperty(Heap::Data &aDataProperty, const uint8_t *aData, uint16_t aLength);
        void UpdateTtl(uint32_t aTtl);

        void     StartAnnouncing(void);
        bool     ShouldAppendTo(TxMessage &aResponse, TimeMilli aNow) const;
        bool     CanAnswer(void) const;
        void     ScheduleAnswer(const AnswerInfo &aInfo);
        void     UpdateStateAfterAnswer(const TxMessage &aResponse);
        void     UpdateFireTimeOn(FireTime &aFireTime);
        uint32_t GetDurationSinceLastMulticast(TimeMilli aTime) const;
        Error    GetLastMulticastTime(TimeMilli &aLastMulticastTime) const;

        // `AppendState` methods: Used to track whether the record
        // is appended in a message, or needs to be appended in
        // Additional Data section.

        void MarkAsNotAppended(void) { mAppendState = kNotAppended; }
        void MarkAsAppended(TxMessage &aTxMessage, Section aSection);
        void MarkToAppendInAdditionalData(void);
        bool IsAppended(void) const;
        bool CanAppend(void) const;
        bool ShouldAppnedInAdditionalDataSection(void) const { return (mAppendState == kToAppendInAdditionalData); }

    private:
        enum AppendState : uint8_t
        {
            kNotAppended,
            kToAppendInAdditionalData,
            kAppendedInMulticastMsg,
            kAppendedInUnicastMsg,
        };

        static constexpr uint32_t kMinIntervalBetweenMulticast = 1000; // msec
        static constexpr uint32_t kLastMulticastTimeAge        = 10 * Time::kOneHourInMsec;

        static_assert(kNotAppended == 0, "kNotAppended MUST be zero, so `Clear()` works correctly");

        bool        mIsPresent : 1;
        bool        mMulticastAnswerPending : 1;
        bool        mUnicastAnswerPending : 1;
        bool        mIsLastMulticastValid : 1;
        uint8_t     mAnnounceCounter;
        AppendState mAppendState;
        Section     mAppendSection;
        uint32_t    mTtl;
        TimeMilli   mAnnounceTime;
        TimeMilli   mAnswerTime;
        TimeMilli   mLastMulticastTime;
    };

    // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

    class Entry : public InstanceLocatorInit, public FireTime, private NonCopyable
    {
        // Base class for `HostEntry` and `ServiceEntry`.

        friend class ServiceType;

    public:
        enum State : uint8_t
        {
            kProbing,
            kRegistered,
            kConflict,
            kRemoving,
        };

        State GetState(void) const { return mState; }
        void  Register(const KeyInfo &aKeyInfo, const Callback &aCallback);
        void  Unregister(const KeyInfo &aKeyInfo);
        void  InvokeCallbacks(void);
        void  ClearAppendState(void);

    protected:
        static constexpr uint32_t kMinIntervalProbeResponse = 250; // msec
        static constexpr uint8_t  kTypeArraySize            = 8;   // We can have SRV, TXT and KEY today.

        struct TypeArray : public Array<uint16_t, kTypeArraySize> // Array of record types for NSEC record
        {
            void Add(uint16_t aType) { SuccessOrAssert(PushBack(aType)); }
        };

        struct RecordAndType
        {
            RecordInfo &mRecord;
            uint16_t    mType;
        };

        typedef void (*NameAppender)(Entry &aEntry, TxMessage &aTxMessage, Section aSection);

        Entry(void);
        void Init(Instance &aInstance);
        void SetCallback(const Callback &aCallback);
        void ClearCallback(void) { mCallback.Clear(); }
        void StartProbing(void);
        void SetStateToConflict(void);
        void SetStateToRemoving(void);
        void UpdateRecordsState(const TxMessage &aResponse);
        void AppendQuestionTo(TxMessage &aTxMessage) const;
        void AppendKeyRecordTo(TxMessage &aTxMessage, Section aSection, NameAppender aNameAppender);
        void AppendNsecRecordTo(TxMessage       &aTxMessage,
                                Section          aSection,
                                const TypeArray &aTypes,
                                NameAppender     aNameAppender);
        bool ShouldAnswerNsec(TimeMilli aNow) const;
        void DetermineNextFireTime(void);
        void ScheduleTimer(void);
        void AnswerProbe(const AnswerInfo &aInfo, RecordAndType *aRecords, uint16_t aRecordsLength);
        void AnswerNonProbe(const AnswerInfo &aInfo, RecordAndType *aRecords, uint16_t aRecordsLength);
        void ScheduleNsecAnswer(const AnswerInfo &aInfo);

        template <typename EntryType> void HandleTimer(Context &aContext);

        RecordInfo mKeyRecord;

    private:
        void SetState(State aState);
        void ClearKey(void);
        void ScheduleCallbackTask(void);
        void CheckMessageSizeLimitToPrepareAgain(TxMessage &aTxMessage, bool &aPrepareAgain);

        State      mState;
        uint8_t    mProbeCount;
        bool       mMulticastNsecPending : 1;
        bool       mUnicastNsecPending : 1;
        bool       mAppendedNsec : 1;
        TimeMilli  mNsecAnswerTime;
        Heap::Data mKeyData;
        Callback   mCallback;
        Callback   mKeyCallback;
    };

    // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

    class HostEntry : public Entry, public LinkedListEntry<HostEntry>, public Heap::Allocatable<HostEntry>
    {
        friend class LinkedListEntry<HostEntry>;
        friend class Entry;
        friend class ServiceEntry;

    public:
        HostEntry(void);
        Error Init(Instance &aInstance, const HostInfo &aHostInfo) { return Init(aInstance, aHostInfo.mHostName); }
        Error Init(Instance &aInstance, const KeyInfo &aKeyInfo) { return Init(aInstance, aKeyInfo.mName); }
        bool  IsEmpty(void) const;
        bool  Matches(const Name &aName) const;
        bool  Matches(const HostInfo &aHostInfo) const;
        bool  Matches(const KeyInfo &aKeyInfo) const;
        bool  Matches(const Heap::String &aName) const;
        bool  Matches(State aState) const { return GetState() == aState; }
        bool  Matches(const HostEntry &aEntry) const { return (this == &aEntry); }
        void  Register(const HostInfo &aHostInfo, const Callback &aCallback);
        void  Register(const KeyInfo &aKeyInfo, const Callback &aCallback);
        void  Unregister(const HostInfo &aHostInfo);
        void  Unregister(const KeyInfo &aKeyInfo);
        void  AnswerQuestion(const AnswerInfo &aInfo);
        void  HandleTimer(Context &aContext);
        void  ClearAppendState(void);
        void  PrepareResponse(TxMessage &aResponse, TimeMilli aNow);
        void  HandleConflict(void);

    private:
        Error Init(Instance &aInstance, const char *aName);
        void  ClearHost(void);
        void  ScheduleToRemoveIfEmpty(void);
        void  PrepareProbe(TxMessage &aProbe);
        void  StartAnnouncing(void);
        void  PrepareResponseRecords(TxMessage &aResponse, TimeMilli aNow);
        void  UpdateRecordsState(const TxMessage &aResponse);
        void  DetermineNextFireTime(void);
        void  AppendAddressRecordsTo(TxMessage &aTxMessage, Section aSection);
        void  AppendKeyRecordTo(TxMessage &aTxMessage, Section aSection);
        void  AppendNsecRecordTo(TxMessage &aTxMessage, Section aSection);
        void  AppendNameTo(TxMessage &aTxMessage, Section aSection);

        static void AppendEntryName(Entry &aEntry, TxMessage &aTxMessage, Section aSection);

        HostEntry   *mNext;
        Heap::String mName;
        RecordInfo   mAddrRecord;
        AddressArray mAddresses;
        uint16_t     mNameOffset;
    };

    // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

    class ServiceEntry : public Entry, public LinkedListEntry<ServiceEntry>, public Heap::Allocatable<ServiceEntry>
    {
        friend class LinkedListEntry<ServiceEntry>;
        friend class Entry;
        friend class ServiceType;

    public:
        ServiceEntry(void);
        Error Init(Instance &aInstance, const ServiceInfo &aServiceInfo);
        Error Init(Instance &aInstance, const KeyInfo &aKeyInfo);
        bool  IsEmpty(void) const;
        bool  Matches(const Name &aName) const;
        bool  Matches(const ServiceInfo &aServiceInfo) const;
        bool  Matches(const KeyInfo &aKeyInfo) const;
        bool  Matches(State aState) const { return GetState() == aState; }
        bool  Matches(const ServiceEntry &aEntry) const { return (this == &aEntry); }
        bool  MatchesServiceType(const Name &aServiceType) const;
        bool  CanAnswerSubType(const char *aSubLabel) const;
        void  Register(const ServiceInfo &aServiceInfo, const Callback &aCallback);
        void  Register(const KeyInfo &aKeyInfo, const Callback &aCallback);
        void  Unregister(const ServiceInfo &aServiceInfo);
        void  Unregister(const KeyInfo &aKeyInfo);
        void  AnswerServiceNameQuestion(const AnswerInfo &aInfo);
        void  AnswerServiceTypeQuestion(const AnswerInfo &aInfo, const char *aSubLabel);
        bool  ShouldSuppressKnownAnswer(uint32_t aTtl, const char *aSubLabel) const;
        void  HandleTimer(Context &aContext);
        void  ClearAppendState(void);
        void  PrepareResponse(TxMessage &aResponse, TimeMilli aNow);
        void  HandleConflict(void);

    private:
        class SubType : public LinkedListEntry<SubType>, public Heap::Allocatable<SubType>, private ot::NonCopyable
        {
        public:
            Error Init(const char *aLabel);
            bool  Matches(const char *aLabel) const { return NameMatch(mLabel, aLabel); }
            bool  Matches(const EmptyChecker &aChecker) const;
            bool  IsContainedIn(const ServiceInfo &aServiceInfo) const;

            SubType     *mNext;
            Heap::String mLabel;
            RecordInfo   mPtrRecord;
            uint16_t     mSubServiceNameOffset;
        };

        Error Init(Instance &aInstance, const char *aServiceInstance, const char *aServiceType);
        void  ClearService(void);
        void  ScheduleToRemoveIfEmpty(void);
        void  PrepareProbe(TxMessage &aProbe);
        void  StartAnnouncing(void);
        void  PrepareResponseRecords(TxMessage &aResponse, TimeMilli aNow);
        void  UpdateRecordsState(const TxMessage &aResponse);
        void  DetermineNextFireTime(void);
        void  DiscoverOffsetsAndHost(HostEntry *&aHost);
        void  UpdateServiceTypes(void);
        void  AppendSrvRecordTo(TxMessage &aTxMessage, Section aSection);
        void  AppendTxtRecordTo(TxMessage &aTxMessage, Section aSection);
        void  AppendPtrRecordTo(TxMessage &aTxMessage, Section aSection, SubType *aSubType = nullptr);
        void  AppendKeyRecordTo(TxMessage &aTxMessage, Section aSection);
        void  AppendNsecRecordTo(TxMessage &aTxMessage, Section aSection);
        void  AppendServiceNameTo(TxMessage &TxMessage, Section aSection);
        void  AppendServiceTypeTo(TxMessage &aTxMessage, Section aSection);
        void  AppendSubServiceTypeTo(TxMessage &aTxMessage, Section aSection);
        void  AppendSubServiceNameTo(TxMessage &aTxMessage, Section aSection, SubType &aSubType);
        void  AppendHostNameTo(TxMessage &aTxMessage, Section aSection);

        static void AppendEntryName(Entry &aEntry, TxMessage &aTxMessage, Section aSection);

        static const uint8_t kEmptyTxtData[];

        ServiceEntry       *mNext;
        Heap::String        mServiceInstance;
        Heap::String        mServiceType;
        RecordInfo          mPtrRecord;
        RecordInfo          mSrvRecord;
        RecordInfo          mTxtRecord;
        OwningList<SubType> mSubTypes;
        Heap::String        mHostName;
        Heap::Data          mTxtData;
        uint16_t            mPriority;
        uint16_t            mWeight;
        uint16_t            mPort;
        uint16_t            mServiceNameOffset;
        uint16_t            mServiceTypeOffset;
        uint16_t            mSubServiceTypeOffset;
        uint16_t            mHostNameOffset;
        bool                mIsAddedInServiceTypes;
    };

    // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

    class ServiceType : public InstanceLocatorInit,
                        public FireTime,
                        public LinkedListEntry<ServiceType>,
                        public Heap::Allocatable<ServiceType>,
                        private NonCopyable
    {
        // Track a service type to answer to `_services._dns-sd._udp.local`
        // queries.

        friend class LinkedListEntry<ServiceType>;

    public:
        Error    Init(Instance &aInstance, const char *aServiceType);
        bool     Matches(const Name &aServcieTypeName) const;
        bool     Matches(const Heap::String &aServiceType) const;
        bool     Matches(const ServiceType &aServiceType) const { return (this == &aServiceType); }
        void     IncrementNumEntries(void) { mNumEntries++; }
        void     DecrementNumEntries(void) { mNumEntries--; }
        uint16_t GetNumEntries(void) const { return mNumEntries; }
        void     ClearAppendState(void);
        void     AnswerQuestion(const AnswerInfo &aInfo);
        bool     ShouldSuppressKnownAnswer(uint32_t aTtl) const;
        void     HandleTimer(Context &aContext);
        void     PrepareResponse(TxMessage &aResponse, TimeMilli aNow);

    private:
        void PrepareResponseRecords(TxMessage &aResponse, TimeMilli aNow);
        void AppendPtrRecordTo(TxMessage &aResponse, uint16_t aServiceTypeOffset);

        ServiceType *mNext;
        Heap::String mServiceType;
        RecordInfo   mServicesPtr;
        uint16_t     mNumEntries; // Number of service entries providing this service type.
    };

    // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

    class TxMessage : public InstanceLocator
    {
    public:
        enum Type : uint8_t
        {
            kMulticastProbe,
            kMulticastQuery,
            kMulticastResponse,
            kUnicastResponse,
        };

        TxMessage(Instance &aInstance, Type aType);
        TxMessage(Instance &aInstance, Type aType, const AddressInfo &aUnicastDest);
        Type          GetType(void) const { return mType; }
        Message      &SelectMessageFor(Section aSection);
        AppendOutcome AppendLabel(Section aSection, const char *aLabel, uint16_t &aCompressOffset);
        AppendOutcome AppendMultipleLabels(Section aSection, const char *aLabels, uint16_t &aCompressOffset);
        void          AppendServiceType(Section aSection, const char *aServiceType, uint16_t &aCompressOffset);
        void          AppendDomainName(Section aSection);
        void          AppendServicesDnssdName(Section aSection);
        void          IncrementRecordCount(Section aSection) { mRecordCounts.Increment(aSection); }
        void          CheckSizeLimitToPrepareAgain(bool &aPrepareAgain);
        void          SaveCurrentState(void);
        void          RestoreToSavedState(void);
        void          Send(void);

    private:
        static constexpr bool kIsSingleLabel = true;

        void          Init(Type aType);
        void          Reinit(void);
        bool          IsOverSizeLimit(void) const;
        AppendOutcome AppendLabels(Section     aSection,
                                   const char *aLabels,
                                   bool        aIsSingleLabel,
                                   uint16_t   &aCompressOffset);
        bool          ShouldClearAppendStateOnReinit(const Entry &aEntry) const;

        static void SaveOffset(uint16_t &aCompressOffset, const Message &aMessage, Section aSection);

        RecordCounts      mRecordCounts;
        OwnedPtr<Message> mMsgPtr;
        OwnedPtr<Message> mAuthorityMsgPtr;
        OwnedPtr<Message> mAdditionalMsgPtr;
        RecordCounts      mSavedRecordCounts;
        uint16_t          mSavedMsgLength;
        uint16_t          mSavedAuthorityLength;
        uint16_t          mSavedAdditionalLength;
        uint16_t          mDomainOffset;        // Offset for domain name `.local.` for name compression.
        uint16_t          mUdpOffset;           // Offset to `_udp.local.`
        uint16_t          mTcpOffset;           // Offset to `_tcp.local.`
        uint16_t          mServicesDnssdOffset; // Offset to `_services._dns-sd`
        AddressInfo       mUnicastDest;
        Type              mType;
    };

    // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

    class Context : public InstanceLocator // Used by `HandleEntryTimer`.
    {
    public:
        Context(Instance &aInstance);

        TimeMilli  GetNow(void) const { return mNow; }
        TimeMilli  GetNextTime(void) const { return mNextTime; }
        void       UpdateNextTime(TimeMilli aTime);
        TxMessage &GetProbeMessage(void) { return mProbeMessage; }
        TxMessage &GetResponseMessage(void) { return mResponseMessage; }

    private:
        TimeMilli mNow;
        TimeMilli mNextTime;
        TxMessage mProbeMessage;
        TxMessage mResponseMessage;
    };

    // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

    class RxMessage : public InstanceLocatorInit,
                      public Heap::Allocatable<RxMessage>,
                      public LinkedListEntry<RxMessage>,
                      private NonCopyable
    {
        friend class LinkedListEntry<RxMessage>;

    public:
        enum ProcessOutcome : uint8_t
        {
            kProcessed,
            kSaveAsMultiPacket,
        };

        Error               Init(Instance          &aInstance,
                                 OwnedPtr<Message> &aMessagePtr,
                                 bool               aIsUnicast,
                                 const AddressInfo &aSenderAddress);
        bool                IsQuery(void) const { return mIsQuery; }
        bool                IsTruncated(void) const { return mTruncated; }
        bool                IsSelfOriginating(void) const { return mIsSelfOriginating; }
        const RecordCounts &GetRecordCounts(void) const { return mRecordCounts; }
        const AddressInfo  &GetSenderAddress(void) const { return mSenderAddress; }
        void                ClearProcessState(void);
        ProcessOutcome      ProcessQuery(bool aShouldProcessTruncated);
        void                ProcessResponse(void);

    private:
        struct Question : public Clearable<Question>
        {
            Question(void) { Clear(); }
            void ClearProcessState(void);

            Entry   *mEntry;                     // Entry which can provide answer (if any).
            uint16_t mNameOffset;                // Offset to start of question name.
            uint16_t mRrType;                    // The question record type.
            bool     mIsRrClassInternet : 1;     // Is the record class Internet or Any.
            bool     mIsProbe : 1;               // Is a probe (contains a matching record in Authority section).
            bool     mUnicastResponse : 1;       // Is QU flag set (requesting a unicast response).
            bool     mCanAnswer : 1;             // Can provide answer for this question
            bool     mIsUnique : 1;              // Is unique record (vs a shared record).
            bool     mIsForService : 1;          // Is for a `ServiceEntry` (vs a `HostEntry`).
            bool     mIsServiceType : 1;         // Is for service type or sub-type of a `ServiceEntry`.
            bool     mIsForAllServicesDnssd : 1; // Is for "_services._dns-sd._udp" (all service types).
        };

        static constexpr uint32_t kMinResponseDelay = 20;  // msec
        static constexpr uint32_t kMaxResponseDelay = 120; // msec

        void ProcessQuestion(Question &aQuestion);
        void AnswerQuestion(const Question &aQuestion, TimeMilli aAnswerTime);
        void AnswerServiceTypeQuestion(const Question &aQuestion, const AnswerInfo &aInfo, ServiceEntry &aFirstEntry);
        bool ShouldSuppressKnownAnswer(const Name         &aServiceType,
                                       const char         *aSubLabel,
                                       const ServiceEntry &aServiceEntry) const;
        bool ParseQuestionNameAsSubType(const Question    &aQuestion,
                                        Name::LabelBuffer &aSubLabel,
                                        Name              &aServiceType) const;
        void AnswerAllServicesQuestion(const Question &aQuestion, const AnswerInfo &aInfo);
        bool ShouldSuppressKnownAnswer(const Question &aQuestion, const ServiceType &aServiceType) const;
        void SendUnicastResponse(const AddressInfo &aUnicastDest);

        RxMessage            *mNext;
        OwnedPtr<Message>     mMessagePtr;
        Heap::Array<Question> mQuestions;
        AddressInfo           mSenderAddress;
        RecordCounts          mRecordCounts;
        uint16_t              mStartOffset[kNumSections];
        bool                  mIsQuery : 1;
        bool                  mIsUnicast : 1;
        bool                  mTruncated : 1;
        bool                  mIsSelfOriginating : 1;
    };

    // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

    void HandleMultiPacketTimer(void) { mMultiPacketRxMessages.HandleTimer(); }

    class MultiPacketRxMessages : public InstanceLocator
    {
    public:
        explicit MultiPacketRxMessages(Instance &aInstance);

        void AddToExisting(OwnedPtr<RxMessage> &aRxMessagePtr);
        void AddNew(OwnedPtr<RxMessage> &aRxMessagePtr);
        void HandleTimer(void);
        void Clear(void);

    private:
        static constexpr uint32_t kMinProcessDelay = 400; // msec
        static constexpr uint32_t kMaxProcessDelay = 500; // msec
        static constexpr uint16_t kMaxNumMessages  = 10;

        struct RxMsgEntry : public InstanceLocator,
                            public LinkedListEntry<RxMsgEntry>,
                            public Heap::Allocatable<RxMsgEntry>,
                            private NonCopyable
        {
            explicit RxMsgEntry(Instance &aInstance);

            bool Matches(const AddressInfo &aAddress) const;
            bool Matches(const ExpireChecker &aExpireChecker) const;
            void Add(OwnedPtr<RxMessage> &aRxMessagePtr);

            OwningList<RxMessage> mRxMessages;
            TimeMilli             mProcessTime;
            RxMsgEntry           *mNext;
        };

        using MultiPacketTimer = TimerMilliIn<Core, &Core::HandleMultiPacketTimer>;

        OwningList<RxMsgEntry> mRxMsgEntries;
        MultiPacketTimer       mTimer;
    };

    // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

    void HandleTxMessageHistoryTimer(void) { mTxMessageHistory.HandleTimer(); }

    class TxMessageHistory : public InstanceLocator
    {
    public:
        explicit TxMessageHistory(Instance &aInstance);
        void Clear(void);
        void Add(const Message &aMessage);
        bool Contains(const Message &aMessage) const;
        void HandleTimer(void);

    private:
        static constexpr uint32_t kExpireInterval = TimeMilli::SecToMsec(10); // in msec

        typedef Crypto::Sha256::Hash Hash;

        struct HashEntry : public LinkedListEntry<HashEntry>, public Heap::Allocatable<HashEntry>
        {
            bool Matches(const Hash &aHash) const { return aHash == mHash; }
            bool Matches(const ExpireChecker &aExpireChecker) const { return mExpireTime <= aExpireChecker.mNow; }

            HashEntry *mNext;
            Hash       mHash;
            TimeMilli  mExpireTime;
        };

        static void CalculateHash(const Message &aMessage, Hash &aHash);

        using TxMsgHistoryTimer = TimerMilliIn<Core, &Core::HandleTxMessageHistoryTimer>;

        OwningList<HashEntry> mHashEntries;
        TxMsgHistoryTimer     mTimer;
    };

    // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

    template <typename EntryType> OwningList<EntryType> &GetList(void);
    template <typename EntryType, typename ItemInfo>
    Error Register(const ItemInfo &aItemInfo, RequestId aRequestId, RegisterCallback aCallback);
    template <typename EntryType, typename ItemInfo> Error Unregister(const ItemInfo &aItemInfo);

    void InvokeConflictCallback(const char *aName, const char *aServiceType);
    void HandleMessage(Message &aMessage, bool aIsUnicast, const AddressInfo &aSenderAddress);
    void RemoveEmptyEntries(void);
    void HandleEntryTimer(void);
    void HandleEntryTask(void);

    static bool     IsKeyInfoForService(const KeyInfo &aKeyInfo) { return aKeyInfo.mServiceType != nullptr; }
    static uint32_t DetermineTtl(uint32_t aTtl, uint32_t aDefaultTtl);
    static bool     NameMatch(const Heap::String &aHeapString, const char *aName);
    static bool     NameMatch(const Heap::String &aFirst, const Heap::String &aSecond);
    static void     UpdateCacheFlushFlagIn(ResourceRecord &aResourceRecord, Section aSection);
    static void     UpdateRecordLengthInMessage(ResourceRecord &aRecord, Message &aMessage, uint16_t aOffset);
    static void     UpdateCompressOffset(uint16_t &aOffset, uint16_t aNewOffse);
    static bool     QuestionMatches(uint16_t aQuestionRrType, uint16_t aRrType);

    using EntryTimer = TimerMilliIn<Core, &Core::HandleEntryTimer>;
    using EntryTask  = TaskletIn<Core, &Core::HandleEntryTask>;

    static const char kLocalDomain[];         // "local."
    static const char kUdpServiceLabel[];     // "_udp"
    static const char kTcpServiceLabel[];     // "_tcp"
    static const char kSubServiceLabel[];     // "_sub"
    static const char kServciesDnssdLabels[]; // "_services._dns-sd._udp"

    bool                     mIsEnabled;
    bool                     mIsQuestionUnicastAllowed;
    uint16_t                 mMaxMessageSize;
    OwningList<HostEntry>    mHostEntries;
    OwningList<ServiceEntry> mServiceEntries;
    OwningList<ServiceType>  mServiceTypes;
    MultiPacketRxMessages    mMultiPacketRxMessages;
    EntryTimer               mEntryTimer;
    EntryTask                mEntryTask;
    TxMessageHistory         mTxMessageHistory;
    ConflictCallback         mConflictCallback;
};

// Specializations of `Core::GetList()` for `HostEntry` and `ServcieEntry`:

template <> inline OwningList<Core::HostEntry>    &Core::GetList<Core::HostEntry>(void) { return mHostEntries; }
template <> inline OwningList<Core::ServiceEntry> &Core::GetList<Core::ServiceEntry>(void) { return mServiceEntries; }

} // namespace Multicast
} // namespace Dns

DefineCoreType(otPlatMdnsAddressInfo, Dns::Multicast::Core::AddressInfo);

} // namespace ot

#endif // OPENTHREAD_CONFIG_MULTICAST_DNS_ENABLE

#endif // MULTICAST_DNS_HPP_
