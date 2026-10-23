// melon-welcome: the Welcome window (first login, and from the launcher), the "Update melon" button and the update
// notice in the tray.
//
//   melon-welcome                 open the window
//   melon-welcome --updates       open it on the Updates page
//   melon-welcome --autostart     what the login autostart runs: the tray notice, and the window on the first login
//   melon-welcome --screenshot out.png [page]   draw a page into a picture and exit (for the build machine; QT_QPA_PLATFORM=offscreen)
//
// Only one copy runs per user: a second start asks the first to show its window. The program never installs anything by
// itself: it reads /var/lib/melon/updates.txt (written by the update-check service, see melon-update-run) and starts
// `pkexec /usr/libexec/melon/melon-update-run --upgrade` when the person presses the button.

#include <QApplication>
#include <QCloseEvent>
#include <QDateTime>
#include <QDesktopServices>
#include <QDir>
#include <QFile>
#include <QFileSystemWatcher>
#include <QHBoxLayout>
#include <QFileInfo>
#include <QLabel>
#include <QListWidget>
#include <QLocalServer>
#include <QLocalSocket>
#include <QMenu>
#include <QPlainTextEdit>
#include <QProcess>
#include <QPushButton>
#include <QRegularExpression>
#include <QStackedWidget>
#include <QSystemTrayIcon>
#include <QTimer>
#include <QUrl>
#include <QVBoxLayout>

#include <unistd.h>

#include <functional>

namespace {

const char *kState = "/var/lib/melon/updates.txt";
const char *kCtlDir = "/var/lib/melon/updates-ctl";
const char *kRun = "/usr/libexec/melon/melon-update-run";

enum Page { PageWelcome = 0, PageUpdates, PageApps, PageHelp };

struct State {
    bool known = false;      // the service has looked at least once
    qint64 checked = 0;      // when (epoch seconds)
    qint64 failed = 0;       // the last failed look
    QStringList packages;    // what could be upgraded
};

State readState()
{
    State s;
    QFile f(kState);
    if (!f.open(QIODevice::ReadOnly))
        return s;
    const QStringList lines = QString::fromUtf8(f.readAll()).split('\n', Qt::SkipEmptyParts);
    for (const QString &raw : lines) {
        const QString l = raw.trimmed();
        if (l.startsWith("checked ")) { s.checked = l.mid(8).toLongLong(); s.known = s.checked > 0; }
        else if (l.startsWith("failed ")) s.failed = l.mid(7).toLongLong();
        else if (l.startsWith("count ")) {}
        else if (!l.isEmpty()) s.packages << l;
    }
    return s;
}

QString osName()
{
    QFile f("/etc/os-release");
    if (f.open(QIODevice::ReadOnly)) {
        for (const QString &l : QString::fromUtf8(f.readAll()).split('\n')) {
            if (l.startsWith("PRETTY_NAME="))
                return l.mid(12).remove('"').trimmed();
        }
    }
    return "melon";
}

QString configFile(const QString &name)
{
    const QString base = qEnvironmentVariable("XDG_CONFIG_HOME", QDir::homePath() + "/.config");
    return base + "/melon/" + name;
}

void run(const QString &program, const QStringList &args = {})
{
    QProcess::startDetached(program, args);
}

QWidget *makePage(const QString &title, const QString &text, const QList<QPair<QString, std::function<void()>>> &buttons)
{
    auto *w = new QWidget;
    auto *l = new QVBoxLayout(w);
    l->setContentsMargins(28, 24, 28, 24);
    l->setSpacing(12);
    auto *t = new QLabel(title);
    t->setStyleSheet("font-size: 22px; font-weight: 600;");
    t->setWordWrap(true);
    l->addWidget(t);
    if (!text.isEmpty()) {
        auto *b = new QLabel(text);
        b->setWordWrap(true);
        b->setTextFormat(Qt::RichText);
        b->setOpenExternalLinks(true);
        l->addWidget(b);
    }
    for (const auto &bt : buttons) {
        auto *p = new QPushButton(bt.first);
        p->setMinimumHeight(34);
        auto fn = bt.second;
        QObject::connect(p, &QPushButton::clicked, p, [fn] { fn(); });
        l->addWidget(p);
    }
    l->addStretch(1);
    return w;
}

} // namespace

