// HostNetworkInterface.cpp

#include "HostNetworkInterface.h"
#include <QDebug>

#ifdef Q_OS_UNIX

#include <linux/if_tun.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <unistd.h>
#include <cstring>

// :contentReference[oaicite:0]{index=0}
HostNetworkInterface::HostNetworkInterface(const QString& ifaceName, QObject* parent)
	: QObject(parent),
	m_ifaceName(ifaceName),
	m_fd(-1),
	m_notifier(nullptr)
{
	// 1) open /dev/net/tun
	m_fd = ::open("/dev/net/tun", O_RDWR);
	if (m_fd < 0) {
		qFatal("Cannot open /dev/net/tun: %s", strerror(errno));
	}

	// 2) prepare ifreq
	struct ifreq ifr;
	std::memset(&ifr, 0, sizeof(ifr));
	QByteArray nameBytes = ifaceName.toUtf8();
	std::strncpy(ifr.ifr_name, nameBytes.constData(), IFNAMSIZ);
	ifr.ifr_flags = IFF_TAP | IFF_NO_PI;    // TAP, no extra packet-info

	// 3) ioctl to bind to this TAP interface
	if (::ioctl(m_fd, TUNSETIFF, &ifr) < 0) {
		qFatal("ioctl(TUNSETIFF) failed: %s", strerror(errno));
	}

	// 4) watch for incoming frames
	m_notifier = new QSocketNotifier(m_fd, QSocketNotifier::Read, this);
	connect(m_notifier, &QSocketNotifier::activated,
		this, &HostNetworkInterface::onTapReady);
}

HostNetworkInterface::~HostNetworkInterface() {
	if (m_notifier) {
		delete m_notifier;
		m_notifier = nullptr;
	}
	if (m_fd >= 0) {
		::close(m_fd);
		m_fd = -1;
	}
}

void HostNetworkInterface::onTapReady() {
	QByteArray buf(2048, 0);
	int len = ::read(m_fd, buf.data(), buf.size());
	if (len > 0) {
		buf.resize(len);
		emit frameReceived(buf);
	}
}

void HostNetworkInterface::sendFrame(const QByteArray& frame) {
	::write(m_fd, frame.constData(), frame.size());
}

#endif  // Q_OS_UNIX


#ifdef Q_OS_WIN

#include <pcap.h>
#include <cstring>

// :contentReference[oaicite:1]{index=1}
HostNetworkInterface::HostNetworkInterface(const QString& ifaceName, QObject* parent)
	: QObject(parent),
	m_ifaceName(ifaceName),
	m_pcapHandle(nullptr)
{
	char errbuf[PCAP_ERRBUF_SIZE] = { 0 };
	// open the adapter in promiscuous mode, 1000ms timeout
	m_pcapHandle = pcap_open_live(ifaceName.toUtf8().constData(),
		65536,    // snaplen
		1,        // promisc
		1000,     // to_ms
		errbuf);
	if (!m_pcapHandle) {
		qFatal("pcap_open_live failed on %s: %s",
			ifaceName.toUtf8().constData(), errbuf);
	}
	// start capture in its own thread
	connect(&m_captureThread, &QThread::started,
		this, &HostNetworkInterface::runPcapLoop);
	this->moveToThread(&m_captureThread);
	m_captureThread.start();
}

HostNetworkInterface::~HostNetworkInterface() {
	if (m_pcapHandle) {
		pcap_breakloop(m_pcapHandle);
		pcap_close(m_pcapHandle);
		m_pcapHandle = nullptr;
	}
	m_captureThread.quit();
	m_captureThread.wait();
}

void HostNetworkInterface::runPcapLoop() {
	// This callback will be called in the context of m_captureThread
	pcap_loop(m_pcapHandle, 0,
		[](u_char* user, const struct pcap_pkthdr* h, const u_char* bytes) {
			HostNetworkInterface* self = reinterpret_cast<HostNetworkInterface*>(user);
			QByteArray frame(reinterpret_cast<const char*>(bytes), h->len);
			emit self->frameReceived(frame);
		},
		reinterpret_cast<u_char*>(this));
}

void HostNetworkInterface::sendFrame(const QByteArray& frame) {
	if (m_pcapHandle) {
		pcap_sendpacket(m_pcapHandle,
			reinterpret_cast<const u_char*>(frame.constData()),
			frame.size());
	}
}

#endif  // Q_OS_WIN


