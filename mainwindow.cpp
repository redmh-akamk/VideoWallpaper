#include "mainwindow.h"
#include "ui_mainwindow.h"
#include <QCloseEvent>
#include <QFileDialog>
#include <QFileInfo>
#include <QToolTip>

#include <QDesktopServices>
#include <QUrl>

#define TIMEOUT 2000 // 每两秒检测一次
#define AUTO_RUN_KEY "HKEY_CURRENT_USER\\Software\\Microsoft\\Windows\\CurrentVersion\\Run"

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);

    videowindow = new VideoWindow(nullptr);

    // 启动一个检测其他程序全屏或最大化的定时器
    timer = new QTimer(this);
    connect(timer, &QTimer::timeout, this, &MainWindow::onTimeOut);
    timer->start(TIMEOUT);

    // 获取显示屏句柄
    hDevice = CreateFile(TEXT("\\\\.\\LCD"), GENERIC_READ, 0, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);

    // 软件退出时保存设置
    connect(qApp, &QApplication::aboutToQuit, this, &MainWindow::SetConfig);

    GetConfig();

    SetSystemTray();

    //确保开始界面是主界面
    ui->tabWidget->setCurrentIndex(0);
}

MainWindow::~MainWindow()
{
    delete videowindow;
    delete ui;
}

void MainWindow::SetConfig()
{
    QSettings config("Settings.ini", QSettings::IniFormat);

    config.setValue("mode", ui->CB_mode->currentIndex());
    config.setValue("fit", ui->CB_fit->currentIndex());
    config.setValue("rate", ui->HS_rate->value());

    if(videowindow->GetMuteState()) config.setValue("mute", true);
    else config.setValue("mute", false);

    config.setValue("volume", ui->HS_volume->value());

    // 存储列表中的路径
    config.remove("Paths");
    int index = 0;
    for(const QString &filepath : filepathsbackup)
    {
        config.setValue("Paths/"+QString::number(index), filepath);
        index++;
    }

    //设置最后播放
    if(videowindow->GetPlayIndex()!=-1)
    {
        int currentindex = videowindow->GetPlayIndex();
        config.setValue("last", filepathsbackup[currentindex]);
    }

    config.setValue("occupied", ui->PB_occupied->isChecked());

    config.setValue("startup", ui->PB_startup->isChecked());

    config.setValue("silentstart", ui->PB_silentstart->isChecked());

    config.setValue("autoplay", ui->PB_autoplay->isChecked());

    config.setValue("alwaysondisplay", ui->PB_alwaysondisplay->isChecked());

    config.setValue("displayoffstop", ui->PB_displayoffstop->isChecked());

    config.setValue("batterypause", ui->PB_batterypause->isChecked());
}

