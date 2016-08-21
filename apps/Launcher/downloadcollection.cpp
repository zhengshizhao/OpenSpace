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

#include "downloadcollection.h"

#include <ghoul/filesystem/filesystem.h>
#include <ghoul/logging/logmanager.h>
#include <ghoul/lua/ghoul_lua.h>
#include <ghoul/misc/assert.h>
#include <ghoul/misc/dictionary.h>

#include <algorithm>
#include <fmt/format.h>
#include <functional>
#include <unordered_set>

namespace {
    struct ModuleInformation {
        std::string name;
        std::string dataFile;
    };

    const std::string _loggerCat = "DownloadCollection";

    const std::string UrlKey = "URL";
    const std::string FileKey = "File";
    const std::string DestinationKey = "Destination";
    const std::string IdentifierKey = "Identifier";
    const std::string VersionKey = "Version";

    const std::string FileDownloadKey = "FileDownload";
    const std::string FileRequestKey = "FileRequest";
    const std::string TorrentFilesKey = "TorrentFiles";
} // namespace

using std::string;
using std::vector;

namespace std {

template <>
struct hash<DownloadCollection::DirectFile> {
    size_t operator()(const DownloadCollection::DirectFile& key) const {

        return hash<std::string>()(
            key.module + "|" + key.url + "|" + key.destination + "|" + key.baseDir
        );
    }
};

template <>
struct hash<DownloadCollection::FileRequest> {
    size_t operator()(const DownloadCollection::FileRequest& key) const {

        return hash<std::string>()(
            key.module + "|" + key.identifier + "|" + key.destination + "|" + 
            key.baseDir + "|" + to_string(key.version)
        );
    }
};

template <>
struct hash<DownloadCollection::TorrentFile> {
    size_t operator()(const DownloadCollection::TorrentFile& key) const {

        return hash<std::string>()(
            key.module + "|" + key.file + "|" + key.destination + "|" + key.baseDir
        );
    }
};

} // namespace std

bool operator==(const DownloadCollection::DirectFile& lhs,
                const DownloadCollection::DirectFile rhs)
{
    return (
        (lhs.module == rhs.module) &&
        (lhs.url == rhs.url) &&
        (lhs.destination == rhs.destination) &&
        (lhs.baseDir == rhs.baseDir)
    );
}

bool operator==(const DownloadCollection::FileRequest& lhs,
                const DownloadCollection::FileRequest& rhs) 
{
    return (
        (lhs.module == rhs.module) &&
        (lhs.identifier == rhs.identifier) &&
        (lhs.destination == rhs.destination) &&
        (lhs.baseDir == rhs.baseDir) &&
        (lhs.version == rhs.version)
    );
}

bool operator==(const DownloadCollection::TorrentFile& lhs,
                const DownloadCollection::TorrentFile& rhs) 
{
    return (
        (lhs.module == rhs.module) &&
        (lhs.file == rhs.file) &&
        (lhs.destination == rhs.destination) &&
        (lhs.baseDir == rhs.baseDir)
    );
}

ModuleInformation commonModule() {
    return{
        "common",
        "common/common.data"
    };
}

vector<ModuleInformation> crawlModule(const string& module, const string& scenePath) {
    vector<ModuleInformation> result;

    string shortModule;
    string::size_type pos = module.find_last_of(FileSys.PathSeparator);
    if (pos != string::npos) {
        shortModule = module.substr(pos + 1);
    }
    else {
        shortModule = module;
    }

    string dataFile = FileSys.pathByAppendingComponent(
        scenePath,
        FileSys.pathByAppendingComponent(module, shortModule) + ".data"
    );
        

    if (FileSys.fileExists(dataFile)) {
        result.push_back({
            module,
            dataFile
        });
    }
    else {
        // A direct data file does not exist, so it might be a meta module, containing
        // other modules

        using namespace ghoul::filesystem;

        Directory dir(module);
        vector<string> files = dir.readFiles(Directory::Recursive::Yes);
        std::remove_if(
            files.begin(), files.end(),
            [](const string& file) {
            return File(file).fileExtension() != "data";
        }
        );

        std::transform(
            files.begin(), files.end(),
            std::back_inserter(result),
            [](const string& file) -> ModuleInformation {
            return {
                Directory(file).path(),
                File(file).filename(),
            };
        }
        );
    }

    return result;
}

