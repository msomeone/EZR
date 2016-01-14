#include "AssimpTools.h"

#include "Core/DebugLog.h"

#include "Rendering/VertexArrayObjects.h"

glm::vec3 toVec3(const aiVector3D& vert)
{
	return glm::vec3(vert.x, vert.y, vert.z);
}


std::vector<AssimpTools::RenderableInfo > AssimpTools::createSimpleRenderablesFromScene(const aiScene* scene, glm::mat4 vertexTransform)
{
	std::vector<RenderableInfo >resultVector; 
	for (unsigned int i = 0; i < scene->mNumMeshes; i++)
	{
		aiMesh* m = scene->mMeshes[i];

		// mesh has vertices
		if(m->HasPositions())
		{
			// read info
			std::vector<unsigned int> indices;
			std::vector<float> vertices;
			std::vector<float> normals;
			std::vector<float> uvs;

			glm::vec3 min( FLT_MAX);
			glm::vec3 max(-FLT_MAX);

			auto checkMax = [](glm::vec3& max, const glm::vec3& vert) 
			{
				max.x = std::max(max.x, vert.x);
				max.y = std::max(max.y, vert.y);
				max.z = std::max(max.z, vert.z);
			};

			auto checkMin = [](glm::vec3& min, const glm::vec3& vert) 
			{
				min.x = std::min(min.x, vert.x);
				min.y = std::min(min.y, vert.y);
				min.z = std::min(min.z, vert.z);
			};
			
			if ( m->HasPositions()){
			for ( unsigned int v = 0; v < m->mNumVertices; v++)
			{
				glm::vec3 vert = toVec3( m->mVertices[v] );
				vert = glm::vec3( vertexTransform * glm::vec4(vert, 1.0f));

				vertices.push_back(vert.x);
				vertices.push_back(vert.y);
				vertices.push_back(vert.z);
				
				checkMin(min,vert);
				checkMax(max,vert);
			}}

			if ( m->HasNormals()){
			for ( unsigned int n = 0; n < m->mNumVertices; n++)
			{

				glm::vec3 norm = toVec3( m->mNormals[n] );
				norm = glm::vec3( glm::transpose(glm::inverse(vertexTransform)) * glm::vec4(norm, 1.0f));

				normals.push_back(norm.x);
				normals.push_back(norm.y);
				normals.push_back(norm.z);
			}}

			if ( m->HasTextureCoords(0)){
			for (unsigned int u = 0; u < m->mNumVertices; u++)
			{
				aiVector3D uv = m->mTextureCoords[0][u];
				uvs.push_back(uv.x);
				uvs.push_back(uv.y);
				
				if(m->GetNumUVChannels() == 3)
				{
					uvs.push_back(uv.z);
				}
			}}

			if ( m->HasFaces()){
			for ( unsigned int f = 0; f < m->mNumFaces; f++)
			{
				aiFace face = m->mFaces[f];
				for ( unsigned int idx = 0; idx < face.mNumIndices; idx++) 
				{
					indices.push_back(face.mIndices[idx]);
				}
			}}

			// method to upload data to GPU and set as vertex attribute
			auto createVbo = [](std::vector<float>& content, GLuint dimensions, GLuint attributeIndex)
			{ 
				GLuint vbo = 0;
				glGenBuffers(1, &vbo);
				glBindBuffer(GL_ARRAY_BUFFER, vbo);
				glBufferData(GL_ARRAY_BUFFER, content.size() * sizeof(float), &content[0], GL_STATIC_DRAW);
				glVertexAttribPointer(attributeIndex, dimensions, GL_FLOAT, 0, 0, 0);
				glEnableVertexAttribArray(attributeIndex);
				return vbo;
			};

			auto createIndexVbo = [](std::vector<unsigned int>& content) 
			{
				GLuint vbo = 0;	
				glGenBuffers(1, &vbo);
				glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, vbo);
				glBufferData(GL_ELEMENT_ARRAY_BUFFER, content.size() * sizeof(unsigned int), &content[0], GL_STATIC_DRAW);
				return vbo;
			};

			// generate VAO
			GLuint vao;
		    glGenVertexArrays(1, &vao);
			Renderable *renderable = new Renderable;
			renderable->m_vao = vao;
			glBindVertexArray(vao);

			if (m->HasPositions()){
			renderable->m_positions.m_vboHandle = createVbo(vertices, 3, 0);
			renderable->m_positions.m_size = vertices.size() / 3;
			}

			if(m->HasTextureCoords(0)){
			renderable->m_uvs.m_vboHandle = createVbo(uvs, (m->GetNumUVChannels() == 3) ? 3 : 2, 1);
			renderable->m_uvs.m_size = (m->GetNumUVChannels() == 3) ? uvs.size() / 3 : uvs.size() / 2;
			}

			if( m->HasNormals()){
			renderable->m_normals.m_vboHandle = createVbo(normals, 3, 2);
			renderable->m_normals.m_size = normals.size() / 3;
			}

			renderable->m_indices.m_vboHandle = createIndexVbo(indices);
			renderable->m_indices.m_size = indices.size();

			renderable->setDrawMode(GL_TRIANGLES);

			glBindVertexArray(0);

			// save mesh info
			RenderableInfo renderableInfo;
			renderableInfo.renderable = renderable;
			renderableInfo.boundingBox.min = min;
			renderableInfo.boundingBox.max = max;
			renderableInfo.name = std::string( m->mName.C_Str() );
			renderableInfo.meshIdx = i;
			
			// add to set of renderables
			resultVector.push_back(renderableInfo);
		}
		return resultVector;
	}
}

