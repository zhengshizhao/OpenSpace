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

#include <modules/base/ephemeris/keplerephemeris.h>

#include <glm/gtx/transform.hpp>
#include <glm/gtc/constants.hpp>

namespace {
    const std::string KeyEccentricity = "Eccentricity";
    const std::string KeySemimajorAxis = "SemimajorAxis";
    const std::string KeyInclination = "Inclination";
    const std::string KeyAscendingNode = "AscendingNode";
    const std::string KeyArgumentOfPeriapsis = "ArgumentOfPeriapsis";
    const std::string KeyMeanAnomaly = "MeanAnomaly";
    const std::string KeyEpoch = "Epoch";
    const std::string KeyPeriod = "Period";
}


template <class T, class F>
T solveIteration(F function, T x0, T err, int maxIterations = 100) {
    T x = 0;
    T x2 = x0;

    for (int i = 0; i < maxIterations; ++i) {
        x = x2;
        x2 = function(x);
        if (abs(x2 - x) < err)
            return x2;
    }

    return x2;
}

namespace openspace {

KeplerEphemeris::KeplerEphemeris(const ghoul::Dictionary& dictionary) {
    bool isComplete = hasCompleteParametrization(dictionary);
    ghoul_assert(isComplete, "KeplerEphemeris description is not complete");

    readParamatrization(dictionary);
    computeOrbitPlane();
}

KeplerEphemeris::~KeplerEphemeris() {}

const psc& KeplerEphemeris::position() const {
    return _pscPosition;
}

void KeplerEphemeris::update(const UpdateData& data) {
    // Julian date v ephemeris time
    const double J2000 = 2451545.0;
    const double SecondsPerDay = 60*60*24;

    double timeInJD = J2000 + data.time / SecondsPerDay;

    double time = timeInJD - _epoch;
    double meanMotion = 2.0 * glm::pi<double>() / _period;
    double meanAnomaly = _meanAnomalyAtEpoch + time * meanMotion;
    
    auto kepler = [&](double x) -> double {
        return meanAnomaly + _eccentricity * sin(x);
    };

    double E = solveIteration(kepler, meanAnomaly, 0.01, 20);

    glm::dvec2 pos;
    double a = _semimajorAxis / (1.0 - _eccentricity);
    pos.x = a * (cos(E) - _eccentricity);
    pos.y = a * sqrt(1 - _eccentricity * _eccentricity) * sin(E);

    _position = _orbitPlaneRotation * glm::dvec3(pos, 0.0) * 1000.0;
    _pscPosition = PowerScaledCoordinate::CreatePowerScaledCoordinate(_position.x, _position.y, _position.z);
}

void KeplerEphemeris::computeOrbitPlane() {
    double ascNode = glm::degrees(_ascendingNode);
    double inc = glm::degrees(_inclination);
    double per = glm::degrees(_argumentOfPeriapsis);

    _orbitPlaneRotation =
        glm::rotate(ascNode, glm::dvec3(0.f, 0.f, 1.f)) *
        glm::rotate(inc, glm::dvec3(1.f, 0.f, 0.f)) *
        glm::rotate(per, glm::dvec3(0.f, 0.f, 1.f));
}

bool KeplerEphemeris::hasCompleteParametrization(const ghoul::Dictionary& dictionary) {
    bool success = true;
    success &= dictionary.hasKeyAndValue<double>(KeyEccentricity);
    success &= dictionary.hasKeyAndValue<double>(KeySemimajorAxis);
    success &= dictionary.hasKeyAndValue<double>(KeyInclination);
    success &= dictionary.hasKeyAndValue<double>(KeyAscendingNode);
    success &= dictionary.hasKeyAndValue<double>(KeyArgumentOfPeriapsis);
    success &= dictionary.hasKeyAndValue<double>(KeyMeanAnomaly);
    success &= dictionary.hasKeyAndValue<double>(KeyEpoch);
    success &= dictionary.hasKeyAndValue<double>(KeyPeriod);
    return success;
}

void KeplerEphemeris::readParamatrization(const ghoul::Dictionary& dictionary) {
    _eccentricity = dictionary.value<double>(KeyEccentricity);
    _semimajorAxis = dictionary.value<double>(KeySemimajorAxis);
    _inclination = dictionary.value<double>(KeyInclination);
    _ascendingNode = dictionary.value<double>(KeyAscendingNode);
    _argumentOfPeriapsis = dictionary.value<double>(KeyArgumentOfPeriapsis);
    _meanAnomalyAtEpoch = dictionary.value<double>(KeyMeanAnomaly);
    _period = dictionary.value<double>(KeyPeriod);

    _epoch = dictionary.value<double>(KeyEpoch);

}


} // namespace openspace