/*
 * Copyright 2013 Canonical Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation; version 3.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * Authors:
 *      Renato Araujo Oliveira Filho <renato@canonical.com>
 */

import QtQuick 2.0
import Ubuntu.Components 0.1
import Ubuntu.Components.ListItems 0.1 as ListItem
import Unity.Indicators 0.1 as Indicators
import "../.."

Page {
    id: page
    anchors.fill: parent
    title: "Plugin list"

    IndicatorsDataModel {
        id: indicatorsModel
    }

    ListView {
        id: mainMenu
        objectName: "mainMenu"
        anchors.fill: parent
        model: indicatorsModel

        delegate: Indicators.MenuItem {
            anchors.left: parent.left
            anchors.right: parent.right
            progression: isValid
            objectName: identifier

            Label {
                anchors.left: parent.left
                anchors.leftMargin: units.gu(0.5)
                anchors.verticalCenter: parent.verticalCenter
                text: title
            }

            onClicked: {
                if (progression) {
                    var new_page = Qt.createComponent("IndicatorsPage.qml");
                    page.pageStack.push(new_page.createObject(pages), {"indicatorProperties" : model.indicatorProperties, "pageSource" : model.pageSource});
                }
            }
        }
    }
}