class Welcome : public QWidget
{
    Q_OBJECT
public:
    Welcome()
    {
        setWindowTitle("Welcome to melon");
        setWindowIcon(QIcon::fromTheme("melon"));
        resize(760, 520);

        auto *root = new QHBoxLayout(this);
        root->setContentsMargins(0, 0, 0, 0);
        root->setSpacing(0);
        nav = new QListWidget;
        nav->setFixedWidth(190);
        nav->setFrameShape(QFrame::NoFrame);
        for (const char *n : {"Welcome", "Updates", "Apps and games", "Help"})
            nav->addItem(n);
        nav->setStyleSheet("QListWidget { font-size: 14px; } QListWidget::item { padding: 10px 14px; }");
        stack = new QStackedWidget;
        root->addWidget(nav);
        root->addWidget(stack, 1);

        const QString home = QDir::homePath();
        stack->addWidget(makePage(
            "Welcome to " + osName(),
            "Your files live in your <b>home folder</b>: Documents, Downloads, Pictures, Music and Videos are already there, "
            "and what you download from a browser lands in Downloads.<br><br>"
            "melon updates itself on request: the <b>Updates</b> page says when something new is ready, and one button installs it.",
            {{"Open my files", [home] { QDesktopServices::openUrl(QUrl::fromLocalFile(home)); }},
             {"Open System Settings", [] { run("systemsettings"); }}}));
        stack->addWidget(buildUpdates());
        stack->addWidget(makePage(
            "Apps and games",
            "melon keeps its own system small. Big apps such as Firefox, VLC, Steam and Prism Launcher come from Flathub: "
            "the button below asks which ones you want and installs them. Discover, the app store, can add any others.",
            {{"Choose apps from Flathub (Firefox, VLC, Steam, Prism Launcher)",
              [] { run("konsole", {"--hide-menubar", "-p", "tabtitle=Apps for melon", "-e", "melon-first-boot", "--again"}); }},
             {"Open Discover (all apps)", [] { run("plasma-discover"); }},
             {"Play melon pinball", [] { run("melon-pinball"); }}}));
        stack->addWidget(makePage(
            "Help",
            "melon is a small project made by people who care how it feels. If something doesn't work, tell us: it gets fixed.",
            {{"melon website", [] { QDesktopServices::openUrl(QUrl("https://melon-77.github.io/melon-os/")); }},
             {"Report a problem", [] { QDesktopServices::openUrl(QUrl("https://github.com/melon-77/melon-os/issues")); }},
             {"Read the documentation", [] { QDesktopServices::openUrl(QUrl("https://github.com/melon-77/melon-os#readme")); }}}));

        connect(nav, &QListWidget::currentRowChanged, stack, &QStackedWidget::setCurrentIndex);
        nav->setCurrentRow(PageWelcome);

        watcher.addPath("/var/lib/melon");
        if (QFile::exists(kState))
            watcher.addPath(kState);
        connect(&watcher, &QFileSystemWatcher::fileChanged, this, [this] { stateChanged(); });
        connect(&watcher, &QFileSystemWatcher::directoryChanged, this, [this] {
            if (QFile::exists(kState) && !watcher.files().contains(kState))
                watcher.addPath(kState);
            stateChanged();
        });
        refreshUpdates();
    }

    void showPage(int page)
    {
        nav->setCurrentRow(page);
        show();
        raise();
        activateWindow();
    }

    // the current state, for the tray
    State state() const { return readState(); }

signals:
    void updatesChanged();
    void closedByPerson();
    void upgradeRequested();

protected:
    void closeEvent(QCloseEvent *e) override
    {
        if (proc) {            // an update is running: keep the window, it shows what happens
            e->ignore();
            showMinimized();
            return;
        }
        QWidget::closeEvent(e);
        emit closedByPerson();
    }

private:
    QWidget *buildUpdates()
    {
        auto *w = new QWidget;
        auto *l = new QVBoxLayout(w);
        l->setContentsMargins(28, 24, 28, 24);
        l->setSpacing(12);
        auto *t = new QLabel("Updates");
        t->setStyleSheet("font-size: 22px; font-weight: 600;");
        l->addWidget(t);
        status = new QLabel;
        status->setWordWrap(true);
        status->setStyleSheet("font-size: 15px;");
        l->addWidget(status);
        list = new QPlainTextEdit;
        list->setReadOnly(true);
        list->setMaximumHeight(120);
        l->addWidget(list);
        auto *row = new QHBoxLayout;
        checkBtn = new QPushButton("Check now");
        updateBtn = new QPushButton("Update melon");
        updateBtn->setDefault(true);
        checkBtn->setMinimumHeight(34);
        updateBtn->setMinimumHeight(34);
        row->addWidget(checkBtn);
        row->addWidget(updateBtn, 1);
        l->addLayout(row);
        log = new QPlainTextEdit;
        log->setReadOnly(true);
        log->setVisible(false);
        log->setStyleSheet("font-family: monospace;");
        l->addWidget(log, 1);
        l->addStretch(0);
        connect(checkBtn, &QPushButton::clicked, this, [this] { checkNow(); });
        connect(updateBtn, &QPushButton::clicked, this, [this] { startUpgrade(); });
        return w;
    }

