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

#ifndef __RENDERABLEMULTIRESVOLUME_H__
#define __RENDERABLEMULTIRESVOLUME_H__

// open space includes
#include <modules/volume/rendering/renderablevolume.h>
#include <openspace/util/updatestructures.h>

// ghoul includes
#include <ghoul/opengl/ghoul_gl.h>

namespace ghoul {
	namespace opengl {
		class Texture;
		class ProgramObject;
		class FramebufferObject;
	}
}

namespace openspace {
// Forward declare
class TSP;
class BrickManager;
class AtlasManager;
class BrickSelector;


class RenderableMultiresVolume : public RenderableVolume {
public:
	RenderableMultiresVolume(const ghoul::Dictionary& dictionary);
	~RenderableMultiresVolume();

	bool initialize();
	bool deinitialize();

	virtual bool isReady() const;

	void render(const RenderData& data) override;
	void update(const UpdateData& data) override;

private:

	// Types
	typedef std::vector<int> Bricks;

	// MultiresVolume internal functions
	void readRequestedBricks();
	void launchRaycaster(int timestep, const std::vector<int>& brickList, const RenderData& data);
	void raycast(int timestep, const std::vector<unsigned int>&atlasMap, const RenderData& data);
	void PBOToAtlas(size_t buffer);
	void buildBrickList(size_t buffer, const Bricks& bricks);
	void diskToPBO(size_t buffer);

	// Internal helper functions
	void initializeColorCubes();
	void renderColorCubeTextures(const RenderData& data, bool front = false);
	void selectBricksGpu(const RenderData& data);

	// 
	TSP* _tsp;
	BrickManager* _brickManager;
	AtlasManager* _atlasManager;
	BrickSelector* _brickSelector;

	float _spatialTolerance;
	float _temporalTolerance;

	GLuint _boxArray;
	GLuint _dispatchBuffers[2];
	//GLuint _brickRequestBuffer, _brickRequestTexture;
	GLuint _reqeustedBrickSSO;
	GLuint _brickSSO;
	GLuint _brickSSOSize;
	ghoul::opengl::ProgramObject* _tspTraversal;
	ghoul::opengl::ProgramObject* _raycasterTsp;
	ghoul::opengl::ProgramObject* _multiresRaycaster;
	ghoul::opengl::ProgramObject* _pscColorPassthrough;

	ghoul::opengl::ProgramObject* _cubeProgram;
	ghoul::opengl::ProgramObject* _textureToAbuffer;

	ghoul::opengl::FramebufferObject* _fbo;
	ghoul::opengl::Texture* _backTexture;
	ghoul::opengl::Texture* _outputTexture;
	ghoul::opengl::Texture* _transferFunction;

	// TSP data members
	Bricks _brickRequest;
	std::vector<int> _brickIndices;

	// Animation
	unsigned int _timestep;
};

} // namespace openspace
#endif // __RENDERABLEMULTIRESVOLUME_H__
