// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "StraightSkeleton.h"
#include "../Math/Geometry.h"
#include <stack>
#include <cmath>

#if defined(_DEBUG)
	// #define EXTRA_VALIDATION
#endif

#pragma warning(disable:4505) // 'SceneEngine::StraightSkeleton::ReplaceVertex': unreferenced local function has been removed

// We can define the handiness of 2D space as such:
// If we wanted to rotate the X axis so that it lies on the Y axis, 
// which is the shortest direction to rotate in? Is it clockwise, or counterclockwise?
// "SPACE_HANDINESS_COUNTERCLOCKWISE" corresponds to a space in which +Y points up the page, and +X to the right
// "SPACE_HANDINESS_CLOCKWISE" corresponds to a space in which +Y points down the page, and +X to the right
#define SPACE_HANDINESS_CLOCKWISE 1
#define SPACE_HANDINESS_COUNTERCLOCKWISE 2
#define SPACE_HANDINESS SPACE_HANDINESS_COUNTERCLOCKWISE 

namespace XLEMath
{
	// static const float epsilon = 1e-4f;
	T1(Primitive) static constexpr Primitive GetEpsilon();
	template<> static constexpr float GetEpsilon<float>() { return 1e-4f; }
	template<> static constexpr double GetEpsilon<double>() { return 1e-8; }
	template<> static constexpr int GetEpsilon<int>() { return 1; }
	static const unsigned BoundaryVertexFlag = 1u<<31u;

	T1(Primitive) class Vertex
	{
	public:
		Vector2T<Primitive>	_position;
		unsigned			_skeletonVertexId;
		Primitive			_initialTime;
		Vector2T<Primitive>	_velocity;
	};

	T1(Primitive) class Graph
	{
	public:
		std::vector<Vertex<Primitive>> _vertices;

		class Segment
		{
		public:
			unsigned	_head, _tail;
			unsigned	_leftFace, _rightFace;
		};
		std::vector<Segment> _wavefrontEdges;

		class MotorcycleSegment
		{
		public:
			unsigned _head;
			unsigned _tail;		// (this is the fixed vertex)
			unsigned _leftFace, _rightFace;
		};
		std::vector<MotorcycleSegment> _motorcycleSegments;

		std::vector<Vector2T<Primitive>> _boundaryPoints;

		StraightSkeleton<Primitive> CalculateSkeleton(Primitive maxTime);

	private:
		void WriteWavefront(StraightSkeleton<Primitive>& dest, Primitive time);
		void AddEdgeForVertexPath(StraightSkeleton<Primitive>& dst, unsigned v, unsigned finalVertId);
	};

	enum WindingType { Left, Right, Straight };
	T1(Primitive) WindingType CalculateWindingType(Vector2T<Primitive> zero, Vector2T<Primitive> one, Vector2T<Primitive> two, Primitive threshold)
	{
		auto sign = (one[0] - zero[0]) * (two[1] - zero[1]) - (two[0] - zero[0]) * (one[1] - zero[1]);
		#if SPACE_HANDINESS == SPACE_HANDINESS_CLOCKWISE
			if (sign > threshold) return Right;
			if (sign < -threshold) return Left;
		#else
			if (sign > threshold) return Left;
			if (sign < -threshold) return Right;
		#endif
		return Straight;
	}

	T1(Primitive) static Vector2T<Primitive> CalculateVertexVelocity(Vector2T<Primitive> vex0, Vector2T<Primitive> vex1, Vector2T<Primitive> vex2)
	{
		// Calculate the velocity of vertex v1, assuming segments vex0->vex1 && vex1->vex2
		// are moving at a constant velocity inwards.
		// Note that the winding order is important. We're assuming these are polygon edge vertices
		// arranged in a clockwise order. This means that v1 will move towards the left side of the
		// segments.

		// let segment 1 be v0->v1
		// let segment 2 be v1->v2
		// let m1,m2 = gradient of segments
		// let u1,u2 = speed in X axis of points on segments
		// let v1,v1 = speed in Y axis of points on segments
		//
		// We're going to center our coordinate system on the initial intersection point, v0
		// We want to know where the intersection point of the 2 segments will be after time 't'
		// (since the intersection point will move in a straight line, we only need to calculate
		// it for t=1)
		//
		// I've calculated this out using basic algebra -- but we may be able to get a more efficient
		// method using vector math.

		if (Equivalent(vex0, vex2, GetEpsilon<Primitive>())) return Zero<Vector2T<Primitive>>();

		auto t0 = Vector2T<Primitive>(vex1-vex0);
		auto t1 = Vector2T<Primitive>(vex2-vex1);

		if (Equivalent(t0, Zero<Vector2T<Primitive>>(), GetEpsilon<Primitive>())) return Zero<Vector2T<Primitive>>();
		if (Equivalent(t1, Zero<Vector2T<Primitive>>(), GetEpsilon<Primitive>())) return Zero<Vector2T<Primitive>>();

		// create normal pointing in direction of movement
		#if SPACE_HANDINESS == SPACE_HANDINESS_CLOCKWISE
			auto N0 = Normalize(Vector2T<Primitive>(t0[1], -t0[0]));
			auto N1 = Normalize(Vector2T<Primitive>(t1[1], -t1[0]));
		#else
			auto N0 = Normalize(Vector2T<Primitive>(-t0[1], t0[0]));
			auto N1 = Normalize(Vector2T<Primitive>(-t1[1], t1[0]));
		#endif
		auto a = N0[0], b = N0[1];
		auto c = N1[0], d = N1[1];
		const auto t = Primitive(1);		// time = 1.0f, because we're calculating the velocity

		// Now, line1 is 0 = xa + yb - t and line2 is 0 = xc + yd - t

		// we can calculate the intersection of the lines using this formula...
		auto B0 = Primitive(0), B1 = Primitive(0);
		if (d<-GetEpsilon<Primitive>() || d>GetEpsilon<Primitive>()) B0 = a - b*c/d;
		if (c<-GetEpsilon<Primitive>() || c>GetEpsilon<Primitive>()) B1 = b - a*d/c;

		Primitive x, y;
		if (std::abs(B0) > std::abs(B1)) {
			if (B0 > -GetEpsilon<Primitive>() && B0 < GetEpsilon<Primitive>()) return Zero<Vector2T<Primitive>>();
			auto A = Primitive(1) - b/d;
			x = t * A / B0;
			y = (t - x*c) / d;
		} else {
			if (B1 > -GetEpsilon<Primitive>() && B1 < GetEpsilon<Primitive>()) return Zero<Vector2T<Primitive>>();
			auto A = Primitive(1) - a/c;
			y = t * A / B1;
			x = (t - y*d) / c;
		}

		assert(Dot(Vector2T<Primitive>(x, y), N0+N1) > Primitive(0));

		assert(IsFiniteNumber(x) && IsFiniteNumber(y));
		return Vector2T<Primitive>(x, y);
	}

