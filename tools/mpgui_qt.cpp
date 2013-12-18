/********************************************************************** 
 Freeciv - Copyright (C) 1996 - A Kjeldberg, L Gregersen, P Unold
   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2, or (at your option)
   any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.
***********************************************************************/

#ifdef HAVE_CONFIG_H
#include <fc_config.h>
#endif

// Qt
#include <QApplication>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QMainWindow>
#include <QPushButton>
#include <QTableWidget>
#include <QVBoxLayout>

// utility
#include "fcintl.h"
#include "log.h"
#include "registry.h"

// modinst
#include "download.h"
#include "mpcmdline.h"
#include "mpdb.h"
#include "mpgui_qt_worker.h"
#include "modinst.h"

#include "mpgui_qt.h"

struct fcmp_params fcmp = { MODPACK_LIST_URL, NULL };

static mpgui *gui;

static mpqt_worker *worker = NULL;

static int mpcount = 0;

#define ML_COL_NAME   0
#define ML_COL_VER    1
#define ML_COL_INST   2
#define ML_COL_TYPE   3
#define ML_COL_LIC    4 
#define ML_COL_URL    5

#define ML_TYPE       6

#define ML_COL_COUNT  7

static void setup_modpack_list(const char *name, const char *URL,
                               const char *version, const char *license,
                               enum modpack_type type, const char *notes);
static void msg_callback(const char *msg);

/**************************************************************************
  Entry point for whole freeciv-mp-qt program.
**************************************************************************/
int main(int argc, char **argv)
{
  enum log_level loglevel = LOG_NORMAL;
  int ui_options;

  registry_module_init();
  log_init(NULL, loglevel, NULL, NULL, -1);

  /* This modifies argv! */
  ui_options = fcmp_parse_cmdline(argc, argv);

  if (ui_options != -1) {
    QApplication *qapp;
    QMainWindow *main_window;
    QWidget *central;
    const char *errmsg;

    load_install_info_lists(&fcmp);

    qapp = new QApplication(ui_options, argv);
    main_window = new QMainWindow;
    central = new QWidget;

    main_window->setGeometry(0, 30, 640, 60);
    main_window->setWindowTitle(_("Freeciv modpack installer (Qt)"));

    gui = new mpgui;

    gui->setup(central);

    main_window->setCentralWidget(central);
    main_window->setVisible(true);

    errmsg = download_modpack_list(&fcmp, setup_modpack_list, msg_callback);
    if (errmsg != NULL) {
      gui->display_msg(errmsg);
    }
    qapp->exec();

    if (worker != NULL) {
      if (worker->isRunning()) {
        worker->wait();
      }
      delete worker;
    }

    delete gui;
    delete qapp;

    save_install_info_lists(&fcmp);
  }

  log_close();
  registry_module_close();

  return EXIT_SUCCESS;
}

/**************************************************************************
  Progress indications from downloader
**************************************************************************/
static void msg_callback(const char *msg)
{
  gui->display_msg(msg);
}

/**************************************************************************
  Setup GUI object
**************************************************************************/
void mpgui::setup(QWidget *central)
{
#define URL_LABEL_TEXT "Modpack URL"
  QVBoxLayout *main_layout = new QVBoxLayout();
  QHBoxLayout *hl = new QHBoxLayout();
  QPushButton *install_button = new QPushButton(_("Install modpack"));
  QStringList headers;

  QLabel *URL_label;

  mplist_table = new QTableWidget();
  mplist_table->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Fixed);
  mplist_table->setColumnCount(ML_COL_COUNT);
  headers << _("Name") << _("Version") << _("Installed");
  headers << Q_("?modpack:Type") << _("License");
  headers << _("URL") << "typeint";
  mplist_table->setHorizontalHeaderLabels(headers);
  mplist_table->verticalHeader()->setVisible(false);
  mplist_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
  mplist_table->setSelectionBehavior(QAbstractItemView::SelectRows);
  mplist_table->setSelectionMode(QAbstractItemView::SingleSelection);
  mplist_table->hideColumn(ML_TYPE);

  connect(mplist_table, SIGNAL(cellClicked(int, int)), this, SLOT(row_selected(int, int)));
  connect(mplist_table, SIGNAL(cellActivated(int, int)), this, SLOT(row_download(int, int)));

  main_layout->addWidget(mplist_table);

  URL_label = new QLabel(_(URL_LABEL_TEXT));
  URL_label->setParent(central);
  hl->addWidget(URL_label);

  URLedit = new QLineEdit(central);
  URLedit->setText(DEFAULT_URL_START);
  URLedit->setFocus();

  connect(URLedit, SIGNAL(returnPressed()), this, SLOT(URL_given()));

  hl->addWidget(URLedit);
  main_layout->addLayout(hl);

  connect(install_button, SIGNAL(pressed()), this, SLOT(URL_given()));
  main_layout->addWidget(install_button);

  msg_dspl = new QLabel(_("Select modpack to install"));
  msg_dspl->setParent(central);
  main_layout->addWidget(msg_dspl);

  msg_dspl->setAlignment(Qt::AlignHCenter);

  central->setLayout(main_layout); 
}

