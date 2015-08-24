/*****************************************************************************************
 *                                                                                       *
 * OpenSpace                                                                             *
 *                                                                                       *
 * Copyright (c) 2014-2015                                                               *
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

#include <vector>
#include <modules/volume/rendering/renderablevolume.h>
#include <modules/volume/rendering/transferfunction.h>
#include <openspace/util/powerscaledcoordinate.h>
#include <ghoul/misc/dictionary.h>

// Forward declare to minimize dependencies
namespace ghoul {
    namespace filesystem {
        class File;
    }
    namespace opengl {
        class ProgramObject;
        class Texture;
    }
}

namespace openspace {
// Forward declare
class TSP;
class AtlasManager;
class BrickSelector;
class ErrorHistogramManager;
class HistogramManager;

class RenderableMultiresVolume: public RenderableVolume {
public:
    RenderableMultiresVolume(const ghoul::Dictionary& dictionary);
    ~RenderableMultiresVolume();

    bool initialize() override;
    bool deinitialize() override;

    bool isReady() const override;

    virtual void update(const UpdateData& data) override;
    virtual void preResolve(ghoul::opengl::ProgramObject* program) override;
    virtual std::string getHeaderPath() override;
    virtual std::string getHelperPath() override;
    virtual std::vector<ghoul::opengl::Texture*> getTextures() override;
    virtual std::vector<int> getBuffers() override;

private:
    int _timestep;
    int _brickBudget;
    std::string _filename;

    std::string _transferFunctionName;
    std::string _volumeName;

    std::string _transferFunctionPath;

    TransferFunction* _transferFunction;

    float _spatialTolerance;
    float _temporalTolerance;

    TSP* _tsp;
    std::vector<int> _brickIndices;
    int _atlasMapSize;

    AtlasManager* _atlasManager;
    BrickSelector* _brickSelector;

    HistogramManager* _histogramManager;
    ErrorHistogramManager* _errorHistogramManager;
};

} // namespace openspace

#endif // __RENDERABLEMULTIRESVOLUME_H__
