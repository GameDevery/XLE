// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "FluidAdvection.h"
#include "../Math/RegularNumberField.h"

#pragma warning(disable:4714)
#pragma push_macro("new")
#undef new
#include <Eigen/Dense>
#pragma pop_macro("new")

namespace SceneEngine
{
    using ScalarField2D = XLEMath::ScalarField2D<Eigen::VectorXf>;
    using VectorField2D = VectorField2DSeparate<Eigen::VectorXf>;

    template<unsigned Interpolation, typename Field>
        static Float2 AdvectRK4(
            const Field& velFieldT0, const Field& velFieldT1,
            UInt2 pt, Float2 velScale)
        {
            const auto s = velScale;
            const auto halfS = decltype(s)(s / 2);
    
            Float2 startTap = Float2(float(pt[0]), float(pt[1]));
            auto k1 = velFieldT0.Load(pt);
            auto k2 = .5f * velFieldT0.Sample<Interpolation|RNFSample::Clamp>(startTap + MultiplyAcross(halfS, k1))
                    + .5f * velFieldT1.Sample<Interpolation|RNFSample::Clamp>(startTap + MultiplyAcross(halfS, k1))
                    ;
            auto k3 = .5f * velFieldT0.Sample<Interpolation|RNFSample::Clamp>(startTap + MultiplyAcross(halfS, k2))
                    + .5f * velFieldT1.Sample<Interpolation|RNFSample::Clamp>(startTap + MultiplyAcross(halfS, k2))
                    ;
            auto k4 = velFieldT1.Sample<Interpolation|RNFSample::Clamp>(startTap + MultiplyAcross(s, k3));
    
            auto finalVel = (1.f / 6.f) * (k1 + 2.f * k2 + 2.f * k3 + k4);
            return startTap + MultiplyAcross(s, finalVel);
        }

    template<unsigned Interpolation>
        static Float2 AdvectRK4(
            const VectorField2D& velFieldT0, const VectorField2D& velFieldT1,
            Float2 pt, Float2 velScale)
        {
            const auto s = velScale;
            const auto halfS = decltype(s)(s / 2);

                // when using a float point input, we need bilinear interpolation
            auto k1 = velFieldT0.Sample<Interpolation|RNFSample::Clamp>(pt);
            auto k2 = .5f * velFieldT0.Sample<Interpolation|RNFSample::Clamp>(pt + MultiplyAcross(halfS, k1))
                    + .5f * velFieldT1.Sample<Interpolation|RNFSample::Clamp>(pt + MultiplyAcross(halfS, k1))
                    ;
            auto k3 = .5f * velFieldT0.Sample<Interpolation|RNFSample::Clamp>(pt + MultiplyAcross(halfS, k2))
                    + .5f * velFieldT1.Sample<Interpolation|RNFSample::Clamp>(pt + MultiplyAcross(halfS, k2))
                    ;
            auto k4 = velFieldT1.Sample<Interpolation|RNFSample::Clamp>(pt + MultiplyAcross(s, k3));

            auto finalVel = (1.f / 6.f) * (k1 + 2.f * k2 + 2.f * k3 + k4);
            return pt + MultiplyAcross(s, finalVel);
        }

    template<typename Type> Type MaxValue();
    template<> float MaxValue()         { return FLT_MAX; }
    template<> Float2 MaxValue()        { return Float2(FLT_MAX, FLT_MAX); }
    float   Min(float lhs, float rhs)   { return std::min(lhs, rhs); }
    Float2  Min(Float2 lhs, Float2 rhs) { return Float2(std::min(lhs[0], rhs[0]), std::min(lhs[1], rhs[1])); }
    float   Max(float lhs, float rhs)   { return std::max(lhs, rhs); }
    Float2  Max(Float2 lhs, Float2 rhs) { return Float2(std::max(lhs[0], rhs[0]), std::max(lhs[1], rhs[1])); }

    template<unsigned SamplingFlags, typename Field>
        typename Field::ValueType LoadWithNearbyRange(typename Field::ValueType& minNeighbour, typename Field::ValueType& maxNeighbour, const Field& field, Float2 pt)
        {
            Field::ValueType predictorParts[9];
            float predictorWeights[4];
            field.GatherNeighbors(predictorParts, predictorWeights, pt);
            
            minNeighbour =  MaxValue<Field::ValueType>();
            maxNeighbour = -MaxValue<Field::ValueType>();
            for (unsigned c=0; c<9; ++c) {
                minNeighbour = Min(predictorParts[c], minNeighbour);
                maxNeighbour = Max(predictorParts[c], maxNeighbour);
            }

            if (constant_expression<(SamplingFlags & RNFSample::Cubic)==0>::result()) {
                return
                      predictorWeights[0] * predictorParts[0]
                    + predictorWeights[1] * predictorParts[1]
                    + predictorWeights[2] * predictorParts[2]
                    + predictorWeights[3] * predictorParts[3];
            } else {
                return field.Sample<RNFSample::Cubic|RNFSample::Clamp>(pt);
            }
        }

