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

#include <openspace/engine/downloadmanager.h>

#include <ghoul/filesystem/filesystem.h>
#include <ghoul/misc/assert.h>

#include <random>

 #ifdef OPENSPACE_CURL_ENABLED
 #include <curl/curl.h>
 #endif

namespace {
    template <typename Target, typename Callback>
    struct ProgressInformation {
        Target& file;
        Callback callback;
    };

    using MemoryProgressInformation = ProgressInformation<
        openspace::DownloadManager::MemoryFile,
        openspace::DownloadManager::ProgressCallbackMemory
    >;

    using FileProgressInformation = ProgressInformation<
        openspace::DownloadManager::File,
        openspace::DownloadManager::ProgressCallbackFile
    >;

    size_t writeMemoryCallback(void* contents, size_t size, size_t nmemb, void* userp) {
        using MemoryFile = openspace::DownloadManager::MemoryFile;

        size_t realsize = size * nmemb;
        MemoryFile* mem = reinterpret_cast<MemoryFile*>(userp);

        size_t previousSize = mem->buffer.size();
        mem->buffer.resize(mem->buffer.size() + realsize);
        std::memcpy(
            mem->buffer.data() + previousSize,
            contents,
            realsize
        );

        return realsize;
    }

    size_t writeFileCallback(void* contents, size_t size, size_t nmemb, FILE* stream) {
        return fwrite(contents, size, nmemb, stream);
    }

    // Some duck-typing
    template <typename Progress>
    int xferinfo(void* ptr, curl_off_t dltotal, curl_off_t dlnow, curl_off_t ultotal,
                 curl_off_t ulnow)
    {
        if (dltotal == 0) {
            return 0;
        }

        ghoul_assert(ptr, "Passed progress information is nullptr");
        Progress* progress = static_cast<Progress*>(ptr);
        ghoul_assert(progress, "Passed pointer is not a ProgressInformation");

        if (progress->callback) {
            progress->callback(progress->file, dlnow, dltotal);
        }
        return 0;
    }

    std::vector<std::string> extractLinesFromRequest(std::string reqFile) {
        std::vector<std::string> res;

        std::string line;
        std::stringstream ss(reqFile);
        while (std::getline(ss, line)) {
            res.push_back(line);
        }

        return res;
    }

    std::string fileNameFromUrl(std::string fileUrl) {
        size_t pos = fileUrl.find_last_of('/');
        return fileUrl.substr(pos + 1);
    }


} // namespace

namespace openspace {

DownloadManager::DownloadException::DownloadException(std::string msg)
    : ghoul::RuntimeError(std::move(msg), "DownloadManager")
{}

std::string DownloadManager::fileExtension(const std::string& contentType) {
    std::stringstream ss(contentType);
    std::string extension;
    std::getline(ss, extension, '/');
    std::getline(ss, extension);
    return extension;
}

void DownloadManager::initialize() {
    curl_global_init(CURL_GLOBAL_ALL);
}

void DownloadManager::deinitialize() {}

DownloadManager::MemoryFileTask DownloadManager::download(const std::string& url,
                                                          int64_t identifier,
                                                          ProgressCallbackMemory progress)
{
    return MemoryFileTask(
        [url, identifier, progress]() {
        
        MemoryFile file;
        file.identifier = identifier;

        CURL* curl = curl_easy_init();
        if (!curl) {
            file.errorMessage = "Could not initialize cURL library";
            return std::move(file);
        }

        curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
        curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, reinterpret_cast<void*>(&file));
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, writeMemoryCallback);
        curl_easy_setopt(curl, CURLOPT_TIMEOUT, 5L);
        // Will fail when response status is 400 or above
        curl_easy_setopt(curl, CURLOPT_FAILONERROR, 1L);

        // Only set the progress reporting if the user specified a progress callback
        MemoryProgressInformation p{
            file,
            std::move(progress)
        };
        if (p.callback) {
            curl_easy_setopt(curl, CURLOPT_XFERINFOFUNCTION, xferinfo<MemoryProgressInformation>);
            curl_easy_setopt(curl, CURLOPT_XFERINFODATA, &p);
            curl_easy_setopt(curl, CURLOPT_NOPROGRESS, 0L);
        }

        CURLcode res = curl_easy_perform(curl);
        if (res == CURLE_OK) {
            // ask for the content-type
            char* ct;
            res = curl_easy_getinfo(curl, CURLINFO_CONTENT_TYPE, &ct);
            if (res == CURLE_OK) {
                file.contentType = std::string(ct);
            }
            else {
                file.errorMessage = curl_easy_strerror(res);
            }
        }
        else {
            file.errorMessage = curl_easy_strerror(res);
        }
        curl_easy_cleanup(curl);

        return std::move(file);
    });
}