    void stateChanged()
    {
        refreshUpdates();
        emit updatesChanged();
    }

    static QString ago(qint64 epoch)
    {
        const qint64 s = QDateTime::currentSecsSinceEpoch() - epoch;
        if (s < 90) return "just now";
        if (s < 5400) return QString("%1 minutes ago").arg(s / 60);
        if (s < 36 * 3600) return QString("%1 hours ago").arg(s / 3600);
        return QString("%1 days ago").arg(s / 86400);
    }

    void refreshUpdates()
    {
        if (proc)
            return;
        const State s = readState();
        if (checking) {
            if (s.checked > checkStarted || s.failed > checkStarted) {
                checking = false;
            } else {
                return;
            }
        }
        const int n = s.packages.size();
        QString text;
        if (!s.known)
            text = "melon hasn't looked for updates yet. It does that by itself a few minutes after the computer starts; "
                   "press <b>Check now</b> to ask right away.";
        else if (n == 0)
            text = "<b>melon is up to date.</b> Last checked " + ago(s.checked) + ".";
        else
            text = QString("<b>%1 update%2 ready.</b> Last checked %3.").arg(n).arg(n == 1 ? "" : "s").arg(ago(s.checked));
        if (s.failed > s.checked)
            text += "<br>The last try (" + ago(s.failed) + ") couldn't reach the package server: is the computer online?";
        status->setText(text);
        list->setPlainText(s.packages.join(", "));
        list->setVisible(n > 0);
        updateBtn->setEnabled(n > 0);
        checkBtn->setEnabled(true);
        updateBtn->setText(n > 0 ? "Update melon" : "Nothing to update");
    }

    void checkNow()
    {
        QDir().mkpath(kCtlDir);            // the service made it; this is for a system where it hasn't started yet
        QFile req(QString("%1/request-%2").arg(kCtlDir).arg(getuid()));
        if (!req.open(QIODevice::WriteOnly)) {
            status->setText("Couldn't ask the update service. Is it running? (<tt>sv status melon-update-check</tt>)");
            return;
        }
        req.close();
        checking = true;
        checkStarted = QDateTime::currentSecsSinceEpoch();
        status->setText("Looking for updates...");
        checkBtn->setEnabled(false);
        QTimer::singleShot(120000, this, [this] {
            if (checking) {
                checking = false;
                status->setText("The update service didn't answer. Is it running? (<tt>sv status melon-update-check</tt>)");
                checkBtn->setEnabled(true);
            }
        });
    }

    void startUpgrade()
    {
        if (proc)
            return;
        log->clear();
        log->setVisible(true);
        updateBtn->setEnabled(false);
        checkBtn->setEnabled(false);
        status->setText("Installing updates. Please keep the computer on and wait until it says Done.");
        restartNeeded = false;
        proc = new QProcess(this);
        proc->setProcessChannelMode(QProcess::MergedChannels);
        connect(proc, &QProcess::readyRead, this, [this] {
            const QString out = QString::fromUtf8(proc->readAll()).remove('\r');
            if (out.contains("MELON-RESTART"))
                restartNeeded = true;
            log->appendPlainText(out.trimmed());
        });
        connect(proc, &QProcess::errorOccurred, this, [this](QProcess::ProcessError) {
            if (proc && proc->state() == QProcess::NotRunning)
                finishUpgrade(-1);
        });
        connect(proc, &QProcess::finished, this, [this](int code, QProcess::ExitStatus) { finishUpgrade(code); });
        proc->start("pkexec", {kRun, "--upgrade"});
    }

    void finishUpgrade(int code)
    {
        if (proc) {
            proc->deleteLater();
            proc = nullptr;
        }
        refreshUpdates();
        if (code == 0) {
            status->setText(restartNeeded ? "<b>Done.</b> The kernel was updated: restart the computer to use it."
                                          : "<b>Done.</b> melon is up to date.");
        } else if (code == 126 || code == 127) {
            status->setText("The update wasn't started (the password prompt was closed, or the password was wrong). Try again when you like.");
            log->setVisible(false);
        } else {
            status->setText("<b>The update didn't finish.</b> Nothing is broken; the log below says what happened. Try again, or report it from the Help page.");
        }
        updateBtn->setEnabled(!readState().packages.isEmpty());
        checkBtn->setEnabled(true);
        emit updatesChanged();
    }

    QListWidget *nav = nullptr;
    QStackedWidget *stack = nullptr;
    QLabel *status = nullptr;
    QPlainTextEdit *list = nullptr, *log = nullptr;
    QPushButton *checkBtn = nullptr, *updateBtn = nullptr;
    QProcess *proc = nullptr;
    QFileSystemWatcher watcher;
    bool checking = false, restartNeeded = false;
    qint64 checkStarted = 0;
};

