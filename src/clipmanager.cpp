/***************************************************************************
 *   Copyright (C) 2008 by Jean-Baptiste Mardelle (jb@kdenlive.org)        *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 *   This program is distributed in the hope that it will be useful,       *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this program; if not, write to the                         *
 *   Free Software Foundation, Inc.,                                       *
 *   51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA          *
 ***************************************************************************/
#include <KDebug>
#include <KFileDialog>
#include <kio/netaccess.h>

#include "addclipcommand.h"
#include "kdenlivesettings.h"
#include "clipmanager.h"
#include "docclipbase.h"
#include "kdenlivedoc.h"

#include <mlt++/Mlt.h>

ClipManager::ClipManager(KdenliveDoc *doc): m_doc(doc), m_audioThumbsEnabled(false), m_audioThumbsQueue(QList <QString> ()), m_generatingAudioId(QString()) {
    m_clipIdCounter = 1;
    m_folderIdCounter = 1;
}

ClipManager::~ClipManager() {
    qDeleteAll(m_clipList);
}

void ClipManager::checkAudioThumbs() {
    if (m_audioThumbsEnabled == KdenliveSettings::audiothumbnails()) return;
    m_audioThumbsEnabled = KdenliveSettings::audiothumbnails();
    for (int i = 0; i < m_clipList.count(); i++) {
        if (m_audioThumbsEnabled) m_audioThumbsQueue.append(m_clipList.at(i)->getId());
        else m_clipList.at(i)->slotClearAudioCache();
    }
    if (m_audioThumbsEnabled) {
        if (m_generatingAudioId.isEmpty()) startAudioThumbsGeneration();
    } else {
        m_audioThumbsQueue.clear();
        m_generatingAudioId = QString();
    }
}

void ClipManager::askForAudioThumb(const QString &id) {
    DocClipBase *clip = getClipById(id);
    if (clip && KdenliveSettings::audiothumbnails()) {
        m_audioThumbsQueue.append(id);
        if (m_generatingAudioId.isEmpty()) startAudioThumbsGeneration();
    }
}

void ClipManager::startAudioThumbsGeneration() {
    if (!KdenliveSettings::audiothumbnails()) {
        m_audioThumbsQueue.clear();
        m_generatingAudioId = QString();
        return;
    }
    if (!m_audioThumbsQueue.isEmpty()) {
        m_generatingAudioId = m_audioThumbsQueue.takeFirst();
        DocClipBase *clip = getClipById(m_generatingAudioId);
        if (!clip || !clip->slotGetAudioThumbs())
            endAudioThumbsGeneration(m_generatingAudioId);
    } else {
        m_generatingAudioId = QString();
    }
}

void ClipManager::endAudioThumbsGeneration(const QString &requestedId) {
    if (!KdenliveSettings::audiothumbnails()) {
        m_audioThumbsQueue.clear();
        m_generatingAudioId = QString();
        return;
    }
    if (!m_audioThumbsQueue.isEmpty()) {
        if (m_generatingAudioId == requestedId) {
            startAudioThumbsGeneration();
        }
    } else {
        m_generatingAudioId = QString();
    }
}

void ClipManager::setThumbsProgress(const QString &message, int progress) {
    m_doc->setThumbsProgress(message, progress);
}

QList <DocClipBase*> ClipManager::documentClipList() const {
    return m_clipList;
}

QMap <QString, QString> ClipManager::documentFolderList() const {
    return m_folderList;
}

void ClipManager::addClip(DocClipBase *clip) {
    m_clipList.append(clip);
    const QString id = clip->getId();
    if (id.toInt() >= m_clipIdCounter) m_clipIdCounter = id.toInt() + 1;
    const QString gid = clip->getProperty("groupid");
    if (!gid.isEmpty() && gid.toInt() >= m_folderIdCounter) m_folderIdCounter = gid.toInt() + 1;
}

void ClipManager::slotDeleteClip(const QString &clipId) {
    for (int i = 0; i < m_clipList.count(); i++) {
        if (m_clipList.at(i)->getId() == clipId) {
            AddClipCommand *command = new AddClipCommand(m_doc, m_clipList.at(i)->toXML(), clipId, false);
            m_doc->commandStack()->push(command);
            break;
        }
    }
}

