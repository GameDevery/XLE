// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "InvalidAssetDisplay.h"
#include "../../Assets/AssetSetManager.h"
#include "../../Assets/AssetServices.h"
#include "../../Assets/AssetHeap.h"
#include <sstream>

namespace PlatformRig { namespace Overlays
{

	void    InvalidAssetDisplay::Render(IOverlayContext& context, Layout& layout, Interactables&interactables, InterfaceState& interfaceState)
	{
		const unsigned lineHeight = 20;
		const auto titleBkground = RenderOverlays::ColorB { 0, 10, 64 }; 

		using namespace Assets;
		auto records = ::Assets::Services::GetAssetSets().LogRecords();
		for (const auto&r:records) {
			if (r._state != AssetState::Invalid) continue;

			auto titleRect = layout.AllocateFullWidth(lineHeight);
			RenderOverlays::DebuggingDisplay::DrawRectangle(&context, titleRect, titleBkground);
			RenderOverlays::DebuggingDisplay::DrawText(&context, titleRect, nullptr, RenderOverlays::ColorB{0xff, 0xff, 0xff}, r._initializer);

			auto msg = std::stringstream{AsString(r._actualizationLog)};
			for (std::string line; std::getline(msg, line, '\n');) {
				RenderOverlays::DebuggingDisplay::DrawText(&context, layout.AllocateFullWidth(lineHeight), nullptr, RenderOverlays::ColorB{0xcf, 0xcf, 0xcf}, line);
			}
		}
	}

	bool    InvalidAssetDisplay::ProcessInput(InterfaceState& interfaceState, const InputContext& inputContext, const InputSnapshot& input)
	{
		return false;
	}

	InvalidAssetDisplay::InvalidAssetDisplay()
	{}

	InvalidAssetDisplay::~InvalidAssetDisplay()
	{}

}}
