// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "ChunkFileContainer.h"
#include "BlockSerializer.h"
#include "IntermediateAssets.h"
#include "IFileSystem.h"
#include "../Utility/StringFormat.h"
#include "../Core/Exceptions.h"
#include "../ConsoleRig/Log.h"

namespace Assets
{
    std::vector<AssetChunkResult> ChunkFileContainer::ResolveRequests(
        IteratorRange<const AssetChunkRequest*> requests) const
    {
		auto file = MainFileSystem::OpenFileInterface(_filename.c_str(), "rb");
        auto chunks = Serialization::ChunkFile::LoadChunkTable(*file);
        
        std::vector<AssetChunkResult> result;
        result.reserve(requests.size());

            // First scan through and check to see if we
            // have all of the chunks we need
        using ChunkHeader = Serialization::ChunkFile::ChunkHeader;
        for (const auto& r:requests) {
            auto i = std::find_if(
                chunks.begin(), chunks.end(), 
                [&r](const ChunkHeader& c) { return c._type == r._type; });
            if (i == chunks.end())
                Throw(::Assets::Exceptions::FormatError(
                    StringMeld<128>() << "Missing chunk (" << r._name << ")", _filename.c_str()));

            if (i->_chunkVersion != r._expectedVersion)
                Throw(::Assets::Exceptions::FormatError(
                    ::Assets::Exceptions::FormatError::Reason::UnsupportedVersion,
                    StringMeld<256>() 
                        << "Data chunk is incorrect version for chunk (" 
                        << r._name << ") expected: " << r._expectedVersion << ", got: " << i->_chunkVersion, 
						_filename.c_str()));
        }

        for (const auto& r:requests) {
            auto i = std::find_if(
                chunks.begin(), chunks.end(), 
                [&r](const ChunkHeader& c) { return c._type == r._type; });
            assert(i != chunks.end());

            AssetChunkResult chunkResult;
            chunkResult._offset = i->_fileOffset;
            chunkResult._size = i->_size;

            if (r._dataType != AssetChunkRequest::DataType::DontLoad) {
                chunkResult._buffer = std::make_unique<uint8[]>(i->_size);
                file->Seek(i->_fileOffset);
                file->Read(chunkResult._buffer.get(), i->_size);

                // initialize with the block serializer (if requested)
                if (r._dataType == AssetChunkRequest::DataType::BlockSerializer)
                    Serialization::Block_Initialize(chunkResult._buffer.get());
            }

            result.emplace_back(std::move(chunkResult));
        }

        return std::move(result);
    }

///////////////////////////////////////////////////////////////////////////////////////////////////

    ChunkFileContainer::ChunkFileContainer(const char assetTypeName[])
    : _filename(assetTypeName)
    {
		_validationCallback = std::make_shared<DependencyValidation>();
		RegisterFileDependency(_validationCallback, MakeStringSection(_filename));
    }

    ChunkFileContainer::~ChunkFileContainer() {}

}