void MainWindow::GetConfig()
{
    QSettings config("Settings.ini", QSettings::IniFormat);
    int temp; // 临时变量

    temp = config.value("mode", CurrentItemInLoop).toInt();
    videowindow->SetPlaybackMode(static_cast<PlaybackMode>(temp));
    ui->CB_mode->setCurrentIndex(temp);

    temp = config.value("fit", KeepAspectRatioByExpanding).toInt();
    videowindow->SetAspectRatioMode(static_cast<AspectRatioMode>(temp));
    ui->CB_fit->setCurrentIndex(temp);

    temp = config.value("rate", 10).toInt();
    videowindow->SetPlaybackRate(temp);
    ui->HS_rate->setValue(temp);

    if(config.value("mute", true).toBool())
    {
        videowindow->VideoMute();
        ui->PB_mute->setIcon(QIcon(":/icons/mute"));
    }
    else
    {
        videowindow->VideoUnmute();
        ui->PB_mute->setIcon(QIcon(":/icons/unmute"));
    }

    temp = config.value("volume", 0).toInt();
    videowindow->SetVideoVolume(temp);
    ui->HS_volume->setValue(temp);

    // 读取路径到列表
    QStringList filepaths;
    QFileInfo fileinfo;
    QString filepath;

    int index = 0;
    while(config.contains("Paths/" + QString::number(index)))
    {
        filepath = config.value("Paths/" + QString::number(index)).toString();

        fileinfo.setFile(filepath);
        if(fileinfo.exists() && fileinfo.isFile())
        {
            filepaths.append(filepath);
        }

        index++;
    }

    filepathsbackup.clear();
    SetFilePaths(filepaths);

    // 设置最后播放
    QString path = config.value("last", "null").toString();
    temp = filepathsbackup.indexOf(path);
    if(temp!=-1) videowindow->SetPlayIndex(temp);

    if(config.value("occupied", true).toBool()) ui->PB_occupied->setChecked(true);
    else ui->PB_occupied->setChecked(false);

    if(config.value("startup", false).toBool()) ui->PB_startup->setChecked(true);
    else ui->PB_startup->setChecked(false);

    if(config.value("silentstart", false).toBool())
    {
        this->hide();
        ui->PB_silentstart->setChecked(true);
    }
    else
    {
        this->show();
        ui->PB_silentstart->setChecked(false);
    }

    if(config.value("autoplay", false).toBool())
    {
        videowindow->VideoPlay(ui->HS_volume->value());
        ui->PB_play->setIcon(QIcon(":/icons/pause"));

        ui->PB_autoplay->setChecked(true);
    }
    else ui->PB_autoplay->setChecked(false);

    if(config.value("alwaysondisplay", false).toBool())
    {
        SetThreadExecutionState(ES_CONTINUOUS|ES_DISPLAY_REQUIRED|ES_SYSTEM_REQUIRED);

        ui->PB_alwaysondisplay->setChecked(true);
    }
    else ui->PB_alwaysondisplay->setChecked(false);

    if(config.value("displayoffstop", true).toBool()) ui->PB_displayoffstop->setChecked(true);
    else ui->PB_displayoffstop->setChecked(false);

    if(config.value("batterypause", true).toBool()) ui->PB_batterypause->setChecked(true);
    else ui->PB_batterypause->setChecked(false);
}

void MainWindow::SetSystemTray()
{
    systemtray = new QSystemTrayIcon(this);
    menu = new QMenu(this);

    // 设置系统托盘提示信息、托盘图标
    systemtray->setToolTip("视频壁纸");
    systemtray->setIcon(QIcon(":/icons/app"));

    // 设置系统托盘的上下文菜单
    systemtray->setContextMenu(menu);

    play = new QAction(this);
    connect(play, &QAction::triggered, this, &MainWindow::on_PB_play_clicked);
    menu->addAction(play);
    if(videowindow->GetVideoState()!=PlayingState) play->setText("播放视频");
    else play->setText("暂停视频");

    stop = new QAction("停止视频", this);
    connect(stop, &QAction::triggered, this, &MainWindow::on_PB_stop_clicked);
    menu->addAction(stop);

    mute = new QAction(this);
    connect(mute, &QAction::triggered, this, &MainWindow::on_PB_mute_clicked);
    menu->addAction(mute);
    if(videowindow->GetMuteState()) mute->setText("开启声音");
    else mute->setText("关闭声音");

    restart = new QAction("重新启动", this);
    connect(restart, &QAction::triggered, [](){qApp->exit(773);});
    menu->addAction(restart);

    quit = new QAction("退出程序", this);
    connect(quit, &QAction::triggered, qApp, &QApplication::quit);
    menu->addAction(quit);

    connect(systemtray, &QSystemTrayIcon::activated, this, &MainWindow::onTrayIconActivated);

    // 显示系统托盘
    systemtray->show();
}

void MainWindow::onTrayIconActivated(QSystemTrayIcon::ActivationReason reason)
{
    switch(reason)
    {
        case QSystemTrayIcon::DoubleClick:
            if(this->isHidden())
            {
                this->show();
                this->raise();
                this->activateWindow();

                // 显示主界面
                ui->tabWidget->setCurrentIndex(0);
            }
            else this->hide();
            break;

        default: break;
    }
}

