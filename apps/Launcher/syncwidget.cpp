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

#include "syncwidget.h"

#include "infowidget.h"

#include <openspace/openspace.h>

#include <ghoul/ghoul.h>
#include <ghoul/filesystem/filesystem.h>
#include <ghoul/filesystem/file.h>
#include <ghoul/misc/dictionary.h>
#include <ghoul/lua/ghoul_lua.h>

#include <QApplication>
#include <QCheckBox>
#include <QDir>
#include <QDirIterator>
#include <QFileInfo>
#include <QGridLayout>
#include <QGroupBox>
#include <QPushButton>
#include <QScrollArea>
#include <QString>
#include <QThread>
#include <QTimer>
#include <QVBoxLayout>

#include <libtorrent/entry.hpp>
#include <libtorrent/bencode.hpp>
#include <libtorrent/session.hpp>
#include <libtorrent/alert_types.hpp>

#include <future>

#include <fstream>
#include <fmt/format.h>

namespace {
    const std::string _loggerCat = "SyncWidget";

    const std::string _configurationFile = "Launcher.config";

    const int DownloadApplicationVersion = 1;

    const int nColumns = 3;

    const bool OverwriteFiles = false;
    const bool CleanInfoWidgets = true;
}

SyncWidget::SyncWidget(QWidget* parent, Qt::WindowFlags f) 
    : QWidget(parent, f)
    , _sceneLayout(nullptr)
    , _session(new libtorrent::session)
    , _threadPool(
        4,
        [](){}, [](){},
        ghoul::thread::ThreadPriorityClass::Idle,
        ghoul::thread::ThreadPriorityLevel::Lowest
    )
{
    setObjectName("SyncWidget");
    setFixedSize(500, 500);

    QBoxLayout* layout = new QVBoxLayout;
    layout->setContentsMargins(0, 0, 0, 0);
    {
        QGroupBox* sceneBox = new QGroupBox;
        _sceneLayout = new QGridLayout;
        sceneBox->setLayout(_sceneLayout);
        layout->addWidget(sceneBox);
    }
    {
        QPushButton* syncButton = new QPushButton("Synchronize Data");
        syncButton->setObjectName("SyncButton");
        QObject::connect(
            syncButton, SIGNAL(clicked(bool)),
            this, SLOT(syncButtonPressed())
        );

        layout->addWidget(syncButton);
    }

    {
        QScrollArea* area = new QScrollArea;
        area->setWidgetResizable(true);

        QWidget* w = new QWidget;
        w->setObjectName("DownloadArea");
        area->setWidget(w);

        _downloadLayout = new QVBoxLayout(w);
        _downloadLayout->setMargin(0);
        _downloadLayout->setSpacing(0);
        _downloadLayout->addStretch(100);

        layout->addWidget(area);
    }
    QPushButton* close = new QPushButton("Close");
    layout->addWidget(close, Qt::AlignRight);
    QObject::connect(
        close, SIGNAL(clicked(bool)),
        this, SLOT(close())
    );

    setLayout(layout);

    ghoul::initialize();
    openspace::DownloadManager::initialize();

    // Make use of the rest of the request urls
    std::vector<std::string> requestUrls = {
        "http://data.openspaceproject.com/request"
    };
    _downloadManager = std::make_unique<openspace::DownloadManager>(
        requestUrls, DownloadApplicationVersion);

    libtorrent::error_code ec;
    _session->listen_on(std::make_pair(20280, 20290), ec);

    libtorrent::session_settings settings = _session->settings();
    settings.user_agent =
        "OpenSpace/" +
        std::to_string(openspace::OPENSPACE_VERSION_MAJOR) + "." +
        std::to_string(openspace::OPENSPACE_VERSION_MINOR) + "." +
        std::to_string(openspace::OPENSPACE_VERSION_PATCH);
    settings.allow_multiple_connections_per_ip = true;
    settings.ignore_limits_on_local_network = true;
    settings.connection_speed = 20;
    settings.active_downloads = -1;
    settings.active_seeds = -1;
    settings.active_limit = 30;
    settings.dht_announce_interval = 60;

    if (ec) {
        LFATAL("Failed to open socket: " << ec.message());
        return;
    }
    _session->start_upnp();

    std::ifstream file(_configurationFile);
    if (!file.fail()) {
        union {
            uint32_t value;
            std::array<char, 4> data;
        } size;

        file.read(size.data.data(), sizeof(uint32_t));
        std::vector<char> buffer(size.value);
        file.read(buffer.data(), size.value);
        file.close();

        libtorrent::entry e = libtorrent::bdecode(buffer.begin(), buffer.end());
        _session->start_dht(e);
    }
    else 
        _session->start_dht();

    _session->add_dht_router({ "router.utorrent.com", 6881 });
    _session->add_dht_router({ "dht.transmissionbt.com", 6881 });
    _session->add_dht_router({ "router.bittorrent.com", 6881 });
    _session->add_dht_router({ "router.bitcomet.com", 6881 });

    QTimer* timer = new QTimer(this);
    QObject::connect(timer, SIGNAL(timeout()), this, SLOT(handleTimer()));
    timer->start(100);

    _mutex.clear();
}