std::unordered_set<DownloadCollection::DirectFile> extractDirectDownloads(
    const ModuleInformation& module)
{
    ghoul_assert(FileSys.fileExists(module.dataFile), "Datafile did not exist");
    std::unordered_set<DownloadCollection::DirectFile> result;

    ghoul::Dictionary data;
    ghoul::lua::loadDictionaryFromFile(module.dataFile, data);

    if (data.hasKey(FileDownloadKey) && !data.hasValue<ghoul::Dictionary>(FileDownloadKey)) {
        throw std::runtime_error(FileDownloadKey + " is not a table");
    }

    if (!data.hasKeyAndValue<ghoul::Dictionary>(FileDownloadKey)) {
        return {};
    }

    ghoul::Dictionary files = data.value<ghoul::Dictionary>(FileDownloadKey);
    for (int i = 1; i <= files.size(); ++i) {
        if (!data.hasKeyAndValue<ghoul::Dictionary>(std::to_string(i))) {
            LERROR(
                fmt::format(
                    "Index '{}' in table '{}' in file '{}' is not a table",
                    i, FileDownloadKey, module.dataFile
                )
            );
            continue;
        }

        ghoul::Dictionary d = data.value<ghoul::Dictionary>(std::to_string(i));
        if (!d.hasKeyAndValue<std::string>(UrlKey)) {
            LERROR(
                fmt::format(
                    "Index '{}' in table '{}' in file '{}' does not have key {}",
                    i, FileDownloadKey, module.dataFile, UrlKey
                )
            );
            continue;
        }
        string url = d.value<string>(UrlKey);

        string destination = ".";
        if (d.hasKeyAndValue<string>(DestinationKey)) {
            destination = d.value<string>(DestinationKey);
        }

        result.insert({
            module.name,
            std::move(url),
            std::move(destination),
            ghoul::filesystem::File(module.dataFile).directoryName()
        });
    }

    return result;
}

std::unordered_set<DownloadCollection::FileRequest> extractRequests(
    const ModuleInformation& module)
{
    ghoul_assert(FileSys.fileExists(module.dataFile), "Datafile did not exist");
    std::unordered_set<DownloadCollection::FileRequest> result;

    ghoul::Dictionary data;
    ghoul::lua::loadDictionaryFromFile(module.dataFile, data);

    if (data.hasKey(FileRequestKey) && !data.hasValue<ghoul::Dictionary>(FileRequestKey)) {
        throw std::runtime_error(FileDownloadKey + " is not a table");
    }

    if (!data.hasKeyAndValue<ghoul::Dictionary>(FileRequestKey)) {
        return {};
    }

    ghoul::Dictionary files = data.value<ghoul::Dictionary>(FileRequestKey);
    for (int i = 1; i <= files.size(); ++i) {
        if (!data.hasKeyAndValue<ghoul::Dictionary>(std::to_string(i))) {
            LERROR(
                fmt::format(
                    "Index '{}' in table '{}' in file '{}' is not a table", 
                    i, FileRequestKey, module.dataFile
                )
            );
            continue;
        }

        // Check for identifier key
        ghoul::Dictionary d = data.value<ghoul::Dictionary>(std::to_string(i));
        if (!d.hasKeyAndValue<string>(IdentifierKey)) {
            LERROR(
                fmt::format(
                    "Index '{}' in table '{}' in file '{}' does not have key {}",
                    i, FileRequestKey, module.dataFile, IdentifierKey
                )
            );
            continue;
        }
        // Check for version key
        if (!d.hasKeyAndValue<double>(VersionKey)) {
            LERROR(
                fmt::format(
                    "Index '{}' in table '{}' in file '{}' does not have key {}",
                    i, FileRequestKey, module.dataFile, VersionKey
                )
            );
            continue;
        }

        string identifier = d.value<string>(IdentifierKey);
        string destination = ".";
        if (d.hasKeyAndValue<string>(DestinationKey)) {
            destination = d.value<string>(DestinationKey);
        }
        int version = static_cast<int>(d.value<double>(VersionKey));

        result.insert({
            module.name,
            std::move(identifier),
            std::move(destination),
            ghoul::filesystem::File(module.dataFile).directoryName(),
            version
        });
    }

    return result;
}

