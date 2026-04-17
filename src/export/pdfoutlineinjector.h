#ifndef PDFOUTLINEINJECTOR_H
#define PDFOUTLINEINJECTOR_H

#include <QByteArray>
#include <QString>
#include <QVector>

namespace vnotex {

struct PdfOutlineItem {
  QString m_title;
  int m_level = 1;
  int m_page = 0;
};

class PdfOutlineInjector {
public:
  static QByteArray addOutline(const QByteArray &p_pdf, const QVector<PdfOutlineItem> &p_items);
};

} // namespace vnotex

#endif // PDFOUTLINEINJECTOR_H