	T1(Primitive) Graph<Primitive> BuildGraphFromVertexLoop(IteratorRange<const Vector2T<Primitive>*> vertices)
	{
		assert(vertices.size() >= 2);
		const auto threshold = Primitive(1e-6f);

		// Construct the starting point for the straight skeleton calculations
		// We're expecting the input vertices to be a closed loop, in counter-clockwise order
		// The first and last vertices should *not* be the same vertex; there is an implied
		// segment between the first and last.
		Graph<Primitive> result;
		result._wavefrontEdges.reserve(vertices.size());
		result._vertices.reserve(vertices.size());
		for (size_t v=0; v<vertices.size(); ++v) {
			// Each segment of the polygon becomes an "edge segment" in the graph
			auto v0 = (v+vertices.size()-1)%vertices.size();
			auto v1 = v;
			auto v2 = (v+1)%vertices.size();
			result._wavefrontEdges.emplace_back(Graph<Primitive>::Segment{unsigned(v2), unsigned(v), ~0u, unsigned(v)});

			// We must calculate the velocity for each vertex, based on which segments it belongs to...
			auto velocity = CalculateVertexVelocity(vertices[v0], vertices[v1], vertices[v2]);
			assert(!Equivalent(velocity, Zero<Vector2T<Primitive>>(), GetEpsilon<Primitive>()));
			result._vertices.emplace_back(Vertex<Primitive>{vertices[v], BoundaryVertexFlag|unsigned(v), Primitive(0), velocity});
		}

		// Each reflex vertex in the graph must result in a "motocycle segment".
		// We already know the velocity of the head of the motorcycle; and it has a fixed tail that
		// stays at the original position
		for (size_t v=0; v<vertices.size(); ++v) {
			auto v0 = (v+vertices.size()-1)%vertices.size();
			auto v1 = v;
			auto v2 = (v+1)%vertices.size();

			// Since we're expecting counter-clockwise inputs, if "v1" is a convex vertex, we should
			// wind around to the left when going v0->v1->v2
			// If we wind to the right then it's a reflex vertex, and we must add a motorcycle edge
			if (CalculateWindingType(vertices[v0], vertices[v1], vertices[v2], threshold) == WindingType::Right) {
				auto fixedVertex = (unsigned)(result._vertices.size());
				result._vertices.emplace_back(Vertex<Primitive>{vertices[v], BoundaryVertexFlag|unsigned(v), Primitive(0), Zero<Vector2T<Primitive>>()});
				result._motorcycleSegments.emplace_back(Graph<Primitive>::MotorcycleSegment{unsigned(v), unsigned(fixedVertex), unsigned(v0), unsigned(v1)});
			}
		}

		result._boundaryPoints = std::vector<Vector2T<Primitive>>(vertices.begin(), vertices.end());
		return result;
	}

	T1(Primitive) static Primitive CalculateCollapseTime(Vector2T<Primitive> p0, Vector2T<Primitive> v0, Vector2T<Primitive> p1, Vector2T<Primitive> v1)
	{
		auto d0x = v0[0] - v1[0];
		auto d0y = v0[1] - v1[1];
		if (std::abs(d0x) > std::abs(d0y)) {
			if (std::abs(d0x) < GetEpsilon<Primitive>()) return std::numeric_limits<Primitive>::max();
			auto t = (p1[0] - p0[0]) / d0x;

			auto ySep = p0[1] + t * v0[1] - p1[1] - t * v1[1];
			if (std::abs(ySep) < 1e-3f) {
				// assert(std::abs(p0[0] + t * v0[0] - p1[0] - t * v1[0]) < GetEpsilon<Primitive>());
				return t;	// (todo -- we could refine with the x results?
			}
		} else {
			if (std::abs(d0y) < GetEpsilon<Primitive>()) return std::numeric_limits<Primitive>::max();
			auto t = (p1[1] - p0[1]) / d0y;

			auto xSep = p0[0] + t * v0[0] - p1[0] - t * v1[0];
			if (std::abs(xSep) < 1e-3f) {
				// sassert(std::abs(p0[1] + t * v0[1] - p1[1] - t * v1[1]) < GetEpsilon<Primitive>());
				return t;	// (todo -- we could refine with the y results?
			}
		}

		return std::numeric_limits<Primitive>::max();
	}

	T1(Primitive) static Primitive CalculateCollapseTime(const Vertex<Primitive>& v0, const Vertex<Primitive>& v1)
	{
		// hack -- if one side is frozen, we must collapse immediately
		if (Equivalent(v0._velocity, Zero<Vector2T<Primitive>>(), GetEpsilon<Primitive>())) return std::numeric_limits<Primitive>::max();
		if (Equivalent(v1._velocity, Zero<Vector2T<Primitive>>(), GetEpsilon<Primitive>())) return std::numeric_limits<Primitive>::max();

		// At some point the trajectories of v0 & v1 may intersect
		// We need to pick out a specific time on the timeline, and find both v0 and v1 at that
		// time. 
		auto calcTime = std::min(v0._initialTime, v1._initialTime);
		auto p0 = Vector2T<Primitive>(v0._position + (calcTime-v0._initialTime) * v0._velocity);
		auto p1 = Vector2T<Primitive>(v1._position + (calcTime-v1._initialTime) * v1._velocity);
		return calcTime + CalculateCollapseTime(p0, v0._velocity, p1, v1._velocity);
	}

	T1(Primitive) static void ReplaceVertex(IteratorRange<const typename Graph<Primitive>::Segment*> segs, unsigned oldVertex, unsigned newVertex)
	{
		for (auto& s:segs) {
			if (s._head == oldVertex) s._head = newVertex;
			if (s._tail == oldVertex) s._tail = newVertex;
		}
	}

	T1(Primitive) static unsigned AddSteinerVertex(StraightSkeleton<Primitive>& skeleton, const Vector3T<Primitive>& vertex)
	{
		assert(vertex[2] != Primitive(0));
		assert(IsFiniteNumber(vertex[0]) && IsFiniteNumber(vertex[1]) && IsFiniteNumber(vertex[2]));
		assert(vertex[0] != std::numeric_limits<Primitive>::max() && vertex[1] != std::numeric_limits<Primitive>::max() && vertex[2] != std::numeric_limits<Primitive>::max());
		auto existing = std::find_if(skeleton._steinerVertices.begin(), skeleton._steinerVertices.end(),
			[vertex](const Vector3T<Primitive>& v) { return Equivalent(v, vertex, GetEpsilon<Primitive>()); });
		if (existing != skeleton._steinerVertices.end()) {
			return (unsigned)std::distance(skeleton._steinerVertices.begin(), existing);
		}
		#if defined(_DEBUG)
			auto test = std::find_if(skeleton._steinerVertices.begin(), skeleton._steinerVertices.end(),
				[vertex](const Vector3T<Primitive>& v) { return Equivalent(Truncate(v), Truncate(vertex), GetEpsilon<Primitive>()); });
			assert(test == skeleton._steinerVertices.end());
		#endif
		auto result = (unsigned)skeleton._steinerVertices.size();
		skeleton._steinerVertices.push_back(vertex);
		return result;
	}

	template<typename T, typename std::enable_if<std::is_floating_point<T>::value>::type* = nullptr>
		bool IsFiniteNumber(T value)
		{
			auto type = std::fpclassify(value);
			return ((type == FP_NORMAL) || (type == FP_SUBNORMAL) || (type == FP_ZERO)) && (value == value);
		}
	template<typename T, typename std::enable_if<std::is_integral<T>::value>::type* = nullptr>
		bool IsFiniteNumber(T) { return true; }

	T1(Primitive) static Vector2T<Primitive> PositionAtTime(const Vertex<Primitive>& v, Primitive time)
	{
		auto result = v._position + v._velocity * (time - v._initialTime);
		assert(IsFiniteNumber(result[0]) && IsFiniteNumber(result[1]));
		return result;
	}

