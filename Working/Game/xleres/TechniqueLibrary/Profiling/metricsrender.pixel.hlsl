// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "../Framework/CommonResources.hlsl"

Texture2D DefaultTexture;

float4 main(float4 position : SV_Position, float2 texCoord : TEXCOORD, float4 color : COLOR) : SV_Target0
{
	float4 texCol = DefaultTexture.SampleLevel(DefaultSampler, texCoord, 0);

	float2 offsets[] =
	{
		float2(-4.f/512.f, -4.f/64.f),
		float2( 4.f/512.f, -4.f/64.f),
		float2(-4.f/512.f,  4.f/64.f),
		float2( 4.f/512.f,  4.f/64.f),

		float2(-4.f/512.f,  0.f/64.f),
		float2( 4.f/512.f,  0.f/64.f),
		float2( 0.f/512.f, -4.f/64.f),
		float2( 0.f/512.f,  4.f/64.f)
	};

	float shadowA = 0.f;
	for (uint c=0; c<8; ++c) {
		shadowA += DefaultTexture.SampleLevel(DefaultSampler, texCoord - offsets[c], 0).a / 3.f;
	}

	return float4(color.rgb*texCol.a, color.a*(texCol.a + shadowA));
}