void MainWindow::SetFilePaths(QStringList filepaths)
{
    QFileInfo fileinfo;

    filepathsbackup.append(filepaths);

    videowindow->VideoAdd(filepaths);

    // 只保留路径中的文件名
    for(QString &filepath : filepaths)
    {
        fileinfo.setFile(filepath);
        filepath = fileinfo.fileName();
    }

    ui->LW_list->addItems(filepaths);
}

BOOL CALLBACK EnumWindowsProc_TimeOut(_In_ HWND hwnd, _In_ LPARAM Lparam)
{
    MainWindow* pthis = reinterpret_cast<MainWindow*>(Lparam);

    // 检测是否有其他程序最大化
    if(IsZoomed(hwnd) && IsWindowVisible(hwnd))
    {
        pthis->timeout_occupied = true;
        return FALSE;
    }

    pthis->timeout_occupied = false;
    return TRUE;
}

void MainWindow::onTimeOut()
{
    if(videowindow->GetVideoState()!=StoppedState && ui->PB_occupied->isChecked())
    {
        EnumWindows(EnumWindowsProc_TimeOut, reinterpret_cast<LPARAM>(this));

        // 检测是否有其他程序全屏
        QUERY_USER_NOTIFICATION_STATE pquns;
        HRESULT hr = SHQueryUserNotificationState(&pquns);
        if(SUCCEEDED(hr))
        {
            switch(pquns)
            {
                case QUNS_BUSY:
                    timeout_occupied = true;
                    break;
                case QUNS_RUNNING_D3D_FULL_SCREEN:
                    timeout_occupied = true;
                    break;
                default:
                    break;
            }
        }
    }

    if(this->timeout_occupied && videowindow->GetVideoState()==PlayingState)
    {
        timeout_playstate = true;
        on_PB_play_clicked();
    }
    else if(!this->timeout_occupied && timeout_playstate)
    {
        timeout_playstate = false;
        on_PB_play_clicked();
    }
//------------------------------------------------------------------------------------------------------------
    if (videowindow->GetVideoState()==PlayingState && ui->PB_displayoffstop->isChecked())
    {
        if(hDevice != INVALID_HANDLE_VALUE)
        {
            BOOL pfon;
            GetDevicePowerState(hDevice, &pfon);
            if(!pfon)
            {
                timeout_playstate = true; // 这里借用上段逻辑
                on_PB_stop_clicked();
            }
        }
    }
//-----------------------------------------------------------------------------------------------------------
    if(videowindow->GetVideoState()==PlayingState && ui->PB_batterypause->isChecked())
    {
        SYSTEM_POWER_STATUS syspwrst;

        if(GetSystemPowerStatus(&syspwrst))
        {
            if(syspwrst.ACLineStatus==AC_LINE_OFFLINE)
            {
                on_PB_play_clicked();
            }
        }
    }
}

void MainWindow::closeEvent(QCloseEvent *event)
{
    this->hide();
    event->ignore();
}

void MainWindow::on_PB_add_clicked()
{
    QStringList filepaths = QFileDialog::getOpenFileNames(this, tr("打开文件"), ".", tr("全部文件 (*.*)"));

    SetFilePaths(filepaths);
}

void MainWindow::on_PB_remove_clicked()
{
    int index = ui->LW_list->currentRow();

    filepathsbackup.removeAt(index);

    videowindow->VideoRemove(index);

    QListWidgetItem *item = ui->LW_list->takeItem(index);
    delete item;
}

void MainWindow::on_PB_play_clicked()
{
    if(videowindow->GetVideoState()==PlayingState)
    {
        videowindow->VideoPause();
        ui->PB_play->setIcon(QIcon(":/icons/play"));
        play->setText("播放视频");
    }
    else
    {
        videowindow->VideoPlay(ui->HS_volume->value());
        ui->PB_play->setIcon(QIcon(":/icons/pause"));
        play->setText("暂停视频");
    }
}

