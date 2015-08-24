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

#include <modules/multiresvolume/rendering/tsp.h>
#include <modules/multiresvolume/rendering/tfbrickselector.h>
#include <modules/multiresvolume/rendering/errorhistogrammanager.h>
#include <modules/multiresvolume/rendering/histogram.h>
#include <modules/volume/rendering/transferfunction.h>
#include <algorithm>
#include <cassert>

namespace {
    const std::string _loggerCat = "TfBrickSelector";
}

namespace openspace {

TfBrickSelector::TfBrickSelector(TSP* tsp, ErrorHistogramManager* hm, TransferFunction* tf, int brickBudget)
    : _tsp(tsp)
    , _histogramManager(hm)
    , _transferFunction(tf)
    , _brickBudget(brickBudget - 7) {}

TfBrickSelector::~TfBrickSelector() {}

bool TfBrickSelector::initialize() {
    return calculateBrickErrors();
}

void TfBrickSelector::setBrickBudget(int brickBudget) {
    _brickBudget = brickBudget - 7;
}

void TfBrickSelector::selectBricks(int timestep, std::vector<int>& bricks) {
    int numTimeSteps = _tsp->header().numTimesteps_;
    int numBricksPerDim = _tsp->header().xNumBricks_;

    unsigned int rootNode = 0;
    BrickSelection::SplitType splitType;
    float rootSplitPoints = splitPoints(rootNode, splitType);
    BrickSelection brickSelection = BrickSelection(numBricksPerDim, numTimeSteps, splitType, rootSplitPoints);

    std::vector<BrickSelection> priorityQueue;
    std::vector<BrickSelection> leafSelections;

    if (splitType != BrickSelection::SplitType::None) {
        priorityQueue.push_back(brickSelection);
    } else {
        leafSelections.push_back(brickSelection);
    }

    int brickBudget = _brickBudget;
    int nSelectedBricks = 1;
    while (nSelectedBricks < brickBudget && priorityQueue.size() > 0) {
        std::pop_heap(priorityQueue.begin(), priorityQueue.end(), BrickSelection::compareSplitPoints);
        BrickSelection bs = priorityQueue.back();

        // TODO: handle edge case when we can only afford temporal splits or no split (only 1 spot left)

        unsigned int brickIndex = bs.brickIndex;
        priorityQueue.pop_back();
        if (bs.splitType == BrickSelection::SplitType::Temporal) {
            int timeSpanCenter = bs.centerT();
            unsigned int childBrickIndex;
            bool pickRightTimeChild = bs.timestepInRightChild(timestep);

            if (pickRightTimeChild) {
                childBrickIndex = _tsp->getBstRight(brickIndex);
            } else {
                childBrickIndex = _tsp->getBstLeft(brickIndex);
            }

            BrickSelection::SplitType childSplitType;
            float childSplitPoints = splitPoints(childBrickIndex, childSplitType);
            BrickSelection childSelection = bs.splitTemporally(pickRightTimeChild, childBrickIndex, childSplitType, childSplitPoints);

            if (childSplitType != BrickSelection::SplitType::None) {
                priorityQueue.push_back(childSelection);
                std::push_heap(priorityQueue.begin(), priorityQueue.end(), BrickSelection::compareSplitPoints);
            } else {
                leafSelections.push_back(childSelection);
            }
        } else if (bs.splitType == BrickSelection::SplitType::Spatial) {
            nSelectedBricks += 7; // Remove one and add eight.
            unsigned int firstChild = _tsp->getFirstChild(brickIndex);

            for (unsigned int i = 0; i < 8; i++) {
                unsigned int childBrickIndex = firstChild + i;

                BrickSelection::SplitType childSplitType;
                float childSplitPoints = splitPoints(childBrickIndex, childSplitType);
                BrickSelection childSelection = bs.splitSpatially(i % 2, (i/2) % 2, i/4, childBrickIndex, childSplitType, childSplitPoints);

                if (childSplitType != BrickSelection::SplitType::None) {
                    priorityQueue.push_back(childSelection);
                    std::push_heap(priorityQueue.begin(), priorityQueue.end(), BrickSelection::compareSplitPoints);
                } else {
                    leafSelections.push_back(childSelection);
                }
            }
        }
    }

    // Write selected inner nodes to brickSelection vector
    for (const BrickSelection& bs : priorityQueue) {
        writeSelection(bs, bricks);
    }

    // Write selected leaf nodes to brickSelection vector
    for (const BrickSelection& bs : leafSelections) {
        writeSelection(bs, bricks);
    }
}

float TfBrickSelector::temporalSplitPoints(unsigned int brickIndex) {
    if (_tsp->isBstLeaf(brickIndex)) {
        return -1;
    }

    unsigned int leftChild = _tsp->getBstLeft(brickIndex);
    unsigned int rightChild = _tsp->getBstRight(brickIndex);

    float currentError = _brickErrors[brickIndex];
    float splitError = _brickErrors[leftChild] + _brickErrors[rightChild];

    return (currentError - splitError) * 0.5;
}

float TfBrickSelector::spatialSplitPoints(unsigned int brickIndex) {
    if (_tsp->isOctreeLeaf(brickIndex)) {
        return -1;
    }

    float currentError = _brickErrors[brickIndex];
    float splitError = 0;

    unsigned int firstChild = _tsp->getFirstChild(brickIndex);
    for (unsigned int i = 0; i < 8; i++) {
        unsigned int child = firstChild + i;
        splitError += _brickErrors[child];
    }

    return (currentError - splitError) * 0.125;
}

float TfBrickSelector::splitPoints(unsigned int brickIndex, BrickSelection::SplitType& splitType) {
    float temporalPoints = temporalSplitPoints(brickIndex);
    float spatialPoints = spatialSplitPoints(brickIndex);
    float splitPoints;

    if (spatialPoints > -1 && spatialPoints > temporalPoints) {
        splitPoints = spatialPoints;
        splitType = BrickSelection::SplitType::Spatial;
    } else if (temporalPoints > -1) {
        splitPoints = temporalPoints;
        splitType = BrickSelection::SplitType::Temporal;
    } else {
        splitPoints = -1;
        splitType = BrickSelection::SplitType::None;
    }
    return splitPoints;
}


bool TfBrickSelector::calculateBrickErrors() {
    TransferFunction *tf = _transferFunction;
    if (!tf) return false;

    size_t tfWidth = tf->width();
    if (tfWidth <= 0) return false;

    std::vector<float> gradients(tfWidth - 1);
    for (size_t offset = 0; offset < tfWidth - 1; offset++) {
        glm::vec4 prevRgba = tf->sample(offset);
        glm::vec4 nextRgba = tf->sample(offset + 1);

        float colorDifference = glm::distance(prevRgba, nextRgba);
        float alpha = (prevRgba.w + nextRgba.w) * 0.5;

        gradients[offset] = colorDifference*alpha;
    }

    unsigned int nHistograms = _tsp->numTotalNodes();
    _brickErrors = std::vector<float>(nHistograms);

    for (unsigned int brickIndex = 0; brickIndex < nHistograms; brickIndex++) {
        if (_tsp->isBstLeaf(brickIndex) && _tsp->isOctreeLeaf(brickIndex)) {
            _brickErrors[brickIndex] = 0;
        } else {
            const Histogram* histogram = _histogramManager->getHistogram(brickIndex);
            float error = 0;
            for (int i = 0; i < gradients.size(); i++) {
                float x = (i + 0.5) / tfWidth;
                float sample = histogram->interpolate(x);
                assert(sample >= 0);
                assert(gradients[i] >= 0);
                error += sample * gradients[i];
            }
            _brickErrors[brickIndex] = error;
        }
    }

    return true;
}

int TfBrickSelector::linearCoords(int x, int y, int z) {
    const TSP::Header &header = _tsp->header();
    return x + (header.xNumBricks_ * y) + (header.xNumBricks_ * header.yNumBricks_ * z);
}

void TfBrickSelector::writeSelection(BrickSelection brickSelection, std::vector<int>& bricks) {
    BrickCover coveredBricks = brickSelection.cover;
    for (int z = coveredBricks.lowZ; z < coveredBricks.highZ; z++) {
        for (int y = coveredBricks.lowY; y < coveredBricks.highY; y++) {
            for (int x = coveredBricks.lowX; x < coveredBricks.highX; x++) {
                bricks[linearCoords(x, y, z)] = brickSelection.brickIndex;
            }
        }
    }
}


} // namespace openspace
