/*****************************************************************************************
 *                                                                                       *
 * OpenSpace                                                                             *
 *                                                                                       *
 * Copyright (c) 2015                                                                    *
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

#include <modules/multiresvolume/rendering/histogram.h>

#include <ghoul/logging/logmanager.h>

#include <cmath>

namespace {
    const std::string _loggerCat = "Histogram";
}

namespace openspace {

Histogram::Histogram() {}

Histogram::Histogram(float minBin, float maxBin, int numBins)
    : _minBin(minBin)
    , _maxBin(maxBin)
    , _numBins(numBins) {

    _data = std::vector<float>(numBins);
}

Histogram::~Histogram() {}


int Histogram::numBins() const {
    return _numBins;
}

float Histogram::minBin() const {
    return _minBin;
}

float Histogram::maxBin() const {
    return _maxBin;
}

bool Histogram::isValid() const {
    return _data.size() > 0;
}


bool Histogram::add(float bin, float value) {
    if (bin < _minBin || bin > _maxBin) {
        // Out of range
        return false;
    }

    float normalizedBin = (bin - _minBin) / (_maxBin - _minBin);    // [0.0, 1.0]
    int binIndex = floor(normalizedBin * _numBins);                 // [0, _numBins]
    if (binIndex == _numBins) binIndex--;                           // [0, _numBins[
    _data[binIndex] += value;
    return true;
}

bool Histogram::add(const Histogram& histogram) {
    if (_minBin == histogram.minBin() && _maxBin == histogram.maxBin() && _numBins == histogram.numBins()) {
        const std::vector<float>& data = histogram.data();
        for (int i = 0; i < _numBins; i++) {
            _data[i] += data[i];
        }
        return true;
    } else {
        LERROR("Dimension mismatch");
        return false;
    }
}


float Histogram::sample(float bin) const {
    float normalizedBin = (bin - _minBin) / (_maxBin - _minBin);
    float binIndex = normalizedBin * _numBins - 0.5; // Center
    // Clamp bins
    if (binIndex < 0) binIndex = 0;
    if (binIndex > _numBins) binIndex = _numBins;

    float interpolator = binIndex - floor(binIndex);
    int binLow = floor(binIndex);
    int binHigh = ceil(binIndex);
    return (1.0 - interpolator) * _data[binLow] + interpolator * _data[binHigh];
}

const std::vector<float>& Histogram::data() const {
    return _data;
}

std::vector<std::pair<float,float>> Histogram::getDecimated(int numBins) const {
    // Return a copy of _data decimated as in Ljung 2004
    return std::vector<std::pair<float,float>>();
}


void Histogram::normalize() {
    float sum = 0.0;
    for (int i = 0; i < _numBins; i++) {
        sum += _data[i];
    }
    for (int i = 0; i < _numBins; i++) {
        _data[i] /= sum;
    }
}

void Histogram::print() {
    std::cout << "number of bins: " << _numBins << std::endl
              << "range: " << _minBin << " - " << _maxBin << std::endl << std::endl;
    for (int i = 0; i < _numBins; i++) {
        float low = _minBin + float(i) / _numBins * (_maxBin - _minBin);
        float high = low + (_maxBin - _minBin) / float(_numBins);
        std::cout << "[" << low << ", " << high << "[" << std::endl
                  << "   " << _data[i] << std::endl;
    }
}

}
