/**
******************************************************************************
*
* @file       tilecachequeue.h
* @author     The OpenPilot Team, http://www.openpilot.org Copyright (C) 2010.
* @brief      
* @see        The GNU Public License (GPL) Version 3
* @defgroup   OPMapWidget
* @{
* 
*****************************************************************************/
/* 
* This program is free software; you can redistribute it and/or modify 
* it under the terms of the GNU General Public License as published by 
* the Free Software Foundation; either version 3 of the License, or 
* (at your option) any later version.
* 
* This program is distributed in the hope that it will be useful, but 
* WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY 
* or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License 
* for more details.
* 
* You should have received a copy of the GNU General Public License along 
* with this program; if not, write to the Free Software Foundation, Inc., 
* 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
*/
#ifndef TILECACHEQUEUE_H
#define TILECACHEQUEUE_H

#include <QQueue>
#include "cacheitemqueue.h"
#include <QThread>
#include <QMutex>
#include <QWaitCondition>
#include <QObject>
#include <QMutexLocker>
#include "pureimagecache.h"


namespace core {
    /// 后台线程：把网络下载的瓦片异步写入磁盘缓存（SQLite）
    class TileCacheQueue:public QThread
    {
        Q_OBJECT
    public:
        explicit TileCacheQueue(PureImageCache *imageCache);
        ~TileCacheQueue();
        void EnqueueCacheTask(CacheItemQueue *task);

    protected:
        QQueue<CacheItemQueue*> tileCacheQueue;
    private:
        void run();
        PureImageCache *imageCache;   ///< 目标磁盘缓存（归 MapService 所有）
        QMutex mutex;
        QMutex waitmutex;
        QWaitCondition waitc;
    };
}
#endif // TILECACHEQUEUE_H
