#include "pch.h"
#include "SingleInstance.h"
#include "Util.h"

namespace
{
    constexpr DWORD kTemporaryHandoffWaitMs = 15000;

    std::wstring NormalizeIdentityPath(const std::filesystem::path& identityPath)
    {
        wchar_t full[MAX_PATH + 1] = {};
        const DWORD n = GetFullPathNameW(identityPath.c_str(), MAX_PATH + 1, full, nullptr);
        std::wstring normalized = (n > 0 && n <= MAX_PATH) ? std::wstring(full, n) : identityPath.wstring();

        CharLowerBuffW(normalized.data(), static_cast<DWORD>(normalized.size()));
        return normalized;
    }

    std::string HashIdentityPath(const std::wstring& normalizedPath)
    {
        SHA256 alg;
        alg.add(reinterpret_cast<const char*>(normalizedPath.data()),
                normalizedPath.size() * sizeof(wchar_t));
        return alg.getHash();
    }

    std::wstring MakeObjectName(const wchar_t* prefix, const std::string& hashHex)
    {
        return std::wstring(prefix) + ConvertAnsiToWide(hashHex);
    }
}

namespace single_instance
{
    Guard::Guard(HANDLE mutex, HANDLE activateEvent, bool ownsMutex) noexcept
        : mutex_(mutex), activateEvent_(activateEvent), ownsMutex_(ownsMutex)
    {
    }

    Guard::Guard(Guard&& other) noexcept
        : mutex_(std::exchange(other.mutex_, nullptr)),
          activateEvent_(std::exchange(other.activateEvent_, nullptr)),
          ownsMutex_(std::exchange(other.ownsMutex_, false))
    {
    }

    Guard& Guard::operator=(Guard&& other) noexcept
    {
        if (this != &other)
        {
            Release();
            mutex_ = std::exchange(other.mutex_, nullptr);
            activateEvent_ = std::exchange(other.activateEvent_, nullptr);
            ownsMutex_ = std::exchange(other.ownsMutex_, false);
        }
        return *this;
    }

    Guard::~Guard()
    {
        Release();
    }

    void Guard::Release() noexcept
    {
        if (mutex_)
        {
            if (ownsMutex_)
            {
                ReleaseMutex(mutex_);
                ownsMutex_ = false;
            }
            CloseHandle(mutex_);
            mutex_ = nullptr;
        }

        if (activateEvent_)
        {
            CloseHandle(activateEvent_);
            activateEvent_ = nullptr;
        }
    }

    bool Guard::ConsumeActivationRequest() const
    {
        if (!activateEvent_)
            return false;

        if (WaitForSingleObject(activateEvent_, 0) != WAIT_OBJECT_0)
            return false;

        ResetEvent(activateEvent_);
        return true;
    }

    void Guard::ActivateWindow(HWND hwnd)
    {
        if (!hwnd || !IsWindow(hwnd))
            return;

        if (IsIconic(hwnd))
            ShowWindow(hwnd, SW_RESTORE);
        else
            ShowWindow(hwnd, SW_SHOW);

        BringWindowToTop(hwnd);
        SetForegroundWindow(hwnd);

        FLASHWINFO fi = {};
        fi.cbSize = sizeof(fi);
        fi.hwnd = hwnd;
        fi.dwFlags = FLASHW_TRAY | FLASHW_TIMERNOFG;
        fi.uCount = 3;
        fi.dwTimeout = 0;
        FlashWindowEx(&fi);

        spdlog::debug("Activated existing updater window {:#x}",
                      static_cast<std::uintptr_t>(reinterpret_cast<ULONG_PTR>(hwnd)));
    }

    std::expected<AcquireResult, std::string>
    AcquireResult::TryAcquire(const std::filesystem::path& identityPath, bool waitForHandoff)
    {
        const std::wstring normalized = NormalizeIdentityPath(identityPath);
        const std::string hashHex = HashIdentityPath(normalized);

        const std::wstring mutexName = MakeObjectName(L"Local\\Nefarius.Vicius.SingleInstance.", hashHex);
        const std::wstring eventName = MakeObjectName(L"Local\\Nefarius.Vicius.Activate.", hashHex);

        HANDLE activateEvent = CreateEventW(nullptr, TRUE /* manual-reset */, FALSE, eventName.c_str());
        if (!activateEvent)
        {
            return std::unexpected(
                std::format("CreateEventW failed: {}", winapi::GetLastErrorStdStr()));
        }

        HANDLE mutex = CreateMutexW(nullptr, FALSE, mutexName.c_str());
        if (!mutex)
        {
            const DWORD err = GetLastError();
            CloseHandle(activateEvent);
            return std::unexpected(
                std::format("CreateMutexW failed: {}", winapi::GetLastErrorStdStr(err)));
        }

        const DWORD waitMs = waitForHandoff ? kTemporaryHandoffWaitMs : 0;
        if (waitForHandoff)
        {
            spdlog::debug("Temporary copy waiting up to {} ms for single-instance handoff ({})",
                          waitMs, identityPath.string());
        }

        const DWORD waitResult = WaitForSingleObject(mutex, waitMs);
        if (waitResult == WAIT_OBJECT_0 || waitResult == WAIT_ABANDONED)
        {
            spdlog::info("Acquired single-instance lock for {}", identityPath.string());
            AcquireResult result;
            result.status = AcquireStatus::Primary;
            result.guard = Guard(mutex, activateEvent, true);
            return std::move(result);
        }

        // Another instance still holds the lock — ask it to focus its UI, then bail.
        if (!SetEvent(activateEvent))
        {
            spdlog::warn("Failed to signal activation event: {}", winapi::GetLastErrorStdStr());
        }

        CloseHandle(mutex);
        CloseHandle(activateEvent);

        spdlog::info(
            "Another updater instance is already running for {}; signaling activation and exiting",
            identityPath.string());

        AcquireResult result;
        result.status = AcquireStatus::Duplicate;
        return std::move(result);
    }
}
