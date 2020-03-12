// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "ShadowResources.h"
#include "../RenderCore/Techniques/RenderStateResolver.h"
#include "../RenderCore/Metal/ObjectFactory.h"
#include "../BufferUploads/ResourceLocator.h"

namespace SceneEngine
{
    using namespace RenderCore;

    class ShadowParameters
    {
    public:
        Float4      _filterKernel[32];
    };

    ShadowResourcesBox::ShadowResourcesBox(const Desc& desc)
    {
            //      32 tap sampling kernel for shadows
        ShadowParameters shadowParameters;
        shadowParameters._filterKernel[ 0] = Float4(-0.1924249f, -0.5685654f,0,0);
        shadowParameters._filterKernel[ 1] = Float4(0.0002287195f, -0.830722f,0,0);
        shadowParameters._filterKernel[ 2] = Float4(-0.6227817f, -0.676464f,0,0);
        shadowParameters._filterKernel[ 3] = Float4(-0.3433303f, -0.8954138f,0,0);
        shadowParameters._filterKernel[ 4] = Float4(-0.3087259f, 0.0593961f,0,0);
        shadowParameters._filterKernel[ 5] = Float4(0.4013956f, 0.005351349f,0,0);
        shadowParameters._filterKernel[ 6] = Float4(0.6675568f, 0.2226908f,0,0);
        shadowParameters._filterKernel[ 7] = Float4(0.4703487f, 0.4219977f,0,0);
        shadowParameters._filterKernel[ 8] = Float4(-0.865732f, -0.1704932f,0,0);
        shadowParameters._filterKernel[ 9] = Float4(0.4836336f, -0.7363456f,0,0);
        shadowParameters._filterKernel[10] = Float4(-0.8455518f, 0.429606f,0,0);
        shadowParameters._filterKernel[11] = Float4(0.2486194f, 0.7276461f,0,0);
        shadowParameters._filterKernel[12] = Float4(0.01841145f, 0.581219f,0,0);
        shadowParameters._filterKernel[13] = Float4(0.9428069f, 0.2151681f,0,0);
        shadowParameters._filterKernel[14] = Float4(-0.2937738f, 0.8432091f,0,0);
        shadowParameters._filterKernel[15] = Float4(0.01657544f, 0.9762882f,0,0);

        shadowParameters._filterKernel[16] = Float4(0.03878351f, -0.1410931f,0,0);
        shadowParameters._filterKernel[17] = Float4(-0.3663213f, -0.348966f,0,0);
        shadowParameters._filterKernel[18] = Float4(0.2333971f, -0.5178556f,0,0);
        shadowParameters._filterKernel[19] = Float4(-0.6433204f, -0.3284476f,0,0);
        shadowParameters._filterKernel[20] = Float4(0.1255225f, 0.3221043f,0,0);
        shadowParameters._filterKernel[21] = Float4(0.4051761f, -0.299208f,0,0);
        shadowParameters._filterKernel[22] = Float4(0.8829983f, -0.1718857f,0,0);
        shadowParameters._filterKernel[23] = Float4(0.6724088f, -0.3562584f,0,0);
        shadowParameters._filterKernel[24] = Float4(-0.826445f, 0.1214067f,0,0);
        shadowParameters._filterKernel[25] = Float4(-0.386752f, 0.406546f,0,0);
        shadowParameters._filterKernel[26] = Float4(-0.5869312f, -0.01993746f,0,0);
        shadowParameters._filterKernel[27] = Float4(0.7842119f, 0.5549603f,0,0);
        shadowParameters._filterKernel[28] = Float4(0.5801646f, 0.7416336f,0,0);
        shadowParameters._filterKernel[29] = Float4(0.7366455f, -0.6388465f,0,0);
        shadowParameters._filterKernel[30] = Float4(-0.6067169f, 0.6372176f,0,0);
        shadowParameters._filterKernel[31] = Float4(0.2743046f, -0.9303559f,0,0);

        _sampleKernel32 = Metal::MakeConstantBuffer(
			Metal::GetObjectFactory(), 
			MakeIteratorRange(&shadowParameters, PtrAdd(&shadowParameters, sizeof(shadowParameters))));
    }

    ShadowResourcesBox::~ShadowResourcesBox() {}

}

