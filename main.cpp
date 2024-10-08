#include "mainwindow.h"
#include <QApplication>
#include <QLockFile>
#include <QDir>
#include <QMessageBox>
#include <QProcess>

int main(int argc, char *argv[])
{
    // 138.对高分屏不同缩放比例的自适应处理方法
    // https://github.com/feiyangqingyun/qtkaifajingyan
    qputenv("QT_ENABLE_HIGHDPI_SCALING", "1");
    QGuiApplication::setHighDpiScaleFactorRoundingPolicy(Qt::HighDpiScaleFactorRoundingPolicy::PassThrough);

    QApplication a(argc, argv);

    QDir::setCurrent(QCoreApplication::applicationDirPath());

    // 避免重复启动
    QString tmpDir = QDir::tempPath();
    QString lockFilePath = tmpDir + "/MyUniqueApplication.lock";
    QLockFile lockFile(lockFilePath);

    if (!lockFile.tryLock(2000)) {
        QMessageBox::warning(nullptr, "警告", "程序正在运行！");
        return 0; // 退出程序
    }

    // 设置应用图标
    a.setWindowIcon(QIcon(":/icons/app"));

    MainWindow w;

    // 软件重启
    // https://blog.csdn.net/x85371169/article/details/89043629
    int ret = a.exec();
    if (ret == 773)
    {
        QProcess::startDetached(qApp->applicationFilePath(), QStringList());
        return 0;
    }

    return ret;
}