	T1(Primitive) static Vector3T<Primitive> ClampedPositionAtTime(const Vertex<Primitive>& v, Primitive time)
	{
		if (Equivalent(v._velocity, Zero<Vector2T<Primitive>>(), GetEpsilon<Primitive>()))
			return Expand(v._position, v._initialTime);
		return Expand(PositionAtTime(v, time), time);
	}

	T1(Primitive) struct CrashEvent
	{
		Primitive _time;
		unsigned _edgeSegment;
	};

	T1(Primitive) static CrashEvent<Primitive> CalculateCrashTime(const Graph<Primitive>& graph, Vertex<Primitive> v)
	{
		CrashEvent<Primitive> bestCollisionEvent { std::numeric_limits<Primitive>::max(), ~0u };

		// Look for an intersection with _wavefrontEdges
		for (const auto&e:graph._wavefrontEdges) {
			const auto& head = graph._vertices[e._head];
			const auto& tail = graph._vertices[e._tail];

			// Since the edge segments are moving, the solution is a little complex
			// We can create a triangle between head, tail & the motorcycle head
			// If there is a collision, the triangle area will be zero at that point.
			// So we can search for a time when the triangle area is zero, and check to see
			// if a collision has actually occurred at that time.
			const auto calcTime = std::max(std::max(head._initialTime, tail._initialTime), v._initialTime);
			auto p0 = PositionAtTime(head, calcTime);
			auto p1 = PositionAtTime(tail, calcTime);
			auto v0 = head._velocity;
			auto v1 = tail._velocity;

			auto p2 = PositionAtTime(v, calcTime);
			auto v2 = v._velocity;

			// 2 * signed triangle area = 
			//		(p1[0]-p0[0]) * (p2[1]-p0[1]) - (p2[0]-p0[0]) * (p1[1]-p0[1])
			//
			// A =	(p1[0]+t*v1[0]-p0[0]-t*v0[0]) * (p2[1]+t*v2[1]-p0[1]-t*v0[1])
			// B =  (p2[0]+t*v2[0]-p0[0]-t*v0[0]) * (p1[1]+t*v1[1]-p0[1]-t*v0[1]);
			//
			// A =   (p1[0]-p0[0]) * (p2[1]+t*v2[1]-p0[1]-t*v0[1])
			//	 + t*(v1[0]-v0[0]) * (p2[1]+t*v2[1]-p0[1]-t*v0[1])
			//
			// A =   (p1[0]-p0[0]) * (p2[1]-p0[1]+t*(v2[1]-v0[1]))
			//	 + t*(v1[0]-v0[0]) * (p2[1]-p0[1]+t*(v2[1]-v0[1]))
			//
			// A =   (p1[0]-p0[0])*(p2[1]-p0[1]) + t*(p1[0]-p0[0])*(v2[1]-v0[1])
			//	 + t*(v1[0]-v0[0])*(p2[1]-p0[1]) + t*t*(v1[0]-v0[0])*(v2[1]-v0[1])
			//
			// B =   (p2[0]-p0[0])*(p1[1]-p0[1]) + t*(p2[0]-p0[0])*(v1[1]-v0[1])
			//	 + t*(v2[0]-v0[0])*(p1[1]-p0[1]) + t*t*(v2[0]-v0[0])*(v1[1]-v0[1])
			//
			// 0 = t*t*a + t*b + c
			// c = (p1[0]-p0[0])*(p2[1]-p0[1]) - (p2[0]-p0[0])*(p1[1]-p0[1])
			// b = (p1[0]-p0[0])*(v2[1]-v0[1]) + (v1[0]-v0[0])*(p2[1]-p0[1]) - (p2[0]-p0[0])*(v1[1]-v0[1]) - (v2[0]-v0[0])*(p1[1]-p0[1])
			// a = (v1[0]-v0[0])*(v2[1]-v0[1]) - (v2[0]-v0[0])*(v1[1]-v0[1])

			auto a = (v1[0]-v0[0])*(v2[1]-v0[1]) - (v2[0]-v0[0])*(v1[1]-v0[1]);
			if (Equivalent(a, Primitive(0), GetEpsilon<Primitive>())) continue;
			
			auto c = (p1[0]-p0[0])*(p2[1]-p0[1]) - (p2[0]-p0[0])*(p1[1]-p0[1]);
			auto b = (p1[0]-p0[0])*(v2[1]-v0[1]) + (v1[0]-v0[0])*(p2[1]-p0[1]) - (p2[0]-p0[0])*(v1[1]-v0[1]) - (v2[0]-v0[0])*(p1[1]-p0[1]);
			
			// x = (-b +/- sqrt(b*b - 4*a*c)) / 2*a
			auto K = b*b - Primitive(4)*a*c;
			if (K < Primitive(0)) continue;

			using PromoteType = decltype(std::sqrt(std::declval<Primitive>()));
			auto Q = std::sqrt(PromoteType(K));
			Primitive ts[] = {
				calcTime + Primitive((-b + Q) / (PromoteType(2)*a)),
				calcTime + Primitive((-b - Q) / (PromoteType(2)*a))
			};

			// Is there is a viable collision at either t0 or t1?
			// All 3 points should be on the same line at this point -- so we just need to check if
			// the motorcycle is between them (or intersecting a vertex)
			for (auto t:ts) {
				if (t > bestCollisionEvent._time || t <= std::max(head._initialTime, tail._initialTime)) continue;	// don't need to check collisions that happen too late
				auto P0 = PositionAtTime(head, t);
				auto P1 = PositionAtTime(tail, t);
				auto P2 = PositionAtTime(v, t);
				if ((Dot(P1-P0, P2-P0) > Primitive(0)) && (Dot(P0-P1, P2-P1) > Primitive(0))) {
					// good collision
					bestCollisionEvent._time = t;
					bestCollisionEvent._edgeSegment = unsigned(&e - AsPointer(graph._wavefrontEdges.begin()));
				} else if (Equivalent(P0, P2, GetEpsilon<Primitive>()) || Equivalent(P1, P2, GetEpsilon<Primitive>())) {
					// collided with vertex (or close enough)
					bestCollisionEvent._time = t;
					bestCollisionEvent._edgeSegment = unsigned(&e - AsPointer(graph._wavefrontEdges.begin()));
				}
			}
		}

		return bestCollisionEvent;
	}

	T1(Primitive) static auto FindInAndOut(IteratorRange<typename Graph<Primitive>::Segment*> edges, unsigned pivotVertex)
		-> std::pair<typename Graph<Primitive>::Segment*, typename Graph<Primitive>::Segment*>
	{
		std::pair<typename Graph<Primitive>::Segment*, typename Graph<Primitive>::Segment*> result(nullptr, nullptr);
		for  (auto&s:edges) {
			if (s._head == pivotVertex) {
				assert(!result.first);
				result.first = &s;
			} else if (s._tail == pivotVertex) {
				assert(!result.second);
				result.second = &s;
			}
		}
		return result;
	}

	T1(Primitive) static bool IsFrozen(const Vertex<Primitive>& v) { return Equivalent(v._velocity, Zero<Vector2T<Primitive>>(), Primitive(0)); }
	T1(Primitive) static void FreezeInPlace(Vertex<Primitive>& v, Primitive atTime)
	{
		assert(atTime != Primitive(0));
		v._position = PositionAtTime(v, atTime);
		v._initialTime = atTime;
		v._skeletonVertexId = ~0u;
		v._velocity = Zero<Vector2T<Primitive>>();
	}

