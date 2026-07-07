/**
 * Taiga
 * Copyright (C) 2010-2024, Eren Okka
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <https://www.gnu.org/licenses/>.
 */

#include "settings.hpp"

#include <QMap>
#include <QXmlStreamReader>
#include <chrono>

#include "base/log.hpp"
#include "base/xml.hpp"
#include "taiga/accounts.hpp"
#include "taiga/settings.hpp"

#define XML_ATTR(name) xml.attributes().value(name)
#define XML_ATTR_INT(name) XML_ATTR(name).toInt()
#define XML_ATTR_STR(name) XML_ATTR(name).toString().toStdString()

namespace compat::v1 {

void parseAccountElement(QXmlStreamReader&, const taiga::Settings&, const taiga::Accounts&);
void parseAnimeElement(QXmlStreamReader&, const taiga::Settings&);
void parseGenericSettingsElement(QXmlStreamReader&, const taiga::Settings&, QStringList path);
void parseRecognitionElement(QXmlStreamReader&, const taiga::Settings&);

void readSettings(const std::string& path, const taiga::Settings& settings,
                  const taiga::Accounts& accounts) {
  base::XmlFileReader xml;

  if (!xml.open(QString::fromStdString(path))) {
    LOGE("{}", xml.file().errorString().toStdString());
    return;
  }

  if (!xml.readElement(u"settings")) {
    xml.raiseError("Invalid settings file.");
  }

  while (xml.readNextStartElement()) {
    if (xml.name() == u"account") {
      parseAccountElement(xml, settings, accounts);
    } else if (xml.name() == u"anime") {
      parseAnimeElement(xml, settings);
    } else if (xml.name() == u"recognition") {
      parseRecognitionElement(xml, settings);
    } else if (xml.name() == u"program" || xml.name() == u"announce" || xml.name() == u"rss") {
      parseGenericSettingsElement(xml, settings, {xml.name().toString()});
    } else {
      xml.skipCurrentElement();
    }
  }

  if (xml.hasError()) {
    LOGE("{}", xml.errorString().toStdString());
  }
}

void parseAccountElement(QXmlStreamReader& xml, const taiga::Settings& settings,
                         const taiga::Accounts& accounts) {
  while (xml.readNextStartElement()) {
    if (xml.name() == u"update") {
      settings.setService(XML_ATTR_STR(u"activeservice"));
      xml.skipCurrentElement();

    } else if (xml.name() == u"anilist") {
      accounts.setAnilistUsername(XML_ATTR_STR(u"username"));
      accounts.setAnilistToken(XML_ATTR_STR(u"token"));
      xml.skipCurrentElement();

    } else if (xml.name() == u"kitsu") {
      accounts.setKitsuEmail(XML_ATTR_STR(u"email"));
      accounts.setKitsuUsername(XML_ATTR_STR(u"username"));
      accounts.setKitsuPassword(XML_ATTR_STR(u"password"));
      xml.skipCurrentElement();

    } else if (xml.name() == u"myanimelist") {
      accounts.setMyanimelistUsername(XML_ATTR_STR(u"username"));
      accounts.setMyanimelistAccessToken(XML_ATTR_STR(u"accesstoken"));
      accounts.setMyanimelistRefreshToken(XML_ATTR_STR(u"refreshtoken"));
      xml.skipCurrentElement();

    } else {
      xml.skipCurrentElement();
    }
  }
}

void parseAnimeElement(QXmlStreamReader& xml, const taiga::Settings& settings) {
  std::vector<std::string> libraryFolders;

  while (xml.readNextStartElement()) {
    if (xml.name() == u"folders") {
      while (xml.readNextStartElement()) {
        if (xml.name() == u"root") {
          libraryFolders.push_back(XML_ATTR_STR(u"folder"));
          xml.skipCurrentElement();
        } else {
          xml.skipCurrentElement();
        }
      }

    } else {
      xml.skipCurrentElement();
    }
  }

  settings.setLibraryFolders(libraryFolders);
}

void parseRecognitionElement(QXmlStreamReader& xml, const taiga::Settings& settings) {
  while (xml.readNextStartElement()) {
    if (xml.name() == u"general") {
      const auto seconds = std::chrono::seconds{XML_ATTR_INT(u"detectioninterval")};
      settings.setMediaDetectionInterval(seconds);
      xml.skipCurrentElement();

    } else {
      xml.skipCurrentElement();
    }
  }
}

QString mappedSettingsKey(const QString& oldPath) {
  static const QMap<QString, QString> table{
      {"program/general/autostart", "program.general.autostart"},
      {"program/general/closetotray", "program.general.closeToTray"},
      {"program/general/externallinks", "program.general.externalLinks"},
      {"program/general/hidesidebar", "program.general.hideSidebar"},
      {"program/general/minimizetotray", "program.general.minimizeToTray"},
      {"program/startup/checkepisodes", "program.startup.checkEpisodes"},
      {"program/startup/checkversion", "program.startup.checkVersion"},
      {"program/startup/minimize", "program.startup.minimize"},
      {"program/connection/norevoke", "program.connection.noRevoke"},
      {"program/connection/reuseactive", "program.connection.reuseActive"},
      {"program/proxy/host", "program.proxy.host"},
      {"program/proxy/password", "program.proxy.password"},
      {"program/proxy/username", "program.proxy.username"},
      {"program/list/action/doubleclick", "program.list.doubleClickAction"},
      {"program/list/action/middleclick", "program.list.middleClickAction"},
      {"program/list/action/titlelang", "program.list.titleLanguage"},
      {"program/list/filter/episodes/highlight", "program.list.highlightNewEpisodes"},
      {"program/list/filter/episodes/highlightedontop", "program.list.highlightedOnTop"},
      {"program/list/progress/showaired", "program.list.progressAired"},
      {"program/list/progress/showavailable", "program.list.progressAvailable"},
      {"program/exit/rememberposition", "program.exit.rememberPosition"},
      {"program/position/width", "program.position.width"},
      {"program/position/height", "program.position.height"},
      {"announce/discord/enabled", "announce.discord.enabled"},
      {"announce/discord/group", "announce.discord.group"},
      {"announce/discord/time", "announce.discord.time"},
      {"announce/discord/username", "announce.discord.username"},
      {"announce/http/enabled", "announce.http.enabled"},
      {"announce/http/format", "announce.http.format"},
      {"announce/http/url", "announce.http.url"},
      {"announce/mirc/channels", "announce.mirc.channels"},
      {"announce/mirc/command", "announce.mirc.command"},
      {"announce/mirc/enabled", "announce.mirc.enabled"},
      {"announce/mirc/format", "announce.mirc.format"},
      {"announce/mirc/mode", "announce.mirc.mode"},
      {"announce/mirc/multiserver", "announce.mirc.multiServer"},
      {"announce/mirc/service", "announce.mirc.service"},
      {"announce/mirc/useaction", "announce.mirc.useAction"},
      {"rss/torrent/appmode", "rss.torrent.appMode"},
      {"rss/torrent/apppath", "rss.torrent.appPath"},
      {"rss/torrent/autocheck", "rss.torrent.autoCheck"},
      {"rss/torrent/checkinterval", "rss.torrent.checkInterval"},
      {"rss/torrent/createsubfolder", "rss.torrent.createSubfolder"},
      {"rss/torrent/downloadlocation", "rss.torrent.downloadLocation"},
      {"rss/torrent/downloadsortby", "rss.torrent.downloadSortBy"},
      {"rss/torrent/downloadsortorder", "rss.torrent.downloadSortOrder"},
      {"rss/torrent/fallbackfolder", "rss.torrent.fallbackFolder"},
      {"rss/torrent/filedownloadlocation", "rss.torrent.fileDownloadLocation"},
      {"rss/torrent/newaction", "rss.torrent.newAction"},
      {"rss/torrent/openapp", "rss.torrent.openApp"},
      {"rss/torrent/search", "rss.torrent.search"},
      {"rss/torrent/source", "rss.torrent.source"},
      {"rss/torrent/useanimefolder", "rss.torrent.useAnimeFolder"},
      {"rss/torrent/usemagnet", "rss.torrent.useMagnet"},
      {"rss/torrent/filters/archivemaxcount", "rss.torrent.filters.archiveMaxCount"},
      {"rss/torrent/filters/enabled", "rss.torrent.filters.enabled"},
  };
  return table.value(oldPath.toLower());
}

void writeMappedSetting(const taiga::Settings& settings, const QString& key,
                        const QString& value) {
  if (value.compare("true", Qt::CaseInsensitive) == 0 || value == "1") {
    settings.setBoolValue(key, true);
    return;
  }
  if (value.compare("false", Qt::CaseInsensitive) == 0 || value == "0") {
    settings.setBoolValue(key, false);
    return;
  }

  bool ok = false;
  const auto intValue = value.toInt(&ok);
  if (ok) {
    settings.setIntValue(key, intValue);
  } else {
    settings.setStringValue(key, value);
  }
}

void parseGenericSettingsElement(QXmlStreamReader& xml, const taiga::Settings& settings,
                                 QStringList path) {
  for (const auto& attribute : xml.attributes()) {
    const auto key = mappedSettingsKey((path + QStringList{attribute.name().toString()}).join('/'));
    if (!key.isEmpty()) {
      writeMappedSetting(settings, key, attribute.value().toString());
    }
  }

  while (xml.readNextStartElement()) {
    auto childPath = path;
    childPath.push_back(xml.name().toString());
    parseGenericSettingsElement(xml, settings, childPath);
  }
}

}  // namespace compat::v1
