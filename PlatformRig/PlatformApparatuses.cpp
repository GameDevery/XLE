// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "PlatformApparatuses.h"
#include "FrameRig.h"
#include "PlatformRigUtil.h"
#include "OverlaySystem.h"
#include "OverlappedWindow.h"
#include "MainInputHandler.h"
#include "InputTranslator.h"
#include "DebugScreensOverlay.h"
#include "DebuggingDisplays/GPUProfileDisplay.h"
#include "DebuggingDisplays/CPUProfileDisplay.h"
#include "DebuggingDisplays/InvalidAssetDisplay.h"
#include "../RenderCore/Techniques/Apparatuses.h"
#include "../RenderCore/IAnnotator.h"
#include "../RenderCore/IDevice.h"
#include "../RenderOverlays/Font.h"
#include "../RenderOverlays/DebuggingDisplay.h"
#include "../Assets/DepVal.h"
#include "../Utility/Profiling/CPUProfiler.h"

namespace PlatformRig
{

	DebugOverlaysApparatus::DebugOverlaysApparatus(
		const std::shared_ptr<RenderCore::Techniques::ImmediateDrawingApparatus>& immediateDrawingApparatus,
		PlatformRig::FrameRig& frameRig)
	{
		_depValPtr = std::make_shared<::Assets::DependencyValidation>();

		using DebugScreensSystem = RenderOverlays::DebuggingDisplay::DebugScreensSystem;
		_debugSystem = std::make_shared<DebugScreensSystem>();
		_debugSystem->Register(
			frameRig.CreateDisplay(_debugSystem),
			"FrameRig", DebugScreensSystem::SystemDisplay);

		_debugScreensOverlaySystem = std::make_shared<PlatformRig::OverlaySystemSet>();
		_debugScreensOverlaySystem->AddSystem(CreateDebugScreensOverlay(_debugSystem, immediateDrawingApparatus->_immediateDrawables, immediateDrawingApparatus->_fontRenderingManager));

		auto overlaySwitch = std::make_shared<PlatformRig::OverlaySystemSwitch>();
		overlaySwitch->AddSystem(PlatformRig::KeyId_Make("~"), PlatformRig::CreateConsoleOverlaySystem(*immediateDrawingApparatus));
		_debugScreensOverlaySystem->AddSystem(overlaySwitch);

		_debugFont0 = RenderOverlays::GetX2Font("Raleway", 16);
		_debugFont1 = RenderOverlays::GetX2Font("Vera", 16);
	}

	DebugOverlaysApparatus::~DebugOverlaysApparatus()
	{}


	WindowApparatus::WindowApparatus(std::shared_ptr<RenderCore::IDevice> device)
	{
		_depValPtr = std::make_shared<::Assets::DependencyValidation>();

		_device = device;

		_osWindow = std::make_unique<PlatformRig::OverlappedWindow>();
		auto clientRect = _osWindow->GetRect();
		_presentationChain = _device->CreatePresentationChain(
			_osWindow->GetUnderlyingHandle(), 
			RenderCore::PresentationChainDesc{unsigned(clientRect.second[0] - clientRect.first[0]), unsigned(clientRect.second[1] - clientRect.first[1])});
		_windowHandler = std::make_shared<PlatformRig::ResizePresentationChain>(_presentationChain);
		_osWindow->AddWindowHandler(_windowHandler);

		_mainInputHandler = std::make_shared<PlatformRig::MainInputHandler>();
		_osWindow->GetInputTranslator().AddListener(_mainInputHandler);
		
		_immediateContext = _device->GetImmediateContext();
	}
	
	WindowApparatus::~WindowApparatus()
	{

	}

	void InitProfilerDisplays(
		RenderOverlays::DebuggingDisplay::DebugScreensSystem& debugSys, 
		RenderCore::IAnnotator* annotator,
		Utility::HierarchicalCPUProfiler& cpuProfiler)
	{
		if (annotator) {
			auto gpuProfilerDisplay = std::make_shared<PlatformRig::Overlays::GPUProfileDisplay>(*annotator);
			debugSys.Register(gpuProfilerDisplay, "[Profiler] GPU Profiler");
		}
		debugSys.Register(
			std::make_shared<PlatformRig::Overlays::HierarchicalProfilerDisplay>(&cpuProfiler),
			"[Profiler] CPU Profiler");

		debugSys.Register(std::make_shared<PlatformRig::Overlays::InvalidAssetDisplay>(), "[Assets] Invalid asset display");
		debugSys.SwitchToScreen("[Assets] Invalid asset display");
	}

}
