
#include "quadtree.h"
#include "../common/3Dmath.h"

#define CHOOPY_SCALE_CORRECTION	1.35f

QuadTree::Node::Node()
{
	subnodes[0] = -1;
	subnodes[1] = -1;
	subnodes[2] = -1;
	subnodes[3] = -1;

	lod = 0;
	length = 0;
}

QuadTree::QuadTree()
{
	numlods		= 0;
	meshdim		= 0;
	patchlength	= 0;
	maxcoverage	= 0;
	screenarea	= 0;
}

void QuadTree::Initialize(const float start[2], float size, int lodcount, int meshsize, float patchsize, float maxgridcoverage, float screensize)
{
	FUNC_PROTO(Vec2Assign)(root.start, start);
	root.length = size;

	numlods		= lodcount;
	meshdim		= meshsize;
	patchlength	= patchsize;
	maxcoverage	= maxgridcoverage;
	screenarea	= screensize;
}

void QuadTree::Rebuild(const float viewproj[16], const float proj[16], const float eye[3])
{
	nodes.clear();

	BuildTree(root, viewproj, proj, eye);
}

int QuadTree::BuildTree(Node& node, const float viewproj[16], const float proj[16], const float eye[3])
{
	if( !IsVisible(node, viewproj) )
		return -1;

	float coverage = CalculateCoverage(node, proj, eye);
	bool visible = true;

	if( coverage > maxcoverage && node.length > patchlength ) {
		Node subnodes[4];

		FUNC_PROTO(Vec2Set)(subnodes[0].start, node.start[0], node.start[1]);
		FUNC_PROTO(Vec2Set)(subnodes[1].start, node.start[0] + 0.5f * node.length, node.start[1]);
		FUNC_PROTO(Vec2Set)(subnodes[2].start, node.start[0] + 0.5f * node.length, node.start[1] + 0.5f * node.length);
		FUNC_PROTO(Vec2Set)(subnodes[3].start, node.start[0], node.start[1] + 0.5f * node.length);

		subnodes[0].length = subnodes[1].length = subnodes[2].length = subnodes[3].length = 0.5f * node.length;

		node.subnodes[0] = BuildTree(subnodes[0], viewproj, proj, eye);
		node.subnodes[1] = BuildTree(subnodes[1], viewproj, proj, eye);
		node.subnodes[2] = BuildTree(subnodes[2], viewproj, proj, eye);
		node.subnodes[3] = BuildTree(subnodes[3], viewproj, proj, eye);

		visible = !node.IsLeaf();
	}

	if( visible ) {
		int lod = 0;

		for( lod = 0; lod < numlods - 1; ++lod ) {
			if( coverage > maxcoverage )
				break;

			coverage *= 4.0f;
		}

		node.lod = std::min(lod, numlods - 2);
	} else {
		return -1;
	}

	int position = (int)nodes.size();
	nodes.push_back(node);

	return position;
}

bool QuadTree::IsVisible(const Node& node, const float viewproj[16]) const
{
	CLASS_PROTO(AABox) box;
	float planes[6][4];
	float length = node.length;

	// NOTE: depends on choopy scale
	box.Add(node.start[0] - CHOOPY_SCALE_CORRECTION, -0.01f, node.start[1] - CHOOPY_SCALE_CORRECTION);
	box.Add(node.start[0] + length + CHOOPY_SCALE_CORRECTION, 0.01f, node.start[1] + length + CHOOPY_SCALE_CORRECTION);

	FUNC_PROTO(FrustumPlanes)(planes, viewproj);
	int result = FUNC_PROTO(FrustumIntersect)(planes, box);

	return (result > 0);
}

int QuadTree::FindLeaf(const float point[2]) const
{
	int index = -1;
	int size = (int)nodes.size();

	Node node = nodes[size - 1];

	while( !node.IsLeaf() ) {
		bool found = false;

		for( int i = 0; i < 4; ++i ) {
			index = node.subnodes[i];

			if( index == -1 )
				continue;

			Node subnode = nodes[index];

			if( point[0] >= subnode.start[0] && point[0] <= subnode.start[0] + subnode.length &&
				point[1] >= subnode.start[1] && point[1] <= subnode.start[1] + subnode.length )
			{
				node = subnode;
				found = true;

				break;
			}
		}

		if( !found )
			return -1;
	}

	return index;
}

