// HostNetworkInterface.h
#pragma once

#include <QObject>
#include <QByteArray>

#ifdef Q_OS_UNIX
# include <QSocketNotifier>
# include <linux/if_tun.h>
# include <sys/ioctl.h>
# include <fcntl.h>
# include <unistd.h>
#elif defined(Q_OS_WIN)
# include <pcap.h>
# include <QThread>
#endif

/**
 * @brief Cross-platform raw-L2 network interface.
 *
 * On Unix: uses a TAP device (/dev/net/tun).
 * On Windows: uses WinPcap/Npcap to send/receive Ethernet frames.
 */
class HostNetworkInterface : public QObject {
    Q_OBJECT
public:
    explicit HostNetworkInterface(const QString& ifaceName, QObject* parent = nullptr);
    ~HostNetworkInterface() override;

signals:
    void frameReceived(const QByteArray& frame);

public slots:
    void sendFrame(const QByteArray& frame);

private slots:
#ifdef Q_OS_UNIX
    void onTapReady();
#endif

private:
    QString m_ifaceName;
#ifdef Q_OS_UNIX
    int m_fd = -1;
    QSocketNotifier* m_notifier = nullptr;
#elif defined(Q_OS_WIN)
    pcap_t* m_pcapHandle = nullptr;
    QThread m_captureThread;
    void runPcapLoop();
#endif
};
