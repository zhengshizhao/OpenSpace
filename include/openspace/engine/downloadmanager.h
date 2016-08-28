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

#ifndef __DOWNLOADMANAGER_H__
#define __DOWNLOADMANAGER_H__

#include <ghoul/filesystem/directory.h>
#include <ghoul/misc/exception.h>

#include <chrono>
#include <functional>
#include <future>

namespace std {
// This is just temporary until N3857 is implemented 
template <typename T>
bool is_ready(const std::future<T>& future) {
    return future.wait_for(std::chrono::seconds(0)) == std::future_status::ready;
}
} // namespace std

namespace openspace {

class DownloadManager {
public:
    enum class OverrideFiles {
        Yes = 0,
        No
    };

    struct DownloadException : public ghoul::RuntimeError {
        explicit DownloadException(std::string msg);
    };

    struct MemoryFile {
        int64_t identifier;
        std::vector<char> buffer;
        std::string contentType;
        std::string errorMessage;
    };

    struct File {
        int64_t identifier;
        std::string filename;
        std::string contentType;
        std::string errorMessage;

    };

    static std::string fileExtension(const std::string& contentType);

    using ProgressCallbackMemory = std::function<
        void(MemoryFile& f, size_t currentSize, size_t totalSize)
    >;

    using FinishedCallbackMemory = std::function<
        void(MemoryFile& f)
    >;

    using ProgressCallbackFile = std::function<
        void(File& f, size_t currentSize, size_t totalSize)
    >;

    using FinishedCallbackFile = std::function<
        void(File& f)
    >;

    using MemoryFileTask = std::packaged_task<MemoryFile()>;
    using FileTask = std::packaged_task<File()>;

    static void initialize();
    static void deinitialize();

    static MemoryFileTask download(const std::string& url, int64_t identifier = 0,
        ProgressCallbackMemory progress = ProgressCallbackMemory(),
        FinishedCallbackMemory finished = FinishedCallbackMemory()
    );

    static MemoryFile downloadSync(
        const std::string& url,
        int64_t identifier = 0,
        ProgressCallbackMemory progress = ProgressCallbackMemory()
    );



    static FileTask download(const std::string& url, const std::string& filename,
        int64_t identifier = 0, ProgressCallbackFile progress = ProgressCallbackFile(),
        FinishedCallbackFile finished = FinishedCallbackFile()
    );

    static File downloadSync(
        const std::string& url,
        const std::string& filename,
        int64_t identifier = 0,
        ProgressCallbackFile progress = ProgressCallbackFile()
    );
    



    DownloadManager(std::vector<std::string> requestUrls, int applicationVersion);
    
    std::vector<FileTask> requestFiles(
        const std::string& identifier,
        int version,
        const ghoul::filesystem::Directory& destination = ".",
        OverrideFiles overrideFiles = OverrideFiles::Yes,
        ProgressCallbackFile progress = ProgressCallbackFile()
    ) const;

    std::vector<std::string> requestFiles(
        const std::string& identifier,
        int version
    );

private:
    std::string request(const std::string& url, const std::string& identfier, int version) const;
    std::string selectRequestUrl() const;

    std::vector<std::string> _requestUrls;
    int _applicationVersion;
};

} // namespace openspace


#endif // __DOWNLOADMANAGER_H__

/*
#ifndef __DOWNLOADMANAGER_H__
#define __DOWNLOADMANAGER_H__

#include <ghoul/filesystem/file.h>
#include <ghoul/filesystem/directory.h>

#include <functional>
#include <memory>
#include <string>
#include <vector>
#include <future>

namespace openspace {

// Multithreaded
class DownloadManager {
public:
    struct FileFuture {
        // Since the FileFuture object will be used from multiple threads, we have to be
        // careful about the access pattern, that is, no values should be read and written
        // by both the DownloadManager and the outside threads.
        FileFuture(std::string file);

        // Values that are written by the DownloadManager to be consumed by others
        long long currentSize;
        long long totalSize;
        float progress; // [0,1]
        float secondsRemaining;
        bool isFinished;
        bool isAborted;
        std::string filePath;
        std::string errorMessage;
        std::string format;
        // Values set by others to be consumed by the DownloadManager
        bool abortDownload;
    };

    struct MemoryFile {
        char* buffer;
        size_t size;
        std::string format;
        bool corrupted;
    };

    using DownloadProgressCallback = std::function<void(const FileFuture&)>;
    using DownloadFinishedCallback = std::function<void(const FileFuture&)>;

    using SuccessCallback = std::function<void(const MemoryFile&)>;
    using ErrorCallback = std::function<void(const std::string&)>;

    using RequestFinishedCallback = std::function<void(std::string)>;
    using AsyncDownloadFinishedCallback =
        std::function<void(const std::vector<std::shared_ptr<FileFuture>>&)>;



    DownloadManager(std::string requestURL, int applicationVersion,
        bool useMultithreadedDownload = true);

    // callbacks happen on a different thread
    std::shared_ptr<FileFuture> downloadFile(const std::string& url, const ghoul::filesystem::File& file,
        bool overrideFile = true,
        DownloadFinishedCallback finishedCallback = DownloadFinishedCallback(),
        DownloadProgressCallback progressCallback = DownloadProgressCallback()
    );

    std::future<MemoryFile> fetchFile(
    const std::string& url,
    SuccessCallback successCallback = SuccessCallback(), ErrorCallback errorCallback = ErrorCallback());

    std::vector<std::shared_ptr<FileFuture>> downloadRequestFiles(const std::string& identifier,
        const ghoul::filesystem::Directory& destination, int version,
        bool overrideFiles = true,
        DownloadFinishedCallback finishedCallback = DownloadFinishedCallback(),
        DownloadProgressCallback progressCallback = DownloadProgressCallback()
    );

    void downloadRequestFilesAsync(const std::string& identifier,
        const ghoul::filesystem::Directory& destination, int version,
        bool overrideFiles, AsyncDownloadFinishedCallback callback
    );

    void getFileExtension(const std::string& url,
        RequestFinishedCallback finishedCallback = RequestFinishedCallback());

private:
    std::vector<std::string> _requestURL;
    int _applicationVersion;
    bool _useMultithreadedDownload;
};

} // namespace openspace

#endif // __DOWNLOADMANAGER_H__

*/