std::unordered_set<DownloadCollection::TorrentFile> extractTorrents(
    const ModuleInformation& module)
{
    ghoul_assert(FileSys.fileExists(module.dataFile), "Datafile did not exist");
    std::unordered_set<DownloadCollection::TorrentFile> result;

    ghoul::Dictionary data;
    ghoul::lua::loadDictionaryFromFile(module.dataFile, data);

    if (data.hasKey(TorrentFilesKey) && !data.hasValue<ghoul::Dictionary>(TorrentFilesKey)) {
        throw std::runtime_error(TorrentFilesKey + " is not a table");
    }

    if (!data.hasKeyAndValue<ghoul::Dictionary>(TorrentFilesKey)) {
        return {};
    }

    ghoul::Dictionary files = data.value<ghoul::Dictionary>(TorrentFilesKey);
    for (int i = 1; i <= files.size(); ++i) {
        if (!data.hasKeyAndValue<ghoul::Dictionary>(std::to_string(i))) {
            LERROR(
                fmt::format(
                    "Index '{}' in table '{}' in file '{}' is not a table",
                    i, TorrentFilesKey, module.dataFile
                )
            );
            continue;
        }

        // Check for file key
        ghoul::Dictionary d = data.value<ghoul::Dictionary>(std::to_string(i));
        if (!d.hasKeyAndValue<string>(FileKey)) {
            LERROR(
                fmt::format(
                    "Index '{}' in table '{}' in file '{}' does not have key {}",
                    i, TorrentFilesKey, module.dataFile, FileKey
                )
            );
            continue;
        }

        string file = d.value<std::string>(FileKey);

        string destination = ".";
        if (d.hasKeyAndValue<string>(DestinationKey)) {
            destination = d.value<string>(DestinationKey);
        }

        result.insert({
            module.name,
            std::move(file),
            std::move(destination),
            ghoul::filesystem::File(module.dataFile).directoryName()
        });
    }

    return result;
}

void crawlScene(const std::string& scene,
                std::unordered_set<DownloadCollection::DirectFile> directFiles,
                std::unordered_set<DownloadCollection::FileRequest> fileRequests,
                std::unordered_set<DownloadCollection::TorrentFile> torrentFiles) 
{
    std::vector<ModuleInformation> modulesList;

    ghoul::Dictionary sceneDictionary;
    ghoul::lua::loadDictionaryFromFile(scene, sceneDictionary);

    std::string scenePath = ".";
    if (sceneDictionary.hasKeyAndValue<std::string>("ScenePath")) {
        scenePath = sceneDictionary.value<std::string>("ScenePath");
    }

    std::string fullScenePath = FileSys.pathByAppendingComponent(
        ghoul::filesystem::File(scene).directoryName(),
        scenePath
    );

    ghoul::Dictionary modules = sceneDictionary.value<ghoul::Dictionary>("Modules");

    for (int i = 1; i <= modules.size(); ++i) {
        // This could be a composite module name
        string module = modules.value<string>(std::to_string(i));
        std::vector<ModuleInformation> info = crawlModule(module, fullScenePath);
        modulesList.insert(modulesList.end(), info.begin(), info.end());
    }

    modulesList.push_back(commonModule());

    // We store the values as sets to prevent duplicates
    // Get all of the file information from all modules
    for (const ModuleInformation& module : modulesList) {
        std::unordered_set<DownloadCollection::DirectFile> df = extractDirectDownloads(
            module
        );
        directFiles.insert(df.begin(), df.end());

        std::unordered_set<DownloadCollection::FileRequest> fr = extractRequests(module);
        fileRequests.insert(fr.begin(), fr.end());

        std::unordered_set<DownloadCollection::TorrentFile> tf = extractTorrents(module);
        torrentFiles.insert(tf.begin(), tf.end());
    }
}

DownloadCollection::Collection DownloadCollection::crawlScenes(
    const vector<string>& scenes) 
{
    std::unordered_set<DirectFile> directFiles;
    std::unordered_set<FileRequest> fileRequests;
    std::unordered_set<TorrentFile> torrentFiles;
    
    for (const std::string& scene : scenes) {
        crawlScene(scene, directFiles, fileRequests, torrentFiles);
    }

    return {
        std::vector<DirectFile>(directFiles.begin(), directFiles.end()),
        std::vector<FileRequest>(fileRequests.begin(), fileRequests.end()),
        std::vector<TorrentFile>(torrentFiles.begin(), torrentFiles.end())
    };
}