SyncWidget::~SyncWidget() {
    libtorrent::entry dht = _session->dht_state();

    std::vector<char> buffer;
    libtorrent::bencode(std::back_inserter(buffer), dht);

    std::ofstream f(_configurationFile);

    union {
        uint32_t value;
        std::array<char, 4> data;
    } size;
    size.value = buffer.size();
    f.write(size.data.data(), sizeof(uint32_t));
    f.write(buffer.data(), buffer.size());

    ghoul::deinitialize();
    delete _session;
}

void SyncWidget::closeEvent(QCloseEvent* event) {
    std::vector<libtorrent::torrent_handle> handles = _session->get_torrents();
    for (libtorrent::torrent_handle h : handles) {
        h.flush_cache();
        _session->remove_torrent(h);
    }
}

void SyncWidget::setSceneFiles(QMap<QString, QString> sceneFiles) {
    _sceneFiles = std::move(sceneFiles);
    QStringList keys = _sceneFiles.keys();
    for (int i = 0; i < keys.size(); ++i) {
        const QString& sceneName = keys[i];

        QCheckBox* checkbox = new QCheckBox(sceneName);
        checkbox->setChecked(true);

        _sceneLayout->addWidget(checkbox, i / nColumns, i % nColumns);
    }
}

void SyncWidget::clear() {
    //for (std::shared_ptr<openspace::DownloadManager::FileFuture> f : _futures)
    //    f->abortDownload = true;

    using libtorrent::torrent_handle;
    for (QMap<torrent_handle, InfoWidget*>::iterator i = _torrentInfoWidgetMap.begin();
         i != _torrentInfoWidgetMap.end();
         ++i)
    {
        delete i.value();
    }
    _torrentInfoWidgetMap.clear();
    _session->abort();


    //_directFiles.clear();
    //_fileRequests.clear();
    //_torrentFiles.clear();
}

//void SyncWidget::handleDirectFiles() {
//    LDEBUG("Direct Files");
//    for (const DirectFile& f : _directFiles) {
//        LDEBUG(f.url.toStdString() << " -> " << f.destination.toStdString());
//
//        std::shared_ptr<openspace::DownloadManager::FileFuture> future = _downloadManager->downloadFile(
//            f.url.toStdString(),
//            absPath("${SCENE}/" + f.module.toStdString() + "/" + f.destination.toStdString()),
//            OverwriteFiles
//        );
//        if (future) {
//            InfoWidget* w = new InfoWidget(f.destination);
//            _downloadLayout->insertWidget(_downloadLayout->count() - 1, w);
//
//            _futures.push_back(future);
//            _futureInfoWidgetMap[future] = w;
//        }
//    }
//}
//
//void SyncWidget::handleFileRequest() {
//    LDEBUG("File Requests");
//    for (const FileRequest& f : _fileRequests) {
//        LDEBUG(f.identifier.toStdString() << " (" << f.version << ") -> " << f.destination.toStdString()); 
//
//        ghoul::filesystem::Directory d = FileSys.currentDirectory();
////        std::string thisDirectory = absPath("${SCENE}/" + f.module.toStdString() + "/");
//        FileSys.setCurrentDirectory(f.baseDir.toStdString());
//
//
//        std::string identifier =  f.identifier.toStdString();
//        std::string path = absPath(f.destination.toStdString());
//        int version = f.version;
//
//        _downloadManager->downloadRequestFilesAsync(
//            identifier,
//            path,
//            version,
//            OverwriteFiles,
//            std::bind(&SyncWidget::handleFileFutureAddition, this, std::placeholders::_1)
//        );
//
//        FileSys.setCurrentDirectory(d);
//    }
//}
//
//void SyncWidget::handleTorrentFiles() {
//    LDEBUG("Torrent Files");
//    for (const TorrentFile& f : _torrentFiles) {
//        LDEBUG(f.file.toStdString() << " -> " << f.destination.toStdString());
//
//        ghoul::filesystem::Directory d = FileSys.currentDirectory();
////        std::string thisDirectory = absPath("${SCENE}/" + f.module.toStdString() + "/");
//        FileSys.setCurrentDirectory(f.baseDir.toStdString());
////        FileSys.setCurrentDirectory(thisDirectory);
//
//        QString file = QString::fromStdString(absPath(f.file.toStdString()));
//
//        if (!QFileInfo(file).exists()) {
//            LERROR(file.toStdString() << " does not exist");
//            continue;
//        }
//
//        libtorrent::error_code ec;
//        libtorrent::add_torrent_params p;
//
//        //if (f.destination.isEmpty())
//            //p.save_path = absPath(fullPath(f.module, ".").toStdString());
//        //else
//            //p.save_path = 
//        p.save_path = absPath(f.destination.toStdString());
//
//        p.ti = new libtorrent::torrent_info(file.toStdString(), ec);
//        p.name = f.file.toStdString();
//        p.storage_mode = libtorrent::storage_mode_allocate;
//        p.auto_managed = true;
//        if (ec) {
//            LERROR(f.file.toStdString() << ": " << ec.message());
//            continue;
//        }
//        libtorrent::torrent_handle h = _session->add_torrent(p, ec);
//        if (ec) {
//            LERROR(f.file.toStdString() << ": " << ec.message());
//            continue;
//        }
//
//        if (_torrentInfoWidgetMap.find(h) == _torrentInfoWidgetMap.end()) {
//            QString fileString = f.file;
//            QString t = QString(".torrent");
//            fileString.replace(fileString.indexOf(t), t.size(), "");
//
//            fileString = f.module + "/" + fileString;
//
//            InfoWidget* w = new InfoWidget(fileString, h.status().total_wanted);
//            _downloadLayout->insertWidget(_downloadLayout->count() - 1, w);
//            _torrentInfoWidgetMap[h] = w;
//        }
//
//        FileSys.setCurrentDirectory(d);
//    }
//}

