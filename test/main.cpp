/*
 * Plugin: Rails
 * Author: Dean Pucsek <dean@lightbulbone.com>
 * Date: 13 August 2012
 *
 * CommCenter test program.
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


#include "main.hpp"

TestDriver::TestDriver(QString & testName, QWidget & mainWin)
{
    textWriter = new QLineEdit();
    textReader = new QTextEdit();
    textReader->setReadOnly(true);

    QObject::connect(textWriter, SIGNAL(editingFinished()), this, SLOT(handleInput()));

    QVBoxLayout *vbox = new QVBoxLayout();
    vbox->addWidget(textReader);
    vbox->addWidget(textWriter);
    mainWin.setLayout(vbox);

    cc = new CommCenter();
    cc->connect();
    qDebug() << testName << " starting...";
}

void TestDriver::handleInput()
{
    QString input(textWriter->text());
    textReader->append(textWriter->text());
    textWriter->clear();

    QStringList splitInput = input.split(" ");

    if(input.compare(QString("quit")) == 0)
    {
        QCoreApplication::exit();
    } 
    else if(input.compare(QString("observers")) == 0)
    {
        textReader->append(QString::number(cc->observers()));
    }
    else if(input.compare(QString("pending")) == 0)
    {
        //textReader->append(QString::number(cc->pending()));
        textReader->append(QString::number(cc->pending(), 2));
    }
    else if(input.compare(QString("msgs")) == 0)
    {
        QStringList msgs = cc->allMessages();
        QStringList::const_iterator constIterator;

        for(constIterator = msgs.constBegin(); constIterator != msgs.constEnd(); ++constIterator)
            textReader->append((*constIterator).toLocal8Bit().constData());
    }
    else if(splitInput[0].compare("bc") == 0)
    {
        QString str;

        textReader->append(input);

        splitInput.removeFirst();
        str = splitInput.join(" ");
        cc->broadcast(str.toAscii());
    }
}

void TestDriver::testQuitting()
{
    qDebug() << "TD slot quitting...";
    cc->disconnect();
    delete cc;
}

int main(int argc, char **argv)
{
    QString testName("CommCenter");

    QApplication *app = new QApplication(argc, argv);
    QWidget *mainWin = new QWidget();
    TestDriver *td = new TestDriver(testName, *mainWin);

    QObject::connect(app, SIGNAL(aboutToQuit()), td, SLOT(testQuitting()));

    mainWin->show();
    return app->exec();
}
