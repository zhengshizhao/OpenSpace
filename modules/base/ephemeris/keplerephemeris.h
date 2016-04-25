/*****************************************************************************************
 *                                                                                       *
 * OpenSpace                                                                             *
 *                                                                                       *
 * Copyright (c) 2014-2016                                                               *
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

#ifndef __KEPLEREPHEMERIS_H__
#define __KEPLEREPHEMERIS_H__

#include <openspace/scene/ephemeris.h>

#include <ghoul/glm.h>

namespace openspace {
    
class KeplerEphemeris : public Ephemeris {
public:
    KeplerEphemeris(const ghoul::Dictionary& dictionary);
    virtual ~KeplerEphemeris();
    virtual const psc& position() const override;
    virtual void update(const UpdateData& data) override;

protected:
    KeplerEphemeris() = default;
    void computeOrbitPlane();

    double _eccentricity = 0.0;
    double _semimajorAxis = 0.0; // in m
    double _inclination = 0.0;
    double _ascendingNode = 0.0;
    double _argumentOfPeriapsis = 0.0;
    double _meanAnomalyAtEpoch = 0.0;
    
    double _epoch = 0.0; // in JD
    double _period = 0.0;

    glm::dmat3 _orbitPlaneRotation;

    glm::dvec3 _position;
    psc _pscPosition;

private:
    bool hasCompleteParametrization(const ghoul::Dictionary& dictionary);
    void readParamatrization(const ghoul::Dictionary& dictionary);
};
    
} // namespace openspace

#endif // __KEPLEREPHEMERIS_H__
