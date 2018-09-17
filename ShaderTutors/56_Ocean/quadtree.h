
#ifndef _QUADTREE_H_
#define _QUADTREE_H_

#include <vector>
#include <functional>

class QuadTree
{
public:
	struct Node {
		int		subnodes[4];
		float	start[2];		// starting position
		float	length;			// world-space size
		int		lod;

		Node();

		inline bool IsLeaf() const {
			return (subnodes[0] == -1 && subnodes[1] == -1 && subnodes[2] == -1 && subnodes[3] == -1);
		}
	};


private:
	typedef std::vector<Node> NodeList;
	typedef std::function<void (const QuadTree::Node&)> NodeCallback;

	NodeList	nodes;
	Node		root;
	int			numlods;		// number of LOD levels
	int			meshdim;		// patch mesh resolution
	float		patchlength;	// world space patch size
	float		maxcoverage;	// any node larger than this will be subdivided
	float		screenarea;

	float CalculateCoverage(const Node& node, const float proj[16], const float eye[3]) const;
	bool IsVisible(const Node& node, const float viewproj[16]) const;
	void InternalTraverse(const Node& node, NodeCallback callback) const;

	int FindLeaf(const float point[2]) const;
	int BuildTree(Node& node, const float viewproj[16], const float proj[16], const float eye[3]);

public:
	QuadTree();

	void FindSubsetPattern(int outindices[4], const Node& node);
	void Initialize(const float start[2], float size, int lodcount, int meshsize, float patchsize, float maxgridcoverage, float screensize);
	void Rebuild(const float viewproj[16], const float proj[16], const float eye[3]);
	void Traverse(NodeCallback callback) const;
};

#endif
