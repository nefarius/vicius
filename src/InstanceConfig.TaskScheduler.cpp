#include "pch.h"
#include "Common.h"
#include "InstanceConfig.hpp"


std::tuple<HRESULT, std::string> models::InstanceConfig::CreateScheduledTask(const std::string& launchArgs) const
{
    // task name
    BSTR bstrTaskName = SysAllocString(ConvertAnsiToWide(appFilename).c_str());
    // fully qualified executable path
    BSTR bstrExecutablePath = SysAllocString(ConvertAnsiToWide(appPath.string()).c_str());
    // string id
    BSTR bstrId = SysAllocString(L"DailyTrigger");

    // randomize start time to avoid DDoS-ing the server on big installations
    std::random_device rd;
    std::mt19937 rng(rd());
    std::uniform_int_distribution<int> uniH(6, 22);
    std::uniform_int_distribution<int> uniM(1, 59);
    std::stringstream ss;
    ss << "2023-01-01T"
        << std::setw(2) << std::setfill('0') << uniH(rng)
        << ":"
        << std::setw(2) << std::setfill('0') << uniM(rng)
        << ":00";
    const std::string timeStr = ss.str();

    // start boundary - format should be YYYY-MM-DDTHH:MM:SS(+-)(timezone).
    BSTR bstrStart = SysAllocString(ConvertAnsiToWide(timeStr).c_str()); // TODO: make configurable
    // not used currently
    BSTR bstrEnd = SysAllocString(L"2053-01-01T12:00:00"); // end boundary - ""
    BSTR bstrAuthor = SysAllocString(ConvertAnsiToWide(appFilename).c_str());

    std::stringstream argsBuilder;
    argsBuilder << launchArgs << " " << NV_CLI_PARAM_LOG_LEVEL << " " << magic_enum::enum_name(spdlog::get_level());
    std::string argsBuilt = argsBuilder.str();
    spdlog::debug("argsBuilt = {}", argsBuilt);
    BSTR bstrLaunchArgs = SysAllocString(ConvertAnsiToWide(argsBuilt).c_str());

    // clean up all resources when going out of scope
    sg::make_scope_guard(
        [bstrTaskName, bstrExecutablePath, bstrId, bstrAuthor, bstrStart, bstrEnd, bstrLaunchArgs]() noexcept
        {
            if (bstrTaskName) SysFreeString(bstrTaskName);
            if (bstrExecutablePath) SysFreeString(bstrExecutablePath);
            if (bstrId) SysFreeString(bstrId);
            if (bstrAuthor) SysFreeString(bstrAuthor);
            if (bstrStart) SysFreeString(bstrStart);
            if (bstrEnd) SysFreeString(bstrEnd);
            if (bstrLaunchArgs)SysFreeString(bstrLaunchArgs);

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
        _com_error err(hr);
        LPCTSTR errMsg = err.ErrorMessage();
        spdlog::error("CoCreateInstance failed with {}", ConvertWideToANSI(errMsg));
        return std::make_tuple(hr, "Failed to create an instance of ITaskService");
    }

    //  Connect to the task service.
    hr = pService->Connect(_variant_t(), _variant_t(),
                           _variant_t(), _variant_t());
    if (FAILED(hr))
    {
        pService->Release();

        _com_error err(hr);
        LPCTSTR errMsg = err.ErrorMessage();
        spdlog::error("Connect failed with {}", ConvertWideToANSI(errMsg));
        return std::make_tuple(hr, "ITaskService Connect failed");
    }

    //  Get the pointer to the root task folder that will hold the new task that is registered.
    ITaskFolder* pRootFolder = nullptr;
    hr = pService->GetFolder(_bstr_t(L"\\"), &pRootFolder);

    if (FAILED(hr))
    {
        pService->Release();

        _com_error err(hr);
        LPCTSTR errMsg = err.ErrorMessage();
        spdlog::error("GetFolder failed with {}", ConvertWideToANSI(errMsg));
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

        _com_error err(hr);
        LPCTSTR errMsg = err.ErrorMessage();
        spdlog::error("NewTask failed with {}", ConvertWideToANSI(errMsg));
        return std::make_tuple(hr, "Failed to create a task definition");
    }

    //  Get the registration info for setting the identification.
    IRegistrationInfo* pRegInfo = nullptr;
    hr = pTask->get_RegistrationInfo(&pRegInfo);

    if (FAILED(hr))
    {
        pRootFolder->Release();
        pTask->Release();

        _com_error err(hr);
        LPCTSTR errMsg = err.ErrorMessage();
        spdlog::error("get_RegistrationInfo failed with {}", ConvertWideToANSI(errMsg));
        return std::make_tuple(hr, "Cannot get identification pointer");
    }

    hr = pRegInfo->put_Author(bstrAuthor);
    pRegInfo->Release(); // COM clean up.  Pointer is no longer used.

    if (FAILED(hr))
    {
        pRootFolder->Release();
        pTask->Release();

        _com_error err(hr);
        LPCTSTR errMsg = err.ErrorMessage();
        spdlog::error("put_Author failed with {}", ConvertWideToANSI(errMsg));
        return std::make_tuple(hr, "Cannot get identification info");
    }

#pragma region Triggers

    // Get the trigger collection to insert the weekly trigger.
    ITriggerCollection* pTriggerCollection = nullptr;
    hr = pTask->get_Triggers(&pTriggerCollection);

    if (FAILED(hr))
    {
        pRootFolder->Release();
        pTask->Release();

        _com_error err(hr);
        LPCTSTR errMsg = err.ErrorMessage();
        spdlog::error("get_Triggers failed with {}", ConvertWideToANSI(errMsg));
        return std::make_tuple(hr, "Cannot get trigger collection");
    }

    ITrigger* pTrigger = nullptr;
    hr = pTriggerCollection->Create(TASK_TRIGGER_DAILY, &pTrigger);
    pTriggerCollection->Release();

    if (FAILED(hr))
    {
        pRootFolder->Release();
        pTask->Release();

        _com_error err(hr);
        LPCTSTR errMsg = err.ErrorMessage();
        spdlog::error("pTriggerCollection->Create failed with {}", ConvertWideToANSI(errMsg));
        return std::make_tuple(hr, "Cannot create trigger");
    }

    IDailyTrigger* pDailyTrigger = nullptr;
    hr = pTrigger->QueryInterface(IID_PPV_ARGS(&pDailyTrigger));
    pTrigger->Release();

    if (FAILED(hr))
    {
        pRootFolder->Release();
        pTask->Release();

        _com_error err(hr);
        LPCTSTR errMsg = err.ErrorMessage();
        spdlog::error("pTrigger->QueryInterface failed with {}", ConvertWideToANSI(errMsg));
        return std::make_tuple(hr, "QueryInterface call for IDailyTrigger failed");
    }

    hr = pDailyTrigger->put_Id(bstrId);

    if (FAILED(hr))
    {
        pRootFolder->Release();
        pTask->Release();

        _com_error err(hr);
        LPCTSTR errMsg = err.ErrorMessage();
        spdlog::error("put_Id failed with {}", ConvertWideToANSI(errMsg));
        return std::make_tuple(hr, "Cannot put trigger ID");
    }

    //  Set the time the trigger is started
    hr = pDailyTrigger->put_StartBoundary(bstrStart);

    if (FAILED(hr))
    {
        pRootFolder->Release();
        pTask->Release();

        _com_error err(hr);
        LPCTSTR errMsg = err.ErrorMessage();
        spdlog::error("put_StartBoundary failed with {}", ConvertWideToANSI(errMsg));
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

    //  Define the interval for the daily trigger. An interval of 2 produces an
    //  every other day schedule
    hr = pDailyTrigger->put_DaysInterval(1);

    if (FAILED(hr))
    {
        pRootFolder->Release();
        pDailyTrigger->Release();
        pTask->Release();

        return std::make_tuple(hr, "Cannot set daily interval");
    }

#pragma endregion

#pragma region Actions

    //  ------------------------------------------------------
    //  Add an Action to the task. This task will execute bstrExecutablePath.
    IActionCollection* pActionCollection = nullptr;

    //  Get the task action collection pointer.
    hr = pTask->get_Actions(&pActionCollection);

    if (FAILED(hr))
    {
        pRootFolder->Release();
        pTask->Release();

        _com_error err(hr);
        LPCTSTR errMsg = err.ErrorMessage();
        spdlog::error("get_Actions failed with {}", ConvertWideToANSI(errMsg));
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

        _com_error err(hr);
        LPCTSTR errMsg = err.ErrorMessage();
        spdlog::error("pActionCollection->Create failed with {}", ConvertWideToANSI(errMsg));
        return std::make_tuple(hr, "Cannot create the action");
    }

    IExecAction* pExecAction = nullptr;
    //  QI for the executable task pointer.
    hr = pAction->QueryInterface(IID_PPV_ARGS(&pExecAction));
    pAction->Release();

    if (FAILED(hr))
    {
        pRootFolder->Release();
        pTask->Release();

        _com_error err(hr);
        LPCTSTR errMsg = err.ErrorMessage();
        spdlog::error("pAction->QueryInterface failed with {}", ConvertWideToANSI(errMsg));
        return std::make_tuple(hr, "QueryInterface call failed on IExecAction");
    }

    //  Set the path of the executable to bstrExecutablePath.
    hr = pExecAction->put_Path(bstrExecutablePath);

    if (FAILED(hr))
    {
        pRootFolder->Release();
        pTask->Release();

        _com_error err(hr);
        LPCTSTR errMsg = err.ErrorMessage();
        spdlog::error("put_Path failed with {}", ConvertWideToANSI(errMsg));
        return std::make_tuple(hr, "Cannot add path for executable action");
    }

    // set launch arguments
    hr = pExecAction->put_Arguments(bstrLaunchArgs);

    pExecAction->Release();

    if (FAILED(hr))
    {
        pRootFolder->Release();
        pTask->Release();

        _com_error err(hr);
        LPCTSTR errMsg = err.ErrorMessage();
        spdlog::error("put_Arguments failed with {}", ConvertWideToANSI(errMsg));
        return std::make_tuple(hr, "Cannot launch arguments for executable action");
    }

#pragma endregion

#pragma region Principal

    //  ------------------------------------------------------
    //  Create the principal for the task
    IPrincipal* pPrincipal = NULL;
    hr = pTask->get_Principal(&pPrincipal);
    if (FAILED(hr))
    {
        pRootFolder->Release();
        pTask->Release();
        return std::make_tuple(hr, "Cannot get principal");
    }

    //  Set up principal information: 
    hr = pPrincipal->put_Id(_bstr_t(L"Author"));
    if (FAILED(hr))
    {
        return std::make_tuple(hr, "Cannot set principal ID");
    }

    hr = pPrincipal->put_LogonType(TASK_LOGON_INTERACTIVE_TOKEN);
    if (FAILED(hr))
    {
        return std::make_tuple(hr, "Cannot set logon type");
    }

    //  Run the task with the least privileges (LUA) 
    hr = pPrincipal->put_RunLevel(TASK_RUNLEVEL_LUA);
    pPrincipal->Release();
    if (FAILED(hr))
    {
        pRootFolder->Release();
        pTask->Release();
        return std::make_tuple(hr, "Cannot put principal run level");
    }

#pragma endregion

#pragma region Settings

    // Get task settings
    ITaskSettings* pTaskSettings = nullptr;
    hr = pTask->get_Settings(&pTaskSettings);

    if (FAILED(hr))
    {
        pRootFolder->Release();
        pTask->Release();

        _com_error err(hr);
        LPCTSTR errMsg = err.ErrorMessage();
        spdlog::error("get_Settings failed with {}", ConvertWideToANSI(errMsg));
        return std::make_tuple(hr, "Cannot get task settings");
    }

    // Allow task to be run when run schedule was missed
    hr = pTaskSettings->put_StartWhenAvailable(TRUE);
    if (FAILED(hr))
    {
        pRootFolder->Release();
        pTask->Release();

        _com_error err(hr);
        LPCTSTR errMsg = err.ErrorMessage();
        spdlog::error("put_StartWhenAvailable failed with {}", ConvertWideToANSI(errMsg));
        return std::make_tuple(hr, "Failed to set start when available");
    }

    pTaskSettings->Release();

#pragma endregion

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

        _com_error err(hr);
        LPCTSTR errMsg = err.ErrorMessage();
        spdlog::error("RegisterTaskDefinition failed with {}", ConvertWideToANSI(errMsg));
        return std::make_tuple(hr, "Error saving the Task");
    }

    //  Success!  now clean up...
    pRootFolder->Release();
    pTask->Release();
    pRegisteredTask->Release();

    return std::make_tuple(hr, "Success!");
}

std::tuple<HRESULT, std::string> models::InstanceConfig::RemoveScheduledTask() const
{
    BSTR bstrTaskName = SysAllocString(ConvertAnsiToWide(appFilename).c_str());

    sg::make_scope_guard(
        [bstrTaskName]() noexcept
        {
            if (bstrTaskName)SysFreeString(bstrTaskName);

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

    pRootFolder->DeleteTask(bstrTaskName, 0);
    pRootFolder->Release();
    pService->Release();

    return std::make_tuple(hr, "Success!");
}
