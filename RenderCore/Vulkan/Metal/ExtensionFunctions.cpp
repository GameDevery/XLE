// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "ExtensionFunctions.h"

namespace RenderCore { namespace Metal_Vulkan
{
    ExtensionFunctions::ExtensionFunctions(VkInstance instance)
    {
        _beginTransformFeedback = (PFN_vkCmdBeginTransformFeedbackEXT)vkGetInstanceProcAddr(instance, "vkCmdBeginTransformFeedbackEXT");
		_bindTransformFeedbackBuffers = (PFN_vkCmdBindTransformFeedbackBuffersEXT)vkGetInstanceProcAddr(instance, "vkCmdBindTransformFeedbackBuffersEXT");
        _endTransformFeedback = (PFN_vkCmdEndTransformFeedbackEXT)vkGetInstanceProcAddr(instance, "vkCmdEndTransformFeedbackEXT");
    }
}}