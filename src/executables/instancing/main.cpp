﻿/*******************************************
 * **** DESCRIPTION ****
 ****************************************/

#include <iostream>
#include <time.h>

#include <Rendering/GLTools.h>
#include <Rendering/VertexArrayObjects.h>
#include <Rendering/RenderPass.h>

//#include "UI/imgui/imgui.h"
//#include <UI/imguiTools.h>
#include <UI/Turntable.h>

#include <glm/gtx/rotate_vector.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <assimp/Importer.hpp>
#include <assimp/postprocess.h>
#include <Importing/AssimpTools.h>
#include <Importing/TextureTools.h>

////////////////////// PARAMETERS /////////////////////////////
const glm::vec2 WINDOW_RESOLUTION = glm::vec2(800.0f, 600.0f);

const int NUM_INSTANCES = 3000;

static glm::vec4 s_lightPos = glm::vec4(0.0,50.0f,0.0,1.0);

static float s_strength = 1.0f;
static bool  s_isRotating = false;

static std::map<Renderable*, glm::vec4*> s_renderable_color_map;
static std::map<Renderable*, int> s_renderable_material_map; //!< mapping a renderable to a material index
static std::vector<std::map<aiTextureType, GLuint>> s_material_texture_handles; //!< mapping material texture types to texture handles

//////////////////// MISC /////////////////////////////////////
float randFloat(float min, float max) //!< returns a random number between min and max
{
	return (((float) rand() / (float) RAND_MAX) * (max - min) + min); 
}

//////////////////////////////////////////////////////////////////////////////
///////////////////////////////// MAIN ///////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////


std::vector<glm::mat4 > generateModels(int numObjects, float xSize, float zSize)
{
	std::vector<glm::mat4> models(numObjects);

	for (int i = 0; i < models.size(); i++)
	{

		// generate random position on x/z plane
		float x = randFloat(-xSize * 0.5f, xSize * 0.5f);
		float z = randFloat(-zSize * 0.5f, zSize * 0.5f);
		float y = randFloat(-5.0f, 5.0f);
		models[i] = glm::scale(glm::vec3(0.1,0.1,0.1)); 
		models[i] = glm::translate(glm::vec3(x, y, z)) * models[i];
	}
	return models;
}

struct BlockUniformInfo 
{
	ShaderProgram::Info info;
	GLint offset; // byte offset in the uniform block / buffer
	GLint arrayStride;
	GLint matrixStride;
};

struct UniformBlockInfo
{
	GLint index; //!< index in the shader program
	GLint byteSize;
	GLint numActiveUniforms; //!< amount of active uniforms in this uniform block
	std::map<std::string, BlockUniformInfo> uniforms; //!< uniform locations in the shader
};

