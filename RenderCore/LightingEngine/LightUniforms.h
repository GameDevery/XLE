// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once

#include "../../Math/Vector.h"

namespace RenderCore { namespace LightingEngine
{
	struct CB_Ambient
	{ 
		Float3      _ambientColour; 
		float       _skyReflectionScale; 
		float       _skyReflectionBlurriness; 
		unsigned    _dummy[3];
	};

	struct CB_RangeFog
	{
		Float3      _rangeFogInscatter;
		float       _rangeFogOpticalThickness;
	};

	struct CB_VolumeFog
	{
		float       _opticalThickness;
		float       _heightStart;
		float       _heightEnd;  unsigned _enableFlag;
		Float3      _sunInscatter; unsigned _dummy1;
		Float3      _ambientInscatter; unsigned _dummy2;
	};

	struct CB_Light
	{
			// Note that this structure is larger than it needs to be
			// for some light types. Only some types need the full 
			// orientation matrix.
			// It seems like we would end up wasting shader constants
			// if we want to store a large number of lights for forward
			// rendering.
		Float3 _position;           float _cutoffRange;
		Float3 _diffuse;            float _sourceRadiusX;
		Float3 _specular;           float _sourceRadiusY;
		Float3 _orientationX;       float _diffuseWideningMin;
		Float3 _orientationY;       float _diffuseWideningMax;
		Float3 _orientationZ;       unsigned _dummy;
	};

	struct CB_BasicEnvironment
	{
		CB_Ambient    _ambient;
		CB_RangeFog   _rangeFog;
		CB_VolumeFog  _volumeFog;
		CB_Light      _dominant[1];
	};

	class SceneLightingDesc;
	class LightDesc;
	CB_Ambient MakeAmbientUniforms(const SceneLightingDesc& desc);
	CB_RangeFog MakeRangeFogUniforms(const SceneLightingDesc& desc);
	CB_Light MakeLightUniforms(const LightDesc& light);
	CB_Light MakeBlankLightDesc();
	CB_VolumeFog MakeBlankVolumeFogDesc();
	CB_BasicEnvironment MakeBasicEnvironmentUniforms(const SceneLightingDesc& globalDesc);
}}