void ClipManager::deleteClip(const QString &clipId) {
    for (int i = 0; i < m_clipList.count(); i++) {
        if (m_clipList.at(i)->getId() == clipId) {
            DocClipBase *clip = m_clipList.takeAt(i);
            delete clip;
            clip = NULL;
            break;
        }
    }
}

DocClipBase *ClipManager::getClipAt(int pos) {
    return m_clipList.at(pos);
}

DocClipBase *ClipManager::getClipById(QString clipId) {
    //kDebug() << "++++  CLIP MAN, LOOKING FOR CLIP ID: " << clipId;
    clipId = clipId.section('_', 0, 0);
    for (int i = 0; i < m_clipList.count(); i++) {
        if (m_clipList.at(i)->getId() == clipId) {
            //kDebug() << "++++  CLIP MAN, FOUND FOR CLIP ID: " << clipId;
            return m_clipList.at(i);
        }
    }
    return NULL;
}

DocClipBase *ClipManager::getClipByResource(QString resource) {
    for (int i = 0; i < m_clipList.count(); i++) {
        if (m_clipList.at(i)->getProperty("resource") == resource) {
            return m_clipList.at(i);
        }
    }
    return NULL;
}

void ClipManager::updatePreviewSettings() {
    for (int i = 0; i < m_clipList.count(); i++) {
        if (m_clipList.at(i)->clipType() == AV || m_clipList.at(i)->clipType() == VIDEO) {
            if (m_clipList.at(i)->producerProperty("meta.media.0.codec.name") && strcmp(m_clipList.at(i)->producerProperty("meta.media.0.codec.name"), "h264") == 0) {
                if (KdenliveSettings::dropbframes()) {
                    m_clipList[i]->setProducerProperty("skip_loop_filter", "all");
                    m_clipList[i]->setProducerProperty("skip_frame", "bidir");
                } else {
                    m_clipList[i]->setProducerProperty("skip_loop_filter", "");
                    m_clipList[i]->setProducerProperty("skip_frame", "");
                }
            }
        }
    }
}

void ClipManager::resetProducersList(QList <Mlt::Producer *> prods) {
    for (int i = 0; i < m_clipList.count(); i++) {
        if (m_clipList.at(i)->numReferences() > 0) {
            m_clipList.at(i)->deleteProducers();
        }
    }
    QString id;
    for (int i = 0; i < prods.count(); i++) {
        id = prods.at(i)->get("id");
        if (id.contains('_')) id = id.section('_', 0, 0);
        DocClipBase *clip = getClipById(id);
        if (clip) {
            clip->setProducer(prods.at(i));
            kDebug() << "// // // REPLACE CLIP: " << id;
        }
    }
}

void ClipManager::slotAddClipList(const KUrl::List urls, const QString group, const QString &groupId) {
    QUndoCommand *addClips = new QUndoCommand();
    addClips->setText(i18n("Add clips"));

    foreach(const KUrl file, urls) {
        if (KIO::NetAccess::exists(file, KIO::NetAccess::SourceSide, NULL)) {
            QDomDocument doc;
            QDomElement prod = doc.createElement("producer");
            if (!group.isEmpty()) {
                prod.setAttribute("groupname", group);
                prod.setAttribute("groupid", groupId);
            }
            prod.setAttribute("resource", file.path());
            uint id = m_clipIdCounter++;
            prod.setAttribute("id", QString::number(id));
            KMimeType::Ptr type = KMimeType::findByUrl(file);
            if (type->name().startsWith("image/")) {
                prod.setAttribute("type", (int) IMAGE);
                prod.setAttribute("in", "0");
                prod.setAttribute("out", m_doc->getFramePos(KdenliveSettings::image_duration()) - 1);
            }
            new AddClipCommand(m_doc, prod, QString::number(id), true, addClips);
        }
    }
    m_doc->commandStack()->push(addClips);
}

