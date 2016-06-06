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


#include <ghoul/logging/logmanager.h>
#include <ghoul/filesystem/filesystem.h> // abspath
#include <ghoul/misc/assert.h>

#include <modules/globebrowsing/other/tiledataset.h>
#include <modules/globebrowsing/other/tileprovider.h>
#include <modules/globebrowsing/geodetics/angle.h>



namespace {
    const std::string _loggerCat = "TileDataset";
}



namespace openspace {

    // INIT THIS TO FALSE AFTER REMOVED FROM TILEPROVIDER
    bool TileDataset::GdalHasBeenInitialized = false; 

    TileDataset::TileDataset(const std::string& gdalDatasetDesc, int minimumPixelSize, GLuint dataType)
        : _minimumPixelSize(minimumPixelSize)
    {
        if (!GdalHasBeenInitialized) {
            GDALAllRegister();
            GdalHasBeenInitialized = true;
        }

        _dataset = (GDALDataset *)GDALOpen(gdalDatasetDesc.c_str(), GA_ReadOnly);
        ghoul_assert(_dataset != nullptr, "Failed to load dataset:\n" << gdalDatasetDesc);
        _dataLayout = DataLayout(_dataset, dataType);

        _depthTransform = calculateTileDepthTransform();
        _tileLevelDifference = calculateTileLevelDifference(_dataset, minimumPixelSize);
    }


    TileDataset::~TileDataset() {
        delete _dataset;
    }

    int TileDataset::calculateTileLevelDifference(GDALDataset* dataset, int minimumPixelSize) {
        GDALRasterBand* firstBand = dataset->GetRasterBand(1);
        int numOverviews = firstBand->GetOverviewCount();
        int sizeLevel0 = firstBand->GetOverview(numOverviews - 1)->GetXSize();
        return log2(minimumPixelSize) - log2(sizeLevel0);
    }

    TileDepthTransform TileDataset::calculateTileDepthTransform() {
        GDALRasterBand* firstBand = _dataset->GetRasterBand(1);
        // Floating point types does not have a fix maximum or minimum value and
        // can not be normalized when sampling a texture. Hence no rescaling is needed.
        double maximumValue = (_dataLayout.gdalType == GDT_Float32 || _dataLayout.gdalType == GDT_Float64) ?
            1.0 : getMaximumValue(_dataLayout.gdalType);
        
        TileDepthTransform transform;
        transform.depthOffset = firstBand->GetOffset();
        transform.depthScale = firstBand->GetScale() * maximumValue;
        return transform;
    }

    int TileDataset::getMaximumLevel() const {
        int numOverviews = _dataset->GetRasterBand(1)->GetOverviewCount();
        int maximumLevel = numOverviews - 1 - _tileLevelDifference;
        return maximumLevel;
    }

    TileDepthTransform TileDataset::getDepthTransform() const {
        return _depthTransform;
    }

    std::shared_ptr<TileIOResult> TileDataset::readTileData(ChunkIndex chunkIndex)
    {               
        GdalDataRegion region(_dataset, chunkIndex, _tileLevelDifference);
        size_t bytesPerLine = _dataLayout.bytesPerPixel * region.numPixels.x;
        size_t totalNumBytes = bytesPerLine * region.numPixels.y;
        char* imageData = new char[totalNumBytes];

        CPLErr worstError = CPLErr::CE_None;

        // Read the data (each rasterband is a separate channel)
        for (size_t i = 0; i < region.numRasters; i++) {
            GDALRasterBand* rasterBand = _dataset->GetRasterBand(i + 1)->GetOverview(region.overview);

            char* dataDestination = imageData + (i * _dataLayout.bytesPerDatum);
            
            CPLErr err = rasterBand->RasterIO(
                GF_Read,
                region.pixelStart.x,           // Begin read x
                region.pixelStart.y,           // Begin read y
                region.numPixels.x,            // width to read x
                region.numPixels.y,            // width to read y
                dataDestination,               // Where to put data
                region.numPixels.x,            // width to write x in destination
                region.numPixels.y,            // width to write y in destination
                _dataLayout.gdalType,		   // Type
                _dataLayout.bytesPerPixel,	   // Pixel spacing
                bytesPerLine);      // Line spacing

            // CE_None = 0, CE_Debug = 1, CE_Warning = 2, CE_Failure = 3, CE_Fatal = 4
            worstError = std::max(worstError, err);
        }

        std::shared_ptr<RawTileData> tileData = nullptr;
        tileData = createRawTileData(region, _dataLayout, imageData);

        std::shared_ptr<TileIOResult> result(new TileIOResult);
        result->error = worstError;
        result->rawTileData = tileData;

        return result;
    }