void QuadTree::FindSubsetPattern(int outindices[4], const Node& node)
{
	outindices[0] = 0;
	outindices[1] = 0;
	outindices[2] = 0;
	outindices[3] = 0;

	// NOTE: bottom: +Z, top: -Z
	float point_left[2]		= { node.start[0] - 0.5f * patchlength, node.start[1] + 0.5f * node.length };
	float point_right[2]	= { node.start[0] + node.length + 0.5f * patchlength, node.start[1] + 0.5f * node.length };
	float point_bottom[2]	= { node.start[0] + 0.5f * node.length, node.start[1] + node.length + 0.5f * patchlength };
	float point_top[2]		= { node.start[0] + 0.5f * node.length, node.start[1] - 0.5f * patchlength };

	int adjacency[4] = {
		FindLeaf(point_left),
		FindLeaf(point_right),
		FindLeaf(point_bottom),
		FindLeaf(point_top)
	};

	// a somewhat weird way to determine which degree to choose
	for( int i = 0; i < 4; ++i ) {
		if( adjacency[i] != -1 && nodes[adjacency[i]].length > node.length * 0.999f ) {
			const Node& adj = nodes[adjacency[i]];
			float scale = adj.length / node.length * (meshdim >> node.lod) / (meshdim >> adj.lod);
		
			if( scale > 3.999f )
				outindices[i] = 2;
			else if( scale > 1.999f )
				outindices[i] = 1;
		}
	}
}

void QuadTree::Traverse(NodeCallback callback) const
{
	if( nodes.size() == 0 )
		return;

	InternalTraverse(nodes.back(), callback);
}

float QuadTree::CalculateCoverage(const Node& node, const float proj[16], const float eye[3]) const
{
	const static float samples[16][2] = {
		{ 0, 0 },
		{ 0, 1 },
		{ 1, 0 },
		{ 1, 1 },
		{ 0.5f, 0.333f },
		{ 0.25f, 0.667f },
		{ 0.75f, 0.111f },
		{ 0.125f, 0.444f },
		{ 0.625f, 0.778f },
		{ 0.375f, 0.222f },
		{ 0.875f, 0.556f },
		{ 0.0625f, 0.889f },
		{ 0.5625f, 0.037f },
		{ 0.3125f, 0.37f },
		{ 0.8125f, 0.704f },
		{ 0.1875f, 0.148f },
	};

	float test[3];
	float vdir[3];
	float length		= node.length;
	float gridlength	= length / meshdim;
	float worldarea		= gridlength * gridlength;
	float maxprojarea	= 0;

	// sample patch at given points and estimate coverage
	for( int i = 0; i < 16; ++i ) {
		test[0] = (node.start[0] - CHOOPY_SCALE_CORRECTION) + (length + 2 * CHOOPY_SCALE_CORRECTION) * samples[i][0];
		test[1] = 0;
		test[2] = (node.start[1] - CHOOPY_SCALE_CORRECTION) + (length + 2 * CHOOPY_SCALE_CORRECTION) * samples[i][1];

		FUNC_PROTO(Vec3Subtract)(vdir, test, eye);

		float dist = FUNC_PROTO(Vec3Length)(vdir);
		float projarea = (worldarea * proj[0] * proj[5]) / (dist * dist);

		if( maxprojarea < projarea )
			maxprojarea = projarea;
	}

	return maxprojarea * screenarea * 0.25f;
}

void QuadTree::InternalTraverse(const Node& node, NodeCallback callback) const
{
	if( node.IsLeaf() ) {
		callback(node);
	} else {
		if( node.subnodes[0] != -1 )
			InternalTraverse(nodes[node.subnodes[0]], callback);

		if( node.subnodes[1] != -1 )
			InternalTraverse(nodes[node.subnodes[1]], callback);

		if( node.subnodes[2] != -1 )
			InternalTraverse(nodes[node.subnodes[2]], callback);

		if( node.subnodes[3] != -1 )
			InternalTraverse(nodes[node.subnodes[3]], callback);
	}
}
