// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "Ocean.hlsl"
#include "OceanShallow.hlsl"

RWTexture2DArray<float>		WaterHeights : register(u0);
RWTexture2DArray<float>		WaterHeightsN1 : register(u1);
RWTexture2DArray<float>		WaterHeightsN2 : register(u2);

#if (USE_LOOKUP_TABLE==1)
	RWTexture2D<uint>			LookupTable : register(u3);
#endif

Texture2D<float>			GlobalWavesHeightsTexture : register(t4);

cbuffer Consts : register(b2)
{
	int2	LookupTableCoords;
	uint	SimulatingGridIndex;
}

[numthreads(SHALLOW_WATER_TILE_DIMENSION, 1, 1)]
	void		main(uint3 dispatchThreadId : SV_DispatchThreadID)
{
		//	setup initial values -- height values are relative to the base
		//	water height

	float surfaceHeight = LoadSurfaceHeight(uint3(dispatchThreadId.xy, SimulatingGridIndex));

	#if SHALLOW_WATER_BOUNDARY == SWB_SURFACE
		float initHeight = surfaceHeight + 0.5f;
	#elif SHALLOW_WATER_BOUNDARY == SWB_GLOBALWAVES
		uint2 texDim;
		GlobalWavesHeightsTexture.GetDimensions(texDim.x, texDim.y);
		float2 worldCoords = (LookupTableCoords.xy + float2(dispatchThreadId.xy) / float(SHALLOW_WATER_TILE_DIMENSION)) * ShallowGridPhysicalDimension;
		float globalWavesHeight = WaterBaseHeight + StrengthConstantZ * StrengthConstantMultiplier *
			OceanTextureCustomInterpolate(GlobalWavesHeightsTexture, texDim, worldCoords / float2(PhysicalWidth, PhysicalHeight));

		float initHeight = globalWavesHeight;
	#elif SHALLOW_WATER_BOUNDARY == SWB_BASEHEIGHT
		float initHeight = WaterBaseHeight;
	#endif

	WaterHeights	[uint3(dispatchThreadId.xy, SimulatingGridIndex)] = max(initHeight, surfaceHeight);
	WaterHeightsN1	[uint3(dispatchThreadId.xy, SimulatingGridIndex)] = max(initHeight, surfaceHeight);
	WaterHeightsN2	[uint3(dispatchThreadId.xy, SimulatingGridIndex)] = max(initHeight, surfaceHeight);

	#if (USE_LOOKUP_TABLE==1)
		if (dispatchThreadId.x == 0 && dispatchThreadId.y == 0) {
			LookupTable[int2(256,256)+LookupTableCoords] = SimulatingGridIndex;
		}
	#endif
}

[numthreads(SHALLOW_WATER_TILE_DIMENSION, 1, 1)]
	void		InitPipeModel(uint3 dispatchThreadId : SV_DispatchThreadID)
{
		//	setup initial values -- for the pipe model simulation

	float surfaceHeight = LoadSurfaceHeight(uint3(dispatchThreadId.xy, SimulatingGridIndex));

	#if SHALLOW_WATER_BOUNDARY == SWB_SURFACE
		float initHeight = surfaceHeight + 0.5f;
	#elif SHALLOW_WATER_BOUNDARY == SWB_GLOBALWAVES
		uint2 texDim;
		GlobalWavesHeightsTexture.GetDimensions(texDim.x, texDim.y);
		float2 worldCoords = (LookupTableCoords.xy + float2(dispatchThreadId.xy) / float(SHALLOW_WATER_TILE_DIMENSION)) * ShallowGridPhysicalDimension;
		float globalWavesHeight = WaterBaseHeight + StrengthConstantZ * StrengthConstantMultiplier *
			OceanTextureCustomInterpolate(GlobalWavesHeightsTexture, texDim, worldCoords / float2(PhysicalWidth, PhysicalHeight));

		float initHeight = globalWavesHeight;
	#elif SHALLOW_WATER_BOUNDARY == SWB_BASEHEIGHT
		float initHeight = WaterBaseHeight;
	#endif

	WaterHeights		[uint3(dispatchThreadId.xy, SimulatingGridIndex)] = max(initHeight, surfaceHeight);

	#if (USE_LOOKUP_TABLE==1)
		if (dispatchThreadId.x == 0 && dispatchThreadId.y == 0) {
			LookupTable[int2(256,256)+LookupTableCoords] = SimulatingGridIndex;
		}
	#endif
}

////////////////////////////////////////////////////////////////////////////////////////////////
	//   c l e a r   g r i d s   //
////////////////////////////////////////////////////////////////////////////////////////////////
cbuffer ClearGridsConstants
{
	uint	ClearGridsCount;
	int2	ClearGridsAddress[128];
}

[numthreads(1, 1, 1)]
	void		ClearGrids(uint3 dispatchThreadId : SV_DispatchThreadID)
{
	#if (USE_LOOKUP_TABLE==1)
		uint index = dispatchThreadId.x;
		if (index < ClearGridsCount) {
			LookupTable[int2(256,256)+ClearGridsAddress[index]] = 0xff;		// (8 bits per element)
		}
	#endif
}