	T1(EdgeType) static void AddUnique(std::vector<EdgeType>& dst, const EdgeType& edge)
	{
		auto existing = std::find_if(dst.begin(), dst.end(), 
			[&edge](const EdgeType&e) { return e._head == edge._head && e._tail == edge._tail; });

		if (existing == dst.end()) {
			dst.push_back(edge);
		} else {
			assert(existing->_type == edge._type);
		}
	}

	T1(Primitive) static void AddEdge(StraightSkeleton<Primitive>& dest, unsigned headVertex, unsigned tailVertex, unsigned leftEdge, unsigned rightEdge, typename StraightSkeleton<Primitive>::EdgeType type)
	{
		if (headVertex == tailVertex) return;

		// todo -- edge ordering may be flipped here....?
		if (rightEdge != ~0u) {
			AddUnique(dest._faces[rightEdge]._edges, {headVertex, tailVertex, type});
		} else {
			AddUnique(dest._unplacedEdges, {headVertex, tailVertex, type});
		}
		if (leftEdge != ~0u) {
			AddUnique(dest._faces[leftEdge]._edges, {tailVertex, headVertex, type});
		} else {
			AddUnique(dest._unplacedEdges, {tailVertex, headVertex, type});
		}
	}

	T1(Primitive) StraightSkeleton<Primitive> Graph<Primitive>::CalculateSkeleton(Primitive maxTime)
	{
		StraightSkeleton<Primitive> result;
		result._faces.resize(_boundaryPoints.size());

		std::vector<std::pair<Primitive, size_t>> bestCollapse;
		std::vector<std::pair<CrashEvent<Primitive>, size_t>> bestMotorcycleCrash;
		bestCollapse.reserve(8);

		Primitive lastEventTime = Primitive(0);
		unsigned lastEvent = 0;

		for (;;) {
			#if defined(EXTRA_VALIDATION)
				// validate vertex velocities
				for (unsigned v=0; v<_vertices.size(); ++v) {
					Segment* in, *out;
					std::tie(in, out) = FindInAndOut<Primitive>(MakeIteratorRange(_wavefrontEdges), v);
					if (in && out) {
						auto calcTime = (_vertices[in->_tail]._initialTime + _vertices[v]._initialTime + _vertices[out->_head]._initialTime) / Primitive(3);
						auto v0 = PositionAtTime(_vertices[in->_tail], calcTime);
						auto v1 = PositionAtTime(_vertices[v], calcTime);
						auto v2 = PositionAtTime(_vertices[out->_head], calcTime);
						auto expectedVelocity = CalculateVertexVelocity(v0, v1, v2);
						assert(Equivalent(_vertices[v]._velocity, expectedVelocity, 1e-3f));
					}
				}
				// every wavefront edge must have a collapse time (assuming it's vertices are not frozen)
				for (const auto&e:_wavefrontEdges) {
					if (IsFrozen(_vertices[e._head]) || IsFrozen(_vertices[e._tail])) continue;
					auto collapseTime = CalculateCollapseTime(_vertices[e._head], _vertices[e._tail]);
					assert(collapseTime != std::numeric_limits<Primitive>::max());		//it can be negative; because some edges are expanding
				}
			#endif

			// Find the next event to occur
			//		-- either a edge collapse or a motorcycle collision
			auto bestCollapseTime = std::numeric_limits<Primitive>::max();
			bestCollapse.clear();
			for (auto e=_wavefrontEdges.begin(); e!=_wavefrontEdges.end(); ++e) {
				const auto& v0 = _vertices[e->_head];
				const auto& v1 = _vertices[e->_tail];
				auto collapseTime = CalculateCollapseTime(v0, v1);
				if (collapseTime < Primitive(0)) continue;
				assert(collapseTime >= lastEventTime);
				if (collapseTime < (bestCollapseTime - GetEpsilon<Primitive>())) {
					bestCollapse.clear();
					bestCollapse.push_back(std::make_pair(collapseTime, std::distance(_wavefrontEdges.begin(), e)));
					bestCollapseTime = collapseTime;
				} else if (collapseTime < (bestCollapseTime + GetEpsilon<Primitive>())) {
					bestCollapse.push_back(std::make_pair(collapseTime, std::distance(_wavefrontEdges.begin(), e)));
					bestCollapseTime = std::min(collapseTime, bestCollapseTime);
				}
			}

			// Always ensure that every entry in "bestCollapse" is within
			// "GetEpsilon<Primitive>()" of bestCollapseTime -- this can become untrue if there
			// are chains of events with very small gaps in between them
			bestCollapse.erase(
				std::remove_if(
					bestCollapse.begin(), bestCollapse.end(),
					[bestCollapseTime](const std::pair<Primitive, size_t>& e) { return !(e.first < bestCollapseTime + GetEpsilon<Primitive>()); }), 
				bestCollapse.end());

			// Also check for motorcycles colliding.
			//		These can collide with segments in the _wavefrontEdges list, or 
			//		other motorcycles, or boundary polygon edges
			auto bestMotorcycleCrashTime = std::numeric_limits<Primitive>::max();
			bestMotorcycleCrash.clear();
			for (auto m=_motorcycleSegments.begin(); m!=_motorcycleSegments.end(); ++m) {
				const auto& head = _vertices[m->_head];
				if (Equivalent(head._velocity, Zero<Vector2T<Primitive>>(), GetEpsilon<Primitive>())) continue;
				assert(head._initialTime == Primitive(0));
				auto crashEvent = CalculateCrashTime(*this, head);
				if (crashEvent._time < Primitive(0)) continue;
				assert(crashEvent._time >= lastEventTime);

				// If our best motorcycle collision happens before our best collapse, then we
				// must do the motorcycle first, and recalculate edge collapses afterwards
				// But if they happen at the same time, we should do the edge collapse first,
				// and then recalculate the motorcycle collisions afterwards (ie, even if there's
				// a motorcycle collision at around the same time as the edge collapses, we're
				// going to ignore it for now)
				if (crashEvent._time < (bestCollapseTime + GetEpsilon<Primitive>())) {
					if (crashEvent._time < (bestMotorcycleCrashTime - GetEpsilon<Primitive>())) {
						bestMotorcycleCrash.clear();
						bestMotorcycleCrash.push_back(std::make_pair(crashEvent, std::distance(_motorcycleSegments.begin(), m)));
						bestMotorcycleCrashTime = crashEvent._time;
					} else if (crashEvent._time < (bestMotorcycleCrashTime + GetEpsilon<Primitive>())) {
						bestMotorcycleCrash.push_back(std::make_pair(crashEvent, std::distance(_motorcycleSegments.begin(), m)));
						bestMotorcycleCrashTime = std::min(crashEvent._time, bestMotorcycleCrashTime);
					}
				}
			}

			// If we get some motorcycle crashes, we're going to ignore the collapse events
			// and just process the motorcycle events
			if (!bestMotorcycleCrash.empty()) {
				if (bestMotorcycleCrashTime > maxTime) break;

				bestMotorcycleCrash.erase(
					std::remove_if(
						bestMotorcycleCrash.begin(), bestMotorcycleCrash.end(),
						[bestMotorcycleCrashTime](const std::pair<CrashEvent<Primitive>, size_t>& e) { return !(e.first._time < bestMotorcycleCrashTime + GetEpsilon<Primitive>()); }), 
					bestMotorcycleCrash.end());

				// we can only process a single crash event at a time currently
				// only the first event in bestMotorcycleCrashwill be processed (note that
				// this isn't necessarily the first event!)
				assert(bestMotorcycleCrash.size() == 1);
				auto crashEvent = bestMotorcycleCrash[0].first;
				const auto& motor = _motorcycleSegments[bestMotorcycleCrash[0].second];
				assert(crashEvent._edgeSegment != ~0u);

				auto crashPt = PositionAtTime(_vertices[motor._head], crashEvent._time);
				auto crashPtSkeleton = AddSteinerVertex(result, Vector3T<Primitive>(crashPt, crashEvent._time));

				auto crashSegment = _wavefrontEdges[crashEvent._edgeSegment];
				Segment newSegment0{ motor._head, motor._head, motor._leftFace, crashSegment._rightFace};
				Segment newSegment1{ motor._head, motor._head, crashSegment._rightFace, motor._rightFace };
				auto calcTime = crashEvent._time;

				// is there volume on the "tout" side?
				{
					auto* tout = FindInAndOut<Primitive>(MakeIteratorRange(_wavefrontEdges), motor._head).second;
					assert(tout);

					auto v0 = ClampedPositionAtTime(_vertices[crashSegment._tail], calcTime);
					auto v2 = ClampedPositionAtTime(_vertices[tout->_head], calcTime);
					if (tout->_head == crashSegment._tail || Equivalent(v0, v2, GetEpsilon<Primitive>())) {
						// no longer need crashSegment or tout
						assert(crashSegment._leftFace == ~0u && tout->_leftFace == ~0u);
						auto endPt = AddSteinerVertex<Primitive>(result, (v0+v2)/Primitive(2));
						AddEdge(result, endPt, crashPtSkeleton, crashSegment._rightFace, tout->_rightFace, StraightSkeleton<Primitive>::EdgeType::VertexPath);
						// tout->_head & crashSegment._tail end here. We must draw the skeleton segment 
						// tracing out their path
						AddEdgeForVertexPath(result, tout->_head, endPt);
						AddEdgeForVertexPath(result, crashSegment._tail, endPt);
						// todo -- there may be a chain of collapsing that occurs now... we should follow it along...
						// We still need to add a wavefront edge to close and the loop, and ensure we don't leave
						// stranded edges. Without this we can easily get a single edge without anything looping
						// it back around (or just an unclosed loop)
						auto toutHead = tout->_head;
						_wavefrontEdges.erase(_wavefrontEdges.begin()+(tout-AsPointer(_wavefrontEdges.begin())));
						if (toutHead != crashSegment._tail) {
							auto existing = std::find_if(_wavefrontEdges.begin(), _wavefrontEdges.end(), 
								[toutHead, crashSegment](const Segment&s) { return s._head == crashSegment._tail && s._tail == toutHead; });
							if (existing != _wavefrontEdges.end()) {
								_wavefrontEdges.push_back({toutHead, crashSegment._tail, existing->_rightFace, existing->_leftFace});
							} else {
								_wavefrontEdges.push_back({toutHead, crashSegment._tail, ~0u, ~0u});
							}
						}
					} else {
						auto newVertex = (unsigned)_vertices.size();
						tout->_tail = newVertex;
						_wavefrontEdges.push_back({newVertex, crashSegment._tail, crashSegment._leftFace, crashSegment._rightFace});	// (hin)

						_vertices.push_back(Vertex<Primitive>{crashPt, crashPtSkeleton, crashEvent._time, CalculateVertexVelocity(Truncate(v0), crashPt, Truncate(v2))});
						newSegment1._head = newVertex;
					}
				}

				// is there volume on the "tin" side?
				{
					auto* tin = FindInAndOut<Primitive>(MakeIteratorRange(_wavefrontEdges), motor._head).first;
					assert(tin);

					auto v0 = ClampedPositionAtTime(_vertices[tin->_tail], calcTime);
					auto v2 = ClampedPositionAtTime(_vertices[crashSegment._head], calcTime);
					if (tin->_tail == crashSegment._head || Equivalent(v0, v2, GetEpsilon<Primitive>())) {
						// no longer need "crashSegment" or tin
						assert(crashSegment._leftFace == ~0u && tin->_leftFace == ~0u);
						auto endPt = AddSteinerVertex<Primitive>(result, (v0+v2)/Primitive(2));
						AddEdge(result, endPt, crashPtSkeleton, tin->_rightFace, crashSegment._rightFace, StraightSkeleton<Primitive>::EdgeType::VertexPath);
						// tin->_tail & crashSegment._head end here. We must draw the skeleton segment 
						// tracing out their path
						AddEdgeForVertexPath(result, tin->_tail, endPt);
						AddEdgeForVertexPath(result, crashSegment._head, endPt);
						// todo -- there may be a chain of collapsing that occurs now... we should follow it along...
						// We still need to add a wavefront edge to close and the loop, and ensure we don't leave
						// stranded edges. Without this we can easily get a single edge without anything looping
						// it back around (or just an unclosed loop)
						auto tinTail = tin->_tail;
						_wavefrontEdges.erase(_wavefrontEdges.begin()+(tin-AsPointer(_wavefrontEdges.begin())));
						if (tinTail != crashSegment._head) {
							auto existing = std::find_if(_wavefrontEdges.begin(), _wavefrontEdges.end(), 
								[tinTail, crashSegment](const Segment&s) { return s._head == tinTail && s._tail == crashSegment._head; });
							if (existing != _wavefrontEdges.end()) {
								_wavefrontEdges.push_back({crashSegment._head, tinTail, existing->_rightFace, existing->_leftFace});
							} else
								_wavefrontEdges.push_back({crashSegment._head, tinTail, ~0u, ~0u});
						}
					} else {
						auto newVertex = (unsigned)_vertices.size();
						tin->_head = newVertex;
						_wavefrontEdges.push_back({crashSegment._head, newVertex, crashSegment._leftFace, crashSegment._rightFace});	// (hout)

						_vertices.push_back(Vertex<Primitive>{crashPt, crashPtSkeleton, crashEvent._time, CalculateVertexVelocity(Truncate(v0), crashPt, Truncate(v2))});
						newSegment0._head = newVertex;
					}
				}

				// if (newSegment0._head != newSegment0._tail) _wavefrontEdges.push_back(newSegment0);
				// if (newSegment1._head != newSegment1._tail) _wavefrontEdges.push_back(newSegment1);

				// note -- we can't erase this edge too soon, because it's used to calculate left and right faces
				// when calling AddEdgeForVertexPath 
				_wavefrontEdges.erase(
					std::remove_if(	_wavefrontEdges.begin(), _wavefrontEdges.end(), 
									[crashSegment](const Segment& s) { return s._head == crashSegment._head && s._tail == crashSegment._tail; }), 
					_wavefrontEdges.end());

				// add skeleton edge from the  
				assert(_vertices[motor._tail]._skeletonVertexId != ~0u);
				AddEdge(result, 
						crashPtSkeleton, _vertices[motor._tail]._skeletonVertexId,
						motor._leftFace, motor._rightFace, StraightSkeleton<Primitive>::EdgeType::VertexPath);
				FreezeInPlace(_vertices[motor._head], crashEvent._time);

				_motorcycleSegments.erase(_motorcycleSegments.begin() + bestMotorcycleCrash[0].second);

				#if defined(EXTRA_VALIDATION)
					{
						for (auto m=_motorcycleSegments.begin(); m!=_motorcycleSegments.end(); ++m) {
							const auto& head = _vertices[m->_head];
							if (Equivalent(head._velocity, Zero<Vector2T<Primitive>>(), GetEpsilon<Primitive>())) continue;
							auto nextCrashEvent = CalculateCrashTime(*this, head._position, head._velocity, head._initialTime, AsPointer(m), head._skeletonVertexId);
							if (nextCrashEvent._time < Primitive(0)) continue;
							assert(nextCrashEvent._time >= crashEvent._time);
						}
					}
				#endif

				lastEventTime = crashEvent._time;
				lastEvent = 1;
			} else {
				if (bestCollapse.empty()) break;
				if (bestCollapseTime > maxTime) break;

				// Process the "edge" events... first separate the edges into collapse groups
				// Each collapse group collapses onto a single vertex. We will search through all
				// of the collapse events we're processing, and separate them into discrete groups.
				std::vector<unsigned> collapseGroups(bestCollapse.size(), ~0u);
				struct CollapseGroupInfo { unsigned _head, _tail, _newVertex; };
				std::vector<CollapseGroupInfo> collapseGroupInfos;
				unsigned nextCollapseGroup = 0;
				for (size_t c=0; c<bestCollapse.size(); ++c) {
					if (collapseGroups[c] != ~0u) continue;

					collapseGroups[c] = nextCollapseGroup;

					// got back as far as possible, from tail to tail
					auto searchingTail =_wavefrontEdges[bestCollapse[c].second]._tail;
					for (;;) {
						auto i = std::find_if(bestCollapse.begin(), bestCollapse.end(),
							[searchingTail, this](const std::pair<Primitive, size_t>& t)
							{ return _wavefrontEdges[t.second]._head == searchingTail; });
						if (i == bestCollapse.end()) break;
						if (collapseGroups[std::distance(bestCollapse.begin(), i)] == nextCollapseGroup) break;
						assert(collapseGroups[std::distance(bestCollapse.begin(), i)] == ~0u);
						collapseGroups[std::distance(bestCollapse.begin(), i)] = nextCollapseGroup;
						searchingTail = _wavefrontEdges[i->second]._tail;
					}

					// also go forward head to head
					auto searchingHead =_wavefrontEdges[bestCollapse[c].second]._head;
					for (;;) {
						auto i = std::find_if(bestCollapse.begin(), bestCollapse.end(),
							[searchingHead, this](const std::pair<Primitive, size_t>& t)
							{ return _wavefrontEdges[t.second]._tail == searchingHead; });
						if (i == bestCollapse.end()) break;
						if (collapseGroups[std::distance(bestCollapse.begin(), i)] == nextCollapseGroup) break;
						assert(collapseGroups[std::distance(bestCollapse.begin(), i)] == ~0u);
						collapseGroups[std::distance(bestCollapse.begin(), i)] = nextCollapseGroup;
						searchingHead = _wavefrontEdges[i->second]._head;
					}

					++nextCollapseGroup;
					collapseGroupInfos.push_back({searchingHead, searchingTail, ~0u});
				}

				// Each collapse group becomes a single new vertex. We can collate them together
				// now, and write out some segments to the output skeleton
				std::vector<unsigned> collapseGroupNewVertex(nextCollapseGroup, ~0u);
				for (auto collapseGroup=0u; collapseGroup<nextCollapseGroup; ++collapseGroup) {
					Vector2T<Primitive> collisionPt(Primitive(0), Primitive(0));
					unsigned contributors = 0;
					for (size_t c=0; c<bestCollapse.size(); ++c) {
						if (collapseGroups[c] != collapseGroup) continue;
						const auto& seg = _wavefrontEdges[bestCollapse[c].second];
						collisionPt += PositionAtTime(_vertices[seg._head], bestCollapseTime);
						collisionPt += PositionAtTime(_vertices[seg._tail], bestCollapseTime);
						contributors += 2;

						// at this point they should not be frozen (but they will all be frozen later)
						assert(!IsFrozen(_vertices[seg._tail]));
						assert(!IsFrozen(_vertices[seg._head]));
					}
					collisionPt /= Primitive(contributors);

					// Validate that our "collisionPt" is close to all of the collapsing points
					#if defined(_DEBUG)
						for (size_t c=0; c<bestCollapse.size(); ++c) {
							if (collapseGroups[c] != collapseGroup) continue;
							const auto& seg = _wavefrontEdges[bestCollapse[c].second];
							auto one = PositionAtTime(_vertices[seg._head], bestCollapseTime);
							auto two = PositionAtTime(_vertices[seg._tail], bestCollapseTime);
							assert(Equivalent(one, collisionPt, Primitive(1e-3f)));
							assert(Equivalent(two, collisionPt, Primitive(1e-3f)));
						}
					#endif

					// add a steiner vertex into the output
					auto collisionVertId = AddSteinerVertex(result, Expand(collisionPt, bestCollapseTime));

					// connect up edges in the output graph
					// Note that since we're connecting both head and tail, we'll end up doubling up each edge
					for (size_t c=0; c<bestCollapse.size(); ++c) {
						if (collapseGroups[c] != collapseGroup) continue;
						const auto& seg = _wavefrontEdges[bestCollapse[c].second];
						unsigned vs[] = { seg._head, seg._tail };
						for (auto& v:vs)
							AddEdgeForVertexPath(result, v, collisionVertId);
					
						FreezeInPlace(_vertices[seg._tail], bestCollapseTime);
						FreezeInPlace(_vertices[seg._head], bestCollapseTime);
					}

					// create a new vertex in the graph to connect the edges to either side of the collapse
					collapseGroupInfos[collapseGroup]._newVertex = (unsigned)_vertices.size();
					_vertices.push_back(Vertex<Primitive>{collisionPt, collisionVertId, bestCollapseTime, Zero<Vector2T<Primitive>>()});
				}

				// Remove all of the collapsed edges (by shifting them to the end)
				// (note, expecting bestCollapse to be sorted by "second")
				auto r = _wavefrontEdges.end()-1;
				for (auto i=bestCollapse.rbegin(); i!=bestCollapse.rend(); ++i) {
					if (i!=bestCollapse.rbegin()) --r;
					// Swap the ones we're going to remove to the end of the list (note that we loose ordering
					// for the list as a whole...
					std::swap(*r, _wavefrontEdges[i->second]);
				}
				_wavefrontEdges.erase(r, _wavefrontEdges.end());

				// For each collapse group, there should be one tail edge, and one head edge
				// We need to find these edges in order to calculate the velocity of the point in between
				// Let's resolve that now...
				for (const auto& group:collapseGroupInfos) {
					if (group._head == group._tail) continue;	// if we remove an entire loop, let's assume that there are no external links to it

					auto tail = FindInAndOut<Primitive>(MakeIteratorRange(_wavefrontEdges), group._tail).first;
					auto head = FindInAndOut<Primitive>(MakeIteratorRange(_wavefrontEdges), group._head).second;
					assert(tail && head);

					tail->_head = group._newVertex;
					head->_tail = group._newVertex;
					auto calcTime = _vertices[group._newVertex]._initialTime;
					auto v0 = PositionAtTime(_vertices[tail->_tail], calcTime);
					auto v1 = _vertices[group._newVertex]._position;
					auto v2 = PositionAtTime(_vertices[head->_head], calcTime);
					_vertices[group._newVertex]._velocity = CalculateVertexVelocity(v0, v1, v2);

					#if defined(EXTRA_VALIDATION)
						{
							assert(CalculateCollapseTime(_vertices[tail->_tail], _vertices[group._newVertex]) >= _vertices[group._newVertex]._initialTime);
							assert(CalculateCollapseTime(_vertices[group._newVertex], _vertices[head->_head]) >= _vertices[group._newVertex]._initialTime);

							auto calcTime = (_vertices[tail->_tail]._initialTime + _vertices[group._newVertex]._initialTime + _vertices[head->_head]._initialTime) / Primitive(3);
							auto v0 = PositionAtTime(_vertices[tail->_tail], calcTime);
							auto v1 = PositionAtTime(_vertices[group._newVertex], calcTime);
							auto v2 = PositionAtTime(_vertices[head->_head], calcTime);
							
							auto validatedVelocity = CalculateVertexVelocity(v0, v1, v2);
							assert(Equivalent(validatedVelocity, _vertices[group._newVertex]._velocity, GetEpsilon<Primitive>()));
						}
					#endif
				}
			
				lastEventTime = bestCollapseTime;
				lastEvent = 2;
			}
		}

		if (maxTime == std::numeric_limits<Primitive>::max())
			maxTime = lastEventTime;
		WriteWavefront(result, maxTime);

		return result;
	}

