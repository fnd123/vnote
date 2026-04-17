#include "pdfoutlineinjector.h"

#include <algorithm>
#include <QMap>
#include <QPair>
#include <QRegularExpression>
#include <QSet>

using namespace vnotex;

namespace {

struct PdfObject {
  int m_number = 0;
  int m_generation = 0;
  QString m_content;
};

struct XrefEntry {
  int m_number = 0;
  int m_generation = 0;
  qint64 m_offset = 0;
};

struct OutlineNode {
  PdfOutlineItem m_item;
  int m_parent = 0;
  QVector<int> m_children;
  int m_objectNumber = 0;
};

QString pdfRef(int p_number, int p_generation) {
  return QStringLiteral("%1 %2 R").arg(p_number).arg(p_generation);
}

QString pdfUtf16HexString(const QString &p_text) {
  QByteArray bytes;
  bytes.append(char(0xFE));
  bytes.append(char(0xFF));

  const auto text = p_text.toStdU16String();
  for (const auto ch : text) {
    bytes.append(char((ch >> 8) & 0xff));
    bytes.append(char(ch & 0xff));
  }

  static const char hex[] = "0123456789ABCDEF";
  QString out;
  out.reserve(bytes.size() * 2 + 2);
  out.append(QLatin1Char('<'));
  for (const auto byte : bytes) {
    const auto value = static_cast<unsigned char>(byte);
    out.append(QLatin1Char(hex[value >> 4]));
    out.append(QLatin1Char(hex[value & 0x0f]));
  }
  out.append(QLatin1Char('>'));
  return out;
}

QMap<int, PdfObject> parseObjects(const QString &p_pdfText) {
  QMap<int, PdfObject> objects;
  const QRegularExpression objectRe(
      QStringLiteral("(\\d+)\\s+(\\d+)\\s+obj\\b(.*?)\\bendobj"),
      QRegularExpression::DotMatchesEverythingOption);

  auto it = objectRe.globalMatch(p_pdfText);
  while (it.hasNext()) {
    const auto match = it.next();
    PdfObject obj;
    obj.m_number = match.captured(1).toInt();
    obj.m_generation = match.captured(2).toInt();
    obj.m_content = match.captured(3).trimmed();
    objects.insert(obj.m_number, obj);
  }

  return objects;
}

bool findLastRef(const QString &p_text, const QString &p_name, QPair<int, int> &p_ref) {
  const QRegularExpression refRe(
      QStringLiteral("/%1\\s+(\\d+)\\s+(\\d+)\\s+R").arg(QRegularExpression::escape(p_name)));
  auto it = refRe.globalMatch(p_text);
  bool found = false;
  while (it.hasNext()) {
    const auto match = it.next();
    p_ref = qMakePair(match.captured(1).toInt(), match.captured(2).toInt());
    found = true;
  }

  return found;
}

qint64 findLastStartXref(const QString &p_pdfText) {
  const QRegularExpression startXrefRe(QStringLiteral("startxref\\s+(\\d+)"));
  auto it = startXrefRe.globalMatch(p_pdfText);
  qint64 startXref = -1;
  while (it.hasNext()) {
    startXref = it.next().captured(1).toLongLong();
  }

  return startXref;
}

QVector<QPair<int, int>> extractRefs(const QString &p_text) {
  QVector<QPair<int, int>> refs;
  const QRegularExpression refRe(QStringLiteral("(\\d+)\\s+(\\d+)\\s+R"));
  auto it = refRe.globalMatch(p_text);
  while (it.hasNext()) {
    const auto match = it.next();
    refs.append(qMakePair(match.captured(1).toInt(), match.captured(2).toInt()));
  }

  return refs;
}

void collectPageRefs(const QMap<int, PdfObject> &p_objects, int p_objectNumber,
                     QVector<QPair<int, int>> &p_pages, QSet<int> &p_visited) {
  if (p_visited.contains(p_objectNumber)) {
    return;
  }
  p_visited.insert(p_objectNumber);

  const auto it = p_objects.constFind(p_objectNumber);
  if (it == p_objects.constEnd()) {
    return;
  }

  const auto &content = it->m_content;
  if (content.contains(QRegularExpression(QStringLiteral("/Type\\s*/Page\\b")))) {
    p_pages.append(qMakePair(it->m_number, it->m_generation));
    return;
  }

  const QRegularExpression kidsRe(QStringLiteral("/Kids\\s*\\[(.*?)\\]"),
                                  QRegularExpression::DotMatchesEverythingOption);
  const auto kidsMatch = kidsRe.match(content);
  if (!kidsMatch.hasMatch()) {
    return;
  }

  const auto kids = extractRefs(kidsMatch.captured(1));
  for (const auto &kid : kids) {
    collectPageRefs(p_objects, kid.first, p_pages, p_visited);
  }
}

QVector<QPair<int, int>> collectPageRefs(const QMap<int, PdfObject> &p_objects,
                                         const PdfObject &p_catalog) {
  QPair<int, int> pagesRef;
  if (!findLastRef(p_catalog.m_content, QStringLiteral("Pages"), pagesRef)) {
    return {};
  }

  QVector<QPair<int, int>> pages;
  QSet<int> visited;
  collectPageRefs(p_objects, pagesRef.first, pages, visited);
  return pages;
}

QString updateCatalog(QString p_catalog, int p_outlinesObjectNumber) {
  p_catalog.remove(
      QRegularExpression(QStringLiteral("/Outlines\\s+\\d+\\s+\\d+\\s+R")));

  const QString pageMode = QStringLiteral("/PageMode /UseOutlines");
  const QRegularExpression pageModeRe(QStringLiteral("/PageMode\\s*/\\w+"));
  if (p_catalog.contains(pageModeRe)) {
    p_catalog.replace(pageModeRe, pageMode);
  } else {
    const int dictEnd = p_catalog.lastIndexOf(QStringLiteral(">>"));
    if (dictEnd != -1) {
      p_catalog.insert(dictEnd, QStringLiteral(" ") + pageMode);
    }
  }

  const int dictEnd = p_catalog.lastIndexOf(QStringLiteral(">>"));
  if (dictEnd == -1) {
    return QString();
  }

  p_catalog.insert(dictEnd,
                   QStringLiteral(" /Outlines %1 0 R").arg(p_outlinesObjectNumber));
  return p_catalog;
}

QVector<OutlineNode> buildOutlineTree(const QVector<PdfOutlineItem> &p_items) {
  QVector<OutlineNode> nodes;
  nodes.append(OutlineNode());

  QVector<int> stack;
  stack.append(0);

  for (const auto &item : p_items) {
    const auto title = item.m_title.trimmed();
    if (title.isEmpty()) {
      continue;
    }

    OutlineNode node;
    node.m_item = item;
    node.m_item.m_title = title;
    node.m_item.m_level = qBound(1, node.m_item.m_level, 6);
    node.m_item.m_page = qMax(0, node.m_item.m_page);

    while (stack.size() > 1 &&
           nodes[stack.constLast()].m_item.m_level >= node.m_item.m_level) {
      stack.removeLast();
    }

    node.m_parent = stack.constLast();
    nodes.append(node);
    const int nodeIndex = nodes.size() - 1;
    nodes[node.m_parent].m_children.append(nodeIndex);
    stack.append(nodeIndex);
  }

  return nodes;
}

int descendantCount(const QVector<OutlineNode> &p_nodes, int p_index) {
  int count = p_nodes[p_index].m_children.size();
  for (const auto child : p_nodes[p_index].m_children) {
    count += descendantCount(p_nodes, child);
  }

  return count;
}

QString outlineItemObject(const QVector<OutlineNode> &p_nodes, int p_index,
                          const QVector<QPair<int, int>> &p_pages, int p_outlinesObjectNumber) {
  const auto &node = p_nodes[p_index];
  const int parentObj =
      node.m_parent == 0 ? p_outlinesObjectNumber : p_nodes[node.m_parent].m_objectNumber;

  const auto page = p_pages[qMin(node.m_item.m_page, p_pages.size() - 1)];
  QString dict = QStringLiteral("<< /Title %1 /Parent %2 0 R /Dest [%3 /Fit]")
                     .arg(pdfUtf16HexString(node.m_item.m_title))
                     .arg(parentObj)
                     .arg(pdfRef(page.first, page.second));

  const auto siblings = p_nodes[node.m_parent].m_children;
  const int siblingIndex = siblings.indexOf(p_index);
  if (siblingIndex > 0) {
    dict += QStringLiteral(" /Prev %1 0 R").arg(p_nodes[siblings[siblingIndex - 1]].m_objectNumber);
  }
  if (siblingIndex + 1 < siblings.size()) {
    dict += QStringLiteral(" /Next %1 0 R").arg(p_nodes[siblings[siblingIndex + 1]].m_objectNumber);
  }

  if (!node.m_children.isEmpty()) {
    dict += QStringLiteral(" /First %1 0 R /Last %2 0 R /Count %3")
                .arg(p_nodes[node.m_children.constFirst()].m_objectNumber)
                .arg(p_nodes[node.m_children.constLast()].m_objectNumber)
                .arg(descendantCount(p_nodes, p_index));
  }

  dict += QStringLiteral(" >>");
  return dict;
}

QByteArray xrefLine(qint64 p_offset, int p_generation) {
  return QByteArray::number(p_offset).rightJustified(10, '0') + " " +
         QByteArray::number(p_generation).rightJustified(5, '0') + " n \n";
}

void appendObject(QByteArray &p_pdf, QVector<XrefEntry> &p_entries, int p_number, int p_generation,
                  const QString &p_content) {
  XrefEntry entry;
  entry.m_number = p_number;
  entry.m_generation = p_generation;
  entry.m_offset = p_pdf.size();
  p_entries.append(entry);

  p_pdf += QByteArray::number(p_number) + " " + QByteArray::number(p_generation) + " obj\n";
  p_pdf += p_content.toUtf8();
  p_pdf += "\nendobj\n";
}

void appendXrefAndTrailer(QByteArray &p_pdf, QVector<XrefEntry> p_entries, int p_size,
                          const QPair<int, int> &p_rootRef, qint64 p_prevXref) {
  std::sort(p_entries.begin(), p_entries.end(), [](const XrefEntry &p_lhs,
                                                   const XrefEntry &p_rhs) {
    return p_lhs.m_number < p_rhs.m_number;
  });

  const qint64 xrefOffset = p_pdf.size();
  p_pdf += "xref\n";
  for (const auto &entry : p_entries) {
    p_pdf += QByteArray::number(entry.m_number) + " 1\n";
    p_pdf += xrefLine(entry.m_offset, entry.m_generation);
  }

  p_pdf += "trailer\n";
  p_pdf += "<< /Size " + QByteArray::number(p_size) + " /Root " +
           QByteArray::number(p_rootRef.first) + " " + QByteArray::number(p_rootRef.second) +
           " R /Prev " + QByteArray::number(p_prevXref) + " >>\n";
  p_pdf += "startxref\n";
  p_pdf += QByteArray::number(xrefOffset) + "\n";
  p_pdf += "%%EOF\n";
}

} // namespace

