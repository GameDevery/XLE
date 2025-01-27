// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "DeferredShaderResource.h"
#include "Services.h"
#include "../Metal/ShaderResource.h"
#include "../../Assets/AsyncLoadOperation.h"
#include "../../BufferUploads/IBufferUploads.h"
#include "../../BufferUploads/DataPacket.h"
#include "../../BufferUploads/ResourceLocator.h"
#include "../../Assets/AssetServices.h"
#include "../../Assets/IntermediateAssets.h"
#include "../../Assets/CompileAndAsyncManager.h"
#include "../../Utility/Streams/PathUtils.h"
#include "../../Utility/Streams/FileUtils.h"
#include "../../Utility/Threading/CompletionThreadPool.h"
#include "../../Foreign/tinyxml2-master/tinyxml2.h"

#include "../../Core/WinAPI/IncludeWindows.h"

namespace RenderCore { namespace Assets 
{
    using ResChar = ::Assets::ResChar;

    enum class SourceColorSpace { SRGB, Linear, Unspecified };

    class MetadataLoadMarker : public ::Assets::AsyncLoadOperation
    {
    public:
        SourceColorSpace _colorSpace;

        virtual ::Assets::AssetState Complete(const void* buffer, size_t bufferSize);
        MetadataLoadMarker() : _colorSpace(SourceColorSpace::Unspecified) {}
    };

    bool LoadColorSpaceFromMetadataFile(SourceColorSpace& result, const void* start, size_t size)
    {
        result = SourceColorSpace::Unspecified;

        if (!start || !size) return false;

        // skip over the "byte order mark", if it exists...
        if (size >= 3 && ((const uint8*)start)[0] == 0xef && ((const uint8*)start)[1] == 0xbb && ((const uint8*)start)[2] == 0xbf) {
            start = PtrAdd(start, 3);
            size -= 3;
        }

        using namespace tinyxml2;
        XMLDocument doc;
	    auto e = doc.Parse((const char*)start, size);
        if (e != XML_SUCCESS) return false;

        const auto* root = doc.RootElement();
        if (root) {
            auto colorSpace = root->FindAttribute("colorSpace");
            if (colorSpace) {
                if (!XlCompareStringI(colorSpace->Value(), "srgb")) { result = SourceColorSpace::SRGB; }
                else if (!XlCompareStringI(colorSpace->Value(), "linear")) { result = SourceColorSpace::Linear; }
            }
        }

        return true;
    }

    ::Assets::AssetState MetadataLoadMarker::Complete(const void* buffer, size_t bufferSize)
    {
            // Attempt to parse the xml in our data buffer...
        if (!LoadColorSpaceFromMetadataFile(_colorSpace, buffer, bufferSize))
            return ::Assets::AssetState::Invalid;

        return ::Assets::AssetState::Ready;
    }

    class DeferredShaderResource::Pimpl
    {
    public:
        BufferUploads::TransactionID _transaction;
        intrusive_ptr<BufferUploads::ResourceLocator> _locator;
        Metal::ShaderResourceView _srv;

        SourceColorSpace _colSpaceRequestString;
        SourceColorSpace _colSpaceDefault;
        std::shared_ptr<MetadataLoadMarker> _metadataMarker;
    };

    class DecodedInitializer
    {
    public:
        FileNameSplitter<ResChar> _splitter;

        SourceColorSpace    _colSpaceRequestString;
        SourceColorSpace    _colSpaceDefault;
        bool                _generateMipmaps;

        DecodedInitializer(const ResChar initializer[]);
    };

    DecodedInitializer::DecodedInitializer(const ResChar initializer[])
    : _splitter(initializer)
    {
        _generateMipmaps = true;
        _colSpaceRequestString = SourceColorSpace::Unspecified;
        _colSpaceDefault = SourceColorSpace::Unspecified;

        for (auto c:_splitter.Parameters()) {
            if (c == 'l' || c == 'L') { _colSpaceRequestString = SourceColorSpace::Linear; }
            if (c == 's' || c == 'S') { _colSpaceRequestString = SourceColorSpace::SRGB; }
            if (c == 't' || c == 'T') { _generateMipmaps = false; }
        }

        if (_colSpaceRequestString == SourceColorSpace::Unspecified) {
            if (XlFindStringI(initializer, "_ddn")) {
                _colSpaceDefault = SourceColorSpace::Linear;
            } else {
                _colSpaceDefault = SourceColorSpace::SRGB;
            }
        }
    }

