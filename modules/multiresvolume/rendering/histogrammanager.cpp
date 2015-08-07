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

#include <float.h>
#include <math.h>

#include <modules/multiresvolume/rendering/histogrammanager.h>
#include <modules/multiresvolume/rendering/histogram.h>

namespace {
    const std::string _loggerCat = "HistogramManager";
}

namespace openspace {

HistogramManager::HistogramManager(TSP* tsp) : _tsp(tsp) {}

HistogramManager::~HistogramManager() {}

bool HistogramManager::buildHistograms(int numBins) {
    std::cout << "Build histograms with " << numBins << " bins each" << std::endl;
    _numBins = numBins;

    _file = &(_tsp->file());
    if (!_file->is_open()) {
        return false;
    }
    _minBin = 0.0; // Should be calculated from tsp file
    _maxBin = 1.0; // Should be calculated from tsp file

    int numTotalNodes = _tsp->numTotalNodes();
    _histograms = std::vector<Histogram>(numTotalNodes);

    minVal = FLT_MAX;
    maxVal = -FLT_MAX;

    bool success = buildHistogram(0);

    if (success) {
        // Print stuff
        _histograms[0].print();
        for (float f = 0.0; f < 1.0; f += 0.1) {
            std::cout << "Value at " << f << ": " << _histograms[0].sample(f) << std::endl;
        }
    } else {
        std::cout << "buildHistogram failed!!!!" << std::endl;
    }

    std::cout << "min: " << minVal << ", max: " << maxVal << std::endl;

    return success;
}

bool HistogramManager::buildHistogram(unsigned int brickIndex) {
    Histogram histogram(_minBin, _maxBin, _numBins);

    bool isBstLeaf = _tsp->isBstLeaf(brickIndex);
    bool isOctreeLeaf = _tsp->isOctreeLeaf(brickIndex);

    if (isBstLeaf && isOctreeLeaf) {
        // TSP leaf, read from file and build histogram
        std::vector<float> voxelValues = readValues(brickIndex);
        unsigned int numVoxels = voxelValues.size();

        for (unsigned int v = 0; v < numVoxels; ++v) {
            // DEBUG
            minVal = std::min(minVal, voxelValues[v]);
            maxVal = std::max(maxVal, voxelValues[v]);
            histogram.add(voxelValues[v], 1.0);
        }
    } else {
        // Has children
        auto children = std::vector<unsigned int>();

        if (!isBstLeaf) {
            // Push BST children
            children.push_back(_tsp->getBstLeft(brickIndex));
            children.push_back(_tsp->getBstRight(brickIndex));
        }
        if (!isOctreeLeaf) {
            // Push Octree children
            unsigned int firstChild = _tsp->getFirstChild(brickIndex);
            for (int c = 0; c < 8; c++) {
                children.push_back(firstChild + c);
            }
        }
        int numChildren = children.size();
        for (int c = 0; c < numChildren; c++) {
            // Visit child
            unsigned int childIndex = children[c];
            if (_histograms[childIndex].isValid() || buildHistogram(childIndex)) {
                if (numChildren <= 8 || c < 2) {
                    // If node has both BST and Octree children, only add BST ones
                    histogram.add(_histograms[childIndex]);
                }
            } else {
                return false;
            }
        }
    }

    histogram.normalize();
    _histograms[brickIndex] = histogram;

    return true;
}

std::vector<float> HistogramManager::readValues(unsigned int brickIndex) {
    unsigned int paddedBrickDim = _tsp->paddedBrickDim();
    unsigned int numBrickVals = paddedBrickDim * paddedBrickDim * paddedBrickDim;
    std::vector<float> voxelValues(numBrickVals);

    std::streampos offset = _tsp->dataPosition() + static_cast<long long>(brickIndex*numBrickVals*sizeof(float));
    _file->seekg(offset);

    _file->read(reinterpret_cast<char*>(&voxelValues[0]),
        static_cast<size_t>(numBrickVals)*sizeof(float));

    return voxelValues;
}

} // namespace openspace
