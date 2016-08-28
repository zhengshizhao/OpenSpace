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
    using libtorrent::torrent_handle;
    for (QMap<torrent_handle, InfoWidget*>::iterator i = _torrentInfoWidgetMap.begin();
         i != _torrentInfoWidgetMap.end();
         ++i)
    {
        delete i.value();
    }

    _updateInformation.clear();
    _finishedInformation.clear();

    _torrentInfoWidgetMap.clear();
    _session->abort();
}

void SyncWidget::syncButtonPressed() {
    using DlManager = openspace::DownloadManager;

    auto downloadFile = [this](std::string url, std::string destination, InfoWidget* w) {
        return DlManager::download(
            url,
            destination,
            0,
            [this, w](DlManager::File& f, size_t currentSize, size_t totalSize) {
                std::lock_guard<std::mutex> lock(_updateInformationMutex);
                _updateInformation[w] = {
                    f.errorMessage,
                    currentSize,
                    totalSize
                };
            },
            [this, w](DlManager::File& f) {
                std::lock_guard<std::mutex> lock(_updateInformationMutex);
                _finishedInformation.push_back(w);
            }
        );
    };


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

        result.push_back(downloadFile(df.url, df.destination, w));
    }

    LDEBUG("File Requests");
    for (const DownloadCollection::FileRequest& fr : collection.fileRequests) {
        LDEBUG(fmt::format("{}({}) -> {}", fr.identifier, fr.identifier, fr.destination));

        std::vector<std::string> urls = _downloadManager->requestFiles(
            fr.identifier,
            fr.version
        );

        for (const std::string& url : urls) {
            std::string file = url.substr(url.find_last_of('/') + 1);

            InfoWidget* w = new InfoWidget(QString::fromStdString(file));
            _downloadLayout->insertWidget(_downloadLayout->count() - 1, w);

            result.push_back(downloadFile(
                url,
                FileSys.pathByAppendingComponent(fr.destination, file),
                w
            ));
        }
    }

    LDEBUG("Torrent Files");
    for (const DownloadCollection::TorrentFile& tf : collection.torrentFiles) {
        LDEBUG(tf.file + " -> " + tf.destination);

        ghoul::filesystem::Directory d = FileSys.currentDirectory();
        FileSys.setCurrentDirectory(tf.destination);
        
        std::string fullFile = absPath(tf.file);
        std::string fullDestination = absPath(tf.destination);
        
        FileSys.setCurrentDirectory(d);

        if (!FileSys.fileExists(fullFile)) {
            LERROR(fmt::format("Torrent file '{}' did not exist", fullFile));
            continue;
        }

        libtorrent::error_code ec;
        libtorrent::add_torrent_params p;

        p.save_path = fullDestination;

        p.ti = new libtorrent::torrent_info(fullFile, ec);
        p.name = tf.file;
        p.storage_mode = libtorrent::storage_mode_allocate;
        p.auto_managed = true;
        if (ec) {
            LERROR(fullFile << ": " << ec.message());
            continue;
        }
        libtorrent::torrent_handle h = _session->add_torrent(p, ec);
        if (ec) {
            LERROR(fullFile << ": " << ec.message());
            continue;
        }

        if (_torrentInfoWidgetMap.find(h) == _torrentInfoWidgetMap.end()) {
            InfoWidget* w = new InfoWidget(
                QString::fromStdString(fullFile),
                h.status().total_wanted
            );
            _downloadLayout->insertWidget(_downloadLayout->count() - 1, w);
            _torrentInfoWidgetMap[h] = w;
        }

    }

    for (openspace::DownloadManager::FileTask& t : result) {
        std::thread th(std::move(t));
        th.detach();
    }

    //for (openspace::DownloadManager::FileTask& t : result) {
    //    _threadPool.queue(std::move(t));
    //}
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
    
    std::map<InfoWidget*, UpdateInformation> updateInformation;
    std::vector<InfoWidget*> finishedInformation;
    {
        std::lock_guard<std::mutex> lock(_updateInformationMutex);

        updateInformation = _updateInformation;
        _updateInformation.clear();

        finishedInformation = _finishedInformation;
        _finishedInformation.clear();
    }

    for (const std::pair<InfoWidget*, UpdateInformation> p : updateInformation) {
        p.first->update(p.second.currentSize, p.second.totalSize, p.second.errorMessage);
    }

    for (InfoWidget* w : finishedInformation) {
        _downloadLayout->removeWidget(w);
        delete w;
    }

    std::vector<torrent_handle> handles = _session->get_torrents();
    for (torrent_handle h : handles) {
        torrent_status s = h.status();
        InfoWidget* w = _torrentInfoWidgetMap[h];

        if (w)
            w->update(s);

        if (CleanInfoWidgets && (s.state == torrent_status::finished || s.state == torrent_status::seeding)) {
            _torrentInfoWidgetMap.remove(h);
            delete w;
        }
    }

    // Only close every torrent if all torrents are finished
    bool allSeeding = true;
    for (torrent_handle h : handles) {
        torrent_status s = h.status();
        allSeeding &= (s.state == torrent_status::seeding);
    }

    if (allSeeding) {
        for (torrent_handle h : handles)
            _session->remove_torrent(h);
    }
}
