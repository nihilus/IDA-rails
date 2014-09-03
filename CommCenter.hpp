/* 
 * Plugin: Rails
 * Author: Dean Pucsek <dean@lightbulbone.com>
 * Date: 13 August 2012
 *
 * Implementation of a shared memory messaging passing framework.
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

#ifndef __COMM_CENTER_HPP__
#define __COMM_CENTER_HPP__

#include <QCoreApplication>
#include <QDateTime>
#include <QDebug>
#include <QObject>
#include <QSharedMemory>
#include <QStringList>
#include <QString>

#define MSG_TIME_EXPR   60000       /* Duration (in miliseconds) a message 
                                     * will remain in the message box.
                                     */
#define MSG_DATA_SIZE   512         /* Size (in bytes) of the data component of
                                     * a message.
                                     */
#define MSG_MAX_COUNT   50          /* Number of entries in the message box.
                                     */

struct Message {
    qint64 msg_time;                /* Time the message was sent at.  This
                                     * is the number of milliseconds since
                                     * epoch as returned by 
                                     * QDateTime::currentMSecsSinceEpoch().
                                     */
    qint64 msg_read;                /* Bit field where each bit is 
                                     * mapped to a connection_id and 
                                     * represents whether or not that 
                                     * connection has read the message.
                                     * A value of 0 indicates the message
                                     * has not been read, 1 indicates that it
                                     * has.
                                     */
    qint64 msg_from;                /* The process ID (from 
                                     * QCoreApplication::applicationPid())
                                     * of the sender.
                                     */
    qint64 msg_to;                  /* The process ID (from 
                                     * QCoreApplication::applicationPid())
                                     * of the receiver.  A value of 0 
                                     * indicates that the message is to be 
                                     * broadcast to all connections.
                                     */
    char msg_data[MSG_DATA_SIZE];   /* The message to be sent.
                                     */
};

class CommCenter : public QObject
{
    Q_OBJECT

public:
    CommCenter(QObject * parent = 0);
    ~CommCenter();

    bool connect();
    bool disconnect();

    void broadcast(const QByteArray & msg);
    void send(qint64 dst_pid, const QByteArray & msg);

    /* NOTE: The pointer returned by readMessage()
     * NOTE: MUST be released by the caller.
     */
    struct Message *readMessage(int msg_box);

    int observers();
    int pending();
    QStringList allMessages();
    bool isConnected();

private slots:
    void timerExpired();

private:
    int nextMsgBox(void *vccp);
    void postMessage(qint64 dst_pid, const QByteArray & msg_ba);

    bool connected;
    qint64 connection_id;

    QSharedMemory sharedMemory;
};

#endif /* __COMM_CENTER_HPP__ */