    template<typename Field, typename VelField>
        void PerformAdvection(
            Field dstValues, Field srcValues, 
            VelField velFieldT0, VelField velFieldT1,
            float deltaTime, const AdvectionSettings& settings)
    {
        //
        // This is the advection step. We will use the method of characteristics.
        //
        // We have a few different options for the stepping method:
        //  * basic euler forward integration (ie, just step forward in time)
        //  * forward integration method divided into smaller time steps
        //  * Runge-Kutta integration
        //  * Modified MacCormack methods
        //  * Back and Forth Error Compensation and Correction (BFECC)
        //
        // Let's start without any complex boundary conditions.
        //
        // We have to be careful about how the velocity sample is aligned with
        // the grid cell. Incorrect alignment will produce a bias in the way that
        // we interpolate the field.
        //
        // We could consider offsetting the velocity field by half a cell (see
        // Visual Simulation of Smoke, Fedkiw, et al)
        //
        // Also consider Semi-Lagrangian methods for large timesteps (when the CFL
        // number is larger than 1)
        //

        const auto advectionMethod = settings._method;
        const auto adjvectionSteps = settings._subSteps;

        const unsigned width = dstValues.Width();
        assert(width == srcValues.Width());
        assert(width == velFieldT0.Width());
        assert(width == velFieldT1.Width());
        const UInt3 dims(width, width, 1);
        const UInt3 margin(1,1,0);
        const Float2 velFieldScale = Float2(
            float(dims[0]-2*margin[0]),
            float(dims[1]-2*margin[1]));   // (grid size without borders)

        if (advectionMethod == AdvectionMethod::ForwardEuler) {

                //  For each cell in the grid, trace backwards
                //  through the velocity field to find an approximation
                //  of where the point was in the previous frame.

            const unsigned SamplingFlags = 0;
            for (unsigned y=margin[1]; y<dims[1]-margin[1]; ++y)
                for (unsigned x=margin[0]; x<dims[0]-margin[0]; ++x) {
                    auto startVel = velFieldT1.Load(UInt2(x, y));
                    Float2 tap = Float2(float(x), float(y)) - MultiplyAcross(deltaTime * velFieldScale, startVel);
                    tap[0] = Clamp(tap[0], 0.f, float(dims[0]-1) - 1e-5f);
                    tap[1] = Clamp(tap[1], 0.f, float(dims[1]-1) - 1e-5f);
                    dstValues.Write(UInt2(x, y), srcValues.Sample<SamplingFlags>(tap));
                }

        } else if (advectionMethod == AdvectionMethod::ForwardEulerDiv) {

            auto stepScale = Float2(deltaTime * velFieldScale / float(adjvectionSteps));
            const unsigned SamplingFlags = 0;
            for (unsigned y=margin[1]; y<dims[1]-margin[1]; ++y)
                for (unsigned x=margin[0]; x<dims[0]-margin[0]; ++x) {

                    Float2 tap = Float2(float(x), float(y));
                    for (unsigned s=0; s<adjvectionSteps; ++s) {
                        float a = (adjvectionSteps-1-s) / float(adjvectionSteps-1);
                        auto vel = 
                            LinearInterpolate(
                                velFieldT0.Sample<SamplingFlags>(tap),
                                velFieldT1.Sample<SamplingFlags>(tap),
                                a);

                        tap -= MultiplyAcross(stepScale, vel);
                        tap[0] = Clamp(tap[0], 0.f, float(dims[0]-1) - 1e-5f);
                        tap[1] = Clamp(tap[1], 0.f, float(dims[1]-1) - 1e-5f);
                    }

                    dstValues.Write(UInt2(x, y), srcValues.Sample<SamplingFlags>(tap));
                }

        } else if (advectionMethod == AdvectionMethod::RungeKutta) {

            if (settings._interpolation == AdvectionInterpolationMethod::Bilinear) {

                const auto SamplingFlags = 0u;
                for (unsigned y=margin[1]; y<dims[1]-margin[1]; ++y)
                    for (unsigned x=margin[0]; x<dims[0]-margin[0]; ++x) {

                            // This is the RK4 version
                            // We'll use the average of the velocity field at t and
                            // the velocity field at t+dt as an estimate of the field
                            // at t+.5*dt

                            // Note that we're tracing the velocity field backwards.
                            // So doing k1 on velField1, and k4 on velFieldT0
                            //      -- hoping this will interact with the velocity diffusion more sensibly
                        const auto tap = AdvectRK4<SamplingFlags>(velFieldT1, velFieldT0, UInt2(x, y), -deltaTime * velFieldScale);
                        dstValues.Write(UInt2(x, y), srcValues.Sample<SamplingFlags|RNFSample::Clamp>(tap));

                    }

            } else {

                const auto SamplingFlags = RNFSample::Cubic;
                for (unsigned y=margin[1]; y<dims[1]-margin[1]; ++y)
                    for (unsigned x=margin[0]; x<dims[0]-margin[0]; ++x) {
                        const auto tap = AdvectRK4<SamplingFlags>(velFieldT1, velFieldT0, UInt2(x, y), -deltaTime * velFieldScale);
                        dstValues.Write(UInt2(x, y), srcValues.Sample<SamplingFlags|RNFSample::Clamp>(tap));
                    }

            }

        } else if (advectionMethod == AdvectionMethod::MacCormackRK4) {

                //
                // This is a modified MacCormack scheme, as described in An Unconditionally
                // Stable MacCormack Method -- Selle & Fedkiw, et al.
                //  http://physbam.stanford.edu/~fedkiw/papers/stanford2006-09.pdf
                //
                // It's also similar to the (oddly long nammed) Back And Forth Error Compensation 
                // and Correction (BFECC).
                //
                // Basically, we want to run an initial predictor step, then run a backwards
                // advection to find an intermediate point. The difference between the value at
                // the initial point and the intermediate point is used as a error term.
                //
                // This way, we get an improved estimate, but with only 2 advection steps.
                //
                // We need to use some advection method for the forward and advection steps. Often
                // a semi-lagrangian method is used (particularly velocities and timesteps are large
                // with respect to the grid size). 
                //
                // But here, we'll use RK4.
                //
                // We also need a way to check for overruns and oscillation cases. Selle & Fedkiw
                // suggest using a normal semi-Lagrangian method in these cases. We'll try a simplier
                // method and just clamp.
                //

            if (settings._interpolation == AdvectionInterpolationMethod::Bilinear) {

                const auto SamplingFlags = 0u;
                for (unsigned y=margin[1]; y<dims[1]-margin[1]; ++y)
                    for (unsigned x=margin[0]; x<dims[0]-margin[0]; ++x) {

                        const auto pt = UInt2(x, y);

                            // advect backwards in time first, to find the predictor
                        const auto predictor = AdvectRK4<SamplingFlags>(velFieldT1, velFieldT0, pt, -deltaTime * velFieldScale);
                            // advect forward again to find the error tap
                        const auto reversedTap = AdvectRK4<SamplingFlags>(velFieldT0, velFieldT1, predictor, deltaTime * velFieldScale);

                        auto originalValue = srcValues.Load(pt);
                        auto reversedValue = srcValues.Sample<SamplingFlags|RNFSample::Clamp>(reversedTap);
                        Field::ValueType finalValue;

                            // Here we clamp the final result within the range of the neighbour cells of the 
                            // original predictor. This prevents the scheme from becoming unstable (by avoiding
                            // irrational values for 0.5f * (originalValue - reversedValue)
                        const bool doRangeClamping = true;
                        if (constant_expression<doRangeClamping>::result()) {
                            Field::ValueType minNeighbour, maxNeighbour;
                            auto predictorValue = LoadWithNearbyRange<SamplingFlags>(minNeighbour, maxNeighbour, srcValues, predictor);
                            finalValue = Field::ValueType(predictorValue + .5f * (originalValue - reversedValue));
                            finalValue = Max(finalValue, minNeighbour);
                            finalValue = Min(finalValue, maxNeighbour);
                        } else {
                            auto predictorValue = srcValues.Sample<SamplingFlags|RNFSample::Clamp>(predictor);
                            finalValue = Field::ValueType(predictorValue + .5f * (originalValue - reversedValue));
                        }

                        dstValues.Write(pt, finalValue);

                    }

            } else {

                const auto SamplingFlags = RNFSample::Cubic;
                for (unsigned y=margin[1]; y<dims[1]-margin[1]; ++y)
                    for (unsigned x=margin[0]; x<dims[0]-margin[0]; ++x) {

                        const auto pt = UInt2(x, y);
                        const auto predictor = AdvectRK4<SamplingFlags>(velFieldT1, velFieldT0, pt, -deltaTime * velFieldScale);
                        const auto reversedTap = AdvectRK4<SamplingFlags>(velFieldT0, velFieldT1, predictor, deltaTime * velFieldScale);

                        auto originalValue = srcValues.Load(pt);
                        auto reversedValue = srcValues.Sample<SamplingFlags|RNFSample::Clamp>(reversedTap);

                        Field::ValueType minNeighbour, maxNeighbour;
                        auto predictorValue = LoadWithNearbyRange<SamplingFlags>(minNeighbour, maxNeighbour, srcValues, predictor);
                        auto finalValue = Field::ValueType(predictorValue + .5f * (originalValue - reversedValue));
                        finalValue = Max(finalValue, minNeighbour);
                        finalValue = Min(finalValue, maxNeighbour);

                        dstValues.Write(pt, finalValue);

                    }

            }

        }

    }

    template void PerformAdvection(
        ScalarField2D, ScalarField2D, 
        VectorField2D, VectorField2D,
        float, const AdvectionSettings&);

    template void PerformAdvection(
        VectorField2D, VectorField2D, 
        VectorField2D, VectorField2D,
        float, const AdvectionSettings&);
}
