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

#ifndef __RENDERABLEVOLUMEGL_H__
#define __RENDERABLEVOLUMEGL_H__

#include <vector>
#include <modules/volume/rendering/renderablevolume.h>
#include <openspace/util/powerscaledcoordinate.h>
#include <modules/base/rendering/transferfunction.h>

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

class RenderableVolumeGL: public RenderableVolume {
public:
    RenderableVolumeGL(const ghoul::Dictionary& dictionary);
    ~RenderableVolumeGL();

    bool initialize() override;
    bool deinitialize() override;

    bool isReady() const override;

    virtual void update(const UpdateData& data) override;
    virtual void preResolve(ghoul::opengl::ProgramObject* program) override;
    virtual std::string getHeaderPath() override;
    virtual std::string getHelperPath() override;
    virtual std::vector<ghoul::opengl::Texture*> getTextures() override;

private:
    ghoul::Dictionary _hintsDictionary;

    std::string _filename;
    std::string _volumeName;
    ghoul::opengl::Texture* _volume;

    std::string _transferFunctionPath;
    TransferFunction* _transferFunction;
};

} // namespace openspace

#endif
