// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once

#include "DeferredLightingDelegate.h"
#include "../Metal/Forward.h"
#include "../Metal/InputLayout.h"
#include <vector>
#include <memory>

namespace RenderCore { class FrameBufferDesc; }
namespace RenderCore { namespace Techniques { class RenderPassInstance; }}

namespace RenderCore { namespace LightingEngine
{
    class LightResolveOperators
	{
	public:
		struct Operator
		{
			std::shared_ptr<Metal::GraphicsPipeline> _pipeline;
			LightResolveOperatorDesc _desc;
		};

		std::vector<Operator> _operators;
		std::shared_ptr<RenderCore::ICompiledPipelineLayout> _pipelineLayout;
		Metal::BoundUniforms _boundUniforms;
		std::shared_ptr<RenderCore::IDescriptorSet> _fixedDescriptorSet;
        bool _debuggingOn = false;
	};

	class IPreparedShadowResult;

    void ResolveLights(
		IThreadContext& threadContext,
		Techniques::ParsingContext& parsingContext,
        Techniques::RenderPassInstance& rpi,
		const SceneLightingDesc& sceneLightingDesc,
		const LightResolveOperators& lightResolveOperators,
		IteratorRange<const std::pair<LightId, std::shared_ptr<IPreparedShadowResult>>*> preparedShadows);

	enum class GBufferType
	{
		PositionNormal,
		PositionNormalParameters
	};

	enum class Shadowing { NoShadows, PerspectiveShadows, OrthShadows, OrthShadowsNearCascade, OrthHybridShadows, CubeMapShadows };

    ::Assets::FuturePtr<LightResolveOperators> BuildLightResolveOperators(
		Techniques::GraphicsPipelineCollection& pipelineCollection,
		IteratorRange<const LightResolveOperatorDesc*> resolveOperators,
		const FrameBufferDesc& fbDesc,
		unsigned subpassIdx,
		bool hasScreenSpaceAO,
		unsigned shadowResolveModel,
		Shadowing shadowing,
		GBufferType gbufferType);
}}