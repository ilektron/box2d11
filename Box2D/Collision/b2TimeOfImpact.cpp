/*
* Copyright (c) 2007-2009 Erin Catto http://www.box2d.org
*
* This software is provided 'as-is', without any express or implied
* warranty.  In no event will the authors be held liable for any damages
* arising from the use of this software.
* Permission is granted to anyone to use this software for any purpose,
* including commercial applications, and to alter it and redistribute it
* freely, subject to the following restrictions:
* 1. The origin of this software must not be misrepresented; you must not
* claim that you wrote the original software. If you use this software
* in a product, an acknowledgment in the product documentation would be
* appreciated but is not required.
* 2. Altered source versions must be plainly marked as such, and must not be
* misrepresented as being the original software.
* 3. This notice may not be removed or altered from any source distribution.
*/

#include <Box2D/Collision/b2Collision.h>
#include <Box2D/Collision/b2Distance.h>
#include <Box2D/Collision/b2TimeOfImpact.h>
#include <Box2D/Collision/Shapes/b2CircleShape.h>
#include <Box2D/Collision/Shapes/b2PolygonShape.h>
#include <Box2D/Common/b2Timer.h>

#include <stdio.h>

using namespace box2d;
    

float b2TOIState::b2_toiTime = 0;
float b2TOIState::b2_toiMaxTime = 0;
int32_t b2TOIState::b2_toiCalls = 0;
int32_t b2TOIState::b2_toiIters = 0;
int32_t b2TOIState::b2_toiMaxIters = 0;
int32_t b2TOIState::b2_toiRootIters = 0;
int32_t b2TOIState::b2_toiMaxRootIters = 0;

namespace
{
    //
    struct b2SeparationFunction
    {
        enum Type
        {
            e_points,
            e_faceA,
            e_faceB
        };

        // TODO_ERIN might not need to return the separation

        float Initialize(const b2SimplexCache* cache, const b2DistanceProxy* proxyA,
                        const b2Sweep& sweepA, const b2DistanceProxy* proxyB, const b2Sweep& sweepB,
                        float t1)
        {
            m_proxyA = proxyA;
            m_proxyB = proxyB;
            int32_t count = cache->count;
            b2Assert(0 < count && count < 3);

            m_sweepA = sweepA;
            m_sweepB = sweepB;

            b2Transform xfA, xfB;
            m_sweepA.GetTransform(&xfA, t1);
            m_sweepB.GetTransform(&xfB, t1);

            if (count == 1)
            {
                m_type = e_points;
                b2Vec<float, 2> localPointA = m_proxyA->GetVertex(cache->indexA[0]);
                b2Vec<float, 2> localPointB = m_proxyB->GetVertex(cache->indexB[0]);
                b2Vec<float, 2> pointA = b2Mul(xfA, localPointA);
                b2Vec<float, 2> pointB = b2Mul(xfB, localPointB);
                m_axis = pointB - pointA;
                float s = m_axis.Normalize();
                return s;
            }
            else if (cache->indexA[0] == cache->indexA[1])
            {
                // Two points on B and one on A.
                m_type = e_faceB;
                b2Vec<float, 2> localPointB1 = proxyB->GetVertex(cache->indexB[0]);
                b2Vec<float, 2> localPointB2 = proxyB->GetVertex(cache->indexB[1]);

                m_axis = b2Cross(localPointB2 - localPointB1, 1.0f);
                m_axis.Normalize();
                b2Vec<float, 2> normal = b2Mul(xfB.q, m_axis);

                m_localPoint = 0.5f * (localPointB1 + localPointB2);
                b2Vec<float, 2> pointB = b2Mul(xfB, m_localPoint);

                b2Vec<float, 2> localPointA = proxyA->GetVertex(cache->indexA[0]);
                b2Vec<float, 2> pointA = b2Mul(xfA, localPointA);

                float s = b2Dot(pointA - pointB, normal);
                if (s < 0.0f)
                {
                    m_axis = -m_axis;
                    s = -s;
                }
                return s;
            }
            else
            {
                // Two points on A and one or two points on B.
                m_type = e_faceA;
                b2Vec<float, 2> localPointA1 = m_proxyA->GetVertex(cache->indexA[0]);
                b2Vec<float, 2> localPointA2 = m_proxyA->GetVertex(cache->indexA[1]);

                m_axis = b2Cross(localPointA2 - localPointA1, 1.0f);
                m_axis.Normalize();
                b2Vec<float, 2> normal = b2Mul(xfA.q, m_axis);

                m_localPoint = 0.5f * (localPointA1 + localPointA2);
                b2Vec<float, 2> pointA = b2Mul(xfA, m_localPoint);

                b2Vec<float, 2> localPointB = m_proxyB->GetVertex(cache->indexB[0]);
                b2Vec<float, 2> pointB = b2Mul(xfB, localPointB);

                float s = b2Dot(pointB - pointA, normal);
                if (s < 0.0f)
                {
                    m_axis = -m_axis;
                    s = -s;
                }
                return s;
            }
        }

