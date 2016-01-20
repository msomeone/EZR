#ifndef TREERENDERING_H
#define TREERENDERING_H

#include <glm/glm.hpp>
#include <vector>

#include "Tree.h"
#include <Rendering/VertexArrayObjects.h>

class aiScene;

namespace TreeAnimation
{

Renderable* generateRenderable(TreeAnimation::Tree::Branch* branch, const aiScene* branchModel = NULL);

Renderable* generateFoliage(TreeAnimation::Tree::Branch* branch, int numLeafs, const aiScene* foliageModel = NULL);

struct FoliageVertexData
{
	std::vector<float> positions;
	std::vector<unsigned int> indices;
	std::vector<float> normals;
	std::vector<float> uvs;
	std::vector<unsigned int> hierarchy;
};

void generateFoliageVertexData(TreeAnimation::Tree::Branch* branch, int numLeafs, FoliageVertexData& target);
Renderable* generateFoliageRenderable(FoliageVertexData& source); // use this source to generate a single renderable


struct BranchesVertexData
{
	std::vector<float> positions;
	std::vector<unsigned int> indices;
	std::vector<float> normals;
	std::vector<float> uvs;
	std::vector<float> tangents;
	std::vector<unsigned int> hierarchy;
};

void generateBranchVertexData(TreeAnimation::Tree::Branch* branch, BranchesVertexData& target, const aiScene* scene = NULL);
Renderable* generateBranchesRenderable(BranchesVertexData& source); // use this source to generate a single renderable


} // TreeAnimation
#endif