DownloadManager::MemoryFile DownloadManager::downloadSync(const std::string& url,
    int64_t identifier, ProgressCallbackMemory progress) 
{
    MemoryFileTask task = download(url, identifier, progress);
    std::future<MemoryFile> future = task.get_future();
    task();
    return future.get();
}

DownloadManager::FileTask DownloadManager::download(const std::string& url,
                                                    const std::string& filename,
                                                    int64_t identifier,
                                                    ProgressCallbackFile progress)
{
    return FileTask(
        [url, filename, identifier, progress](){
        
        FileSys.createDirectory(
            ghoul::filesystem::File(filename).directoryName(),
            ghoul::filesystem::FileSystem::Recursive::Yes
        );

        File file;
        file.identifier = identifier;
        file.filename = std::move(filename);


        FILE* fp = fopen(file.filename.c_str(), "wb");

        CURL* curl = curl_easy_init();
        if (!curl) {
            file.errorMessage = "Could not initialize cURL library";
            return std::move(file);
        }

        curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
        curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, fp);
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, writeFileCallback);

        FileProgressInformation p{
            file,
            std::move(progress)
        };

        if (p.callback) {
            curl_easy_setopt(curl, CURLOPT_XFERINFOFUNCTION, xferinfo<FileProgressInformation>);
            curl_easy_setopt(curl, CURLOPT_XFERINFODATA, &p);
            curl_easy_setopt(curl, CURLOPT_NOPROGRESS, 0L);
        }
    
        CURLcode res = curl_easy_perform(curl);
        if (res == CURLE_OK) {
            // ask for the content-type
            char* ct;
            res = curl_easy_getinfo(curl, CURLINFO_CONTENT_TYPE, &ct);
            if (res == CURLE_OK) {
                if (ct != nullptr) {
                    file.contentType = std::string(ct);
                }
            }
            else {
                file.errorMessage = curl_easy_strerror(res);
            }
        }
        else {
            file.errorMessage = curl_easy_strerror(res);
        }
        curl_easy_cleanup(curl);
        fclose(fp);

        return std::move(file);
    });
}

DownloadManager::File DownloadManager::downloadSync(const std::string& url,
    const std::string& filename, int64_t identifier, ProgressCallbackFile progress)
{
    FileTask task = download(url, filename, identifier, progress);
    std::future<File> future = task.get_future();
    task();
    return future.get();
}

DownloadManager::DownloadManager(std::vector<std::string> requestUrls, int appVersion)
    : _requestUrls(std::move(requestUrls))
    , _applicationVersion(appVersion) 
{}


std::vector<DownloadManager::FileTask> DownloadManager::requestFiles(
    const std::string& identifier,
    int version,
    const ghoul::filesystem::Directory& destination,
    OverrideFiles overrideFiles,
    ProgressCallbackFile progress) const
{

    std::string url = selectRequestUrl();
    std::string req = request(url, identifier, version);

    // This should be changed to an asynchronous call when C++ std::future .then is
    // implemented
    MemoryFile reqFile = downloadSync(req);

    if (!reqFile.errorMessage.empty()) {
        throw DownloadException(reqFile.errorMessage);
    }
    if (reqFile.contentType != "text/plain") {
        throw DownloadException("Wrong content type for request: " + reqFile.contentType);
    }

    std::vector<std::string> fileUrls = extractLinesFromRequest(
        std::string(reqFile.buffer.begin(), reqFile.buffer.end())
    );

    std::vector<FileTask> result;
    for (const std::string& fileUrl : fileUrls) {
        std::string file = fileNameFromUrl(fileUrl);
        std::string fullPath = FileSys.pathByAppendingComponent(destination, file);

        result.push_back(
            download(fileUrl, fullPath, -1, progress)
        );
    }

    return result;
}

