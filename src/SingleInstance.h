#pragma once

#include <filesystem>
#include <optional>
#include <string>
#include <expected>

namespace single_instance
{
    /**
     * \brief Outcome of attempting to become the sole updater for an identity path.
     */
    enum class AcquireStatus
    {
        /** This process now owns the single-instance lock. */
        Primary,
        /** Another process already owns the lock; activation was signaled. */
        Duplicate,
    };

    /**
     * \brief RAII guard that serializes updater instances for a given executable path.
     *
     * Ownership is held for the lifetime of the object. A second process targeting the
     * same identity signals a named activation event and reports \c Duplicate so the
     * caller can exit. Temporary-copy handoff may wait briefly for the parent to release
     * the mutex before deciding.
     */
    class Guard
    {
    public:
        Guard() = default;
        Guard(const Guard&) = delete;
        Guard& operator=(const Guard&) = delete;

        Guard(Guard&& other) noexcept;
        Guard& operator=(Guard&& other) noexcept;

        ~Guard();

        /**
         * \brief Consumes a pending activation request from a duplicate instance.
         * \return True if a request was pending (and is now cleared).
         */
        [[nodiscard]] bool ConsumeActivationRequest() const;

        /** Restores and best-effort focuses \p hwnd (no-op if null / invalid). */
        static void ActivateWindow(HWND hwnd);

    private:
        friend struct AcquireResult;

        Guard(HANDLE mutex, HANDLE activateEvent, bool ownsMutex) noexcept;

        void Release() noexcept;

        HANDLE mutex_{nullptr};
        HANDLE activateEvent_{nullptr};
        bool ownsMutex_{false};
    };

    /**
     * \brief Result of \c TryAcquire — move-only because it may own a \c Guard.
     */
    struct AcquireResult
    {
        AcquireStatus status{AcquireStatus::Duplicate};
        /** Present only when \c status is \c Primary. */
        std::optional<Guard> guard;

        AcquireResult() = default;
        AcquireResult(const AcquireResult&) = delete;
        AcquireResult& operator=(const AcquireResult&) = delete;
        AcquireResult(AcquireResult&&) noexcept = default;
        AcquireResult& operator=(AcquireResult&&) noexcept = default;

        /**
         * \brief Attempts to acquire the per-path single-instance lock.
         * \param identityPath Logical updater path used as the concurrency key
         *                     (original path for temporary copies).
         * \param waitForHandoff When true (temporary child), wait briefly for the
         *                       parent process to release the mutex.
         * \return On success: Primary with a living Guard, or Duplicate with no Guard.
         *         On hard failure (kernel object creation), an error string.
         */
        [[nodiscard]] static std::expected<AcquireResult, std::string>
        TryAcquire(const std::filesystem::path& identityPath, bool waitForHandoff);
    };
}
