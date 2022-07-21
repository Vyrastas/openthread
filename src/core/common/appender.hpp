/*
 *  Copyright (c) 2021, The OpenThread Authors.
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
 *  This file defines OpenThread `Appender` class.
 */

#ifndef APPENDER_HPP_
#define APPENDER_HPP_

#include "openthread-core-config.h"

#include "common/data.hpp"
#include "common/message.hpp"
#include "common/type_traits.hpp"

namespace ot {

/**
 * The `Appender` class acts as a wrapper over either a `Message` or a data buffer and provides different flavors of
 * `Append()` method.
 *
 * This class helps in construction of message content where the destination can be either a `Message` or a buffer.
 *
 */
class Appender
{
public:
    /**
     * This enumeration represent the `Appender` Type (whether appending to a `Message` or data buffer).
     *
     */
    enum Type : uint8_t
    {
        kMessage, ///< `Appender` appends to a `Message`
        kBuffer,  ///< `Appender` appends to a buffer.
    };

    /**
     * This constructor initializes the `Appender` to append to a `Message`.
     *
     * New content is appended to the end of @p aMessage, growing its length.
     *
     * @param[in] aMessage   The message to append to.
     *
     */
    explicit Appender(Message &aMessage);

    /**
     * This constructor initializes the `Appender` to append in a given a buffer
     *
     * New content is append in the buffer starting from @p aBuffer up to is size @p aSize. `Appender` does not allow
     * content to be appended beyond the size of the buffer.
     *
     * @param[in] aBuffer  A pointer to start of buffer.
     * @param[in] aSize    The maximum size of @p aBuffer (number of available bytes in buffer).
     *
     */
    Appender(uint8_t *aBuffer, uint16_t aSize);

    /**
     * This method indicates the `Appender` type (whether appending to a `Message` or data buffer).
     *
     * @returns The type of `Appender`.
     *
     */
    Type GetType(void) const { return mType; }

    /**
     * This method appends bytes to the `Appender` object.
     *
     * @param[in] aBuffer  A pointer to a data buffer (MUST NOT be `nullptr`) to append.
     * @param[in] aLength  The number of bytes to append.
     *
     * @retval kErrorNone    Successfully appended the bytes.
     * @retval kErrorNoBufs  Insufficient available buffers.
     *
     */
    Error AppendBytes(const void *aBuffer, uint16_t aLength);

    /**
     * This method appends bytes read from a given message to the `Appender` object.
     *
     * @param[in] aMessage   The message to read from (it can be the same as the message associated with `Appender`).
     * @param[in] aOffset    The offset in @p aMessage to start reading the bytes from.
     * @param[in] aLength    The number of bytes to read from @p aMessage and append.
     *
     * @retval kErrorNone    Successfully appended the bytes.
     * @retval kErrorNoBufs  Insufficient available buffers to grow the message.
     * @retval kErrorParse   Not enough bytes in @p aMessage to read @p aLength bytes from @p aOffset.
     *
     */
    Error AppendBytesFromMessage(const Message &aMessage, uint16_t aOffset, uint16_t aLength);

    /**
     * This method appends an object to the end of the `Appender` object.
     *
     * @tparam    ObjectType   The object type to append to the message.
     *
     * @param[in] aObject      A reference to the object to append to the message.
     *
     * @retval kErrorNone    Successfully appended the object.
     * @retval kErrorNoBufs  Insufficient available buffers to append @p aObject.
     *
     */
    template <typename ObjectType> Error Append(const ObjectType &aObject)
    {
        static_assert(!TypeTraits::IsPointer<ObjectType>::kValue, "ObjectType must not be a pointer");

        return AppendBytes(&aObject, sizeof(ObjectType));
    }

    /**
     * This method writes bytes to the `Appender` at a given offset overwriting previously appended content.
     *
     * This method does not perform any bound checks. The caller MUST ensure the given data length fits within the
     * previously appended content. Otherwise the behavior of this method is undefined.
     *
     * In `Appender` is using a buffer, then offset is defined relative to start of the buffer. If the `Appender` is
     * using a `Message`, then offset is defined relative to end of `Message` at the point it was used to initialize
     * the `Appender` instance.  For example, if `Message` starts with 10 bytes, and then we create an instance
     * `Appender(Message)`, then offset 5 in `Appender` would map to (10+5) 15th byte in the message.
     *
     *
     * @param[in] aOffset    The offset to begin writing.
     * @param[in] aBuffer    A pointer to a data buffer to write.
     * @param[in] aLength    Number of bytes in @p aBuffer.
     *
     */
    void WriteBytes(uint16_t aOffset, const void *aBuffer, uint16_t aLength);

    /**
     * This methods writes an object to the `Appender` at a given offset overwriting previously appended content.
     *
     * This method does not perform any bound checks. The caller MUST ensure the given data length fits within the
     * previously appended content. Otherwise the behavior of this method is undefined.
     *
     * @p aOffset is defined similar to `WriteBytes()`.
     *
     * @tparam     ObjectType   The object type to write to the message.
     *
     * @param[in]  aOffset      The offset to begin writing.
     * @param[in]  aObject      A reference to the object to write.
     *
     */
    template <typename ObjectType> void Write(uint16_t aOffset, const ObjectType &aObject)
    {
        static_assert(!TypeTraits::IsPointer<ObjectType>::kValue, "ObjectType must not be a pointer");

        WriteBytes(aOffset, &aObject, sizeof(ObjectType));
    }

    /**
     * This method returns the number of bytes appended so far using `Appender` methods.
     *
     * This method can be used independent of the `Type` of `Appender`.
     *
     * @returns The number of byes appended so far.
     *
     */
    uint16_t GetAppendedLength(void) const;

    /**
     * This method returns the `Message` associated with `Appender`.
     *
     * This method MUST be used when `GetType() == kMessage`. Otherwise its behavior is undefined.
     *
     * @returns The `Message` instance associated with `Appender`.
     *
     */
    Message &GetMessage(void) { return *mShared.mMessage.mMessage; }

    /**
     * This method returns a pointer to the start of the data buffer associated with `Appender`.
     *
     * This method MUST be used when `GetType() == kBuffer`. Otherwise its behavior is undefined.
     *
     * @returns A pointer to the start of the data buffer associated with `Appender`.
     *
     */
    uint8_t *GetBufferStart(void) { return mShared.mBuffer.mStart; }

    /**
     * This method gets the data buffer associated with `Appender` as a `Data`.
     *
     * This method MUST be used when `GetType() == kBuffer`. Otherwise its behavior is undefined.
     *
     * @pram[out] aData  A reference to a `Data` to output the data buffer.
     *
     */
    void GetAsData(Data<kWithUint16Length> &aData);

private:
    Type mType;
    union
    {
        struct
        {
            Message *mMessage;
            uint16_t mStartOffset;
        } mMessage;

        struct
        {
            uint8_t *mStart;
            uint8_t *mCur;
            uint8_t *mEnd;
        } mBuffer;
    } mShared;
};

} // namespace ot

#endif // APPENDER_HPP_