	static bool CheckShadowingFile(const FileNameSplitter<::Assets::ResChar>& splitter)
	{
		return !XlEqStringI(splitter.Extension(), "dds");
	}

	template<int Count>
		static void BuildRequestString(
			::Assets::ResChar (&buffer)[Count],
			const FileNameSplitter<::Assets::ResChar>& splitter)
	{
		auto& store = ::Assets::Services::GetAsyncMan().GetShadowingStore();
		store.MakeIntermediateName(
			buffer, Count, MakeStringSection(splitter.DriveAndPath().begin(), splitter.File().end()));
		XlCatString(buffer, Count, ".dds;");
		XlCatString(buffer, Count, splitter.AllExceptParameters());
	}

    DeferredShaderResource::DeferredShaderResource(const ResChar initializer[])
    {
        DEBUG_ONLY(XlCopyString(_initializer, dimof(_initializer), initializer);)
        _pimpl = std::make_unique<Pimpl>();

        _validationCallback = std::make_shared<::Assets::DependencyValidation>();

            // parse initialiser for flags
        DecodedInitializer init(initializer);
        _pimpl->_colSpaceRequestString = init._colSpaceRequestString;
        _pimpl->_colSpaceDefault = init._colSpaceDefault;

		::Assets::ResChar filename[MaxPath];
        if (_pimpl->_colSpaceRequestString == SourceColorSpace::Unspecified) {
                // No color space explicitly requested. We need to calculate the default
                // color space for this texture...
                // Most textures should be in SRGB space. However, some texture represent
                // geometry details (or are lookup tables for shader calculations). These
                // need to be marked as linear space (so they don't go through the SRGB->Linear 
                // conversion.
                //
                // Some resources might have a little xml ".metadata" file attached. This 
                // can contain a setting that can tell us the intended source color format.
                //
                // Some textures use "_ddn" to mark them as normal maps... So we'll take this
                // as a hint that they are in linear space. 

                // trigger a load of the metadata file (which should proceed in the background)
            
            XlCopyString(filename, init._splitter.AllExceptParameters());
            XlCatString(filename, ".metadata");
            RegisterFileDependency(_validationCallback, filename);

            _pimpl->_metadataMarker = std::make_shared<MetadataLoadMarker>();
            _pimpl->_metadataMarker->Enqueue(filename, ConsoleRig::GlobalServices::GetShortTaskThreadPool());
        }

        using namespace BufferUploads;
        TextureLoadFlags::BitField flags = init._generateMipmaps ? TextureLoadFlags::GenerateMipmaps : 0;

		// We're going to check for the existance of a "shadowing" file first. We'll write onto "filename"
		// two names -- a possible shadowing file, and the original file as well. But don't do this for
		// DDS files. We'll assume they do not have a shadowing file.
		intrusive_ptr<DataPacket> pkt;
		const bool checkForShadowingFile = CheckShadowingFile(init._splitter);
		if (checkForShadowingFile) {
			BuildRequestString(filename, init._splitter);
			pkt = CreateStreamingTextureSource(MakeStringSection(filename), flags);
		} else {
			pkt = CreateStreamingTextureSource(init._splitter.AllExceptParameters(), flags);
		}

        _pimpl->_transaction = Services::GetBufferUploads().Transaction_Begin(
            CreateDesc(
                BindFlag::ShaderResource,
                0, GPUAccess::Read,
                TextureDesc::Empty(), initializer),
            pkt.get());

        RegisterFileDependency(_validationCallback, initializer);
    }

    DeferredShaderResource::~DeferredShaderResource()
    {
        if (_pimpl->_transaction != ~BufferUploads::TransactionID(0))
            Services::GetBufferUploads().Transaction_End(_pimpl->_transaction);
    }

