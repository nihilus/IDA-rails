/* 
 * Plugin: Rails
 * Author: Dean Pucsek <dean@lightbulbone.com>
 * Date: 13 August 2012
 *
 * Implementation of a shared memory message passing framework.
 *
 *
 * Copyright (c) 2012, Dean Pucsek
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the LightBulbOne nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 *
 */

#include "CommCenter.hpp"

#define SHM_RAILS_KEY    "rails"
#define SHM_RAILS_SIZE   32768     /* bytes */

struct CommCenterPrivate 
{
    qint64 next_id;
    qint64 nobservers;
    struct Message msgs[MSG_MAX_COUNT];
};

CommCenter::CommCenter(QObject * parent)
    : QObject(parent), sharedMemory(SHM_RAILS_KEY)
{
    if(sharedMemory.attach())
        return;

    if(sharedMemory.error() != QSharedMemory::NotFound)
    {
        qDebug() << "Error: sharedMemory.attach() :: " << \
            sharedMemory.errorString();
        return;
    }

    if(sharedMemory.create(SHM_RAILS_SIZE))
    {
        sharedMemory.lock();
        bzero(sharedMemory.data(), SHM_RAILS_SIZE);
        sharedMemory.unlock();

        return;
    }

    qDebug() << "Error: sharedMemory.create() :: " << \
        sharedMemory.errorString();
    return;
}

CommCenter::~CommCenter()
{
    if(sharedMemory.detach() == false)
    {
        qDebug() << "dtor: failed to detach";
        qDebug() << "dtor: isAttached = " << sharedMemory.isAttached();
    }
}

bool CommCenter::connect()
{
    sharedMemory.lock();
    CommCenterPrivate *ccp = (CommCenterPrivate *)sharedMemory.constData();
    connection_id = ccp->next_id;
    ccp->next_id += 1;
    ccp->nobservers += 1;
    sharedMemory.unlock();

    connected = true;

    return true;
}

bool CommCenter::disconnect()
{
    if(connected == false)
        return false;

    sharedMemory.lock();
    CommCenterPrivate *ccp = (CommCenterPrivate *)sharedMemory.data();
    ccp->nobservers -= 1;
    sharedMemory.unlock();

    connected = false;

    return true;
}

void CommCenter::broadcast(const QByteArray & msg_ba)
{
    // qDebug() << "bcast: " << msg_ba;

    sharedMemory.lock();
    postMessage(0, msg_ba);
    sharedMemory.unlock();
}

void CommCenter::send(qint64 dst_pid, const QByteArray & msg_ba)
{
    // qDebug() << "send [" << dst_pid<< "]: " << msg_ba;

    sharedMemory.lock();
    postMessage(dst_pid, msg_ba);
    sharedMemory.unlock();
}

/* ---------- Utility Functions ---------- */

bool CommCenter::isConnected()
{
    return connected;
}

struct Message *CommCenter::readMessage(int msg_box)
{
    CommCenterPrivate *ccp;
    struct Message *shm_msgp;
    struct Message *priv_msgp;
    qint64 curr_time_ms, pid;

    sharedMemory.lock();
    ccp = (CommCenterPrivate *)sharedMemory.data();

    shm_msgp = &((ccp->msgs)[msg_box]);

    /* check if there is a message in this box */
    if(shm_msgp->msg_time == 0)
    {
        goto rm_error;
    }

    /* check if we need to read it */
    pid = QCoreApplication::applicationPid();
    if(shm_msgp->msg_from == pid)
    {
        goto rm_error;
    }

    if(shm_msgp->msg_to != 0 && shm_msgp->msg_to != pid)
    {
        goto rm_error;
    }

    /* check if message lifetime has expired */
    curr_time_ms = QDateTime::currentMSecsSinceEpoch();
    if((curr_time_ms - shm_msgp->msg_time) >= MSG_TIME_EXPR)
    {
        bzero(shm_msgp, sizeof(struct Message));
        goto rm_error;
    }

    /* check if previously read the message */
    if((shm_msgp->msg_read & (0x1 << connection_id)) != 0)
    {
        goto rm_error;
    }
    
    /* read the message */
    priv_msgp = (struct Message *)calloc(1, sizeof(struct Message));
    if(priv_msgp == NULL)
    {
        goto rm_error;
    }

    memcpy(priv_msgp, shm_msgp, sizeof(struct Message));

    /* mark the message as read */
    shm_msgp->msg_read |= (0x1 << connection_id);

    sharedMemory.unlock();
    return priv_msgp;

rm_error:
    sharedMemory.unlock();
    return NULL;
}

/* CommCenter::postMessage() must be entered with the 
 * sharedMemory lock already in place.
 */
void CommCenter::postMessage(qint64 dst_pid, const QByteArray & msg_ba)
{
    CommCenterPrivate *ccp;
    struct Message *msgs;
    struct Message *mailbox;
    int msgBox;
    
    ccp = (CommCenterPrivate *)sharedMemory.data();
    
    msgBox = nextMsgBox(ccp);
    if(msgBox < 0)
    {
        qDebug() << "Warning: Message boxes are full.  Skipping message.";
        return;
    }

    msgs = ccp->msgs;
    mailbox = &(msgs[msgBox]);

    mailbox->msg_time = QDateTime::currentMSecsSinceEpoch();
    mailbox->msg_read = 0;
    mailbox->msg_from = QCoreApplication::applicationPid();;
    mailbox->msg_to   = dst_pid;

    memcpy(mailbox->msg_data, msg_ba.data(), 
           qMin(msg_ba.size(), MSG_DATA_SIZE));    
}

int CommCenter::nextMsgBox(void *vccp)
{
    CommCenterPrivate *ccp;
    struct Message *msgs_p;
    int msgBox;

    ccp = (CommCenterPrivate *)vccp;
    msgs_p = ccp->msgs;

    for(msgBox = 0; msgBox < MSG_MAX_COUNT; msgBox++)
    {
        if((msgs_p[msgBox]).msg_time == 0)
        {
            return msgBox;
        }
        else if((QDateTime::currentMSecsSinceEpoch() - \
                 (msgs_p[msgBox]).msg_time) >= MSG_TIME_EXPR)
        {
            bzero(&(msgs_p[msgBox]), sizeof(struct Message));
            return msgBox;
        }
    }

    return -1;
}

int CommCenter::observers()
{
    int obs = -1;

    sharedMemory.lock();
    CommCenterPrivate *ccp = (CommCenterPrivate *)sharedMemory.data();
    obs = ccp->nobservers;
    sharedMemory.unlock();

    return obs;
}

int CommCenter::pending()
{
    int pnd = -1;
    return pnd;
}

QStringList CommCenter::allMessages()
{
    QStringList msgs;
    int i;

    sharedMemory.lock();
    CommCenterPrivate *ccp = (CommCenterPrivate *)sharedMemory.data();
    struct Message *msgp = ccp->msgs;

    QString builder;
    for(i = 0; i < MSG_MAX_COUNT; i++)
    {
        builder.sprintf("[%d] time: %lld, read: %lld, "
                        "from: %lld, to: %lld -> %s"
                        , i
                        , (msgp[i]).msg_time, (msgp[i]).msg_read
                        , (msgp[i]).msg_from, (msgp[i]).msg_to
                        , (char *)((msgp[i]).msg_data));
        msgs << builder;
    }

    sharedMemory.unlock();

    return msgs;
}

/* ---------- Private Slots ---------- */

void CommCenter::timerExpired()
{
    qDebug() << "CommCenter timer expired";
}