/**************************************************************************
  Display status message
**************************************************************************/
void mpgui::display_msg(const char *msg)
{
  msg_dspl->setText(msg);
}

/**************************************************************************
  User entered URL
**************************************************************************/
void mpgui::URL_given()
{
  if (worker != NULL) {
    if (worker->isRunning()) {
      display_msg(_("Another download already active"));
      return;
    }
  } else {
    worker = new mpqt_worker;
  }
  
  worker->download(URLedit->text(), this, &fcmp, msg_callback, NULL);
}

/**************************************************************************
  Refresh display of modpack list modpack versions
**************************************************************************/
void mpgui::refresh_list_versions()
{
  for (int i = 0; i < mpcount; i++) {
    QString name_str;
    int type_int;
    const char *new_inst;
    enum modpack_type type;

    name_str = mplist_table->item(i, ML_COL_NAME)->text();
    type_int = mplist_table->item(i, ML_TYPE)->text().toInt();
    type = (enum modpack_type) type_int;
    new_inst = get_installed_version(name_str.toUtf8().data(), type);

    if (new_inst == NULL) {
      new_inst = _("Not installed");
    }

    mplist_table->item(i, ML_COL_INST)->setText(QString::fromUtf8(new_inst));
  }

  mplist_table->resizeColumnsToContents();
}

/**************************************************************************
  Build main modpack list view
**************************************************************************/
void mpgui::setup_list(const char *name, const char *URL,
                       const char *version, const char *license,
                       enum modpack_type type, const char *notes)
{
  const char *type_str;
  const char *lic_str;
  const char *inst_str;
  QString type_nbr;

  if (modpack_type_is_valid(type)) {
    type_str = _(modpack_type_name(type));
  } else {
    /* TRANS: Unknown modpack type */
    type_str = _("?");
  }

  if (license != NULL) {
    lic_str = license;
  } else {
    /* TRANS: License of modpack is not known */
    lic_str = Q_("?license:Unknown");
  }

  inst_str = get_installed_version(name, type);
  if (inst_str == NULL) {
    inst_str = _("Not installed");
  }

  mplist_table->setRowCount(mpcount+1);

  mplist_table->setItem(mpcount, ML_COL_NAME, new QTableWidgetItem(QString::fromUtf8(name)));
  mplist_table->setItem(mpcount, ML_COL_VER, new QTableWidgetItem(version));
  mplist_table->setItem(mpcount, ML_COL_INST, new QTableWidgetItem(inst_str));
  mplist_table->setItem(mpcount, ML_COL_TYPE, new QTableWidgetItem(type_str));
  mplist_table->setItem(mpcount, ML_COL_LIC, new QTableWidgetItem(lic_str));
  mplist_table->setItem(mpcount, ML_COL_URL, new QTableWidgetItem(URL));

  type_nbr.setNum(type);
  mplist_table->setItem(mpcount, ML_TYPE, new QTableWidgetItem(type_nbr));

  mplist_table->resizeColumnsToContents();
  mpcount++;
}

/**************************************************************************
  Build main modpack list view
**************************************************************************/
static void setup_modpack_list(const char *name, const char *URL,
                               const char *version, const char *license,
                               enum modpack_type type, const char *notes)
{
  // Just call setup_list for gui singleton
  gui->setup_list(name, URL, version, license, type, notes);
}

/**************************************************************************
  User activated another table row
**************************************************************************/
void mpgui::row_selected(int row, int column)
{
  QString URL = mplist_table->item(row, ML_COL_URL)->text();

  URLedit->setText(URL);
}

/**************************************************************************
  User activated another table row
**************************************************************************/
void mpgui::row_download(int row, int column)
{
  QString URL = mplist_table->item(row, ML_COL_URL)->text();

  URLedit->setText(URL);

  URL_given();
}