AssimpTools::BoundingBox AssimpTools::computeBoundingBox(const aiMesh* mesh)
{
	glm::vec3 min( FLT_MAX);
	glm::vec3 max(-FLT_MAX);

	auto checkMax = [](glm::vec3& max, const aiVector3D& vert) 
	{
		max.x = std::max(max.x, vert.x);
		max.y = std::max(max.y, vert.y);
		max.z = std::max(max.z, vert.z);
	};

	auto checkMin = [](glm::vec3& min, const aiVector3D& vert) 
	{
		min.x = std::min(min.x, vert.x);
		min.y = std::min(min.y, vert.y);
		min.z = std::min(min.z, vert.z);
	};

	for ( unsigned int v = 0; v < mesh->mNumVertices; v++)
	{
		aiVector3D vert = mesh->mVertices[v];
				
		checkMin(min,vert);
		checkMax(max,vert);
	}

	BoundingBox result = {min,max};
	return result;
}

void AssimpTools::checkMin(glm::vec3& min, const glm::vec3& point)
{
	min.x = std::min(min.x, point.x);
	min.y = std::min(min.y, point.y);
	min.z = std::min(min.z, point.z);
}

void AssimpTools::checkMax(glm::vec3& max, const glm::vec3& point)
{
	max.x = std::max(max.x, point.x);
	max.y = std::max(max.y, point.y);
	max.z = std::max(max.z, point.z);
}


#include <assimp/Importer.hpp>
const aiScene* AssimpTools::importAssetFromResourceFolder(std::string filename, Assimp::Importer& importer,int steps)
{
	const aiScene* scene = importer.ReadFile( RESOURCES_PATH "/" + filename, steps);
	
	if (scene == NULL)
	{
		std::string errorString = importer.GetErrorString();
		DEBUGLOG->log("ERROR: " + errorString);
	} else {
		DEBUGLOG->log("Model has been loaded successfully");
	}

	return scene;
}

std::map<aiTextureType, AssimpTools::TextureInfo> AssimpTools::getTexturesInfo(const aiScene* scene, int matIdx)
{
	std::map<aiTextureType, AssimpTools::TextureInfo> result;
	
	if (matIdx >= scene->mNumMaterials)
	{
		DEBUGLOG->log("ERROR: invalid material index");
		return result;
	}

	auto m = scene->mMaterials[matIdx];
	for (int t = aiTextureType::aiTextureType_NONE; t <= aiTextureType::aiTextureType_UNKNOWN; t++)
	{
		if (m->GetTextureCount( (aiTextureType) t ) != 0)
		{
			aiString path;
			m->GetTexture( (aiTextureType) t, 0, &path);
			std::string sPath(path.C_Str());
			
			TextureInfo info = {matIdx, t, sPath}; 
			result[(aiTextureType) t] = info;
		}
	}
	return result;
}