    DeferredShaderResource::DeferredShaderResource(DeferredShaderResource&& moveFrom)
    : _pimpl(std::move(moveFrom._pimpl))
    , _validationCallback(std::move(moveFrom._validationCallback))
    {
        DEBUG_ONLY(XlCopyString(_initializer, moveFrom._initializer));
    }

    DeferredShaderResource& DeferredShaderResource::operator=(DeferredShaderResource&& moveFrom)
    {
        _pimpl = std::move(moveFrom._pimpl);
        _validationCallback = std::move(moveFrom._validationCallback);
        DEBUG_ONLY(XlCopyString(_initializer, moveFrom._initializer));
        return *this;
    }

    const Metal::ShaderResourceView&       DeferredShaderResource::GetShaderResource() const
    {
        if (!_pimpl->_srv.IsGood()) {
            if (_pimpl->_transaction == ~BufferUploads::TransactionID(0))
                Throw(::Assets::Exceptions::InvalidAsset(Initializer(), "Unknown error during loading"));

            auto& bu = Services::GetBufferUploads();
            if (!bu.IsCompleted(_pimpl->_transaction))
                Throw(::Assets::Exceptions::PendingAsset(Initializer(), ""));

            auto state = TryResolve();
            if (state == ::Assets::AssetState::Invalid) {
                Throw(::Assets::Exceptions::InvalidAsset(Initializer(), "Unknown error during loading"));
            } else if (state == ::Assets::AssetState::Pending)
                Throw(::Assets::Exceptions::PendingAsset(Initializer(), ""));

            assert(_pimpl->_srv.IsGood());
        }

        return _pimpl->_srv;
    }

    ::Assets::AssetState DeferredShaderResource::GetAssetState() const
    {
        if (_pimpl->_srv.IsGood())
            return ::Assets::AssetState::Ready;
        if (_pimpl->_transaction == ~BufferUploads::TransactionID(0))
            return ::Assets::AssetState::Invalid;
        return ::Assets::AssetState::Pending;
    }

    ::Assets::AssetState DeferredShaderResource::TryResolve() const
    {
        if (_pimpl->_srv.IsGood())
            return ::Assets::AssetState::Ready;

        if (_pimpl->_transaction == ~BufferUploads::TransactionID(0))
            return ::Assets::AssetState::Invalid;

        auto& bu = Services::GetBufferUploads();
        if (!bu.IsCompleted(_pimpl->_transaction))
            return ::Assets::AssetState::Pending;

        _pimpl->_locator = bu.GetResource(_pimpl->_transaction);
        bu.Transaction_End(_pimpl->_transaction);
        _pimpl->_transaction = ~BufferUploads::TransactionID(0);

        if (!_pimpl->_locator || !_pimpl->_locator->GetUnderlying())
            return ::Assets::AssetState::Invalid;

        auto desc = BufferUploads::ExtractDesc(*_pimpl->_locator->GetUnderlying());
        if (desc._type != BufferUploads::BufferDesc::Type::Texture)
            return ::Assets::AssetState::Invalid;

            // calculate the color space to use (resolving the defaults, request string and metadata)
        auto colSpace = SourceColorSpace::SRGB;
        if (_pimpl->_colSpaceRequestString != SourceColorSpace::Unspecified) colSpace = _pimpl->_colSpaceRequestString;
        else {
            if (_pimpl->_colSpaceDefault != SourceColorSpace::Unspecified) colSpace = _pimpl->_colSpaceDefault;

            if (_pimpl->_metadataMarker) {
                auto state = _pimpl->_metadataMarker->GetAssetState();
                if (state == ::Assets::AssetState::Pending)
                    return ::Assets::AssetState::Pending;

                if (state == ::Assets::AssetState::Ready && _pimpl->_metadataMarker->_colorSpace != SourceColorSpace::Unspecified) {
                    colSpace = _pimpl->_metadataMarker->_colorSpace;
                }
            }
        }

        auto format = (Metal::NativeFormat::Enum)desc._textureDesc._nativePixelFormat;
        if (colSpace == SourceColorSpace::SRGB) format = Metal::AsSRGBFormat(format);
        else if (colSpace == SourceColorSpace::Linear) format = Metal::AsLinearFormat(format);

        _pimpl->_srv = Metal::ShaderResourceView(_pimpl->_locator->GetUnderlying(), format);
        return ::Assets::AssetState::Ready;
    }