// The tray notice: visible only while updates are waiting; says so once per change.
class Tray : public QObject
{
    Q_OBJECT
public:
    explicit Tray(Welcome *w) : win(w)
    {
        icon = new QSystemTrayIcon(QIcon::fromTheme("melon"), this);
        auto *menu = new QMenu;
        menu->addAction("Update melon...", this, [this] { win->showPage(PageUpdates); });
        menu->addAction("Open Welcome", this, [this] { win->showPage(PageWelcome); });
        icon->setContextMenu(menu);
        connect(icon, &QSystemTrayIcon::activated, this, [this](QSystemTrayIcon::ActivationReason r) {
            if (r == QSystemTrayIcon::Trigger || r == QSystemTrayIcon::DoubleClick)
                win->showPage(PageUpdates);
        });
        connect(icon, &QSystemTrayIcon::messageClicked, this, [this] { win->showPage(PageUpdates); });
        connect(win, &Welcome::updatesChanged, this, [this] { update(); });
        timer.setInterval(10 * 60 * 1000);
        connect(&timer, &QTimer::timeout, this, [this] { update(); });
        timer.start();
        QTimer::singleShot(3000, this, [this] { update(); });
    }

private:
    void update()
    {
        const State s = readState();
        const int n = s.packages.size();
        if (n == 0) {
            icon->hide();
            lastNotified = 0;
            return;
        }
        const QString tip = QString("%1 update%2 ready for melon").arg(n).arg(n == 1 ? "" : "s");
        icon->setToolTip(tip);
        icon->show();
        if (n != lastNotified) {
            lastNotified = n;
            icon->showMessage("melon updates", tip + ". Click to install them.", QSystemTrayIcon::Information, 15000);
        }
    }

    Welcome *win;
    QSystemTrayIcon *icon = nullptr;
    QTimer timer;
    int lastNotified = 0;
};

int main(int argc, char **argv)
{
    QApplication app(argc, argv);
    app.setApplicationName("melon-welcome");
    app.setDesktopFileName("melon-welcome");
    QString shot;
    int shotPage = PageWelcome;
    bool autostart = false;
    int page = PageWelcome;
    const QStringList args = app.arguments().mid(1);
    for (int i = 0; i < args.size(); ++i) {
        if (args[i] == "--autostart") autostart = true;
        else if (args[i] == "--updates") page = PageUpdates;
        else if (args[i] == "--screenshot" && i + 1 < args.size()) {
            shot = args[++i];
            if (i + 1 < args.size() && !args[i + 1].startsWith("--")) shotPage = args[++i].toInt();
        }
    }

    app.setQuitOnLastWindowClosed(!autostart);     // the login copy stays for the tray; one opened from the menu ends with its window

    if (!shot.isEmpty()) {
        Welcome w;
        w.showPage(shotPage);
        QTimer::singleShot(300, [&] { w.grab().save(shot); app.quit(); });
        return app.exec();
    }

    // the live session's user has nothing to update and nothing to be welcomed to
    if (autostart && qEnvironmentVariable("USER") == "live")
        return 0;

    // one copy per user: a second start asks the first to show its window
    const QString sock = QString("melon-welcome-%1").arg(getuid());
    {
        QLocalSocket c;
        c.connectToServer(sock);
        if (c.waitForConnected(300)) {
            if (!autostart)
                c.write(QByteArray::number(page) + "\n");
            c.waitForBytesWritten(300);
            return 0;
        }
    }
    QLocalServer::removeServer(sock);
    QLocalServer server;
    server.listen(sock);

    Welcome win;
    Tray tray(&win);
    QObject::connect(&server, &QLocalServer::newConnection, &win, [&] {
        while (QLocalSocket *c = server.nextPendingConnection()) {
            QObject::connect(c, &QLocalSocket::readyRead, c, [&win, c] {
                win.showPage(qBound(0, c->readAll().trimmed().toInt(), (int)PageHelp));
                c->deleteLater();
            });
        }
    });

    // the first login shows the window; once it is closed, melon-first-boot offers the Flathub apps as it always has
    const QString seen = configFile("welcome-seen");
    const bool first = !QFile::exists(seen);
    if (autostart && first) {
        QObject::connect(&win, &Welcome::closedByPerson, &win, [seen] {
            QDir().mkpath(QFileInfo(seen).absolutePath());
            QFile f(seen);
            if (f.open(QIODevice::WriteOnly)) f.close();
            run("melon-first-boot");
        });
        win.showPage(PageWelcome);
    } else if (!autostart) {
        win.showPage(page);
    }
    return app.exec();
}

#include "main.moc"
