// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once

#include "../../Utility/StringUtils.h"
#include <unordered_map>
#include <memory>

namespace Assets { class DirectorySearchRules; class DependencyValidation; }
namespace Utility { class ConditionalProcessingTokenizer; }
namespace RenderCore { class PipelineLayoutInitializer; enum class ShaderLanguage; }

namespace RenderCore { namespace Assets
{
    class PredefinedDescriptorSetLayout;
    class PredefinedCBLayout;

    /// <summary>Configuration file for pipeline layouts</summary>
    /// Used to deserialize a .pipeline file, which contains information about a pipeline layout
    /// The serialized form is a C-style language, which fits in nicely when using a C-style shader language
    ///
    /// Generally this is used to construct a RenderCore::PipelineLayoutInitializer, which can be used to
    /// in-turn generate a RenderCore::ICompiledPipelineLayout via a IDevice. However this form contains a little
    /// more information, which can be handy when configuring higher-level types (such as the PipelineAcceleratorPool)
    class PredefinedPipelineLayoutFile
	{
	public:
        std::unordered_map<std::string, std::shared_ptr<PredefinedDescriptorSetLayout>> _descriptorSets;
        class PipelineLayout
        {
        public:
            std::vector<std::pair<std::string, std::shared_ptr<PredefinedDescriptorSetLayout>>> _descriptorSets;
            std::pair<std::string, std::shared_ptr<PredefinedCBLayout>> _vsPushConstants;
            std::pair<std::string, std::shared_ptr<PredefinedCBLayout>> _psPushConstants;
            std::pair<std::string, std::shared_ptr<PredefinedCBLayout>> _gsPushConstants;

            PipelineLayoutInitializer MakePipelineLayoutInitializer(ShaderLanguage language) const;
        };
        std::unordered_map<std::string, std::shared_ptr<PipelineLayout>> _pipelineLayouts;

        PredefinedPipelineLayoutFile(
			StringSection<> inputData,
			const ::Assets::DirectorySearchRules& searchRules,
			const std::shared_ptr<::Assets::DependencyValidation>& depVal);
        PredefinedPipelineLayoutFile(
			StringSection<> sourceFileName);
		PredefinedPipelineLayoutFile();
		~PredefinedPipelineLayoutFile();

		const std::shared_ptr<::Assets::DependencyValidation>& GetDependencyValidation() const { return _depVal; }

    protected:
        std::shared_ptr<::Assets::DependencyValidation> _depVal;

        std::shared_ptr<PipelineLayout> ParsePipelineLayout(Utility::ConditionalProcessingTokenizer&);
        void Parse(Utility::ConditionalProcessingTokenizer&);
    };

}}
