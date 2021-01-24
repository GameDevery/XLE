// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once

#include "AssetUtils.h"
#include "../Utility/MemoryUtils.h"
#include <memory>

namespace Utility { class CompletionThreadPool; }

namespace Assets
{

    class AsyncLoadOperation
    {
    public:
        static void Enqueue(const std::shared_ptr<AsyncLoadOperation>& op, StringSection<ResChar> filename, CompletionThreadPool& pool);

        AsyncLoadOperation();
        virtual ~AsyncLoadOperation();

        AsyncLoadOperation(const AsyncLoadOperation&) = delete;
        AsyncLoadOperation& operator=(const AsyncLoadOperation&) = delete;

    protected:
        const uint8* GetBuffer() const;
        size_t GetBufferSize() const;

        mutable ResChar _filename[MaxPath];

        virtual void Complete(const void* buffer, size_t bufferSize) = 0;
		virtual void OnFailure() = 0;
    private:
        std::unique_ptr<uint8[], PODAlignedDeletor> _buffer;
        size_t _bufferLength;
        mutable bool _hasBeenQueued;

        class SpecialOverlapped;
        std::unique_ptr<SpecialOverlapped> _overlapped;
    };
    
}