void SyncWidget::syncButtonPressed() {
    using DlManager = openspace::DownloadManager;

    clear();

    std::vector<std::string> scenes;
    QStringList list = selectedScenes();
    std::transform(
        list.begin(),
        list.end(),
        std::back_inserter(scenes),
        [](const QString& scene) { return scene.toStdString(); }
    );

    DownloadCollection::Collection collection = DownloadCollection::crawlScenes(scenes);

    std::vector<DlManager::FileTask> result;


    LDEBUG("Direct Files");
    for (const DownloadCollection::DirectFile& df : collection.directFiles) {
        LDEBUG(df.url + " -> " + df.destination);

        InfoWidget* w = new InfoWidget(QString::fromStdString(df.destination));
        _downloadLayout->insertWidget(_downloadLayout->count() - 1, w);

        result.push_back(
            DlManager::download(
                df.url,
                df.destination,
                0,
                [this, w](DlManager::File& f, size_t currentSize, size_t totalSize) {
                    std::lock_guard<std::mutex> lock(_updateInformationMutex);
                    _updateInformation.push_back({
                        w,
                        f.errorMessage,
                        currentSize,
                        totalSize
                    });
                }
            )
        );
    }

    LDEBUG("File Requests");
    for (const DownloadCollection::FileRequest& fr : collection.fileRequests) {
        LDEBUG(fmt::format("{}({}) -> {}", fr.identifier, fr.identifier, fr.destination));

        InfoWidget* w = new InfoWidget(QString::fromStdString(fr.destination));
        _downloadLayout->insertWidget(_downloadLayout->count() - 1, w);

        std::vector<DlManager::FileTask> tasks = _downloadManager->requestFiles(
            fr.identifier,
            fr.version,
            fr.destination,
            DlManager::OverrideFiles::Yes,
            [this, w](DlManager::File& f, size_t currentSize, size_t totalSize) {
                std::lock_guard<std::mutex> lock(_updateInformationMutex);
                _updateInformation.push_back({
                    w,
                    f.errorMessage,
                    currentSize,
                    totalSize
                });
            }
        );

        std::move(tasks.begin(), tasks.end(), std::back_inserter(result));
    }

    LDEBUG("Torrent Files");
    for (const DownloadCollection::TorrentFile& tf : collection.torrentFiles) {
        LDEBUG(tf.file + " -> " + tf.destination);
        if (!FileSys.fileExists(tf.file)) {
            LERROR(fmt::format("Torrent file '{}' did not exist", tf.file));
            continue;
        }

        libtorrent::error_code ec;
        libtorrent::add_torrent_params p;

        p.save_path = tf.destination;

        //p.save_path = absPath(f.destination.toStdString());

        p.ti = new libtorrent::torrent_info(tf.file, ec);
        p.name = tf.file;
        p.storage_mode = libtorrent::storage_mode_allocate;
        p.auto_managed = true;
        if (ec) {
            LERROR(tf.file << ": " << ec.message());
            continue;
        }
        libtorrent::torrent_handle h = _session->add_torrent(p, ec);
        if (ec) {
            LERROR(tf.file << ": " << ec.message());
            continue;
        }

        if (_torrentInfoWidgetMap.find(h) == _torrentInfoWidgetMap.end()) {
            //QString fileString = QString::fromStdString(tf.file);
            //QString t = QString(".torrent");
            //fileString.replace(fileString.indexOf(t), t.size(), "");

            //fileString = tf. f.module + "/" + fileString;

            InfoWidget* w = new InfoWidget(
                QString::fromStdString(tf.file),
                h.status().total_wanted
            );
            _downloadLayout->insertWidget(_downloadLayout->count() - 1, w);
            _torrentInfoWidgetMap[h] = w;
        }

    }

    for (openspace::DownloadManager::FileTask& t : result) {
        _threadPool.queue(std::move(t));
    }
}

