/**
 * Taiga
 * Copyright (C) 2010-2024, Eren Okka
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#include "rss.hpp"

#include <QByteArray>
#include <QXmlStreamReader>

namespace rss {

namespace {

std::string text(QXmlStreamReader& xml) {
  return xml.readElementText(QXmlStreamReader::IncludeChildElements).trimmed().toStdString();
}

void parseItemElement(QXmlStreamReader& xml, Item& item) {
  const auto name = xml.name();
  if (name == u"title") {
    item.title = text(xml);
  } else if (name == u"link") {
    item.link = text(xml);
  } else if (name == u"description") {
    item.description = text(xml);
  } else if (name == u"author") {
    item.author = text(xml);
  } else if (name == u"comments") {
    item.comments = text(xml);
  } else if (name == u"pubDate") {
    item.pub_date = text(xml);
  } else if (name == u"category") {
    item.category.domain = xml.attributes().value("domain").toString().toStdString();
    item.category.value = text(xml);
  } else if (name == u"enclosure") {
    item.enclosure.url = xml.attributes().value("url").toString().toStdString();
    item.enclosure.length = xml.attributes().value("length").toString().toStdString();
    item.enclosure.type = xml.attributes().value("type").toString().toStdString();
    xml.skipCurrentElement();
  } else if (name == u"guid") {
    item.guid.is_permalink = xml.attributes().value("isPermaLink").toString() != "false";
    item.guid.value = text(xml);
  } else if (name == u"source") {
    item.source.url = xml.attributes().value("url").toString().toStdString();
    item.source.name = text(xml);
  } else {
    const auto key = xml.qualifiedName().toString().toStdString();
    item.namespace_elements[key] = text(xml);
  }
}

Item parseItem(QXmlStreamReader& xml) {
  Item item;
  while (xml.readNextStartElement()) {
    parseItemElement(xml, item);
  }
  return item;
}

}  // namespace

Feed parseDocument(const QByteArray& data) {
  Feed feed;
  QXmlStreamReader xml{data};

  while (xml.readNextStartElement()) {
    if (xml.name() != u"rss" && xml.name() != u"feed") {
      xml.skipCurrentElement();
      continue;
    }

    while (xml.readNextStartElement()) {
      if (xml.name() != u"channel") {
        xml.skipCurrentElement();
        continue;
      }

      while (xml.readNextStartElement()) {
        if (xml.name() == u"title") {
          feed.channel.title = text(xml);
        } else if (xml.name() == u"link") {
          feed.channel.link = text(xml);
        } else if (xml.name() == u"description") {
          feed.channel.description = text(xml);
        } else if (xml.name() == u"item") {
          auto item = parseItem(xml);
          if (!item.title.empty() || !item.description.empty()) {
            feed.items.push_back(std::move(item));
          }
        } else {
          xml.skipCurrentElement();
        }
      }
    }
  }

  return feed;
}

}  // namespace rss