void MainWindow::on_PB_up_clicked()
{
    videowindow->VideoUp();
}

void MainWindow::on_PB_stop_clicked()
{
    if(videowindow->GetVideoState()!=StoppedState)
    {
        videowindow->VideoStop();
        ui->PB_play->setIcon(QIcon(":/icons/play"));
        play->setText("播放视频");
    }
}

void MainWindow::on_PB_next_clicked()
{
    videowindow->VideoNext();
}

void MainWindow::on_PB_mute_clicked()
{
    if(videowindow->GetMuteState())
    {
        videowindow->VideoUnmute();
        ui->PB_mute->setIcon(QIcon(":/icons/unmute"));
        mute->setText("关闭声音");
    }
    else
    {
        videowindow->VideoMute();
        ui->PB_mute->setIcon(QIcon(":/icons/mute"));
        mute->setText("开启声音");
    }
}

void MainWindow::on_HS_volume_valueChanged(int value)
{
    QPoint pos = ui->HS_volume->rect().center();
    pos = ui->HS_volume->mapToGlobal(pos);

    videowindow->SetVideoVolume(value);
    QToolTip::showText(pos, QString::number(value*VolumeRatio) + '%', ui->HS_volume);
}

void MainWindow::on_LW_list_itemDoubleClicked(QListWidgetItem *item)
{
    videowindow->SetPlayIndex(ui->LW_list->row(item));

    if(videowindow->GetVideoState()!=PlayingState)
    {
        videowindow->VideoPlay(ui->HS_volume->value());
        ui->PB_play->setIcon(QIcon(":/icons/pause"));
        play->setText("暂停视频");
    }
}

void MainWindow::on_CB_mode_currentIndexChanged(int index)
{
    switch(index)
    {
        case 0: videowindow->SetPlaybackMode(CurrentItemOnce); break;
        case 1: videowindow->SetPlaybackMode(CurrentItemInLoop); break;
        case 2: videowindow->SetPlaybackMode(Sequential); break;
        case 3: videowindow->SetPlaybackMode(Loop); break;
        case 4: videowindow->SetPlaybackMode(Random); break;
    }
}

void MainWindow::on_CB_fit_currentIndexChanged(int index)
{
    switch(index)
    {
        case 0: videowindow->SetAspectRatioMode(IgnoreAspectRatio); break;
        case 1: videowindow->SetAspectRatioMode(KeepAspectRatio); break;
        case 2: videowindow->SetAspectRatioMode(KeepAspectRatioByExpanding); break;
    }
}

void MainWindow::on_HS_rate_valueChanged(int value)
{
    QPoint pos = ui->HS_rate->rect().center();
    pos = ui->HS_rate->mapToGlobal(pos);

    videowindow->SetPlaybackRate(value);
    QToolTip::showText(pos, QString::number(value/PlaybackRateRatio) + 'x', ui->HS_rate);
}

void MainWindow::on_PB_startup_toggled(bool checked)
{
    // https://blog.csdn.net/null_plus/article/details/123428466
    QString appname = QApplication::applicationName();        // 获取应用名称
    QSettings config(AUTO_RUN_KEY, QSettings::NativeFormat);  // 创建QSetting, 需要添加QSetting头文件

    if(checked)
    {
        QString apppath = QApplication::applicationFilePath(); // 找到应用的目录
        config.setValue(appname, apppath.replace("/", "\\"));  // 写入注册表
    }
    else
    {
        config.remove(appname);                                // 从注册表中删除
    }
}

void MainWindow::on_PB_alwaysondisplay_toggled(bool checked)
{
    if(checked) SetThreadExecutionState(ES_CONTINUOUS|ES_DISPLAY_REQUIRED|ES_SYSTEM_REQUIRED);
    else SetThreadExecutionState(ES_CONTINUOUS);
}

void MainWindow::on_PB_github_clicked()
{
    const QUrl url("https://github.com/redmh-gh/VideoWallpaper");
    QDesktopServices::openUrl(url);
    this->hide();
}
