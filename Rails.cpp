/* 
 * Plugin: Rails
 * Author: Dean Pucsek <dean@lightbulbone.com>
 * Date: 13 August 2012
 *
 * Provide a means to have multiple IDA instances interact and share resources
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


/* IDA Includes */
#include <ida.hpp>
#include <idp.hpp>
#include <pro.h>
#include <loader.hpp>
#include <kernwin.hpp>
#include <nalt.hpp>
#include <entry.hpp>

/* Rails includes */
#include "CommCenter.hpp"
#include "RailsResponder.hpp"

/* Qt includes */
#include <QTextBrowser>
#include <QSplitter>
#include <QListWidget>

#define RAILS_VERSION "0.1"

/* -------------- Communication Protocol -------------- */

/* There are three categories of messages available in Rails.
 * - rails : This category of messages pertain to global-scope
 *           messages such as get an IDA databases to process IDs
 * - cmt   : This category of messages deals with requesting and
 *           and receiving comments from external databases.
 * - nav   : This category is for navigation to, and within, external 
 *           databases.
 */

#define RP_OP_SIZE    1   /* Rails Protocol OPeration Size (in bytes) */

/* Category: rails */
#define RP_OP_RAILS_JOIN  0x01  /* OP<executable-path> */
#define RP_OP_RAILS_KILL  0x02  /* OP<executable-path> */
#define RP_OP_RAILS_PING  0x03  /* OP */
#define RP_OP_RAILS_PONG  0x04  /* OP<executable-name> */

/* Category: cmt */
#define RP_OP_CMT_GET     0x11  /* OP<func-name> */
#define RP_OP_CMT_SET     0x12  /* OP<executable-name>:<func-name>:<comment> */

/* Category: nav */
#define RP_OP_NAV_OFUN    0x21  /* OP<func-name> */
#define RP_OP_NAV_OEXE    0x22  /* OP<exe-name> */

/* Utility macro's */
#define RAILS_OP(p)      (*(char *)p)
#define RAILS_DATA(p)    ((char *)p + 1)

/* -------------- Globals -------------- */

/* The only reason for creating this global
 * variable is so that term() is able to 
 * access the CommCenter and properly disconnect.
 * Without doing this the shared memory region used
 * is not deleted upon disconnecting all clients.
 *
 * This is ONLY to be accessed using the global
 * identifier in term().  It is set to NULL in
 * init() and set to an allocated CommCenter in run()
 * all other access to CommCenter should be done through
 * the argument passed to the respective function.
 */
CommCenter *gCommCenter = NULL;

/* gConsole serves two purposes as a global.  First, it allows us to properly
 * clean up when the plugin is terminated.  Second, it enables the plugin
 * to bring the containing window to the front of the stack when an external
 * jump occurs.
 *
 * This global must ONLY be used through the rails_mesg() and bring_to_front()
 * functions except when creating the console and ensuring that it is NULL.
 */
QTextBrowser *gConsole;

/* In order to read messages the plugin polls the communication center.  This
 * is the timer used for polling. It is a global to allow for the timer to be
 * unregistered (and thus prevent further access to the shared region) when
 * the plugin is terminated.
 */
qtimer_t gTimer;

QListWidget *gInstanceList;

/* RailsResponder enables us to catch signals from the list view inside the 
 * Rails UI.  They can be used to bring other instances of IDA to the front.
 */
RailsResponder *gResponder;

/* gSplitter is the horizontal splitter used in the Rails UI.
 * The only reason it is global is so that it can be accessed
 * both from the creation point inside ui_callback() and
 * the point where windows are brought to the front
 * in bring_to_front().
 */
QSplitter *gSplitter;

/* -------------- Rails Console -------------- */

void rails_msg(const char *fmt, ...)
{
    QString msg_str;
    va_list argp;

    if(gConsole == NULL || fmt == NULL)
        return;

    va_start(argp, fmt);
    gConsole->append(msg_str.vsprintf(fmt, argp));
    va_end(argp);
}

/* -------------- Rails Responder -------------- */

void RailsResponder::instanceItemSelected(QListWidgetItem * item)
{
    QByteArray ba;

    ba.append(RP_OP_NAV_OEXE);
    ba.append(item->text());

    gCommCenter->broadcast(ba);
}

/* -------------- Menu Item Callbacks -------------- */

#define BUF_SIZE    128
#define IDENT_FLAGS 0    /* from documentation in kernwin.hpp */

struct _enum_import_data {
    char *module_name;
    char *highlighted_name;
};

