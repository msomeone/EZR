//
// Created by Windrian on 26.01.2016.
//

#include <Rendering/GLTools.h>
#include "VolumetricLighting.h"

VolumetricLighting::VolumetricLighting(int width, int height) 
:
    _blockSize(64),
    _blockSide(8),
    _radiocity(10000000.0f),
    _scatterProbability(0.02f)
 {
    _width = width;
    _height = height;

    // setup shader program
    _raymarchingFBO = new FrameBufferObject(width, height);
    _raymarchingFBO->addColorAttachments(1);
    _raymarchingShader = new ShaderProgram("/screenSpace/fullscreen.vert", "/vml/raymarching_gbuffer.frag");
    _raymarchingShader->update("albedo", _scatterProbability);

    // setup render pass
    _raymarchingRenderPass = new RenderPass(_raymarchingShader, _raymarchingFBO);
    _raymarchingRenderPass->addRenderable(&_screenfillingQuad);
    _raymarchingRenderPass->setClearColor(0.0f, 0.0f, 0.0f, 0.0f);
    _raymarchingRenderPass->addClearBit(GL_COLOR_BUFFER_BIT);
    _raymarchingRenderPass->addDisable(GL_DEPTH_BUFFER_BIT);
};

VolumetricLighting::~VolumetricLighting() {

};

void VolumetricLighting::setupNoiseTexture() {
    std::vector<int> indices;
    for (int i = 0; i < _blockSize; i++) {
        indices.push_back(i);
    }

    // calculate number of blocks
    int numberOfBlocks = (int) ceil((float)(_width * _height) / _blockSize);
    int numberOfBlocksInX = (int) ceil((float)_width / _blockSide);

    // create all permutations
    std::vector<int> indexPermutations;
    for (int i = 0; i < numberOfBlocks; i++) {
        std::random_shuffle(indices.begin(), indices.end());
        indexPermutations.insert(indexPermutations.end(), indices.begin(), indices.end());
    }

    // fill permutations into unsigned char array in the right order
    int windowSize = _width * _height;
    unsigned char* noiseData = new unsigned char[windowSize]();
    unsigned int* values = new unsigned int[64];
    for (int y = 0; y < _height; y++) {
        for (int x = 0; x < _width; x++) {
            // calculate block number
            int blockX = floor(x / _blockSide);
            int blockY = floor(y / _blockSide);
            int blockIndex1D = blockY * numberOfBlocksInX + blockX;

            // calculate block start
            int blockPosX = blockX * _blockSide;
            int blockPosY = blockY * _blockSide;

            // get pixel position within block
            int pixelIndexX = x - blockPosX;
            int pixelIndexY = y - blockPosY;
            int pixelIndex1D = pixelIndexY * _blockSide + pixelIndexX;

            // get the 1D position within the permutations array
            int indexInPermutationsVector = blockIndex1D * _blockSize + pixelIndex1D;

            // calculate position within noise data array
            int dataIndex = y * _width + x;

            // write permutation index in noise data array at position dataIndex
            noiseData[dataIndex] = indexPermutations[indexInPermutationsVector];
            values[indexPermutations[indexInPermutationsVector]]++;
        }
    }

/*    for (int i = 0; i < 64; i++) {
        std::cout << "[" << i << "] = " << values[i] << std::endl;
    }*/

    // create a fbo to render to
    FrameBufferObject::s_internalFormat  = GL_R8;
    FrameBufferObject noiseMap = FrameBufferObject(_width, _height);
    noiseMap.addColorAttachments(1);
    FrameBufferObject::s_internalFormat  = GL_RGBA;

    // write noise to fbo texture
    OPENGLCONTEXT->bindTexture(noiseMap.getColorAttachmentTextureHandle(GL_COLOR_ATTACHMENT0));
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0,0, _width, _height, GL_RED, GL_UNSIGNED_BYTE, (GLvoid*)noiseData);
    OPENGLCONTEXT->bindTexture(0);

    // bind to shader
    _raymarchingShader->bindTextureOnUse("noiseMap", noiseMap.getColorAttachmentTextureHandle(GL_COLOR_ATTACHMENT0));
    checkGLError(true);
};

// void VolumetricLighting::updateView(glm::mat4 cameraView, glm::mat4 lightView) {

// }
