// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#if !defined(CASCADE_RESOLVE_H)
#define CASCADE_RESOLVE_H

#include "LightDesc.hlsl"      // CascadeAddress
#include "ShadowProjection.hlsl"
#include "../../Framework/SystemUniforms.hlsl"
#include "../../Math/ProjectionMath.hlsl"

///////////////////////////////////////////////////////////////////////////////////////////////////

CascadeAddress ResolveCascade_FromWorldPosition(float3 worldPosition, uint cascadeMode, bool enableNearCascade)
{
        // find the first frustum we're within
    uint projectionCount = min(GetShadowSubProjectionCount(cascadeMode), ShadowMaxSubProjections);

    if (cascadeMode == SHADOW_CASCADE_MODE_ORTHOGONAL) {
        float3 basePosition = mul(OrthoShadowWorldToProj, float4(worldPosition, 1));
        if (enableNearCascade) {
            float4 frustumCoordinates = float4(mul(OrthoNearCascade, float4(basePosition, 1)), 1.f);
            if (PtInFrustum(frustumCoordinates))
                return CascadeAddress_Create(frustumCoordinates, projectionCount, OrthoShadowNearMinimalProjection);
        }

            // In ortho mode, all frustums have the same near and far depth
            // so we can check Z independently from XY
            // (except for the near cascade, which is focused on the near geometry)
        [branch] if (PtInFrustumZ(float4(basePosition, 1.f))) {
            CascadeAddress result = CascadeAddress_Invalid();
            [unroll] for (int c=ShadowMaxSubProjections-1; c>=0; c--) {
                float4 frustumCoordinates = float4(AdjustForOrthoCascade(basePosition, c), 1.f);
                if (PtInFrustumXY(frustumCoordinates))
                    result = CascadeAddress_Create(frustumCoordinates, c, ShadowProjection_GetMiniProj_NotNear(c, cascadeMode));
            }
            return result;
        }
    } else {
        [unroll] for (uint c=0; c<ShadowMaxSubProjections; c++) {
            float4 frustumCoordinates = mul(ShadowWorldToProj[c], float4(worldPosition, 1));
            if (PtInFrustum(frustumCoordinates))
                return CascadeAddress_Create(frustumCoordinates, c, ShadowProjection_GetMiniProj(c, cascadeMode));
        }
    }

    return CascadeAddress_Invalid();
}

float4 CameraCoordinateToShadow(float2 camCoordinate, float worldSpaceDepth, float4x4 camToShadow, uint cascadeMode)
{
    const float cameraCoordinateScale = worldSpaceDepth; // (linear0To1Depth * SysUniform_GetFarClip());

        //
        //	Accuracy of this transformation is critical...
        //		We'll be comparing to values in the shadow buffer, so we
        //		should try to use the most accurate transformation method
        //
    if (cascadeMode==SHADOW_CASCADE_MODE_ORTHOGONAL) {

        float3x3 cameraToShadow3x3 = float3x3(camToShadow[0].xyz, camToShadow[1].xyz, camToShadow[2].xyz);
        float3 offset = mul(cameraToShadow3x3, float3(camCoordinate, -1.f));
        offset *= cameraCoordinateScale;	// try doing this scale here (maybe increase accuracy a bit?)

        float3 translatePart = float3(camToShadow[0].w, camToShadow[1].w, camToShadow[2].w);
        return float4(offset + translatePart, 1.f);

    } else {

        float4x3 cameraToShadow4x3 = float4x3(
            camToShadow[0].xyz, camToShadow[1].xyz,
            camToShadow[2].xyz, camToShadow[3].xyz);

            // Note the "-1" here is due to our view of camera space, where -Z is into the screen.
            // the scale by cameraCoordinateScale will later scale this up to the correct depth.
        float4 offset = mul(cameraToShadow4x3, float3(camCoordinate, -1.f));
        offset *= cameraCoordinateScale;	// try doing this scale here (maybe increase accuracy a bit?)

        float4 translatePart = float4(camToShadow[0].w, camToShadow[1].w, camToShadow[2].w, camToShadow[3].w);
        return offset + translatePart;

    }

    // return mul(camToShadow, float4(float3(camCoordinate, -1.f) * cameraCoordinateScale, 1.f));
}

CascadeAddress ResolveCascade_CameraToShadowMethod(float2 texCoord, float worldSpaceDepth, uint cascadeMode, bool enableNearCascade)
{
    const float2 camCoordinate = XYScale * texCoord + XYTrans;

    uint projectionCount = min(GetShadowSubProjectionCount(cascadeMode), ShadowMaxSubProjections);

        // 	Find the first frustum we're within
        //	This first loop is kept separate and simple
        //	even though it means we need another comparison
        //	below. This is just to try to keep the generated
        //	shader code simplier.
        //
        //	Note that in order to unroll this first loop, we
        //	must make the loop terminator a compile time constant.
        //	Normally, the number of cascades is passed in a shader
        //	constant (ie, not available at compile time).
        //	However, if the cascade loop is simple, it may be better
        //	to unroll, even if it means a few extra redundant checks
        //	at the end.
        //
        //	It looks like these 2 tweaks (separating the first loop,
        //	and unrolling it) reduces the number of temporary registers
        //	required by 4 (but obvious increases the instruction count).
        //	That seems like a good improvement.

    if (cascadeMode==SHADOW_CASCADE_MODE_ORTHOGONAL) {

        if (enableNearCascade) {
            float4 nearCascadeCoord = float4(CameraCoordinateToShadow(camCoordinate, worldSpaceDepth, OrthoNearCameraToShadow, cascadeMode).xyz, 1.f);
            if (PtInFrustum(nearCascadeCoord))
                return CascadeAddress_Create(nearCascadeCoord, projectionCount, OrthoShadowNearMinimalProjection);
        }

            // in ortho mode, this is much simplier... Here is a
            // separate implementation to take advantage of that case!
        float3 baseCoord = CameraCoordinateToShadow(camCoordinate, worldSpaceDepth, OrthoCameraToShadow, cascadeMode).xyz;
        [branch] if (PtInFrustumZ(float4(baseCoord, 1.f))) {
            CascadeAddress result = CascadeAddress_Invalid();
            [unroll] for (int c=ShadowMaxSubProjections-1; c>=0; c--) {
                float4 t = float4(AdjustForOrthoCascade(baseCoord, c), 1.f);
                if (PtInFrustumXY(t))
                    result = CascadeAddress_Create(t, c, ShadowProjection_GetMiniProj_NotNear(c, cascadeMode));
            }
            return result;
        }

    } else {

        for (uint c=0; c<projectionCount; c++) {
            float4 frustumCoordinates = CameraCoordinateToShadow(camCoordinate, worldSpaceDepth, CameraToShadow[c], cascadeMode);
            if (PtInFrustum(frustumCoordinates))
                return CascadeAddress_Create(frustumCoordinates, c, ShadowProjection_GetMiniProj(c, cascadeMode));
        }

    }

    return CascadeAddress_Invalid();
}

CascadeAddress CascadeAddress_CubeMap(float3 lightToSamplePoint)
{
    CascadeAddress result;
    result.cascadeIndex = 0;
    result.frustumCoordinates = float4(lightToSamplePoint, 1);
    result.miniProjection = ShadowMinimalProjection[0];
    return result;
}

#endif