QStringList SyncWidget::selectedScenes() const {
    QStringList result;
    int nChildren = _sceneLayout->count();
    for (int i = 0; i < nChildren; ++i) {
        QWidget* w = _sceneLayout->itemAt(i)->widget();
        QCheckBox* c = static_cast<QCheckBox*>(w);
        if (c->isChecked()) {
            QString t = c->text();
            result.append(_sceneFiles[t]);
        }
    }
    std::string scenes;
    for (QString s : result)
        scenes += s.toStdString() + "; ";
    LDEBUG("Downloading scenes: " << scenes);
    return result;
}

void SyncWidget::handleTimer() {
    using namespace libtorrent;
    
    //std::vector<UpdateInformation> updateInformation;
    //{
    //    std::lock_guard<std::mutex> lock(_updateInformationMutex);
    //    updateInformation = _updateInformation;
    //    _updateInformation.clear();
    //}

    //std::stable_sort(
    //    updateInformation.begin(),
    //    updateInformation.end(),
    //    [](const UpdateInformation& lhs, const UpdateInformation& rhs) {
    //        return lhs.widget < rhs.widget;
    //    }
    //);
    //updateInformation.erase(
    //    std::unique(
    //        updateInformation.begin(),
    //        updateInformation.end(),
    //        [](const UpdateInformation& lhs, const UpdateInformation& rhs) {
    //            return lhs.widget == rhs.widget;
    //        }
    //    ),
    //    updateInformation.end()
    //);


    //for (const UpdateInformation& info : updateInformation) {
    //    info.widget->update(info.currentSize, info.totalSize, info.errorMessage);
    //}



    
    
    //using FileFuture = openspace::DownloadManager::FileFuture;

    //std::vector<std::shared_ptr<FileFuture>> toRemove;
    //for (std::shared_ptr<FileFuture> f : _futures) {
    //    InfoWidget* w = _futureInfoWidgetMap[f];

    //    if (CleanInfoWidgets && (f->isFinished || f->isAborted)) {
    //        toRemove.push_back(f);
    //        _downloadLayout->removeWidget(w);
    //        _futureInfoWidgetMap.erase(f);
    //        delete w;
    //    }
    //    else
    //        w->update(f);
    //}

    //for (std::shared_ptr<FileFuture> f : toRemove) {
    //    _futures.erase(std::remove(_futures.begin(), _futures.end(), f), _futures.end()); 
    //}

    //while (_mutex.test_and_set()) {}
    //for (std::shared_ptr<FileFuture> f : _futuresToAdd) {
    //    InfoWidget* w = new InfoWidget(QString::fromStdString(f->filePath), -1);
    //    _downloadLayout->insertWidget(_downloadLayout->count() - 1, w);

    //    _futureInfoWidgetMap[f] = w;
    //    _futures.push_back(f);
    //}
    //_futuresToAdd.clear();
    //_mutex.clear();


    //std::vector<torrent_handle> handles = _session->get_torrents();
    //for (torrent_handle h : handles) {
    //    torrent_status s = h.status();
    //    InfoWidget* w = _torrentInfoWidgetMap[h];

    //    if (w)
    //        w->update(s);

    //    if (CleanInfoWidgets && (s.state == torrent_status::finished || s.state == torrent_status::seeding)) {
    //        _torrentInfoWidgetMap.remove(h);
    //        delete w;
    //    }
    //}

    //// Only close every torrent if all torrents are finished
    //bool allSeeding = true;
    //for (torrent_handle h : handles) {
    //    torrent_status s = h.status();
    //    allSeeding &= (s.state == torrent_status::seeding);
    //}

    //if (allSeeding) {
    //    for (torrent_handle h : handles)
    //        _session->remove_torrent(h);
    //}
}

//void SyncWidget::handleFileFutureAddition(
//    const std::vector<std::shared_ptr<openspace::DownloadManager::FileFuture>>& futures)
//{
//    while (_mutex.test_and_set()) {}
//    _futuresToAdd.insert(_futuresToAdd.end(), futures.begin(), futures.end());
//    _mutex.clear();
//}

