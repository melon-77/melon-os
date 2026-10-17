// melon-look-probe-qt: what a Qt app really gets from the desktop (font, icon theme, style, colours), for
// melon-check-look. Prints key=value lines and exits without opening a window.
#include <QApplication>
#include <QFontDatabase>
#include <QIcon>
#include <QPalette>
#include <QStyle>
#include <QTextStream>

int main(int argc, char **argv)
{
    QApplication app(argc, argv);
    QTextStream out(stdout);
    const QFont f = QApplication::font();
    out << "font-family=" << f.family() << "\n"
        << "font-size=" << f.pointSize() << "\n"
        << "fixed-font=" << QFontDatabase::systemFont(QFontDatabase::FixedFont).family() << "\n"
        << "icons=" << QIcon::themeName() << "\n"
        << "style=" << app.style()->name() << "\n"
        << "highlight=" << app.palette().color(QPalette::Highlight).name() << "\n"
        << "window=" << app.palette().color(QPalette::Window).name() << "\n"
        << "text=" << app.palette().color(QPalette::WindowText).name() << "\n";
    return 0;
}
