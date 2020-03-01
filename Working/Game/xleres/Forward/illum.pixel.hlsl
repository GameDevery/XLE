// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "../TechniqueLibrary/Framework/MainGeometry.hlsl"
#include "../Objects/IllumShader/PerPixel.h"
#include "../TechniqueLibrary/SceneEngine/Lighting/Forward.hlsl"

#if !((OUTPUT_TEXCOORD==1) && (MAT_ALPHA_TEST==1)) && (VULKAN!=1)
	[earlydepthstencil]
#endif
float4 main(VSOutput geo, SystemInputs sys) : SV_Target0
{
	DoAlphaTest(geo, GetAlphaThreshold());

	GBufferValues sample = IllumShader_PerPixel(geo);

	float3 directionToEye = 0.0.xxx;
	#if (OUTPUT_WORLD_VIEW_VECTOR==1)
		directionToEye = normalize(geo.worldViewVector);
	#endif

	float4 result = float4(
		ResolveLitColor(
			sample, directionToEye, GetWorldPosition(geo),
			LightScreenDest_Create(int2(geo.position.xy), GetSampleIndex(sys))), 1.f);

	#if OUTPUT_FOG_COLOR == 1
		result.rgb = geo.fogColor.rgb + result.rgb * geo.fogColor.a;
	#endif

	result.a = sample.blendingAlpha;

    #if (OUTPUT_COLOUR>=1) && (MAT_VCOLOR_IS_ANIM_PARAM==0)
        result.rgb *= geo.colour.rgb;
    #endif

	#if MAT_SKIP_LIGHTING_SCALE==0
		result.rgb *= LightingScale;		// (note -- should we scale by this here? when using this shader with a basic lighting pipeline [eg, for material preview], the scale is unwanted)
	#endif
	return result;
}

float4 invalid(VSOutput geo) : SV_Target0
{
	float3 color0 = float3(1.0f, 0.f, 0.f);
	float3 color1 = float3(0.0f, 0.f, 1.f);
	uint flag = (uint(geo.position.x/4.f) + uint(geo.position.y/4.f))&1;
	return float4(flag?color0:color1, 1.f);
}
