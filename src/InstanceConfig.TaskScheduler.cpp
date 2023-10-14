#include "Updater.h"
#include "InstanceConfig.hpp"

#include <scope_guard.hpp>

#include <tuple>

#include <tchar.h>
#include <comdef.h>
#include <ole2.h>
#include <taskschd.h>

#pragma comment(lib, "taskschd.lib")
#pragma comment(lib, "comsupp.lib")


std::tuple<HRESULT, const char*> models::InstanceConfig::CreateScheduledTask() const
{
	// task name
	BSTR bstrTaskName = SysAllocString(ConvertAnsiToWide(appFilename).c_str());
	// fully qualified executable path
	BSTR bstrExecutablePath = SysAllocString(ConvertAnsiToWide(appPath.string()).c_str());
	// string id
	BSTR bstrId = SysAllocString(L"DailyTrigger");
	// start boundary - format should be YYYY-MM-DDTHH:MM:SS(+-)(timezone).
	BSTR bstrStart = SysAllocString(L"2023-01-01T12:00:00"); // TODO: make configurable
	BSTR bstrEnd = SysAllocString(L"2053-01-01T12:00:00"); // end boundary - ""
	BSTR bstrAuthor = SysAllocString(ConvertAnsiToWide(appFilename).c_str());

	// clean up all resources when going out of scope
	sg::make_scope_guard(
		[bstrTaskName, bstrExecutablePath, bstrId, bstrAuthor, bstrStart, bstrEnd]() noexcept
		{
			if (bstrTaskName) SysFreeString(bstrTaskName);
			if (bstrExecutablePath) SysFreeString(bstrExecutablePath);
			if (bstrId) SysFreeString(bstrId);
			if (bstrAuthor) SysFreeString(bstrAuthor);
			if (bstrStart) SysFreeString(bstrStart);
			if (bstrEnd) SysFreeString(bstrEnd);

			CoUninitialize();
		});

	HRESULT hr = CoInitialize(nullptr);
	if (FAILED(hr))
	{
		return std::make_tuple(hr, "COM initialization failed");
	}

	//  Create an instance of the Task Service.
	ITaskService* pService = nullptr;
	hr = CoCreateInstance(
		CLSID_TaskScheduler,
		nullptr,
		CLSCTX_INPROC_SERVER,
		IID_ITaskService,
		(void**)&pService
	);
	if (FAILED(hr))
	{
		return std::make_tuple(hr, "Failed to create an instance of ITaskService");
	}

	//  Connect to the task service.
	hr = pService->Connect(_variant_t(), _variant_t(),
	                       _variant_t(), _variant_t());
	if (FAILED(hr))
	{
		pService->Release();

		return std::make_tuple(hr, "ITaskService Connect failed");
	}

	//  Get the pointer to the root task folder that will hold the new task that is registered.
	ITaskFolder* pRootFolder = nullptr;
	hr = pService->GetFolder(_bstr_t(L"\\"), &pRootFolder);

	if (FAILED(hr))
	{
		pService->Release();

		return std::make_tuple(hr, "Cannot get Root Folder pointer");
	}

	//  If the same task exists, remove it.
	pRootFolder->DeleteTask(bstrTaskName, 0);

	//  Create the task builder object to create the task.
	ITaskDefinition* pTask = nullptr;
	hr = pService->NewTask(0, &pTask);

	pService->Release(); // COM clean up.  Pointer is no longer used.

	if (FAILED(hr))
	{
		pRootFolder->Release();

		return std::make_tuple(hr, "Failed to create a task definition");
	}

	//  Get the registration info for setting the identification.
	IRegistrationInfo* pRegInfo = nullptr;
	hr = pTask->get_RegistrationInfo(&pRegInfo);

	if (FAILED(hr))
	{
		pRootFolder->Release();
		pTask->Release();

		return std::make_tuple(hr, "Cannot get identification pointer");
	}

	hr = pRegInfo->put_Author(bstrAuthor);
	pRegInfo->Release(); // COM clean up.  Pointer is no longer used.

	if (FAILED(hr))
	{
		pRootFolder->Release();
		pTask->Release();

		return std::make_tuple(hr, "Cannot get identification info");
	}

	//  Get the trigger collection to insert the weekly trigger.
	ITriggerCollection* pTriggerCollection = nullptr;
	hr = pTask->get_Triggers(&pTriggerCollection);

	if (FAILED(hr))
	{
		pRootFolder->Release();
		pTask->Release();

		return std::make_tuple(hr, "Cannot get trigger collection");
	}

	ITrigger* pTrigger = nullptr;
	hr = pTriggerCollection->Create(TASK_TRIGGER_DAILY, &pTrigger);
	pTriggerCollection->Release();

	if (FAILED(hr))
	{
		pRootFolder->Release();
		pTask->Release();

		return std::make_tuple(hr, "Cannot create trigger");
	}

	IDailyTrigger* pDailyTrigger = nullptr;
	hr = pTrigger->QueryInterface(IID_IDailyTrigger, (void**)&pDailyTrigger);
	pTrigger->Release();

	if (FAILED(hr))
	{
		pRootFolder->Release();
		pTask->Release();

		return std::make_tuple(hr, "QueryInterface call for IDailyTrigger failed");
	}

	hr = pDailyTrigger->put_Id(bstrId);

	if (FAILED(hr))
	{
		pRootFolder->Release();
		pTask->Release();

		return std::make_tuple(hr, "Cannot put trigger ID");
	}

	//  Set the time the trigger is started
	hr = pDailyTrigger->put_StartBoundary(bstrStart);

	if (FAILED(hr))
	{
		pRootFolder->Release();
		pTask->Release();

		return std::make_tuple(hr, "Cannot put the start boundary");
	}

	//  Set the time when the trigger is ended
	hr = pDailyTrigger->put_EndBoundary(bstrEnd);

	if (FAILED(hr))
	{
		pRootFolder->Release();
		pTask->Release();

		return std::make_tuple(hr, "Cannot put the end boundary");
	}

	//  ------------------------------------------------------
	//  Add an Action to the task. This task will execute bstrExecutablePath.
	IActionCollection* pActionCollection = nullptr;

	//  Get the task action collection pointer.
	hr = pTask->get_Actions(&pActionCollection);

	if (FAILED(hr))
	{
		pRootFolder->Release();
		pTask->Release();

		return std::make_tuple(hr, "Cannot get task collection pointer");
	}

	//  Create the action, specifying that it is an executable action.
	IAction* pAction = nullptr;
	hr = pActionCollection->Create(TASK_ACTION_EXEC, &pAction);
	pActionCollection->Release();

	if (FAILED(hr))
	{
		pRootFolder->Release();
		pTask->Release();

		return std::make_tuple(hr, "Cannot create the action");
	}

	IExecAction* pExecAction = nullptr;
	//  QI for the executable task pointer.
	hr = pAction->QueryInterface(IID_IExecAction, (void**)&pExecAction);
	pAction->Release();

	if (FAILED(hr))
	{
		pRootFolder->Release();
		pTask->Release();

		return std::make_tuple(hr, "QueryInterface call failed on IExecAction");
	}

	//  Set the path of the executable to bstrExecutablePath.
	hr = pExecAction->put_Path(bstrExecutablePath);

	pExecAction->Release();

	if (FAILED(hr))
	{
		pRootFolder->Release();
		pTask->Release();

		return std::make_tuple(hr, "Cannot add path for executable action");
	}

	//  Register the task in the root folder.
	IRegisteredTask* pRegisteredTask = nullptr;
	hr = pRootFolder->RegisterTaskDefinition(
		bstrTaskName,
		pTask,
		TASK_CREATE_OR_UPDATE,
		_variant_t(),
		_variant_t(),
		TASK_LOGON_INTERACTIVE_TOKEN,
		_variant_t(L""),
		&pRegisteredTask
	);

	if (FAILED(hr))
	{
		pRootFolder->Release();
		pTask->Release();

		return std::make_tuple(hr, "Error saving the Task");
	}

	//  Success!  now clean up...
	pRootFolder->Release();
	pTask->Release();
	pRegisteredTask->Release();

	return std::make_tuple(hr, "Success!");
}