QByteArray PdfOutlineInjector::addOutline(const QByteArray &p_pdf,
                                          const QVector<PdfOutlineItem> &p_items) {
  if (p_pdf.isEmpty() || p_items.isEmpty()) {
    return p_pdf;
  }

  const auto pdfText = QString::fromLatin1(p_pdf);
  const auto prevXref = findLastStartXref(pdfText);
  if (prevXref < 0) {
    return p_pdf;
  }

  QPair<int, int> rootRef;
  if (!findLastRef(pdfText, QStringLiteral("Root"), rootRef)) {
    return p_pdf;
  }

  auto objects = parseObjects(pdfText);
  const auto rootIt = objects.constFind(rootRef.first);
  if (rootIt == objects.constEnd()) {
    return p_pdf;
  }

  const auto pages = collectPageRefs(objects, *rootIt);
  if (pages.isEmpty()) {
    return p_pdf;
  }

  auto nodes = buildOutlineTree(p_items);
  if (nodes.size() <= 1 || nodes[0].m_children.isEmpty()) {
    return p_pdf;
  }

  int maxObjectNumber = rootRef.first;
  for (auto it = objects.constBegin(); it != objects.constEnd(); ++it) {
    maxObjectNumber = qMax(maxObjectNumber, it.key());
  }

  const int outlinesObjectNumber = maxObjectNumber + 1;
  int nextObjectNumber = outlinesObjectNumber + 1;
  for (int i = 1; i < nodes.size(); ++i) {
    nodes[i].m_objectNumber = nextObjectNumber++;
  }

  const auto catalog = updateCatalog(rootIt->m_content, outlinesObjectNumber);
  if (catalog.isEmpty()) {
    return p_pdf;
  }

  QByteArray output = p_pdf;
  if (!output.endsWith('\n')) {
    output += '\n';
  }

  QVector<XrefEntry> entries;
  appendObject(output, entries, rootRef.first, rootRef.second, catalog);

  const auto &rootChildren = nodes[0].m_children;
  const QString outlinesObject =
      QStringLiteral("<< /Type /Outlines /First %1 0 R /Last %2 0 R /Count %3 >>")
          .arg(nodes[rootChildren.constFirst()].m_objectNumber)
          .arg(nodes[rootChildren.constLast()].m_objectNumber)
          .arg(descendantCount(nodes, 0));
  appendObject(output, entries, outlinesObjectNumber, 0, outlinesObject);

  for (int i = 1; i < nodes.size(); ++i) {
    appendObject(output, entries, nodes[i].m_objectNumber, 0,
                 outlineItemObject(nodes, i, pages, outlinesObjectNumber));
  }

  appendXrefAndTrailer(output, entries, nextObjectNumber, rootRef, prevXref);
  return output;
}