	T1(Primitive) static Primitive ClosestPointOnLine2D(Vector2T<Primitive> rayStart, Vector2T<Primitive> rayEnd, Vector2T<Primitive> testPt)
	{
		auto o = testPt - rayStart;
		auto l = rayEnd - rayStart;
		return Dot(o, l) / MagnitudeSquared(l);
	}

	T1(Primitive) static bool DoColinearLinesIntersect(Vector2T<Primitive> AStart, Vector2T<Primitive> AEnd, Vector2T<Primitive> BStart, Vector2T<Primitive> BEnd)
	{
		// return false if the lines share a point, but otherwise do not intersect
		// but returns true if the lines overlap completely (even if the lines have zero length)
		auto closestBStart = ClosestPointOnLine2D(AStart, AEnd, BStart);
		auto closestBEnd = ClosestPointOnLine2D(AStart, AEnd, BEnd);
		return ((closestBStart > GetEpsilon<Primitive>()) && (closestBStart > Primitive(1)-GetEpsilon<Primitive>()))
			|| ((closestBEnd > GetEpsilon<Primitive>()) && (closestBEnd > Primitive(1)-GetEpsilon<Primitive>()))
			|| (Equivalent(AStart, BStart, GetEpsilon<Primitive>()) && Equivalent(AEnd, BEnd, GetEpsilon<Primitive>()))
			|| (Equivalent(AEnd, BStart, GetEpsilon<Primitive>()) && Equivalent(AStart, BEnd, GetEpsilon<Primitive>()))
			;
	}