int enum_import_cb(ea_t ea __attribute__((unused)), 
                   const char *name, 
                   uval_t ord __attribute__((unused)), 
                   void *param)
{
    assert(param != NULL);
    
    struct _enum_import_data *match_info = (struct _enum_import_data *)param;

    assert(match_info->module_name != NULL);
    assert(match_info->highlighted_name != NULL);

    if(strcmp(match_info->highlighted_name, name) == 0)
    {
        rails_msg("Attempting to jump to <code>%s</code> in <code>%s</code>",
                  name, match_info->module_name);
        return 0;
    }

    return 1;
}

bool rails_nav_cb(void *ud)
{
    assert(ud != NULL);

    CommCenter *cc = (CommCenter *)ud;          \

    char *buf = (char *)calloc(1, BUF_SIZE);
    if(!buf)
    {
        printf("Error: calloc\n");
        return false;
    }

    *buf = RP_OP_NAV_OFUN;
    get_highlighted_identifier(buf+RP_OP_SIZE, 
                               BUF_SIZE-RP_OP_SIZE, 
                               IDENT_FLAGS);

    int imp_qty = get_import_module_qty();
    char *imp_buf = (char *)calloc(1, BUF_SIZE);
    if(!imp_buf)
    {
        printf("Error: calloc\n");
        return false;
    }

    int imp_id = 0;
    struct _enum_import_data match_info;
    for(imp_id = 0; imp_id < imp_qty; imp_id++)
    {
        bzero(imp_buf, BUF_SIZE);
        get_import_module_name(imp_id, imp_buf, BUF_SIZE);

        match_info.module_name = imp_buf;
        match_info.highlighted_name = buf + 1;  /* Skip the op byte */

        enum_import_names(imp_id, enum_import_cb, (void *)&match_info);
    }

    cc->broadcast(QByteArray(buf));
    free(buf);
    free(imp_buf);

    return true;
}

bool rails_comments_cb(void *ud)
{
    assert(ud != NULL);

    CommCenter *cc = (CommCenter *)ud;          \

    char *buf = (char *)calloc(1, BUF_SIZE);
    if(!buf)
    {
        printf("Error: calloc\n");
        return false;
    }

    *buf = RP_OP_CMT_GET;
    get_highlighted_identifier(buf+RP_OP_SIZE, 
                               BUF_SIZE-RP_OP_SIZE, 
                               IDENT_FLAGS);

    cc->broadcast(QByteArray(buf));
    free(buf);

    return true;
}

/* -------------- Handling Rails Requests --------------- */

void bring_to_front()
{
    QWidget *win;

    if(gSplitter == NULL)
        return;

    win = gSplitter->window();
    while(!win->isWindow())
    {
        win = win->window();
    }

    win->activateWindow();
    win->raise();
}

void rails_nav_open_exe(const char *exe_name)
{
    char *name_buf = (char *)calloc(1, BUF_SIZE);
    get_root_filename(name_buf, BUF_SIZE);

    if(strcmp(exe_name, name_buf) == 0)
    {
        bring_to_front();
    }

    free(name_buf);
}

void rails_nav_open_func(const char *func_name)
{
    char *entry_buf;
    size_t n_entry_points;
    unsigned int i;
    ea_t entry_ea;
    uval_t ord;

    entry_buf = (char *)calloc(1, BUF_SIZE);
    if(!entry_buf)
        return;

    /* entry points */
    n_entry_points = get_entry_qty();
    for(i = 0; i < n_entry_points; i++)
    {
        ord = get_entry_ordinal(i);
        get_entry_name(ord, entry_buf, BUF_SIZE);
        if(strcmp(entry_buf, func_name) == 0)
        {
            entry_ea = get_entry(ord);
            jumpto(entry_ea);
            bring_to_front();
        }
    }
}

void rails_cmt_get(CommCenter *cc, const char *func_name)
{
    char *entry_buf, *cmt_buf;
    char *func_cmt, *path_buf;
    const char *set_cmt_fmt;
    size_t n_entry_points;
    unsigned int i;
    ea_t entry_ea;
    func_t *func;
    uval_t ord;

    entry_buf = (char *)calloc(1, BUF_SIZE);
    if(!entry_buf)
        return;

    /* entry points */
    n_entry_points = get_entry_qty();
    for(i = 0; i < n_entry_points; i++)
    {
        ord = get_entry_ordinal(i);
        get_entry_name(ord, entry_buf, BUF_SIZE);
        if(strcmp(entry_buf, func_name) == 0)
        {
            entry_ea = get_entry(ord);
            func = get_func(entry_ea);
            func_cmt = get_func_cmt(func, false);
            
            cmt_buf = (char *)calloc(1, BUF_SIZE);
            if(!cmt_buf)
            {
                printf("Error: calloc\n");
                free(entry_buf);
                return;
            }

            *cmt_buf = RP_OP_CMT_SET;
            set_cmt_fmt = "%s:%s:%s";

            path_buf = (char *)calloc(1, BUF_SIZE);
            get_input_file_path(path_buf, BUF_SIZE);

            /* We need to use the scope operator with qsnprintf()
             * because QT makes an equivalent function available
             * and the compiler can't determine which we want
             * (IDA version or QT's) to use.  Since the version
             * provided by IDA is in the global namespace we don't
             * give the scope operator any namespace.
             */
            ::qsnprintf(cmt_buf+RP_OP_SIZE, (size_t)BUF_SIZE, 
                        (const char *)set_cmt_fmt, 
                        (char *)path_buf,
                        (char *)func_name, 
                        (char *)func_cmt);

            cc->broadcast(QByteArray(cmt_buf));
            free(path_buf);
            free(cmt_buf);
        }
    }

    free(entry_buf);
}

