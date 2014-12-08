/*
* Copyright (c) 2006-2009 Erin Catto http://www.box2d.org
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

#ifndef B2_POLYGON_SHAPE_H
#define B2_POLYGON_SHAPE_H

#include <Box2D/Collision/Shapes/b2Shape.h>

namespace box2d
{
/// A convex polygon. It is assumed that the interior of the polygon is to
/// the left of each edge.
/// Polygons have a maximum number of vertices equal to MAX_POLYGON_VERTICES.
/// In most cases you should not need many vertices for a convex polygon.
class b2PolygonShape : public b2Shape
{
   public:
    b2PolygonShape();

    /// Implement b2Shape.
    b2Shape* Clone(b2BlockAllocator* allocator) const override;

    /// @see b2Shape::GetChildCount
    int32_t GetChildCount() const override;

    /// Create a convex hull from the given array of local points.
    /// The count must be in the range [3, MAX_POLYGON_VERTICES].
    /// @warning the points may be re-ordered, even if they form a convex polygon
    /// @warning collinear points are handled but not removed. Collinear points
    /// may lead to poor stacking behavior.
    void Set(const b2Vec2* points, int32_t count);

    /// Build vertices to represent an axis-aligned box centered on the local origin.
    /// @param hx the half-width.
    /// @param hy the half-height.
    void SetAsBox(float32 hx, float32 hy);

    /// Build vertices to represent an oriented box.
    /// @param hx the half-width.
    /// @param hy the half-height.
    /// @param center the center of the box in local coordinates.
    /// @param angle the rotation of the box in local coordinates.
    void SetAsBox(float32 hx, float32 hy, const b2Vec2& center, float32 angle);

    /// @see b2Shape::TestPoint
    bool TestPoint(const b2Transform& transform, const b2Vec2& p) const override;

    /// Implement b2Shape.
    bool RayCast(b2RayCastOutput* output, const b2RayCastInput& input, const b2Transform& transform,
                 int32_t childIndex) const override;

    /// @see b2Shape::ComputeAABB
    void ComputeAABB(b2AABB* aabb, const b2Transform& transform, int32_t childIndex) const override;

    /// @see b2Shape::ComputeMass
    void ComputeMass(b2MassData* massData, float32 density) const override;

    /// Get the vertex count.
    int32_t GetVertexCount() const
    {
        return m_count;
    }

    /// Get a vertex by index.
    const b2Vec2& GetVertex(int32_t index) const;

    /// Validate convexity. This is a very time consuming operation.
    /// @returns true if valid
    bool Validate() const;

    b2Vec2 m_centroid;
    b2Vec2 m_vertices[MAX_POLYGON_VERTICES];
    b2Vec2 m_normals[MAX_POLYGON_VERTICES];
    int32_t m_count;
};

inline b2PolygonShape::b2PolygonShape()
{
    m_type = e_polygon;
    m_radius = POLYGON_RADIUS;
    m_count = 0;
    m_centroid.SetZero();
}

inline const b2Vec2& b2PolygonShape::GetVertex(int32_t index) const
{
    b2Assert(0 <= index && index < m_count);
    return m_vertices[index];
}
}

#endif
