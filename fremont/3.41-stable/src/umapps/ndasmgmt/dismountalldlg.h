#pragma once
#include "progressbarctrlex.h"

class CDismountAllDialog :
	public CDialogImpl<CDismountAllDialog>
{
public:

	enum { IDD = IDD_DISMOUNT_ALL };

	enum { WM_WORKITEM_COMPLETED = WM_APP + 0xC00};

	BEGIN_MSG_MAP_EX(CDismountAllDialog)
		MSG_WM_INITDIALOG(OnInitDialog)
		MSG_WM_CLOSE(OnClose)
		MSG_WM_SETCURSOR(OnSetCursor)
		MESSAGE_HANDLER_EX(WM_WORKITEM_COMPLETED,OnWorkItemCompleted)
		COMMAND_ID_HANDLER_EX(IDRETRY, OnCmdRetry)
		COMMAND_ID_HANDLER_EX(IDCANCEL, OnCmdCancel)
	END_MSG_MAP()

	LRESULT OnInitDialog(HWND hWnd, LPARAM lParam);
	void OnClose();
	BOOL OnSetCursor(HWND, UINT HitTestCode, UINT MouseMsgId);
	void OnCmdRetry(UINT NotifyCode, int ID, HWND hWndCtl);
	void OnCmdCancel(UINT NotifyCode, int ID, HWND hWndCtl);
	LRESULT OnWorkItemCompleted(UINT Msg, WPARAM wParam, LPARAM lParam);

private:

	bool m_closeQueued;
	CCursorHandle m_hWaitCursor;
	CString m_staticMessage;
	CStatic m_messageText;
	CButton m_cancelButton;
	CButton m_retryButton;
	CMarqueeProgressBarCtrl m_progressBarCtl;
	CListViewCtrl m_deviceListView;

	ndas::LogicalDeviceVector* m_logDevices;

	typedef struct _WORK_ITEM_PARAM {
		CDismountAllDialog* DialogInstance;
		DWORD Index;
		DWORD ErrorCode;
		NDAS_LOGICALDEVICE_EJECT_PARAM EjectParam;
		CString DisplayName;
	} WORK_ITEM_PARAM, *PWORK_ITEM_PARAM;

	LONG m_errorCount;
	LONG m_workItemCount;
	std::vector<WORK_ITEM_PARAM> m_paramVector;

	void pStartWorkItems();

	static DWORD WorkerThreadStart(LPVOID Context)
	{
		PWORK_ITEM_PARAM p = static_cast<PWORK_ITEM_PARAM>(Context);
		return p->DialogInstance->WorkerThreadStart(p);
	}

	DWORD WorkerThreadStart(PWORK_ITEM_PARAM Param);
	BOOL WorkItemCompleted(PWORK_ITEM_PARAM Param);

	void QueueEjectWorkItemCallback(WORK_ITEM_PARAM& LogDevice);
	void PrepareEjectWorkItemCallback(ndas::LogicalDevicePtr LogDevice);

	struct PrepareEjectWorkItem :
		std::unary_function<ndas::LogicalDevicePtr, void>
	{
		CDismountAllDialog* DialogInstance;
		PrepareEjectWorkItem(CDismountAllDialog* DialogInstance) :
			DialogInstance(DialogInstance)
		{
		}
		void operator()(ndas::LogicalDevicePtr LogDevice) const
		{
			DialogInstance->PrepareEjectWorkItemCallback(LogDevice);
		}
	};

	struct QueueEjectWorkItem : 
		std::unary_function<WORK_ITEM_PARAM&, void> 
	{
		CDismountAllDialog* DialogInstance;
		QueueEjectWorkItem(CDismountAllDialog* DialogInstance) :
			DialogInstance(DialogInstance)
		{
		}
		void operator()(WORK_ITEM_PARAM& Param) const 
		{
			DialogInstance->QueueEjectWorkItemCallback(Param);
		}
	};

};