#define NR_DISP_FIELDS 3
void rails_cmt_set(const char *cmt)
{
    char *token, *str;
    int id;

    const char *fields[NR_DISP_FIELDS] = {"<b>Executable:</b> <code>%s</code>"
                                          , "<b>Function:</b> <code>%s</code>"
                                          , "<b>Comment:</b> %s"};

    id = 0;
    str = (char *)cmt;
    while((token = strsep(&str, ":")) != NULL)
    {
        rails_msg(fields[id], token);
        id++;
    }
}

void rails_peer_joined(const char *peer_path)
{
    QStringList list;
    QString path(peer_path);
    
    list = path.split("\\");
    if(list.isEmpty() || list.size() == 1)
    {
        list = path.split("/");
        if(list.isEmpty())
            return;
    }

    gInstanceList->addItem(list.last());
}

void rails_peer_killed(const char *peer_path)
{
    QStringList list;
    QString path(peer_path);
    QList<QListWidgetItem *> itemList;

    list = path.split("\\");
    if(list.isEmpty() || list.size() == 1)
    {
        list = path.split("/");
        if(list.isEmpty())
            return;
    }

    itemList = gInstanceList->findItems(list.last(), Qt::MatchExactly);

    QList<QListWidgetItem *>::iterator i;
    for (i = itemList.begin(); i != itemList.end(); i++)
    {
        gInstanceList->takeItem(gInstanceList->row(*i));
        gInstanceList->update();
    }

}

void rails_peer_ping(CommCenter *cc, qint64 pong_to)
{
    char *name_buf = (char *)calloc(1, BUF_SIZE);
    *name_buf = RP_OP_RAILS_PONG;
    get_root_filename(name_buf+RP_OP_SIZE, BUF_SIZE-RP_OP_SIZE);
    cc->send(pong_to, QByteArray(name_buf));
    free(name_buf);
}

void rails_peer_pong(const char *exe_name)
{
    QList<QListWidgetItem *> itemsList;

    itemsList = gInstanceList->findItems(QString(exe_name), Qt::MatchExactly);
    if(itemsList.count() == 0)
    {
        gInstanceList->addItem(QString(exe_name));
    }
}

void processMessage(CommCenter *cc, struct Message *msgp)
{
    if(!msgp || !cc)
        return;

    switch(RAILS_OP(msgp->msg_data))
    {
    case RP_OP_RAILS_JOIN: {
        rails_peer_joined(RAILS_DATA(msgp->msg_data));
    } break;
    case RP_OP_RAILS_KILL: {
        rails_peer_killed(RAILS_DATA(msgp->msg_data));
    } break;
    case RP_OP_RAILS_PING: {
        rails_peer_ping(cc, msgp->msg_from);
    } break;
    case RP_OP_RAILS_PONG: {
        rails_peer_pong(RAILS_DATA(msgp->msg_data));
    } break;
    case RP_OP_CMT_GET: {
        rails_cmt_get(cc, RAILS_DATA(msgp->msg_data));
    } break;
    case RP_OP_CMT_SET: {
        rails_cmt_set(RAILS_DATA(msgp->msg_data));
    } break;
    case RP_OP_NAV_OFUN: {
        rails_nav_open_func(RAILS_DATA(msgp->msg_data));
    } break;
    case RP_OP_NAV_OEXE: {
        rails_nav_open_exe(RAILS_DATA(msgp->msg_data));
    } break;
    default: 
        rails_msg("Rails: Unknown operation (0x%x)\n", 
                  RAILS_OP(msgp->msg_data));
    };

    free(msgp);
}

/* -------------- Communication Timer -------------- */

#define TIMER_INTERVAL  500  /* milliseconds */
int idaapi timerExpired(void *ud)
{
    struct Message *msgp;
    CommCenter *cc;
    int i;

    assert(ud != NULL);

    cc = (CommCenter *)ud;

    i = 0;
    while((msgp = cc->readMessage(i)) == NULL)
    {
        if(i >= MSG_MAX_COUNT)
            break;

        i++;
    }

    processMessage(cc, msgp);

    return TIMER_INTERVAL;
}