        //
        float FindMinSeparation(int32_t* indexA, int32_t* indexB, float t) const
        {
            b2Transform xfA, xfB;
            m_sweepA.GetTransform(&xfA, t);
            m_sweepB.GetTransform(&xfB, t);

            switch (m_type)
            {
                case e_points:
                {
                    b2Vec<float, 2> axisA = b2MulT(xfA.q, m_axis);
                    b2Vec<float, 2> axisB = b2MulT(xfB.q, -m_axis);

                    *indexA = m_proxyA->GetSupport(axisA);
                    *indexB = m_proxyB->GetSupport(axisB);

                    b2Vec<float, 2> localPointA = m_proxyA->GetVertex(*indexA);
                    b2Vec<float, 2> localPointB = m_proxyB->GetVertex(*indexB);

                    b2Vec<float, 2> pointA = b2Mul(xfA, localPointA);
                    b2Vec<float, 2> pointB = b2Mul(xfB, localPointB);

                    float separation = b2Dot(pointB - pointA, m_axis);
                    return separation;
                }

                case e_faceA:
                {
                    b2Vec<float, 2> normal = b2Mul(xfA.q, m_axis);
                    b2Vec<float, 2> pointA = b2Mul(xfA, m_localPoint);

                    b2Vec<float, 2> axisB = b2MulT(xfB.q, -normal);

                    *indexA = -1;
                    *indexB = m_proxyB->GetSupport(axisB);

                    b2Vec<float, 2> localPointB = m_proxyB->GetVertex(*indexB);
                    b2Vec<float, 2> pointB = b2Mul(xfB, localPointB);

                    float separation = b2Dot(pointB - pointA, normal);
                    return separation;
                }

                case e_faceB:
                {
                    b2Vec<float, 2> normal = b2Mul(xfB.q, m_axis);
                    b2Vec<float, 2> pointB = b2Mul(xfB, m_localPoint);

                    b2Vec<float, 2> axisA = b2MulT(xfA.q, -normal);

                    *indexB = -1;
                    *indexA = m_proxyA->GetSupport(axisA);

                    b2Vec<float, 2> localPointA = m_proxyA->GetVertex(*indexA);
                    b2Vec<float, 2> pointA = b2Mul(xfA, localPointA);

                    float separation = b2Dot(pointA - pointB, normal);
                    return separation;
                }

                default:
                    b2Assert(false);
                    *indexA = -1;
                    *indexB = -1;
                    return 0.0f;
            }
        }

