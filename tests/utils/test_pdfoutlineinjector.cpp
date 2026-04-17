#include <QtTest>

#include <export/pdfoutlineinjector.h>

namespace tests {

class TestPdfOutlineInjector : public QObject {
  Q_OBJECT

private slots:
  void testEmptyOutlineKeepsPdf();
  void testAddOutline();

private:
  QByteArray buildMinimalPdf() const;
};

QByteArray TestPdfOutlineInjector::buildMinimalPdf() const {
  QByteArray pdf("%PDF-1.4\n");
  QVector<qint64> offsets(5, 0);

  auto appendObject = [&](int p_number, const QByteArray &p_content) {
    offsets[p_number] = pdf.size();
    pdf += QByteArray::number(p_number) + " 0 obj\n";
    pdf += p_content;
    pdf += "\nendobj\n";
  };

  appendObject(1, "<< /Type /Catalog /Pages 2 0 R >>");
  appendObject(2, "<< /Type /Pages /Kids [3 0 R 4 0 R] /Count 2 >>");
  appendObject(3, "<< /Type /Page /Parent 2 0 R /MediaBox [0 0 612 792] >>");
  appendObject(4, "<< /Type /Page /Parent 2 0 R /MediaBox [0 0 612 792] >>");

  const auto xref = pdf.size();
  pdf += "xref\n";
  pdf += "0 5\n";
  pdf += "0000000000 65535 f \n";
  for (int i = 1; i < offsets.size(); ++i) {
    pdf += QByteArray::number(offsets[i]).rightJustified(10, '0') + " 00000 n \n";
  }
  pdf += "trailer\n";
  pdf += "<< /Size 5 /Root 1 0 R >>\n";
  pdf += "startxref\n";
  pdf += QByteArray::number(xref) + "\n";
  pdf += "%%EOF\n";
  return pdf;
}

void TestPdfOutlineInjector::testEmptyOutlineKeepsPdf() {
  const auto pdf = buildMinimalPdf();
  QCOMPARE(vnotex::PdfOutlineInjector::addOutline(pdf, {}), pdf);
}

void TestPdfOutlineInjector::testAddOutline() {
  const auto pdf = buildMinimalPdf();

  QVector<vnotex::PdfOutlineItem> items;
  vnotex::PdfOutlineItem chapter;
  chapter.m_title = QStringLiteral("第一章");
  chapter.m_level = 1;
  chapter.m_page = 0;
  items.append(chapter);

  vnotex::PdfOutlineItem section;
  section.m_title = QStringLiteral("Section");
  section.m_level = 2;
  section.m_page = 1;
  items.append(section);

  const auto out = vnotex::PdfOutlineInjector::addOutline(pdf, items);
  QVERIFY(out.size() > pdf.size());
  QVERIFY(out.contains("/Type /Outlines"));
  QVERIFY(out.contains("/PageMode /UseOutlines"));
  QVERIFY(out.contains("/Outlines 5 0 R"));
  QVERIFY(out.contains("/Dest [3 0 R /Fit]"));
  QVERIFY(out.contains("/Dest [4 0 R /Fit]"));
  QVERIFY(out.contains("/Count 2"));
  QVERIFY(out.contains("/Prev "));
  QVERIFY(out.contains("/Title <FEFF7B2C4E007AE0>"));
}

} // namespace tests

QTEST_GUILESS_MAIN(tests::TestPdfOutlineInjector)
#include "test_pdfoutlineinjector.moc"