	T1(Primitive) void Graph<Primitive>::WriteWavefront(StraightSkeleton<Primitive>& result, Primitive time)
	{
		// Write the current wavefront to the destination skeleton. Each edge in 
		// _wavefrontEdges comes a segment in the output
		// However, we must check for overlapping / intersecting edges
		//	-- these happen very frequently
		// The best way to remove overlapping edges is just to go through the list of segments, 
		// and for each one look for other segments that intersect

		std::vector<Segment> filteredSegments;
		std::stack<Segment> segmentsToTest;

		// We need to combine overlapping points at this stage, also
		// (2 different vertices could end up at the same location at time 'time')

		for (auto i=_wavefrontEdges.begin(); i!=_wavefrontEdges.end(); ++i) {
			auto A = ClampedPositionAtTime(_vertices[i->_head], time);
			auto B = ClampedPositionAtTime(_vertices[i->_tail], time);
			auto v0 = AddSteinerVertex(result, A);
			auto v1 = AddSteinerVertex(result, B);
			if (v0 != v1)
				segmentsToTest.push(Segment{v0, v1, i->_leftFace, i->_rightFace});
		}

		while (!segmentsToTest.empty()) {
			auto seg = segmentsToTest.top();
			segmentsToTest.pop();

			auto A = Truncate(result._steinerVertices[seg._head]);
			auto B = Truncate(result._steinerVertices[seg._tail]);
			bool filterOutSeg = false;

			// Compare against all edges already in "filteredSegments"
			for (auto i2=filteredSegments.begin(); i2!=filteredSegments.end();++i2) {

				if (i2->_head == seg._head && i2->_tail == seg._tail) {
					if (i2->_leftFace == ~0u) i2->_leftFace = seg._leftFace;
					if (i2->_rightFace == ~0u) i2->_rightFace = seg._rightFace;
					filterOutSeg = true; 
					break; // (overlap completely)
				} else if (i2->_head == seg._tail && i2->_tail == seg._head) {
					if (i2->_leftFace == ~0u) i2->_leftFace = seg._rightFace;
					if (i2->_rightFace == ~0u) i2->_rightFace = seg._leftFace;
					filterOutSeg = true; 
					break; // (overlap completely)
				}

				// If they intersect, they should be colinear, and at least one 
				// vertex if i2 should lie on i
				auto C = Truncate(result._steinerVertices[i2->_head]);
				auto D = Truncate(result._steinerVertices[i2->_tail]);
				auto closestC = ClosestPointOnLine2D(A, B, C);
				auto closestD = ClosestPointOnLine2D(A, B, D);

				bool COnLine = closestC > Primitive(0) && closestC < Primitive(1) && MagnitudeSquared(LinearInterpolate(A, B, closestC) - C) < GetEpsilon<Primitive>();
				bool DOnLine = closestD > Primitive(0) && closestD < Primitive(1) && MagnitudeSquared(LinearInterpolate(A, B, closestD) - D) < GetEpsilon<Primitive>();
				if (!COnLine && !DOnLine) { continue; }

				auto m0 = (B[1] - A[1]) / (B[0] - A[0]);
				auto m1 = (D[1] - C[1]) / (D[0] - C[0]);
				if (!Equivalent(m0, m1, GetEpsilon<Primitive>())) { continue; }

				if (i2->_head == seg._head) {
					if (closestD < Primitive(1)) {
						seg._head = i2->_tail;
					} else {
						i2->_head = seg._tail;
					}
				} else if (i2->_head == seg._tail) {
					if (closestD > Primitive(0)) {
						seg._tail = i2->_tail;
					} else {
						i2->_head = seg._head;
					}
				} else if (i2->_tail == seg._head) {
					if (closestC < Primitive(1)) {
						seg._head = i2->_head;
					} else {
						i2->_tail = seg._tail;
					}
				} else if (i2->_tail == seg._tail) {
					if (closestC > Primitive(0)) {
						seg._tail = i2->_head;
					} else {
						i2->_tail = seg._head;
					}
				} else {
					// The lines are colinear, and at least one point of i2 is on i
					// We must separate these 2 segments into 3 segments.
					// Replace i2 with something that is strictly with i2, and then schedule
					// the remaining split parts for intersection tests.
					Segment newSeg;
					if (closestC < Primitive(0)) {
						if (closestD > Primitive(1)) newSeg = {seg._tail, i2->_tail};
						else { newSeg = {i2->_tail, seg._tail}; seg._tail = i2->_tail; }
						i2->_tail = seg._head;
					} else if (closestD < Primitive(0)) {
						if (closestC > Primitive(1)) newSeg = {seg._tail, i2->_head};
						else { newSeg = {i2->_head, seg._tail}; seg._tail = i2->_head; }
						i2->_head = seg._head;
					} else if (closestC < closestD) {
						if (closestD > Primitive(1)) newSeg = {seg._tail, i2->_tail};
						else { newSeg = {i2->_tail, seg._tail}; seg._tail = i2->_tail; }
						seg._tail = i2->_head;
					} else {
						if (closestC > Primitive(1)) newSeg = {seg._tail, i2->_head};
						else { newSeg = {i2->_head, seg._tail}; seg._tail = i2->_head; }
						seg._tail = i2->_tail;
					}

					assert(!DoColinearLinesIntersect(
						Truncate(result._steinerVertices[newSeg._head]),
						Truncate(result._steinerVertices[newSeg._tail]),
						Truncate(result._steinerVertices[seg._head]),
						Truncate(result._steinerVertices[seg._tail])));
					assert(!DoColinearLinesIntersect(
						Truncate(result._steinerVertices[newSeg._head]),
						Truncate(result._steinerVertices[newSeg._tail]),
						Truncate(result._steinerVertices[i2->_head]),
						Truncate(result._steinerVertices[i2->_tail])));
					assert(!DoColinearLinesIntersect(
						Truncate(result._steinerVertices[i2->_head]),
						Truncate(result._steinerVertices[i2->_tail]),
						Truncate(result._steinerVertices[seg._head]),
						Truncate(result._steinerVertices[seg._tail])));
					assert(newSeg._head != newSeg._tail);
					assert(i2->_head != i2->_tail);
					assert(seg._head != seg._tail);

					// We will continue testing "seg", and we will push "newSeg" onto the stack to
					// be tested later.
					// i2 has also been changed; it is now shorter and no longer intersects 'seg'
					segmentsToTest.push(newSeg);
				}

				// "seg" has changed, so we need to calculate the end points
				A = Truncate(result._steinerVertices[seg._head]);
				B = Truncate(result._steinerVertices[seg._tail]);
			}

			if (!filterOutSeg)
				filteredSegments.push_back(seg);
		}

		// add all of the segments in "filteredSegments" to the skeleton
		for (const auto&seg:filteredSegments) {
			assert(seg._head != seg._tail);
			AddEdge(result, seg._head, seg._tail, seg._leftFace, seg._rightFace, StraightSkeleton<Primitive>::EdgeType::Wavefront);
		}

		// Also have to add the traced out path of the each vertex (but only if it doesn't already exist in the result)
		for (const auto&seg:_wavefrontEdges) {
			unsigned vs[] = {seg._head, seg._tail};
			for (auto v:vs) {
				const auto& vert = _vertices[v];
				AddEdgeForVertexPath(result, v, AddSteinerVertex(result, ClampedPositionAtTime(vert, time)));
			}
		}
	}

