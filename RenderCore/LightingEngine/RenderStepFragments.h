// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once

#include "../Techniques/RenderPass.h"
#include "../Techniques/Drawables.h"		// for BatchFilter
#include "../../Utility/ParameterBox.h"

namespace RenderCore { namespace Techniques 
{
	class FrameBufferDescFragment;
	class ITechniqueDelegate;
	class SequencerConfig;
}}

namespace RenderCore { namespace LightingEngine
{
	class RenderStepFragmentInterface
	{
	public:
		RenderCore::AttachmentName DefineAttachment(uint64_t semantic, const RenderCore::AttachmentDesc& request = {});
		RenderCore::AttachmentName DefineTemporaryAttachment(const RenderCore::AttachmentDesc& request);
		void AddSubpass(
			RenderCore::SubpassDesc&& subpass,
			const std::shared_ptr<RenderCore::Techniques::ITechniqueDelegate>& techniqueDelegate = nullptr,
			Techniques::BatchFilter batchFilter = Techniques::BatchFilter::Max,
			ParameterBox&& sequencerSelectors = {});

		RenderStepFragmentInterface(RenderCore::PipelineType);
		~RenderStepFragmentInterface();

		const RenderCore::Techniques::FrameBufferDescFragment& GetFrameBufferDescFragment() const { return _frameBufferDescFragment; }

		struct SubpassExtension
		{
			std::shared_ptr<RenderCore::Techniques::ITechniqueDelegate> _techniqueDelegate;
			ParameterBox _sequencerSelectors;
			Techniques::BatchFilter _batchFilter;
		};
		IteratorRange<const SubpassExtension*> GetSubpassAddendums() const { return MakeIteratorRange(_subpassExtensions); }
	private:
		RenderCore::Techniques::FrameBufferDescFragment _frameBufferDescFragment;
		std::vector<SubpassExtension> _subpassExtensions;
	};

	class RenderStepFragmentInstance
	{
	public:
		const RenderCore::Techniques::SequencerConfig* GetSequencerConfig() const;
		const RenderCore::Techniques::RenderPassInstance& GetRenderPassInstance() const { return *_rpi; }
		RenderCore::Techniques::RenderPassInstance& GetRenderPassInstance() { return *_rpi; }

		RenderStepFragmentInstance(
			RenderCore::Techniques::RenderPassInstance& rpi,
			IteratorRange<const std::shared_ptr<RenderCore::Techniques::SequencerConfig>*> sequencerConfigs);
		RenderStepFragmentInstance();
	private:
		RenderCore::Techniques::RenderPassInstance* _rpi;
		IteratorRange<const std::shared_ptr<RenderCore::Techniques::SequencerConfig>*> _sequencerConfigs;
		unsigned _firstSubpassIndex;
	};

}}