        //
        float Evaluate(int32_t indexA, int32_t indexB, float t) const
        {
            b2Transform xfA, xfB;
            m_sweepA.GetTransform(&xfA, t);
            m_sweepB.GetTransform(&xfB, t);

            switch (m_type)
            {
                case e_points:
                {
                    b2Vec<float, 2> localPointA = m_proxyA->GetVertex(indexA);
                    b2Vec<float, 2> localPointB = m_proxyB->GetVertex(indexB);

                    b2Vec<float, 2> pointA = b2Mul(xfA, localPointA);
                    b2Vec<float, 2> pointB = b2Mul(xfB, localPointB);
                    float separation = b2Dot(pointB - pointA, m_axis);

                    return separation;
                }

                case e_faceA:
                {
                    b2Vec<float, 2> normal = b2Mul(xfA.q, m_axis);
                    b2Vec<float, 2> pointA = b2Mul(xfA, m_localPoint);

                    b2Vec<float, 2> localPointB = m_proxyB->GetVertex(indexB);
                    b2Vec<float, 2> pointB = b2Mul(xfB, localPointB);

                    float separation = b2Dot(pointB - pointA, normal);
                    return separation;
                }

                case e_faceB:
                {
                    b2Vec<float, 2> normal = b2Mul(xfB.q, m_axis);
                    b2Vec<float, 2> pointB = b2Mul(xfB, m_localPoint);

                    b2Vec<float, 2> localPointA = m_proxyA->GetVertex(indexA);
                    b2Vec<float, 2> pointA = b2Mul(xfA, localPointA);

                    float separation = b2Dot(pointA - pointB, normal);
                    return separation;
                }

                default:
                    b2Assert(false);
                    return 0.0f;
            }
        }

        const b2DistanceProxy* m_proxyA;
        const b2DistanceProxy* m_proxyB;
        b2Sweep m_sweepA, m_sweepB;
        Type m_type;
        b2Vec<float, 2> m_localPoint;
        b2Vec<float, 2> m_axis;
    };

}

