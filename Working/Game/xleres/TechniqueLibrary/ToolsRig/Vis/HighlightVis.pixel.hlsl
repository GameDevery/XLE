// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "../../Math/EdgeDetection.hlsl"
#include "../../Utility/DistinctColors.hlsl"

float3 OutlineColour;
uint4 HighlightMarker;
uint4 StencilToMarkerMap[256];

#if !defined(INPUT_MODE)
	#define INPUT_MODE 1
#endif

#if INPUT_MODE == 0
		// In this mode, we're binding a depth buffer/stencil combo to
		// "StencilInput"
		// Oddly, on some hardware Texture2D<uint> works fine to access
		// the stencil parts (assuming the view was created with X24_TYPELESS_G8_UINT
		// or some similar stencil-only format).
		// But, on other hardware we must explicitly access the "G" channel.
	Texture2D<uint2>	StencilInput;
#elif INPUT_MODE == 1
 	Texture2D			StencilInput;
#endif

static const uint DummyMarker = 0xffffffff;

uint GetHighlightMarker()
{
	return HighlightMarker.x;
}

uint Marker(uint2 pos)
{
	#if INPUT_MODE == 0
		uint stencilValue = StencilInput.Load(uint3(pos, 0)).g;
	#elif INPUT_MODE == 1
		uint stencilValue = uint(255.f * StencilInput.Load(uint3(pos, 0)).a);
	#endif
	if (stencilValue == 0) { return DummyMarker; }

	uint result = StencilToMarkerMap[stencilValue%256].x;
	#if ONLY_HIGHLIGHTED!=0
			// in this mode, we ignore every material except the highlighted one
		if (result != GetHighlightMarker()) {
			result = DummyMarker;
		}
	#endif
	return result;
}

bool HatchFilter(uint2 position)
{
	uint p = uint(position.x) + uint(position.y);
	return ((p/4) % 3) == 0;
}

float4 HighlightByStencil(float4 position : SV_Position, float2 texCoord : TEXCOORD0) : SV_Target0
{
	if (!HatchFilter(position.xy)) { discard; }

	uint marker = Marker(uint2(position.xy));
	if (marker == DummyMarker) { discard; }

	return float4(.5f*GetDistinctFloatColour(marker), .5f*1.f);
}

float4 OutlineByStencil(float4 position : SV_Position, float2 texCoord : TEXCOORD0) : SV_Target0
{
	uint2 basePos = uint2(position.xy);
	uint testMarker = Marker(int2(basePos));

	if (testMarker != DummyMarker) discard;

	float2 dhdp = 0.0.xx;
	[unroll] for (int y=0; y<5; ++y) {
		[unroll] for (int x=0; x<5; ++x) {
			uint marker = Marker(int2(basePos) + 2 * int2(x-2, y-2));
			float value = (marker == testMarker);
			dhdp.x += SharrHoriz5x5[x][y] * value;
			dhdp.y += SharrVert5x5[x][y] * value;
		}
	}

	float alpha = max(abs(dhdp.x), abs(dhdp.y));
	alpha = 1.0f - pow(1.0f-alpha, 8.f);
	return float4(alpha * OutlineColour, alpha);
}
