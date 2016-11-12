/****************************************************************************
 *   Copyright (C) 2006-2013 by Jason Ansel, Kapil Arya, and Gene Cooperman *
 *   jansel@csail.mit.edu, kapil@ccs.neu.edu, and gene@ccs.neu.edu          *
 *                                                                          *
 *   This file is part of the dmtcp/src module of DMTCP (DMTCP:dmtcp/src).  *
 *                                                                          *
 *  DMTCP:dmtcp/src is free software: you can redistribute it and/or        *
 *  modify it under the terms of the GNU Lesser General Public License as   *
 *  published by the Free Software Foundation, either version 3 of the      *
 *  License, or (at your option) any later version.                         *
 *                                                                          *
 *  DMTCP:dmtcp/src is distributed in the hope that it will be useful,      *
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of          *
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the           *
 *  GNU Lesser General Public License for more details.                     *
 *                                                                          *
 *  You should have received a copy of the GNU Lesser General Public        *
 *  License along with DMTCP:dmtcp/src.  If not, see                        *
 *  <http://www.gnu.org/licenses/>.                                         *
 ****************************************************************************/

#pragma once
#ifndef FILECONNLIST_H
# define FILECONNLIST_H

// THESE INCLUDES ARE IN RANDOM ORDER.  LET'S CLEAN IT UP AFTER RELEASE. - Gene
# include "connectionlist.h"
# include "fileconnection.h"
# include "jbuffer.h"
# include "jconvert.h"
# include "jfilesystem.h"
# include "procmapsarea.h"
# include <mqueue.h>
# include <signal.h>
# include <stdint.h>
# include <sys/socket.h>
# include <sys/stat.h>
# include <sys/types.h>
# include <sys/types.h>
# include <unistd.h>

namespace dmtcp
{
class FileConnList : public ConnectionList
{
  public:
    static FileConnList &instance();

    static void saveOptions() { instance().preLockSaveOptions(); }

    static void leaderElection() { instance().preCkptFdLeaderElection(); }

    static void drainFd() { instance().drain(); }

    static void ckpt() { instance().preCkpt(); }

    static void resumeRefill() { instance().refill(false); }

    static void resumeResume() { instance().resume(false); }

    static void restart() { instance().postRestart(); }

    static void restartRegisterNSData() { instance().registerNSData(); }

    static void restartSendQueries() { instance().sendQueries(); }

    static void restartRefill() { instance().refill(true); }

    static void restartResume() { instance().resume(true); }

    virtual void preLockSaveOptions();
    virtual void drain();
    virtual void refill(bool isRestart);
    virtual void resume(bool isRestart);
    virtual void postRestart();
    virtual int protectedFd() { return PROTECTED_FILE_FDREWIRER_FD; }

    // examine /proc/self/fd for unknown connections
    virtual void scanForPreExisting();
    virtual Connection *createDummyConnection(int type);

    Connection *findDuplication(int fd, const char *path);
    void processFileConnection(int fd, const char *path, int flags,
                               mode_t mode);

    void prepareShmList();
    void remapShmMaps();
    void recreateShmFileAndMap(const ProcMapsArea &area);
    void restoreShmArea(const ProcMapsArea &area, int fd = -1);
};
}
#endif // ifndef FILECONNLIST_H