    std::shared_ptr<RawTileData> TileDataset::createRawTileData(const GdalDataRegion& region,
        const DataLayout& dataLayout, const char* imageData)
    {
        size_t bytesPerLine = dataLayout.bytesPerPixel * region.numPixels.x;
        size_t totalNumBytes = bytesPerLine * region.numPixels.y;

        //if(cplError == CPLErr::CE_Fatal)

        // GDAL reads image data top to bottom. We want the opposite.
        char* imageDataYflipped = new char[totalNumBytes];
        for (size_t y = 0; y < region.numPixels.y; y++) {
            for (size_t x = 0; x < bytesPerLine; x++) {
                imageDataYflipped[x + y * bytesPerLine] =
                    imageData[x + (region.numPixels.y - 1 - y) * bytesPerLine];
            }
        }

        delete[] imageData;

        glm::uvec3 dims(region.numPixels.x, region.numPixels.y, 1);
        RawTileData::TextureFormat textureFormat = getTextureFormat(region.numRasters, dataLayout.gdalType);
        GLuint glType = getOpenGLDataType(dataLayout.gdalType);
        RawTileData* textureDataPtr = new RawTileData(imageDataYflipped, dims,
            textureFormat, glType, region.chunkIndex);

        std::shared_ptr<RawTileData> textureData =
            std::shared_ptr<RawTileData>(textureDataPtr);

        return textureData;
    }





    size_t TileDataset::numberOfBytes(GDALDataType gdalType) {
        switch (gdalType) {
            case GDT_Byte: return sizeof(GLubyte);
            case GDT_UInt16: return sizeof(GLushort);
            case GDT_Int16: return sizeof(GLshort);
            case GDT_UInt32: return sizeof(GLuint);
            case GDT_Int32: return sizeof(GLint);
            case GDT_Float32: return sizeof(GLfloat);
            case GDT_Float64: return sizeof(GLdouble);
            default:  
                LERROR("Unknown data type"); 
                return -1; 
        }
    }

    size_t TileDataset::getMaximumValue(GDALDataType gdalType) {
        switch (gdalType) {
            case GDT_Byte: return 2 << 7;
            case GDT_UInt16: return 2 << 15;
            case GDT_Int16: return 2 << 14;
            case GDT_UInt32: return 2 << 31;
            case GDT_Int32: return 2 << 30;
            default:
                LERROR("Unknown data type"); 
                return -1;
        }
    }


    
    glm::uvec2 TileDataset::geodeticToPixel(GDALDataset* dataSet, const Geodetic2& geo) {
        double padfTransform[6];
        CPLErr err = dataSet->GetGeoTransform(padfTransform);

        ghoul_assert(err != CE_Failure, "Failed to get transform");

        Scalar Y = Angle<Scalar>::fromRadians(geo.lat).asDegrees();
        Scalar X = Angle<Scalar>::fromRadians(geo.lon).asDegrees();
        
        // convert from pixel and line to geodetic coordinates
        // Xp = padfTransform[0] + P*padfTransform[1] + L*padfTransform[2];
        // Yp = padfTransform[3] + P*padfTransform[4] + L*padfTransform[5];

        // <=>
        double* a = &(padfTransform[0]);
        double* b = &(padfTransform[3]);

        // Xp = a[0] + P*a[1] + L*a[2];
        // Yp = b[0] + P*b[1] + L*b[2];
        
        // <=>
        double divisor = (a[2]*b[1] - a[1]*b[2]);
        ghoul_assert(divisor != 0.0, "Division by zero!");
        //ghoul_assert(a[2] != 0.0, "a2 must not be zero!");
        double P = (a[0]*b[2] - a[2]*b[0] + a[2]*Y - b[2]*X) / divisor;
        double L = (-a[0]*b[1] + a[1]*b[0] - a[1]*Y + b[1]*X) / divisor;
        // ref: https://www.wolframalpha.com/input/?i=X+%3D+a0+%2B+a1P+%2B+a2L,+Y+%3D+b0+%2B+b1P+%2B+b2L,+solve+for+P+and+L

        double Xp = a[0] + P*a[1] + L*a[2];
        double Yp = b[0] + P*b[1] + L*b[2];

        ghoul_assert(abs(X - Xp) < 1e-10, "inverse should yield X as before");
        ghoul_assert(abs(Y - Yp) < 1e-10, "inverse should yield Y as before");
        
        return glm::uvec2(glm::round(P), glm::round(L));
    }