	T1(Primitive) void Graph<Primitive>::AddEdgeForVertexPath(StraightSkeleton<Primitive>& dst, unsigned v, unsigned finalVertId)
	{
		const auto& vert = _vertices[v];
		auto inAndOut = FindInAndOut<Primitive>(MakeIteratorRange(_wavefrontEdges), v);
		unsigned leftFace = ~0u, rightFace = ~0u;
		if (inAndOut.first) leftFace = inAndOut.first->_rightFace;
		if (inAndOut.second) rightFace = inAndOut.second->_rightFace;
		if (vert._skeletonVertexId != ~0u) {
			if (vert._skeletonVertexId&BoundaryVertexFlag) {
				auto q = vert._skeletonVertexId&(~BoundaryVertexFlag);
				AddEdge(dst, finalVertId, vert._skeletonVertexId, unsigned((q+_boundaryPoints.size()-1) % _boundaryPoints.size()), q, StraightSkeleton<Primitive>::EdgeType::VertexPath);
			}
			AddEdge(dst, finalVertId, vert._skeletonVertexId, leftFace, rightFace, StraightSkeleton<Primitive>::EdgeType::VertexPath);
		} else {
			AddEdge(dst,
				finalVertId, AddSteinerVertex(dst, Expand(vert._position, vert._initialTime)),
				leftFace, rightFace, StraightSkeleton<Primitive>::EdgeType::VertexPath);
		}
	}