std::map<std::string, UniformBlockInfo> getAllUniformBlockInfo(ShaderProgram& shaderProgram)
{
	std::map<std::string, UniformBlockInfo> result;
	GLint numBlocks = 0;
	glGetProgramInterfaceiv(shaderProgram.getShaderProgramHandle(), GL_UNIFORM_BLOCK, GL_ACTIVE_RESOURCES, &numBlocks);
	
	std::vector<GLenum> blockProperties;
	blockProperties.push_back(GL_NUM_ACTIVE_VARIABLES);
	blockProperties.push_back(GL_NAME_LENGTH);
	blockProperties.push_back(GL_BUFFER_DATA_SIZE);

	std::vector<GLenum> uniformProperties;
	uniformProperties.push_back(GL_NAME_LENGTH);
	uniformProperties.push_back(GL_TYPE);
	uniformProperties.push_back(GL_OFFSET);
	//uniformProperties.push_back(GL_SIZE); // what the ...
	uniformProperties.push_back(GL_ARRAY_STRIDE);
	uniformProperties.push_back(GL_MATRIX_STRIDE);

	for ( int blockIdx = 0; blockIdx < numBlocks; blockIdx++)
	{
		std::vector<GLint> blockValues(blockProperties.size());
		glGetProgramResourceiv(shaderProgram.getShaderProgramHandle(), GL_UNIFORM_BLOCK, blockIdx, blockProperties.size(), &blockProperties[0], blockValues.size(), NULL, &blockValues[0]);
		GLint numActiveUniforms = blockValues[0]; // query amount of uniforms in this block

		std::vector<GLchar> nameData(256);
		nameData.resize(blockValues[1]); //The length of the name.
		glGetProgramResourceName(shaderProgram.getShaderProgramHandle(), GL_UNIFORM_BLOCK, blockIdx, nameData.size(), NULL, &nameData[0]);
		std::string blockName = std::string((char*)&nameData[0], nameData.size() - 1);
		blockName = std::string(blockName.c_str());
		
		std::vector<GLint> uniformHandles(numActiveUniforms); // allocate room for uniform indices
		const GLenum blockVariableProperties[1] = {GL_ACTIVE_VARIABLES};
		glGetProgramResourceiv(shaderProgram.getShaderProgramHandle(), GL_UNIFORM_BLOCK, blockIdx, 1, blockVariableProperties, numActiveUniforms, NULL, &uniformHandles[0] );

		result[blockName].index = blockIdx;
		result[blockName].numActiveUniforms = numActiveUniforms;
		result[blockName].byteSize = blockValues[2];

		for (auto attribIdx : uniformHandles)
		{
			std::vector<GLint> uniformValues(uniformProperties.size());

			// retrieve uniform properties
			glGetProgramResourceiv(shaderProgram.getShaderProgramHandle(), GL_UNIFORM, attribIdx, uniformProperties.size(),
			&uniformProperties[0], uniformValues.size(), NULL, &uniformValues[0]);

			std::vector<GLchar> nameData(256);
			nameData.resize(uniformValues[0]); //The length of the name.
			glGetProgramResourceName(shaderProgram.getShaderProgramHandle(), GL_UNIFORM, attribIdx, nameData.size(), NULL, &nameData[0]);
			std::string uniformName = std::string((char*)&nameData[0], nameData.size() - 1);
			uniformName = std::string(uniformName.c_str());

			DEBUGLOG->log("uniform: " + blockName + "." + uniformName); DEBUGLOG->indent();
			DEBUGLOG->log("type   : " + shaderProgram.getTypeString(uniformValues[1]));
			DEBUGLOG->log("offset : ", uniformValues[2]);
			DEBUGLOG->log("arr-str: ",uniformValues[3]);
			DEBUGLOG->log("mat-str: ",uniformValues[4]);

			const GLuint idx[1] = {attribIdx};
			GLint uniformSize;
			glGetActiveUniformsiv(shaderProgram.getShaderProgramHandle(), 1, idx, GL_UNIFORM_SIZE, &uniformSize );
			DEBUGLOG->log("size   :", uniformSize);
			
			result[blockName].uniforms[uniformName].info.location= attribIdx;
			result[blockName].uniforms[uniformName].info.type = uniformValues[1];
			result[blockName].uniforms[uniformName].offset = uniformValues[2];
			result[blockName].uniforms[uniformName].arrayStride = uniformValues[3];
			result[blockName].uniforms[uniformName].matrixStride = uniformValues[4];

			DEBUGLOG->outdent();
		}

	}
	return result;
}


void updateValuesInBufferData(std::string uniformName, const float* values, int numValues, UniformBlockInfo& info, std::vector<float>& buffer)
{
	auto u = info.uniforms.find(uniformName);
	if ( u == info.uniforms.end()) { return; }
	if (numValues >= buffer.size()) {return;}

	int valStartIdx = u->second.offset / 4;
	if (buffer.size() < valStartIdx + numValues){return;} 

	for (int i = 0; i < numValues; i++)
	{
		buffer[valStartIdx + i] = values[i];
	}
}