    const char* DeferredShaderResource::Initializer() const
    {
        #if defined(_DEBUG)
            return _initializer;
        #else
            return "";
        #endif
    }

///////////////////////////////////////////////////////////////////////////////////////////////////

    static Metal::NativeFormat::Enum ResolveFormatImmediate(Metal::NativeFormat::Enum typelessFormat, const DecodedInitializer& init)
    {
        auto result = typelessFormat;
        if (Metal::HasLinearAndSRGBFormats(result)) {
            auto finalColSpace = init._colSpaceRequestString;
            if (finalColSpace == SourceColorSpace::Unspecified) {
                    // need to load the metadata file to get SRGB settings!
                ::Assets::ResChar metadataFile[MaxPath];
                XlCopyString(metadataFile, init._splitter.AllExceptParameters());
                XlCatString(metadataFile, ".metadata");

                size_t filesize = 0;
                auto rawFile = LoadFileAsMemoryBlock(metadataFile, &filesize);
                if (rawFile.get())
                    LoadColorSpaceFromMetadataFile(finalColSpace, rawFile.get(), filesize);
            
                if (finalColSpace == SourceColorSpace::Unspecified)
                    finalColSpace = (init._colSpaceDefault != SourceColorSpace::Unspecified) ? init._colSpaceDefault : SourceColorSpace::SRGB;
            }

            if (finalColSpace == SourceColorSpace::SRGB) result = Metal::AsSRGBFormat(result);
            else if (finalColSpace == SourceColorSpace::Linear) result = Metal::AsLinearFormat(result);
        }
        return result;
    }

    Metal::NativeFormat::Enum DeferredShaderResource::LoadFormat(const ::Assets::ResChar initializer[])
    {
        DecodedInitializer init(initializer);

		Metal::NativeFormat::Enum result;
		const bool checkForShadowingFile = CheckShadowingFile(init._splitter);
		if (checkForShadowingFile) {
			::Assets::ResChar filename[MaxPath];
			BuildRequestString(filename, init._splitter);
			result = (Metal::NativeFormat::Enum)BufferUploads::LoadTextureFormat(MakeStringSection(filename))._nativePixelFormat;
		} else
			result = (Metal::NativeFormat::Enum)BufferUploads::LoadTextureFormat(init._splitter.AllExceptParameters())._nativePixelFormat;

        return ResolveFormatImmediate(result, init);
    }

    Metal::ShaderResourceView DeferredShaderResource::LoadImmediately(const char initializer[])
    {
        DecodedInitializer init(initializer);

        using namespace BufferUploads;
        TextureLoadFlags::BitField flags = init._generateMipmaps ? TextureLoadFlags::GenerateMipmaps : 0;

		intrusive_ptr<DataPacket> pkt;
		const bool checkForShadowingFile = CheckShadowingFile(init._splitter);
		if (checkForShadowingFile) {
			::Assets::ResChar filename[MaxPath];
			BuildRequestString(filename, init._splitter);
			pkt = CreateStreamingTextureSource(MakeStringSection(filename), flags);
		} else
			pkt = CreateStreamingTextureSource(init._splitter.AllExceptParameters(), flags);

        auto result = Services::GetBufferUploads().Transaction_Immediate(
            CreateDesc(
                BindFlag::ShaderResource,
                0, GPUAccess::Read,
                TextureDesc::Empty(), initializer),
            pkt.get());

            //  We don't have to change the SRGB modes here -- the caller should select the
            //  right srgb mode when creating a shader resource view

        if (!result)
            Throw(::Assets::Exceptions::InvalidAsset(initializer, "Failure while attempting to load texture immediately"));

        auto desc = ExtractDesc(*result->GetUnderlying());
        assert(desc._type == BufferDesc::Type::Texture);
        return Metal::ShaderResourceView(
            result->GetUnderlying(), 
            ResolveFormatImmediate((Metal::NativeFormat::Enum)desc._textureDesc._nativePixelFormat, init));
    }

}}