std::string DownloadManager::request(const std::string& url, const std::string& identfier, int version) const {
    const std::string RequestIdentifier = "identifier";
    const std::string RequestFileVersion = "file_version";
    const std::string RequestApplicationVersion = "application_version";

    return url +
        "?" + RequestIdentifier + "=" + identfier +
        "&" + RequestFileVersion + "=" + std::to_string(version) +
        "&" + RequestApplicationVersion + "=" + std::to_string(_applicationVersion)
    ;
}

std::string DownloadManager::selectRequestUrl() const {
    int n = _requestUrls.size();
    if (n == 1) {
        return _requestUrls.front();
    }

    // Generate a random number uniformly distributed between [0, n-1]
    int i = std::uniform_int_distribution<>(0, n-1)(std::mt19937(std::random_device()()));
    return _requestUrls[i];
}


} // namespace openspace

//std::vector<std::shared_ptr<DownloadManager::FileFuture>> DownloadManager::downloadRequestFiles(
//    const std::string& identifier, const ghoul::filesystem::Directory& destination,
//    int version, bool overrideFiles, DownloadFinishedCallback finishedCallback,
//    DownloadProgressCallback progressCallback)
//{
//    std::vector<std::shared_ptr<FileFuture>> futures;
//    FileSys.createDirectory(destination, ghoul::filesystem::FileSystem::Recursive::Yes);
//    // TODO: Check s ---abock
//    // TODO: Escaping is necessary ---abock
//    const std::string fullRequest =_requestURL.back() + "?" +
//        RequestIdentifier + "=" + identifier + "&" +
//        RequestFileVersion + "=" + std::to_string(version) + "&" +
//        RequestApplicationVersion + "=" + std::to_string(_applicationVersion);
//    LDEBUG("Request: " << fullRequest);
//
//    std::string requestFile = absPath("${TEMPORARY}/" + identifier);
//    LDEBUG("Request File: " << requestFile);
//
//    bool isFinished = false;
//    auto callback = [&](const FileFuture& f) {
//        LDEBUG("Finished: " << requestFile);
//        std::ifstream temporary(requestFile);
//        std::string line;
//        int nFiles = 0;
//        while (std::getline(temporary, line)) {
//            ++nFiles;
//#ifdef __APPLE__
//            // @TODO: Fix this so that the ifdef is not necessary anymore ---abock
//          std::string file = ghoul::filesystem::File(line, ghoul::filesystem::File::RawPath::Yes).filename();
//#else
//            std::string file = ghoul::filesystem::File(line).filename();
//#endif
//
//            LDEBUG("\tLine: " << line << " ; Dest: " << destination.path() + "/" + file);
//
//            std::shared_ptr<FileFuture> future = downloadFile(
//                line,
//                destination.path() + "/" + file,
//                overrideFiles,
//                [](const FileFuture& f) { LDEBUG("Finished: " << f.filePath); }
//            );
//            if (future)
//                futures.push_back(future);
//        }
//        isFinished = true;
//    };
//    
//    std::shared_ptr<FileFuture> f = downloadFile(
//        fullRequest,
//        requestFile,
//        true,
//        callback
//    );
//
//    while (!isFinished) {}
//
//    return futures;
//}
//
//void DownloadManager::downloadRequestFilesAsync(const std::string& identifier,
//    const ghoul::filesystem::Directory& destination, int version, bool overrideFiles,
//    AsyncDownloadFinishedCallback callback)
//{
//    auto downloadFunction = [this, identifier, destination, version, overrideFiles, callback](){
//        std::vector<std::shared_ptr<FileFuture>> f = downloadRequestFiles(
//            identifier,
//            destination,
//            version,
//            overrideFiles
//        );
//        
//        callback(f);
//    };
//    
//    if (_useMultithreadedDownload) {
//        using namespace ghoul::thread;
//        std::thread t = std::thread(downloadFunction);
//        ghoul::thread::setPriority(
//            t, ThreadPriorityClass::Idle, ThreadPriorityLevel::Lowest
//        );
//        t.detach();
//    }
//    else
//        downloadFunction();
//}