    RawTileData::TextureFormat TileDataset::getTextureFormat(
        int rasterCount, GDALDataType gdalType)
    {
        RawTileData::TextureFormat format;

        switch (rasterCount) {
        case 1: // Red
            format.ghoulFormat = Texture::Format::Red;
            switch (gdalType) {
            case GDT_Byte:      format.glFormat = GL_R8; break;
            case GDT_UInt16:    format.glFormat = GL_R16; break;
            case GDT_Int16:     format.glFormat = GL_R16; break;
            case GDT_UInt32:    format.glFormat = GL_R32UI; break;
            case GDT_Int32:     format.glFormat = GL_R32I; break;
            case GDT_Float32:   format.glFormat = GL_R32F; break;
            //case GDT_Float64:   format.glFormat = GL_RED; break; // No representation of 64 bit float?  
            default: LERROR("GDAL data type unknown to OpenGL: " << gdalType);
            }
            break;
        case 2:
            format.ghoulFormat = Texture::Format::RG;
            switch (gdalType) {
            case GDT_Byte: format.glFormat = GL_RG8; break;
            case GDT_UInt16: format.glFormat = GL_RG16; break;
            case GDT_Int16: format.glFormat = GL_RG16; break;
            case GDT_UInt32: format.glFormat = GL_RG32UI; break;
            case GDT_Int32: format.glFormat = GL_RG32I; break;
            case GDT_Float32: format.glFormat = GL_RG32F; break;    
            case GDT_Float64: format.glFormat = GL_RED; break; // No representation of 64 bit float?
            default: LERROR("GDAL data type unknown to OpenGL: " << gdalType);
            }
            break;
        case 3:
            format.ghoulFormat = Texture::Format::RGB;
            switch (gdalType) {
            case GDT_Byte: format.glFormat = GL_RGB8; break;
            case GDT_UInt16: format.glFormat = GL_RGB16; break;
            case GDT_Int16: format.glFormat = GL_RGB16; break;
            case GDT_UInt32: format.glFormat = GL_RGB32UI; break;
            case GDT_Int32: format.glFormat = GL_RGB32I; break;
            case GDT_Float32: format.glFormat = GL_RGB32F; break;    
            // case GDT_Float64: format.glFormat = GL_RED; break;// No representation of 64 bit float? 
            default: LERROR("GDAL data type unknown to OpenGL: " << gdalType);
            }
            break;
        case 4:
            format.ghoulFormat = Texture::Format::RGBA;
            switch (gdalType) {
            case GDT_Byte: format.glFormat = GL_RGBA8; break;
            case GDT_UInt16: format.glFormat = GL_RGBA16; break;
            case GDT_Int16: format.glFormat = GL_RGBA16; break;
            case GDT_UInt32: format.glFormat = GL_RGBA32UI; break;
            case GDT_Int32: format.glFormat = GL_RGBA32I; break;
            case GDT_Float32: format.glFormat = GL_RGBA32F; break;
            case GDT_Float64: format.glFormat = GL_RED; break; // No representation of 64 bit float?
            default: LERROR("GDAL data type unknown to OpenGL: " << gdalType);
            }
            break;
        default:
            LERROR("Unknown number of channels for OpenGL texture: " << rasterCount);
            break;
        }
        return format;
    }