// CCD via the local separating axis method. This seeks progression
// by computing the largest time at which separation is maintained.
void box2d::b2TimeOfImpact(b2TOIOutput* output, const b2TOIInput* input)
{
    b2Timer timer;

    ++b2TOIState::b2_toiCalls;

    output->state = b2TOIOutput::e_unknown;
    output->t = input->tMax;

    const b2DistanceProxy* proxyA = &input->proxyA;
    const b2DistanceProxy* proxyB = &input->proxyB;

    b2Sweep sweepA = input->sweepA;
    b2Sweep sweepB = input->sweepB;

    // Large rotations can make the root finder fail, so we normalize the
    // sweep angles.
    sweepA.Normalize();
    sweepB.Normalize();

    float tMax = input->tMax;

    float totalRadius = proxyA->m_radius + proxyB->m_radius;
    float target = b2Max(LINEAR_SLOP, totalRadius - 3.0f * LINEAR_SLOP);
    float tolerance = 0.25f * LINEAR_SLOP;
    b2Assert(target > tolerance);

    float t1 = 0.0f;
    const int32_t k_maxIterations = 20;  // TODO_ERIN b2Settings
    int32_t iter = 0;

    // Prepare input for distance query.
    b2SimplexCache cache;
    cache.count = 0;
    b2DistanceInput distanceInput;
    distanceInput.proxyA = input->proxyA;
    distanceInput.proxyB = input->proxyB;
    distanceInput.useRadii = false;

    // The outer loop progressively attempts to compute new separating axes.
    // This loop terminates when an axis is repeated (no progress is made).
    for (;;)
    {
        b2Transform xfA, xfB;
        sweepA.GetTransform(&xfA, t1);
        sweepB.GetTransform(&xfB, t1);

        // Get the distance between shapes. We can also use the results
        // to get a separating axis.
        distanceInput.transformA = xfA;
        distanceInput.transformB = xfB;
        b2DistanceOutput distanceOutput;
        b2Distance(&distanceOutput, &cache, &distanceInput);

        // If the shapes are overlapped, we give up on continuous collision.
        if (distanceOutput.distance <= 0.0f)
        {
            // Failure!
            output->state = b2TOIOutput::e_overlapped;
            output->t = 0.0f;
            break;
        }

        if (distanceOutput.distance < target + tolerance)
        {
            // Victory!
            output->state = b2TOIOutput::e_touching;
            output->t = t1;
            break;
        }

        // Initialize the separating axis.
        b2SeparationFunction fcn;
        fcn.Initialize(&cache, proxyA, sweepA, proxyB, sweepB, t1);
#if 0
		// Dump the curve seen by the root finder
		{
			const int32_t N = 100;
			float dx = 1.0f / N;
			float xs[N+1];
			float fs[N+1];

			float x = 0.0f;

			for (int32_t i = 0; i <= N; ++i)
			{
				sweepA.GetTransform(&xfA, x);
				sweepB.GetTransform(&xfB, x);
				float f = fcn.Evaluate(xfA, xfB) - target;

				printf("%g %g\n", x, f);

				xs[i] = x;
				fs[i] = f;

				x += dx;
			}
		}
#endif

        // Compute the TOI on the separating axis. We do this by successively
        // resolving the deepest point. This loop is bounded by the number of
        // vertices.
        bool done = false;
        float t2 = tMax;
        int32_t pushBackIter = 0;
        for (;;)
        {
            // Find the deepest point at t2. Store the witness point indices.
            int32_t indexA, indexB;
            float s2 = fcn.FindMinSeparation(&indexA, &indexB, t2);

            // Is the final configuration separated?
            if (s2 > target + tolerance)
            {
                // Victory!
                output->state = b2TOIOutput::e_separated;
                output->t = tMax;
                done = true;
                break;
            }

            // Has the separation reached tolerance?
            if (s2 > target - tolerance)
            {
                // Advance the sweeps
                t1 = t2;
                break;
            }

            // Compute the initial separation of the witness points.
            float s1 = fcn.Evaluate(indexA, indexB, t1);

            // Check for initial overlap. This might happen if the root finder
            // runs out of iterations.
            if (s1 < target - tolerance)
            {
                output->state = b2TOIOutput::e_failed;
                output->t = t1;
                done = true;
                break;
            }

            // Check for touching
            if (s1 <= target + tolerance)
            {
                // Victory! t1 should hold the TOI (could be 0.0).
                output->state = b2TOIOutput::e_touching;
                output->t = t1;
                done = true;
                break;
            }

            // Compute 1D root of: f(x) - target = 0
            int32_t rootIterCount = 0;
            float a1 = t1, a2 = t2;
            for (;;)
            {
                // Use a mix of the secant rule and bisection.
                float t;
                if (rootIterCount & 1)
                {
                    // Secant rule to improve convergence.
                    t = a1 + (target - s1) * (a2 - a1) / (s2 - s1);
                }
                else
                {
                    // Bisection to guarantee progress.
                    t = 0.5f * (a1 + a2);
                }

                ++rootIterCount;
                ++b2TOIState::b2_toiRootIters;

                float s = fcn.Evaluate(indexA, indexB, t);

                if (std::abs(s - target) < tolerance)
                {
                    // t2 holds a tentative value for t1
                    t2 = t;
                    break;
                }

                // Ensure we continue to bracket the root.
                if (s > target)
                {
                    a1 = t;
                    s1 = s;
                }
                else
                {
                    a2 = t;
                    s2 = s;
                }

                if (rootIterCount == 50)
                {
                    break;
                }
            }

            b2TOIState::b2_toiMaxRootIters = b2Max(b2TOIState::b2_toiMaxRootIters, rootIterCount);

            ++pushBackIter;

            if (pushBackIter == MAX_POLYGON_VERTICES)
            {
                break;
            }
        }

        ++iter;
        ++b2TOIState::b2_toiIters;

        if (done)
        {
            break;
        }

        if (iter == k_maxIterations)
        {
            // Root finder got stuck. Semi-victory.
            output->state = b2TOIOutput::e_failed;
            output->t = t1;
            break;
        }
    }

    b2TOIState::b2_toiMaxIters = b2Max(b2TOIState::b2_toiMaxIters, iter);

    float time = timer.GetMilliseconds();
    b2TOIState::b2_toiMaxTime = b2Max(b2TOIState::b2_toiMaxTime, time);
    b2TOIState::b2_toiTime += time;
}
