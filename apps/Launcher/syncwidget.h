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

#ifndef __SYNCWIDGET_H__
#define __SYNCWIDGET_H__

#include <QWidget>

#include <QMap>

#include <openspace/engine/downloadmanager.h>
#include "downloadcollection.h"

#include <libtorrent/torrent_handle.hpp>
#include <ghoul/misc/threadpool.h>

#include <atomic>

class QBoxLayout;
class QGridLayout;

class InfoWidget;

namespace libtorrent {
    class session;
    struct torrent_handle;
}

class SyncWidget : public QWidget {
Q_OBJECT
public:
    SyncWidget(QWidget* parent, Qt::WindowFlags f = 0);
    ~SyncWidget();
    
    void setSceneFiles(QMap<QString, QString> sceneFiles);

private slots:
    void syncButtonPressed();
    void handleTimer();

    void closeEvent(QCloseEvent* event);

private:
    void clear();
    QStringList selectedScenes() const;

    struct UpdateInformation {
        std::string errorMessage;
        size_t currentSize;
        size_t totalSize;
    };
    std::mutex _updateInformationMutex;
    std::map<InfoWidget*, UpdateInformation> _updateInformation;
    std::vector<InfoWidget*> _finishedInformation;

    QMap<QString, QString> _sceneFiles;
    QString _modulesDirectory;
    QGridLayout* _sceneLayout;
    QBoxLayout* _downloadLayout;

    std::unique_ptr<libtorrent::session> _session;
    QMap<libtorrent::torrent_handle, InfoWidget*> _torrentInfoWidgetMap;

    std::unique_ptr<openspace::DownloadManager> _downloadManager;

    ghoul::ThreadPool _threadPool;

};

#endif // __SYNCWIDGET_H__
