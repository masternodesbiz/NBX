// Copyright (c) 2018-2020 Netbox.Global
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "hdseeddialog.h"
#include "ui_hdseeddialog.h"

#include "guiconstants.h"
#include "guiutil.h"
#include "init.h"
#include "walletmodel.h"

#include "allocators.h"

#include <QKeyEvent>
#include <QMessageBox>
#include <QPushButton>
#include <QWidget>
#include <QProcess>

HdSeedDialog::HdSeedDialog(QWidget* parent, WalletModel* model) : QDialog(parent, Qt::WindowSystemMenuHint | Qt::WindowTitleHint | Qt::WindowCloseButtonHint),
                                                                                                            ui(new Ui::HdSeedDialog),
                                                                                                            model(model)
{
    ui->setupUi(this);
    this->setStyleSheet(GUIUtil::loadStyleSheet());

    if (model->isLocked())
        ui->hdseed->setPlainText("wallet is locked");
    else
        ui->hdseed->setPlainText(model->getHdSeed().c_str());
}

HdSeedDialog::~HdSeedDialog()
{
    // Attempt to overwrite text so that they do not linger around in memory
    ui->hdseed->setPlainText(QString(" ").repeated(ui->hdseed->toPlainText().size()));
    delete ui;
}

void HdSeedDialog::accept()
{
    if (!model)
        return;
    SecureString seed = SecureStringFromString(ui->hdseed->toPlainText().toStdString());

    try {
        model->setHdSeed(seed);
        QMessageBox::warning(this, tr("Mnemonic phrase changed"), tr("Netbox.Wallet will be restarted now to finish mnemonic phrase changing process."));
        QStringList args = QApplication::arguments();
        args.removeFirst();
        emit model->needRestart(args);
        QDialog::accept(); // Success
    } catch (const std::runtime_error &e) {
        QMessageBox::critical(this, tr("Setting mnemonic phrase failed"), tr(e.what()));
    }
}