    GLuint TileDataset::getOpenGLDataType(GDALDataType gdalType) {
        switch (gdalType) {
        case GDT_Byte: return GL_UNSIGNED_BYTE; 
        case GDT_UInt16: return GL_UNSIGNED_SHORT;
        case GDT_Int16: return GL_SHORT;
        case GDT_UInt32: return GL_UNSIGNED_INT;
        case GDT_Int32: return GL_INT;
        case GDT_Float32: return GL_FLOAT;
        case GDT_Float64: return GL_DOUBLE; 
        default:
            LERROR("GDAL data type unknown to OpenGL: " << gdalType);
            return GL_UNSIGNED_BYTE;
        }
    }

    GDALDataType TileDataset::getGdalDataType(GLuint glType) {
        switch (glType) {
        case GL_UNSIGNED_BYTE: return GDT_Byte;
        case GL_UNSIGNED_SHORT: return GDT_UInt16;
        case GL_SHORT: return GDT_Int16;
        case GL_UNSIGNED_INT: return GDT_UInt32;
        case GL_INT: return GDT_Int32;
        case GL_FLOAT: return GDT_Float32;
        case GL_DOUBLE: return GDT_Float64;
        default:
            LERROR("OpenGL data type unknown to GDAL: " << glType);
            return GDT_Unknown;
        }
    }


    TileDataset::GdalDataRegion::GdalDataRegion(GDALDataset * dataSet,
        const ChunkIndex& chunkIndex, int tileLevelDifference)
        : chunkIndex(chunkIndex)
    {

        GDALRasterBand* firstBand = dataSet->GetRasterBand(1);

        // Assume all raster bands have the same data type

        // Level = overviewCount - overview (default, levels may be overridden)
        int numOverviews = firstBand->GetOverviewCount();


        // Generate a patch from the chunkIndex, extract the bounds which
        // are used to calculated where in the GDAL data set to read data. 
        // pixelStart0 and pixelEnd0 defines the interval in the pixel space 
        // at overview 0
        GeodeticPatch patch = GeodeticPatch(chunkIndex);

        glm::uvec2 pixelStart0 = geodeticToPixel(dataSet, patch.northWestCorner());
        glm::uvec2 pixelEnd0 = geodeticToPixel(dataSet, patch.southEastCorner());
        glm::uvec2 numPixels0 = pixelEnd0 - pixelStart0;

        // Calculate a suitable overview to choose from the GDAL dataset
        int minNumPixels0 = glm::min(numPixels0.x, numPixels0.y);
        int sizeLevel0 = firstBand->GetOverview(numOverviews - 1)->GetXSize();
        int ov = std::log2(minNumPixels0) - std::log2(sizeLevel0 + 1) - tileLevelDifference;
        ov = glm::clamp(ov, 0, numOverviews - 1);

        // Convert the interval [pixelStart0, pixelEnd0] to pixel space at 
        // the calculated suitable overview, ov. using a >> b = a / 2^b
        int toShift = ov + 1;

        // Set member variables
        overview = ov;
        numRasters = dataSet->GetRasterCount();
        ghoul_assert(numRasters > 0, "Bad dataset. Contains no rasterband.");

        pixelStart = glm::uvec2(pixelStart0.x >> toShift, pixelStart0.y >> toShift);
        pixelEnd = glm::uvec2(pixelEnd0.x >> toShift, pixelEnd0.y >> toShift);
        numPixels = pixelEnd - pixelStart;
    }

    TileDataset::DataLayout::DataLayout() {
    }

    TileDataset::DataLayout::DataLayout(GDALDataset* dataSet, GLuint glType) {
        // Assume all raster bands have the same data type
        gdalType =  glType != 0 ? getGdalDataType(glType) : dataSet->GetRasterBand(1)->GetRasterDataType();
        bytesPerDatum = numberOfBytes(gdalType);
        bytesPerPixel = bytesPerDatum * dataSet->GetRasterCount();
    }






    
            
}  // namespace openspace