	T1(Primitive) StraightSkeleton<Primitive> CalculateStraightSkeleton(IteratorRange<const Vector2T<Primitive>*> vertices, Primitive maxInset)
	{
		auto graph = BuildGraphFromVertexLoop(vertices);
		return graph.CalculateSkeleton(maxInset);
	}

	std::vector<std::vector<unsigned>> AsVertexLoopsOrdered(
		IteratorRange<const std::pair<unsigned, unsigned>*> segments)
	{
		// From a line segment soup, generate vertex loops. This requires searching
		// for segments that join end-to-end, and following them around until we
		// make a loop.
		// Let's assume for the moment there are no 3-or-more way junctions (this would
		// require using some extra math to determine which is the correct path)
		std::vector<std::pair<unsigned, unsigned>> pool(segments.begin(), segments.end());
		std::vector<std::vector<unsigned>> result;
		while (!pool.empty()) {
			std::vector<unsigned> workingLoop;
			{
				auto i = pool.end()-1;
				workingLoop.push_back(i->first);
				workingLoop.push_back(i->second);
				pool.erase(i);
			}
			for (;;) {
				assert(!pool.empty());	// if we hit this, we have open segments
				auto searching = *(workingLoop.end()-1);
				auto hit = pool.end(); 
				for (auto i=pool.begin(); i!=pool.end(); ++i) {
					if (i->first == searching /*|| i->second == searching*/) {
						assert(hit == pool.end());
						hit = i;
					}
				}
				assert(hit != pool.end());
				auto newVert = hit->second; // (hit->first == searching) ? hit->second : hit->first;
				pool.erase(hit);
				if (std::find(workingLoop.begin(), workingLoop.end(), newVert) != workingLoop.end())
					break;	// closed the loop
				workingLoop.push_back(newVert);
			}
			result.push_back(std::move(workingLoop));
		}

		return result;
	}

	T1(Primitive) std::vector<std::vector<unsigned>> StraightSkeleton<Primitive>::WavefrontAsVertexLoops()
	{
		std::vector<std::pair<unsigned, unsigned>> segmentSoup;
		for (auto&f:_faces)
			for (auto&e:f._edges)
				if (e._type == EdgeType::Wavefront)
					segmentSoup.push_back({e._head, e._tail});
		// We shouldn't need the edges in _unplacedEdges, so long as each edge has been correctly
		// assigned to it's source face
		return AsVertexLoopsOrdered(MakeIteratorRange(segmentSoup));
	}

	template StraightSkeleton<float> CalculateStraightSkeleton<float>(IteratorRange<const Vector2T<float>*> vertices, float maxInset);
	template StraightSkeleton<double> CalculateStraightSkeleton<double>(IteratorRange<const Vector2T<double>*> vertices, double maxInset);
	template StraightSkeleton<int32_t> CalculateStraightSkeleton<int32_t>(IteratorRange<const Vector2T<int32_t>*> vertices, int32_t maxInset);
	template class StraightSkeleton<float>;
	template class StraightSkeleton<double>;
	template class StraightSkeleton<int32_t>;

}