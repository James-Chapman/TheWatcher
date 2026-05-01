#pragma once

#include <condition_variable>
#include <exception>
#include <mutex>
#include <string>

namespace thewatcher::server
{

struct StartupResult
{
    bool started = false;
    std::string message;
    std::exception_ptr error;
};

// Coordinates foreground startup with the worker thread that runs Server::run().
class StartupSignal
{
public:
    void succeed()
    {
        std::lock_guard lock(mutex_);
        if (ready_)
            return;
        ready_ = true;
        result_.started = true;
        cv_.notify_all();
    }

    void fail(std::exception_ptr error)
    {
        std::lock_guard lock(mutex_);
        if (ready_)
            return;
        ready_ = true;
        result_.started = false;
        result_.error = error;
        result_.message = message_from(error);
        cv_.notify_all();
    }

    StartupResult wait()
    {
        std::unique_lock lock(mutex_);
        cv_.wait(lock, [this] {
            return ready_;
        });
        return result_;
    }

private:
    static std::string message_from(std::exception_ptr error)
    {
        if (!error)
            return "unknown startup failure";

        try
        {
            std::rethrow_exception(error);
        }
        catch (const std::exception& e)
        {
            return e.what();
        }
        catch (...)
        {
            return "unknown startup failure";
        }
    }

    std::mutex mutex_;
    std::condition_variable cv_;
    bool ready_ = false;
    StartupResult result_;
};

} // namespace thewatcher::server