/* -------------- UI Callback -------------- */

static int idaapi ui_callback(void *user_data, 
                              int notification_code, 
                              va_list va)
{
    if(notification_code == ui_tform_visible)
    {
        TForm *form = va_arg(va, TForm *);
        if(form == user_data)
        {
            QWidget *w = (QWidget *)form;
            QWidget *wp = w->parentWidget();
            if(wp == NULL)
            {
                wp = w;
            }

            wp = wp->parentWidget();
            if(wp == NULL)
            {
                wp = w;
            }

            gSplitter = new QSplitter(Qt::Horizontal, wp);

            gInstanceList = new QListWidget();
            gSplitter->addWidget(gInstanceList);
            
            gConsole = new QTextBrowser();
            gSplitter->addWidget(gConsole);

            QObject::connect(gInstanceList, 
                             SIGNAL(itemActivated(QListWidgetItem *)),
                             gResponder, 
                             SLOT(instanceItemSelected(QListWidgetItem *)));

            QRect wGeo = wp->geometry();
            gSplitter->setGeometry(wGeo.x() + 5,
                                   wGeo.y() - 15,
                                   wGeo.width() - 10,
                                   wGeo.height() - 10);
            gSplitter->show();

            rails_msg("Rails - v%s", RAILS_VERSION);
        }
    } 
    else if(notification_code == ui_tform_invisible)
    {
        TForm *form = va_arg(va, TForm *);
        if(form == user_data)
        {
            /* form is gone, destroy anything */
        }
    }

    return 0;
}

/* -------------- IDA Plugin Interface -------------- */

int idaapi init(void)
{
    gCommCenter = NULL;
    gConsole = NULL;
    gSplitter = NULL;
    gTimer = NULL;
    gResponder = NULL;
    gInstanceList = NULL;
    return is_idaq() ? PLUGIN_OK : PLUGIN_SKIP;
}

void idaapi term(void)
{
    unhook_from_notification_point(HT_UI, ui_callback);

    if(gTimer != NULL)
    {
        unregister_timer(gTimer);
    }

    if(gCommCenter != NULL)
    {
        char *path_buf = (char *)calloc(1, BUF_SIZE);
        *path_buf = RP_OP_RAILS_KILL;
        get_input_file_path(path_buf+RP_OP_SIZE, BUF_SIZE-RP_OP_SIZE);
        gCommCenter->broadcast(QByteArray(path_buf));
        free(path_buf);
        
        gCommCenter->disconnect();
        delete gCommCenter;
    }
}

void idaapi run(int arg __attribute__((unused)))
{
    gCommCenter = new CommCenter();
    gCommCenter->connect();

    gConsole = NULL;

    gResponder = new RailsResponder();

    HWND hwnd = NULL;
    TForm *form = create_tform("Rails", &hwnd);
    if(hwnd != NULL)
    {
        hook_to_notification_point(HT_UI, ui_callback, form);
        open_tform(form, FORM_TAB|FORM_MENU|FORM_RESTORE|FORM_QWIDGET);
    }
    else
    {
        close_tform(form, FORM_SAVE);
    }

    char *path_buf = (char *)calloc(1, BUF_SIZE);
    *path_buf = RP_OP_RAILS_JOIN;
    get_input_file_path(path_buf+RP_OP_SIZE, BUF_SIZE-RP_OP_SIZE);
    gCommCenter->broadcast(QByteArray(path_buf));
    free(path_buf);

    QByteArray getAlive;
    getAlive.append(RP_OP_RAILS_PING);
    gCommCenter->broadcast(getAlive);

    /* add menus */
    add_menu_item("Edit/Plugins", "Rails - Comments"
                  , "Alt-c", SETMENU_CTXAPP | SETMENU_INS
                  , rails_comments_cb, (void *)gCommCenter);
    add_menu_item("Edit/Plugins", "Rails - Jump"
                  , "Alt-j", SETMENU_CTXAPP | SETMENU_INS
                  , rails_nav_cb, (void *)gCommCenter);

    /* add a timer */
    gTimer = register_timer(TIMER_INTERVAL, timerExpired, gCommCenter);
}

const char *comment = "Interconnect IDA Instances";
const char *help = "";
const char *wanted_name = "Rails";
const char *wanted_hotkey = "Alt-r";

plugin_t PLUGIN = {
  IDP_INTERFACE_VERSION,
  0,
  init,
  term,
  run,
  comment,
  help,
  wanted_name,
  wanted_hotkey
};
