/*****************************************************************************************
 *                                                                                       *
 * OpenSpace                                                                             *
 *                                                                                       *
 * Copyright (c) 2014                                                                    *
 *                                                                                       *
 * Permission is hereby granted, free of charge, to any person obtaining a copy of this  *
 * software and associated documentation files (the "Software"), to deal in the Software *
 * without restriction, including without limitation the rights to use, copy, modify,    *
 * merge, publish, distribute, sublicense, and/or sell copies of the Software, and to    *
 * permit persons to whom the Software is furnished to do so, subject to the following   *
 * conditions:                                                                           *
 *                                                                                       *
 * The above copyright notice and this permission notice shall be included in all copies *
 * or substantial portions of the Software.                                              *
 *                                                                                       *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED,   *
 * INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A         *
 * PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT    *
 * HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF  *
 * CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE  *
 * OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.                                         *
 ****************************************************************************************/

#include <openspace/rendering/flare/brickmanager.h>

#include <ghoul/filesystem/filesystem.h>
#include <ghoul/logging/logmanager.h>
#include <ghoul/opengl/texture.h>

#include <iostream>
#include <fstream>

namespace {
	const std::string _loggerCat = "BrickManager";
}

namespace openspace {

BrickManager::BrickManager(const std::string& filename) 
	: _filename(filename)
	, textureAtlas_(nullptr)
	, hasReadHeader_(false)
	, atlasInitialized_(false)
{}

BrickManager::~BrickManager() {

}

bool BrickManager::readHeader() {

	std::ifstream file(_filename, std::ios::in | std::ios::binary);

	if (!file.is_open())
		return false;

	file.read(reinterpret_cast<char*>(&_header), sizeof(TSP::Header));

	LDEBUG("Grid type: " << _header.gridType_);
	LDEBUG("Original num timesteps: " << _header.numOrigTimesteps_);
	LDEBUG("Num timesteps: " << _header.numTimesteps_);
	LDEBUG("Brick dims: " << _header.xBrickDim_ << " " << _header.yBrickDim_ << " " << _header.zBrickDim_);
	LDEBUG("Num bricks: " << _header.xNumBricks_ << " " << _header.yNumBricks_ << " " << _header.zNumBricks_);
	LDEBUG("");

	// Keep track of position for data in file
	_dataOffset = file.tellg();

	brickDim_ = _header.xBrickDim_;
	numBricks_ = _header.xNumBricks_;
	paddedBrickDim_ = brickDim_ + paddingWidth_ * 2;
	atlasDim_ = paddedBrickDim_*numBricks_;

	LDEBUG("Padded brick dim: " << paddedBrickDim_);
	LDEBUG("Atlas dim: " << atlasDim_);

	numBrickVals_ = paddedBrickDim_*paddedBrickDim_*paddedBrickDim_;
	// Number of bricks per frame
	numBricksFrame_ = numBricks_*numBricks_*numBricks_;

	// Calculate number of bricks in tree
	unsigned int numOTLevels = static_cast<unsigned int>(log((int)numBricks_) / log(2) + 1);
	unsigned int numOTNodes = static_cast<unsigned int>((pow(8, numOTLevels) - 1) / 7);
	unsigned int numBSTNodes = static_cast<unsigned int>(_header.numTimesteps_ * 2 - 1);
	numBricksTree_ = numOTNodes * numBSTNodes;
	LDEBUG("Num OT levels: " << numOTLevels);
	LDEBUG("Num OT nodes: " << numOTNodes);
	LDEBUG("Num BST nodes: " << numBSTNodes);
	LDEBUG("Num bricks in tree: " << numBricksTree_);
	LDEBUG("Num values per brick: " << numBrickVals_);

	brickSize_ = sizeof(float)*numBrickVals_;
	volumeSize_ = brickSize_*numBricksFrame_;
	numValsTot_ = numBrickVals_*numBricksFrame_;

	file.seekg(0, file.end);
	long long fileSize = file.tellg();
	long long calcFileSize = static_cast<long long>(numBricksTree_)*
		static_cast<long long>(brickSize_)+_dataOffset;


	if (fileSize != calcFileSize) {
		LERROR("Sizes don't match");
		LERROR("calculated file size: " << calcFileSize);
		LERROR("file size: " << fileSize);
		return false;
	}

	hasReadHeader_ = true;

	// Hold two brick lists
	brickLists_.resize(2);
	// Make sure the brick list can hold the maximum number of bricks
	// Each entry holds tree coordinates
	brickLists_[EVEN].resize(numBricksTree_ * 3, -1);
	brickLists_[ODD].resize(numBricksTree_ * 3, -1);

	// Allocate space for keeping tracks of bricks in PBO
	bricksInPBO_.resize(2);
	bricksInPBO_[EVEN].resize(numBricksTree_, -1);
	bricksInPBO_[ODD].resize(numBricksTree_, -1);

	// Allocate space for keeping track of the used coordinates in atlas
	usedCoords_.resize(2);
	usedCoords_[EVEN].resize(numBricksFrame_, false);
	usedCoords_[ODD].resize(numBricksFrame_, false);

	return true;
}

bool BrickManager::initialize() {
	if (atlasInitialized_) {
		LWARNING("InitAtlas() - already initialized");
	}

	if (!hasReadHeader_) {
		LWARNING("InitAtlas() - Has not read header, trying to read");
		return readHeader();
	}

	// Prepare the 3D texture
	std::vector<unsigned int> dims;
	dims.push_back(atlasDim_);
	dims.push_back(atlasDim_);
	dims.push_back(atlasDim_);
	textureAtlas_ = new ghoul::opengl::Texture(
		glm::size3_t(atlasDim_, atlasDim_, atlasDim_), 
		ghoul::opengl::Texture::Format::RGBA, 
		GL_RGBA, 
		GL_FLOAT);
	textureAtlas_->uploadTexture();
	//textureAtlas_ = Texture3D::New(dims);

	//if (!textureAtlas_->Init()) return false;

	atlasInitialized_ = true;

	return true;
}

}