/*
 * Copyright (c) 2015 Meinhard Ritscher <cyc1ingsir@gmail.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 * For more information on the GPL, please go to:
 * http://www.gnu.org/copyleft/gpl.html
 */
#include "sessionmanager.h"

#include <QLineEdit>

SessionManager::SessionManager(Settings *settings, QWidget *parent) :
  QDialog(parent)
  ,m_settings(settings)
  ,m_current_item(0)
  ,m_current_session(0)
  ,m_isCloning(false)
  ,m_isRenaming(false)
{
    setupUi(this);

    m_session_list->setEditTriggers(QAbstractItemView::NoEditTriggers);

    connect(m_session_list, &QListWidget::currentItemChanged, this, &SessionManager::currentItemChanged);

    // special session "Default" is always on top of the list
    m_session_list->addItem(QStringLiteral("Default"));

    QStringList sessions = m_settings->getSessionNames();
    if(sessions.size() > 0) {
        if(sessions.contains(QStringLiteral("Default"))) {
            sessions.removeOne(QStringLiteral("Default"));
        }
        sessions.sort();
        m_session_list->addItems(sessions);
    }
    QList<QListWidgetItem *> items = m_session_list->findItems(m_settings->getCurrentSessionName(), Qt::MatchExactly);
    if (items.size() > 0) {
        m_session_list->setCurrentItem(items.at(0));
        QFont font = m_current_item->font();
        font.setBold(true);
        m_current_item->setFont(font);
    }

    m_current_session = m_current_item;
    m_bt_switch->setEnabled(false);
    connect(m_bt_switch, &QPushButton::clicked, this, &SessionManager::switchSession);
    connect(m_bt_delete, &QPushButton::clicked, this, &SessionManager::removeSession);
    connect(m_bt_rename, &QPushButton::clicked, this, &SessionManager::renameSession);

    connect(m_session_list->itemDelegate(), &QAbstractItemDelegate::commitData, this, &SessionManager::currentTextChanged);

}

void SessionManager::currentItemChanged(QListWidgetItem *current, QListWidgetItem */*previous*/)
{

    if(current != m_current_session)
        m_bt_switch->setEnabled(true);

    m_current_item = current;
    if(current->text() == QStringLiteral("Default")){
        m_bt_delete->setEnabled(false);
        m_bt_rename->setEnabled(false);
    } else {
        m_bt_delete->setEnabled(true);
        m_bt_rename->setEnabled(true);
    }
}

/*
 * stackoverflow.com/questions/9410039/how-to-issue-signal-each-time-a-row-is-edited-in-qlistqidget
 * validation is the problem here. I can't get the editor to beeing left open after
 * a replacment text is set
 */
void SessionManager::currentTextChanged(QWidget* pLineEdit)
{
    static bool recall = false;
    if(recall) {
        recall = false;
        return;
    }
    QLineEdit *lineEdit = reinterpret_cast<QLineEdit*>(pLineEdit);
    QString newSessionName = lineEdit->text();

    if( newSessionName != m_previousItemText ) {

        QList<QListWidgetItem *> items = m_session_list->findItems(newSessionName, Qt::MatchExactly);

        if(items.size() > 1) {
            QString newText;
            int index = newSessionName.lastIndexOf("-");
            if(index > -1) {
                bool ok;
                int orderNum = newSessionName.right(index).toInt(&ok);
                if(ok) {
                    orderNum++;
                    newText = newSessionName.left(index) + QString::number(orderNum);
                } else {
                    newText = newSessionName + QStringLiteral("-%1").arg(1);
                }

            } else {
                newText = newSessionName + QStringLiteral("-%1").arg(1);
            }
            //m_current_item->setText(newText);
            recall = true;
            lineEdit->setText(newText);
            lineEdit->selectAll();
            lineEdit->setFocus();
            return;
        }

        if(m_isRenaming) {
            recall = true;
            m_session_list->closePersistentEditor(m_current_item);
            emit sessionRenamed(m_previousItemText, newSessionName );
            m_isRenaming = false;
        } else if (m_isCloning) {
            emit sessionCloned(m_previousItemText, newSessionName );
            m_isCloning = false;
        } else {
            qDebug() << "This should never be reached";
        }
    }
    m_bt_rename->setEnabled(true);
    // m_bt_clone->setEnabled(true);

}

void SessionManager::switchSession()
{
    m_bt_switch->setEnabled(false);
    emit sessionSwitched(m_current_item->text());
    QFont font = m_current_item->font();
    if(m_current_session)
        m_current_session->setFont(font);
    font.setBold(true);
    m_current_item->setFont(font);

    m_current_session = m_current_item;
}

/**
 * @brief SessionManager::removeSession
 */
void SessionManager::removeSession()
{
    emit sessionRemoved(m_current_item->text());
    QListWidgetItem *remove_item = m_current_item;

    if(m_current_session == remove_item) {
        // the currently used session was removed
        // we would be better off, switching to a different one
        // the default session is save to switch too
        m_session_list->setCurrentRow(0);
        switchSession();
    } else {
        // otherwise we select the current session
        m_session_list->setCurrentRow(m_session_list->row(m_current_session));
        m_bt_switch->setEnabled(false);
    }
    m_session_list->takeItem(m_session_list->row(remove_item));
}

/**
 * @brief SessionManager::cloneSession
 */
void SessionManager::cloneSession()
{
    m_previousItemText = m_current_item->text();
    QListWidgetItem *new_item = new QListWidgetItem(m_previousItemText, m_session_list);
    new_item->setFlags(new_item->flags() | Qt::ItemIsEditable);

    m_session_list->setCurrentItem(new_item);
    m_isCloning = true;
    m_bt_clone->setEnabled(false);
    m_session_list->editItem(m_current_item);
}
/**
 * @brief SessionManager::renameSession
 */
void SessionManager::renameSession()
{
   m_previousItemText = m_current_item->text();
   m_current_item->setFlags(m_current_item->flags() | Qt::ItemIsEditable);
   m_isRenaming = true;
   m_bt_rename->setEnabled(false);
   m_session_list->openPersistentEditor(m_current_item);
//   m_session_list->editItem(m_current_item);

}