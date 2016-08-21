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
 *  permit persons to whom the Software is furnished to do so, subject to the following   *
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

#ifndef __DOWNLOADCOLLECTION_H__
#define __DOWNLOADCOLLECTION_H__

#include <string>
#include <vector>

class DownloadCollection {
public:
    struct DirectFile {
        std::string module;
        std::string url;
        std::string destination;
        std::string baseDir;
    };

    struct FileRequest {
        std::string module;
        std::string identifier;
        std::string destination;
        std::string baseDir;
        int version;
    };

    struct TorrentFile {
        std::string module;
        std::string file;
        std::string destination;
        std::string baseDir;
    };

    void crawlScene(const std::string& scene);

private:


    std::vector<DirectFile> _directFiles;
    std::vector<FileRequest> _fileRequests;
    std::vector<TorrentFile> _torrentFiles;

};


#endif // __DOWNLOADCOLLECTION_H__
