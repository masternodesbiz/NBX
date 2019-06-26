// Copyright (c) 2018-2019 Netbox.Global
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_QT_HDSEEDDIALOG_H
#define BITCOIN_QT_HDSEEDDIALOG_H

#include <QDialog>

class WalletModel;

namespace Ui
{
class HdSeedDialog;
}

/** View or change mnemonic phrase dialog
 */
class HdSeedDialog : public QDialog
{
    Q_OBJECT

public:
    explicit HdSeedDialog(QWidget* parent, WalletModel* model);
    ~HdSeedDialog();
    void accept();

private:
    Ui::HdSeedDialog* ui;
    WalletModel* model;
};

#endif // BITCOIN_QT_HDSEEDDIALOG_H
