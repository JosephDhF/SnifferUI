#include "stdafx.h"
//#include "SnifferUI.h"
#include "PacketCatcher.h"
#include "Packet.h"
#include "ThreadParam.h"
#include "pcap.h"
#include "remote-ext.h"
//#include "resource.h"

PacketCatcher::PacketCatcher()
{
	m_adhandle = NULL;
	m_pool = NULL;
	m_dumper = NULL;
}

PacketCatcher::PacketCatcher(PacketPool * pool)
{
	m_adhandle = NULL;
	m_pool = pool;
	m_dumper = NULL;
}


PacketCatcher::~PacketCatcher()
{
	m_adhandle = NULL;
	m_pool = NULL;
	m_dumper = NULL;
}

bool PacketCatcher::setPool(PacketPool *pool)
{
	if (pool)
	{
		m_pool = pool;
		return true;
	}
	return false;
}

/**
*	@brief	��ָ���������������������adhandle
*	@param	selItemIndexOfDevList	�����б�ѡ������±�
*	@return true	�򿪳ɹ�	false	��ʧ��
*/
bool PacketCatcher::openAdapter(int selItemIndexOfDevList, const CTime &currentTime)
{
	if (selItemIndexOfDevList < 0 || m_adhandle)
		return false;

	int count = 0, selDevIndex = selItemIndexOfDevList - 1;
	pcap_if_t *dev, *allDevs;
	if (pcap_findalldevs(&allDevs, NULL) == -1)
	{
		AfxMessageBox(_T("pcap_findalldevs����!"), MB_OK);
		return false;
	}
	for (dev = allDevs; count < selDevIndex; dev = dev->next, ++count);
	// ������Ϣ����
	m_dev = dev->description + CString(" ( ") + dev->name + " )";

	// ������
	if ((m_adhandle = pcap_open_live(dev->name,
		65535,						// ��󲶻񳤶�
		PCAP_OPENFLAG_PROMISCUOUS,	// ��������Ϊ����ģʽ 
		READ_PACKET_TIMEOUT,		// ��ȡ��ʱʱ��
		NULL)) == NULL)
	{
		AfxMessageBox(_T("pcap_open_live����!"), MB_OK);
		return false;
	}

	/* ��ת���ļ� */
	CString file = "SnifferUI_" + currentTime.Format("%Y%m%d%H%M%S") + ".pcap";
	CString path = ".\\tmp\\" + file;
	m_dumper = pcap_dump_open(m_adhandle, path);

	pcap_freealldevs(allDevs);
	return true;
}

/**
*	@brief	��ָ���ļ�������ļ�������adhandle
*	@param	path	�ļ�·��
*	@return true	�򿪳ɹ�	false	��ʧ��
*/
bool PacketCatcher::openAdapter(CString path)
{
	if (path.IsEmpty())
		return false;
	m_dev = path;
	if ( (m_adhandle = pcap_open_offline(path, NULL))  == NULL)
	{
		AfxMessageBox(_T("pcap_open_offline����!"), MB_OK);
		return false;
	}
	return true;
}

/**
*	@brief	�ر��Ѵ�����
*	@param	-
*	@return true	�رճɹ�	false	�ر�ʧ��
*/
bool PacketCatcher::closeAdapter()
{
	if (m_adhandle)
	{
		pcap_close(m_adhandle);
		m_adhandle = NULL;
		if (m_dumper)
		{
			pcap_dump_close(m_dumper);
			m_dumper = NULL;
		}
		return true;
	}
	return false;
}

/**
*	@brief	��ʼץ��
*	@param	-
*	@return -
*/
void PacketCatcher::startCapture(int mode)
{
	if (m_adhandle && m_pool)
		AfxBeginThread(capture_thread, new ThreadParam(m_adhandle, m_pool, m_dumper, mode));
}

/**
*	@brief	ֹͣץ��
*	@param	-
*	@return -
*/
void PacketCatcher::stopCapture()
{
	if (m_adhandle)
		pcap_breakloop(m_adhandle);
}

CString PacketCatcher::getDevName()
{
	return m_dev;
}


/**
*	@brief �������ݰ��߳���ں�����ȫ�ֺ���
*	@param pParam �����̵߳Ĳ���
*	@return 0 ��ʾץ���ɹ�	-1 ��ʾץ��ʧ��
*/
UINT capture_thread(LPVOID pParam)
{
	ThreadParam *p = (ThreadParam*)pParam;

	/* ��ʼ�������ݰ� */
	pcap_loop(p->m_adhandle, -1, packet_handler, (unsigned char *)p);
	PostMessage(AfxGetMainWnd()->m_hWnd, WM_TEXIT, NULL, NULL);
	return 0;
}

/**
*	@brief	�������ݰ�����������ȫ�ֻص�����
*	@param	param		�Զ������
*	@param	header		���ݰ��ײ�
*	@param	pkt_data	���ݰ���֡��
*	@return
*/
void packet_handler(u_char *param, const struct pcap_pkthdr *header, const u_char *pkt_data)
{
	ThreadParam *threadParam = (ThreadParam *)param;
	// ���ݲ���ģʽץ��
	switch (threadParam->m_mode)
	{
	case MODE_CAPTURE_LIVE:
	{		
		threadParam->m_pool->add(header, pkt_data);
		pcap_dump((u_char*)threadParam->m_dumper, header, pkt_data);
		break;
	}
	case MODE_CAPTURE_OFFLINE:
	{
		threadParam->m_pool->add(header, pkt_data);
		break;
	}
	}

	// ������Ϣ��������SnifferUIDlg
	PostMessage(AfxGetMainWnd()->m_hWnd, WM_PKTCATCH, NULL, (LPARAM)(threadParam->m_pool->getLast().num));
}