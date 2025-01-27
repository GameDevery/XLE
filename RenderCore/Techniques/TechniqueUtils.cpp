// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "TechniqueUtils.h"
#include "../../Math/Transformations.h"
#include "../../Math/ProjectionMath.h"

namespace RenderCore { namespace Techniques
{
    Float3 NegativeLightDirection    = Normalize(Float3(0.f, 1.0f, 1.f));

    CameraDesc::CameraDesc()
    {
        _cameraToWorld          = Identity<Float4x4>();
        _nearClip               = 0.1f;
        _farClip                = 100000.f;
        _verticalFieldOfView    = Deg2Rad(34.8246f);
        _projection             = Projection::Perspective;
        _left = _top = -1.f;
        _right = _bottom = 1.f;
    }


    Float4x4 Projection(const CameraDesc& sceneCamera, float viewportAspect)
    {
        if (sceneCamera._projection == CameraDesc::Projection::Orthogonal) {
            return OrthogonalProjection(
                sceneCamera._left, sceneCamera._top, 
                sceneCamera._right, sceneCamera._bottom, 
                sceneCamera._nearClip, sceneCamera._farClip,
                GeometricCoordinateSpace::RightHanded, 
                GetDefaultClipSpaceType());
        } else {
            return PerspectiveProjection(
                sceneCamera._verticalFieldOfView, viewportAspect,
                sceneCamera._nearClip, sceneCamera._farClip, 
                GeometricCoordinateSpace::RightHanded, 
                GetDefaultClipSpaceType());
        }
    }

    ClipSpaceType::Enum GetDefaultClipSpaceType()
    {
            // (todo -- this condition could be a runtime test)
        #if (GFXAPI_ACTIVE == GFXAPI_DX11) || (GFXAPI_ACTIVE == GFXAPI_DX9)         
            return ClipSpaceType::Positive;
        #else
            return ClipSpaceType::StraddlingZero;
        #endif
    }

    std::pair<Float3, Float3> BuildRayUnderCursor(
        Int2 mousePosition, CameraDesc& sceneCamera, 
        const std::pair<Float2, Float2>& viewport)
    {
            // calculate proper worldToProjection for this cameraDesc and viewport
            //      -- then get the frustum corners. We can use these to find the
            //          correct direction from the view position under the given 
            //          mouse position
        Float3 frustumCorners[8];
        const float viewportAspect = (viewport.second[0] - viewport.first[0]) / float(viewport.second[1] - viewport.first[1]);
        auto projectionMatrix = Projection(sceneCamera, viewportAspect);

        auto worldToProjection = Combine(InvertOrthonormalTransform(sceneCamera._cameraToWorld), projectionMatrix);
        CalculateAbsFrustumCorners(frustumCorners, worldToProjection);

        Float3 cameraPosition = ExtractTranslation(sceneCamera._cameraToWorld);
        return XLEMath::BuildRayUnderCursor(
            mousePosition, frustumCorners, cameraPosition, 
            sceneCamera._nearClip, sceneCamera._farClip,
            viewport);
    }

    ProjectionDesc::ProjectionDesc()
    {
        _worldToProjection = Identity<Float4x4>();
        _cameraToProjection = Identity<Float4x4>();
        _cameraToWorld = Identity<Float4x4>();
        _verticalFov = 0.f;
        _aspectRatio = 0.f;
        _nearClip = 0.f;
        _farClip = 0.f;
    }

    GlobalTransformConstants BuildGlobalTransformConstants(const ProjectionDesc& projDesc)
    {
        GlobalTransformConstants globalTransform;
        globalTransform._worldToClip = projDesc._worldToProjection;
        globalTransform._viewToWorld = projDesc._cameraToWorld;
        globalTransform._worldSpaceView = ExtractTranslation(projDesc._cameraToWorld);
        globalTransform._minimalProjection = ExtractMinimalProjection(projDesc._cameraToProjection);
        globalTransform._farClip = CalculateNearAndFarPlane(globalTransform._minimalProjection, GetDefaultClipSpaceType()).second;

            //  We can calculate the projection corners either from the camera to world,
            //  transform or from the final world-to-clip transform. Let's try to pick 
            //  the method that gives the most accurate results.
            //
            //  Using the world to clip matrix should be the most reliable, because it 
            //  will most likely agree with the shader results. The shaders only use 
            //  cameraToWorld occasionally, but WorldToClip is an important part of the
            //  pipeline.

        enum FrustumCornersMode { FromWorldToClip, FromCameraToWorld };
        const FrustumCornersMode cornersMode = FromWorldToClip;

        if (constant_expression<cornersMode == FromWorldToClip>::result()) {

            Float3 absFrustumCorners[8];
            CalculateAbsFrustumCorners(absFrustumCorners, globalTransform._worldToClip);
            for (unsigned c=0; c<4; ++c) {
                globalTransform._frustumCorners[c] = 
                    Expand(Float3(absFrustumCorners[4+c] - globalTransform._worldSpaceView), 1.f);
            }

        } else if (constant_expression<cornersMode == FromCameraToWorld>::result()) {

                //
                //      "transform._frustumCorners" should be the world offsets of the corners of the frustum
                //      from the camera position.
                //
                //      Camera coords:
                //          Forward:    -Z
                //          Up:         +Y
                //          Right:      +X
                //
            const float top = projDesc._nearClip * XlTan(.5f * projDesc._verticalFov);
            const float right = top * projDesc._aspectRatio;
            Float3 preTransformCorners[] = {
                Float3(-right,  top, -projDesc._nearClip),
                Float3(-right, -top, -projDesc._nearClip),
                Float3( right,  top, -projDesc._nearClip),
                Float3( right, -top, -projDesc._nearClip) 
            };
            float scale = projDesc._farClip / projDesc._nearClip;
            for (unsigned c=0; c<4; ++c) {
                globalTransform._frustumCorners[c] = 
                    Expand(Float3(TransformDirectionVector(projDesc._cameraToWorld, preTransformCorners[c]) * scale), 1.f);
            }
        }

        return globalTransform;
    }

    SharedPkt MakeLocalTransformPacket(const Float4x4& localToWorld, const CameraDesc& camera)
    {
        return MakeLocalTransformPacket(localToWorld, ExtractTranslation(camera._cameraToWorld));
    }

    LocalTransformConstants MakeLocalTransform(const Float4x4& localToWorld, const Float3& worldSpaceCameraPosition)
    {
        LocalTransformConstants localTransform;
        CopyTransform(localTransform._localToWorld, localToWorld);
        auto worldToLocal = InvertOrthonormalTransform(localToWorld);
        localTransform._localSpaceView = TransformPoint(worldToLocal, worldSpaceCameraPosition);
        localTransform._materialGuid = ~0x0ull;
        return localTransform;
    }

    SharedPkt MakeLocalTransformPacket(const Float4x4& localToWorld, const Float3& worldSpaceCameraPosition)
    {
        return MakeSharedPkt(MakeLocalTransform(localToWorld, worldSpaceCameraPosition));
    }


}}