void ClipManager::slotAddClipFile(const KUrl url, const QString group, const QString &groupId) {
    kDebug() << "/////  CLIP MANAGER, ADDING CLIP: " << url;
    QDomDocument doc;
    QDomElement prod = doc.createElement("producer");
    prod.setAttribute("resource", url.path());
    uint id = m_clipIdCounter++;
    prod.setAttribute("id", QString::number(id));
    if (!group.isEmpty()) {
        prod.setAttribute("groupname", group);
        prod.setAttribute("groupid", groupId);
    }
    KMimeType::Ptr type = KMimeType::findByUrl(url);
    if (type->name().startsWith("image/")) {
        prod.setAttribute("type", (int) IMAGE);
        prod.setAttribute("in", "0");
        prod.setAttribute("out", m_doc->getFramePos(KdenliveSettings::image_duration()) - 1);
    }
    AddClipCommand *command = new AddClipCommand(m_doc, prod, QString::number(id), true);
    m_doc->commandStack()->push(command);
}

void ClipManager::slotAddColorClipFile(const QString name, const QString color, QString duration, const QString group, const QString &groupId) {
    QDomDocument doc;
    QDomElement prod = doc.createElement("producer");
    prod.setAttribute("mlt_service", "colour");
    prod.setAttribute("colour", color);
    prod.setAttribute("type", (int) COLOR);
    uint id = m_clipIdCounter++;
    prod.setAttribute("id", QString::number(id));
    prod.setAttribute("in", "0");
    prod.setAttribute("out", m_doc->getFramePos(duration) - 1);
    prod.setAttribute("name", name);
    if (!group.isEmpty()) {
        prod.setAttribute("groupname", group);
        prod.setAttribute("groupid", groupId);
    }
    AddClipCommand *command = new AddClipCommand(m_doc, prod, QString::number(id), true);
    m_doc->commandStack()->push(command);
}

void ClipManager::slotAddSlideshowClipFile(const QString name, const QString path, int count, const QString duration, const bool loop, const bool fade, const QString &luma_duration, const QString &luma_file, const int softness, QString group, const QString &groupId) {
    QDomDocument doc;
    QDomElement prod = doc.createElement("producer");
    prod.setAttribute("resource", path);
    prod.setAttribute("type", (int) SLIDESHOW);
    uint id = m_clipIdCounter++;
    prod.setAttribute("id", QString::number(id));
    prod.setAttribute("in", "0");
    prod.setAttribute("out", m_doc->getFramePos(duration) * count - 1);
    prod.setAttribute("ttl", m_doc->getFramePos(duration));
    prod.setAttribute("luma_duration", m_doc->getFramePos(luma_duration));
    prod.setAttribute("name", name);
    prod.setAttribute("loop", loop);
    prod.setAttribute("fade", fade);
    prod.setAttribute("softness", QString::number(softness));
    prod.setAttribute("luma_file", luma_file);
    if (!group.isEmpty()) {
        prod.setAttribute("groupname", group);
        prod.setAttribute("groupid", groupId);
    }
    AddClipCommand *command = new AddClipCommand(m_doc, prod, QString::number(id), true);
    m_doc->commandStack()->push(command);
}



void ClipManager::slotAddTextClipFile(const QString titleName, const QString imagePath, const QString xml, const QString group, const QString &groupId) {
    QDomDocument doc;
    QDomElement prod = doc.createElement("producer");
    prod.setAttribute("resource", imagePath);
    prod.setAttribute("titlename", titleName);
    prod.setAttribute("xmldata", xml);
    uint id = m_clipIdCounter++;
    prod.setAttribute("id", QString::number(id));
    if (!group.isEmpty()) {
        prod.setAttribute("groupname", group);
        prod.setAttribute("groupid", groupId);
    }
    prod.setAttribute("type", (int) TEXT);
    prod.setAttribute("transparency", "1");
    prod.setAttribute("in", "0");
    prod.setAttribute("out", m_doc->getFramePos(KdenliveSettings::image_duration()) - 1);
    AddClipCommand *command = new AddClipCommand(m_doc, prod, QString::number(id), true);
    m_doc->commandStack()->push(command);
}

int ClipManager::getFreeClipId() {
    return m_clipIdCounter++;
}

int ClipManager::getFreeFolderId() {
    return m_folderIdCounter++;
}

int ClipManager::lastClipId() const {
    return m_clipIdCounter - 1;
}

QString ClipManager::projectFolder() const {
    return m_doc->projectFolder().path();
}

void ClipManager::addFolder(const QString &id, const QString &name) {
    m_folderList.insert(id, name);
}

void ClipManager::deleteFolder(const QString &id) {
    m_folderList.remove(id);
}