int main()
{
	DEBUGLOG->setAutoPrint(true);
	auto window = generateWindow(WINDOW_RESOLUTION.x,WINDOW_RESOLUTION.y);

	//////////////////////////////////////////////////////////////////////////////
	/////////////////////// INIT /////////////////////////////////////////////////
	//////////////////////////////////////////////////////////////////////////////

	/////////////////////    load tree related assets   //////////////////////////
	DEBUGLOG->log("Setup: generating renderables"); DEBUGLOG->indent();

	// import using ASSIMP and check for errors
	Assimp::Importer importer;
	DEBUGLOG->log("Loading model");
	std::string modelFile = "cube.dae";
	
	const aiScene* scene = AssimpTools::importAssetFromResourceFolder(modelFile, importer);
	auto renderable = AssimpTools::createSimpleRenderablesFromScene(scene);
	std::map<aiTextureType, AssimpTools::MaterialTextureInfo> texturesInfo;
	if (renderable.empty()) { DEBUGLOG->log("ERROR: no renderable. Going to Exit."); float wait; cin >> wait; exit(-1);}
	if (scene != NULL) texturesInfo = AssimpTools::getMaterialTexturesInfo(scene, 0);
	if (scene != NULL) s_material_texture_handles.resize(scene->mNumMaterials);

	for (auto e : texturesInfo)
	{
		GLuint texHandle = TextureTools::loadTextureFromResourceFolder(texturesInfo[e.first].relativePath);
		if (texHandle != -1){ s_material_texture_handles[e.second.matIdx][e.first] = texHandle; }
	}

	/////////////////////    create wind field          //////////////////////////
	// TreeAnimation::WindField windField(64,64);
	// windField.updateVectorTexture(0.0f);

	DEBUGLOG->outdent();
	//////////////////////////////////////////////////////////////////////////////
	/////////////////////////////// RENDERING  ///////////////////////////////////
	//////////////////////////////////////////////////////////////////////////////
	
	/////////////////////     Scene / View Settings     //////////////////////////
	glm::vec4 eye(10.0f, 10.0f, 10.0f, 1.0f);
	glm::vec4 center(0.0f,0.0f,0.0f,1.0f);
	glm::mat4 view = glm::lookAt(glm::vec3(eye), glm::vec3(center), glm::vec3(0,1,0));

	glm::mat4 perspective = glm::perspective(glm::radians(65.f), getRatio(window), 0.5f, 100.f);
	
	DEBUGLOG->log("Setup: generating model matrices"); DEBUGLOG->indent();
	srand (time(NULL));	
	std::vector<glm::mat4 > model = generateModels(NUM_INSTANCES, -15.0f, 15.0f);
	DEBUGLOG->outdent();

	/////////////////////   Instancing Settings        //////////////////////////
	DEBUGLOG->log("Setup: buffering model matrices"); DEBUGLOG->indent();
	// buffer model matrices
	renderable[0].renderable->bind();

    // Vertex Buffer Object
    //GLuint instanceModelBufferHandle;
    //glGenBuffers(1, &instanceModelBufferHandle);
    //glBindBuffer(GL_ARRAY_BUFFER, instanceModelBufferHandle);
    //glBufferData(GL_ARRAY_BUFFER, NUM_INSTANCES * sizeof(glm::mat4), &model[0], GL_STATIC_DRAW);
	 GLuint instanceModelBufferHandle = bufferData<glm::mat4>(model, GL_STATIC_DRAW);
    
    // mat4 Vertex Attribute == 4 x vec4 attributes (consecutively)
	GLuint instancedAttributeLocation = 4; // beginning attribute location (0..3 are reserved for pos,uv,norm,tangents
    GLsizei vec4Size = sizeof(glm::vec4);
    glEnableVertexAttribArray(instancedAttributeLocation); 
    glVertexAttribPointer(instancedAttributeLocation, 4, GL_FLOAT, GL_FALSE, 4 * vec4Size, (GLvoid*)0); // offset = 0 x vec4 size , stride = 4x vec4 size
    glEnableVertexAttribArray(instancedAttributeLocation+1); 
    glVertexAttribPointer(instancedAttributeLocation+1, 4, GL_FLOAT, GL_FALSE, 4 * vec4Size, (GLvoid*)(vec4Size)); //offset = 1 x vec4 size
    glEnableVertexAttribArray(instancedAttributeLocation+2); 
    glVertexAttribPointer(instancedAttributeLocation+2, 4, GL_FLOAT, GL_FALSE, 4 * vec4Size, (GLvoid*)(2 * vec4Size)); //offset = 2 x vec4 size
    glEnableVertexAttribArray(instancedAttributeLocation+3); 
    glVertexAttribPointer(instancedAttributeLocation+3, 4, GL_FLOAT, GL_FALSE, 4 * vec4Size, (GLvoid*)(3 * vec4Size)); // offset = 2x vec4 size

	// enable instanced attribute processing
    glVertexAttribDivisor(instancedAttributeLocation,   1);
    glVertexAttribDivisor(instancedAttributeLocation+1, 1);
    glVertexAttribDivisor(instancedAttributeLocation+2, 1);
    glVertexAttribDivisor(instancedAttributeLocation+3, 1);
	renderable[0].renderable->unbind();
	DEBUGLOG->outdent();

	/////////////////////// 	Shader     ///////////////////////////
	// simple shader
	DEBUGLOG->log("Shader Compilation: instanced simple lighting"); DEBUGLOG->indent();
	ShaderProgram shaderProgram("/modelSpace/instancedModelViewProjection.vert", "/modelSpace/simpleLighting.frag"); DEBUGLOG->outdent();
	shaderProgram.printUniformInfo();

	auto uniformBlockInfos = getAllUniformBlockInfo(shaderProgram);
	UniformBlockInfo uniformBlockInfo = uniformBlockInfos.at("MatrixBlock");

	std::vector<float> matrixData;
	matrixData.resize(uniformBlockInfo.byteSize / sizeof(float));
	
	updateValuesInBufferData("projection", glm::value_ptr(perspective), sizeof(glm::mat4) / sizeof(float), uniformBlockInfo, matrixData);  
	updateValuesInBufferData("view", glm::value_ptr(view), sizeof(glm::mat4) / sizeof(float), uniformBlockInfo, matrixData);  

	GLuint bindingPoint = 1, buffer, blockIndex;
 
	blockIndex = glGetUniformBlockIndex(shaderProgram.getShaderProgramHandle(), "MatrixBlock");
	glUniformBlockBinding(shaderProgram.getShaderProgramHandle(), blockIndex, bindingPoint); // bind block 0 to binding point 1
 
	glGenBuffers(1, &buffer);
	glBindBuffer(GL_UNIFORM_BUFFER, buffer);
	glBufferData(GL_UNIFORM_BUFFER, sizeof(matrixData), &matrixData[0], GL_DYNAMIC_DRAW);

	glBindBufferBase(GL_UNIFORM_BUFFER, bindingPoint, buffer);

	shaderProgram.update("color", glm::vec4(0.7,0.7,0.7,1.0));
	DEBUGLOG->outdent();

	
	//////////////////////////////////////////////////////////////////////////////
	///////////////////////    GUI / USER INPUT   ////////////////////////////////
	//////////////////////////////////////////////////////////////////////////////

	// Setup ImGui binding
    //ImGui_ImplGlfwGL3_Init(window, true);

	Turntable turntable;
	double old_x;
    double old_y;
	glfwGetCursorPos(window, &old_x, &old_y);
	
	auto cursorPosCB = [&](double x, double y)
	{
		//ImGuiIO& io = ImGui::GetIO();
		//if ( io.WantCaptureMouse )
		//{ return; } // ImGUI is handling this

		double d_x = x - old_x;
		double d_y = y - old_y;

		if ( turntable.getDragActive() )
		{
			turntable.dragBy(d_x, d_y, view);
		}

		old_x = x;
		old_y = y;
	};

	auto mouseButtonCB = [&](int b, int a, int m)
	{
		if (b == GLFW_MOUSE_BUTTON_LEFT && a == GLFW_PRESS)
		{
			turntable.setDragActive(true);
		}
		if (b == GLFW_MOUSE_BUTTON_LEFT && a == GLFW_RELEASE)
		{
			turntable.setDragActive(false);
		}

		//ImGui_ImplGlfwGL3_MouseButtonCallback(window, b, a, m);
	};

	setCursorPosCallback(window, cursorPosCB);
	setMouseButtonCallback(window, mouseButtonCB);

	//////////////////////////////////////////////////////////////////////////////
	//////////////////////////////// RENDER LOOP /////////////////////////////////
	//////////////////////////////////////////////////////////////////////////////

	double elapsedTime = 0.0;
	render(window, [&](double dt)
	{
		elapsedTime += dt;
		std::string window_header = "Instancing Test - " + DebugLog::to_string( 1.0 / dt ) + " FPS";
		glfwSetWindowTitle(window, window_header.c_str() );

		////////////////////////////////     GUI      ////////////////////////////////
		// ImGuiIO& io = ImGui::GetIO();
		// ImGui_ImplGlfwGL3_NewFrame(); // tell ImGui a new frame is being rendered	
		// ImGui::PushItemWidth(-125);

		// ImGui::PopItemWidth();
        //////////////////////////////////////////////////////////////////////////////

		///////////////////////////// MATRIX UPDATING ///////////////////////////////
		glm::vec3  rotatedEye = glm::vec3(turntable.getRotationMatrix() * eye);  
		view = glm::lookAt(glm::vec3(rotatedEye), glm::vec3(center), glm::vec3(0.0f, 1.0f, 0.0f));
		//////////////////////////////////////////////////////////////////////////////
				
		////////////////////////  SHADER / UNIFORM UPDATING //////////////////////////
		updateValuesInBufferData("view", glm::value_ptr(view), sizeof(glm::mat4) / sizeof(float), uniformBlockInfo, matrixData); 
		glBindBuffer(GL_UNIFORM_BUFFER, buffer);
		glBufferData(GL_UNIFORM_BUFFER, matrixData.size() * sizeof(glm::mat4), &matrixData[0], GL_DYNAMIC_DRAW);
		//////////////////////////////////////////////////////////////////////////////
		
		////////////////////////////////  RENDERING //// /////////////////////////////
		// clear stuff
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
		
		// bind VAO
		renderable[0].renderable->bind();

		// activate shader
		shaderProgram.use();

		// render instanced
		renderable[0].renderable->drawInstanced(NUM_INSTANCES);
	
		// ImGui::Render();
		// glDisable(GL_BLEND);
		// glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA); // this is altered by ImGui::Render(), so reset it every frame
		//////////////////////////////////////////////////////////////////////////////

	});

	destroyWindow(window);

	return 0;
}