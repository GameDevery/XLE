// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "Log.h"
#include "LogStartup.h"
#include "OutputStream.h"
#include "GlobalServices.h"
#include "../Utility/Streams/FileUtils.h"
#include "../Utility/Streams/Stream.h"
#include "../Utility/FunctionUtils.h"
#include "../Utility/MemoryUtils.h"
#include "../Utility/SystemUtils.h"
#include "../Utility/StringFormat.h"
#include <assert.h>

#if PLATFORMOS_TARGET == PLATFORMOS_WINDOWS
    #include "../Core/WinAPI/IncludeWindows.h"
    #include "../Foreign/StackWalker/StackWalker.h"
#endif

    // We can't use the default initialisation method for easylogging++
    // because is causes a "LoaderLock" exception when used with C++/CLI dlls.
    // It also doesn't work well when sharing a single log file across dlls.
    // Anyway, the default behaviour isn't great for our needs.
    // So, we need to use "INITIALIZE_NULL" here, and manually construct
    // a "GlobalStorage" object below...
INITIALIZE_NULL_EASYLOGGINGPP

#if defined(_DEBUG)
    #define REDIRECT_COUT
#endif

#pragma warning(disable:4592)

//////////////////////////////////

static auto Fn_GetStorage = ConstHash64<'getl', 'ogst', 'orag', 'e'>::Value;
static auto Fn_CoutRedirectModule = ConstHash64<'cout', 'redi', 'rect'>::Value;
static auto Fn_LogMainModule = ConstHash64<'logm', 'ainm', 'odul', 'e'>::Value;
static auto Fn_GuidGen = ConstHash64<'guid', 'gen'>::Value;
static auto Fn_RedirectCout = ConstHash64<'redi', 'rect', 'cout'>::Value;

namespace ConsoleRig
{
    #if defined(REDIRECT_COUT)
        template <typename CharType>
            class StdCToXLEStreamAdapter : public std::basic_streambuf<CharType>
        {
        public:
            void Reset(std::shared_ptr<Utility::OutputStream> chain) { _chain = chain; }
            StdCToXLEStreamAdapter(std::shared_ptr<Utility::OutputStream> chain);
            ~StdCToXLEStreamAdapter();
        protected:
            std::shared_ptr<Utility::OutputStream> _chain;

            virtual std::streamsize xsputn(const CharType* s, std::streamsize count);
            virtual int sync();
        };

        template <typename CharType>
            StdCToXLEStreamAdapter<CharType>::StdCToXLEStreamAdapter(std::shared_ptr<Utility::OutputStream> chain) : _chain(chain) {}
        template <typename CharType>
            StdCToXLEStreamAdapter<CharType>::~StdCToXLEStreamAdapter() {}

        template <typename CharType>
            std::streamsize StdCToXLEStreamAdapter<CharType>::xsputn(const CharType* s, std::streamsize count)
        {
            assert(_chain);
            _chain->Write(s, int(sizeof(CharType) * count));
            return count;
        }

        template <typename CharType>
            int StdCToXLEStreamAdapter<CharType>::sync()
        {
            _chain->Flush();
            return 0;
        }

        std::shared_ptr<Utility::OutputStream>      GetSharedDebuggerWarningStream();

        static StdCToXLEStreamAdapter<char> s_coutAdapter(nullptr);
        static std::basic_streambuf<char>* s_oldCoutStreamBuf = nullptr;
    #endif

    static void SendExceptionToLogger(const ::Exceptions::BasicLabel&);

    void Logging_Startup(const char configFile[], const char logFileName[])
    {
        auto currentModule = GetCurrentModuleId();
        auto& serv = GlobalServices::GetCrossModule()._services;

            // It can be handy to redirect std::cout to the debugger output
            // window in Visual Studio (etc)
            // We can do this with an adapter to connect out DebufferWarningStream
            // object to a c++ std::stream_buf
        #if defined(REDIRECT_COUT)
            
            bool doRedirect = serv.Call<bool>(Fn_RedirectCout);
            if (doRedirect && !serv.Has<ModuleId()>(Fn_CoutRedirectModule)) {
                s_coutAdapter.Reset(GetSharedDebuggerWarningStream());
                s_oldCoutStreamBuf = std::cout.rdbuf();
                std::cout.rdbuf(&s_coutAdapter);

                serv.Add(Fn_CoutRedirectModule, [=](){ return currentModule; });
            }

        #endif

        using StoragePtr = decltype(el::Helpers::storage());

            //
            //  Check to see if there is an existing logging object in the
            //  global services. If there is, it will have been created by
            //  another module.
            //  If it's there, we can just re-use it. Otherwise we need to
            //  create a new one and set it up...
            //
        if (!serv.Has<StoragePtr()>(Fn_GetStorage)) {

            el::Helpers::setStorage(
                std::make_shared<el::base::Storage>(
                    el::LogBuilderPtr(new el::base::DefaultLogBuilder())));

            if (!logFileName) { logFileName = "int/log.txt"; }
            el::Configurations c;
            c.setToDefault();
            c.setGlobally(el::ConfigurationType::Filename, logFileName);

                // if a configuration file exists, 
            if (configFile) {
                size_t configFileLength = 0;
                auto configFileData = RawFS::TryLoadFileAsMemoryBlock(configFile, &configFileLength);
                if (configFileData && configFileLength) {
                    c.parseFromText(std::string(configFileData.get(), &configFileData[configFileLength]));
                }
            }

            el::Loggers::reconfigureAllLoggers(c);

            serv.Add(Fn_GetStorage, el::Helpers::storage);
            serv.Add(Fn_LogMainModule, [=](){ return currentModule; });

            auto& onThrow = GlobalOnThrowCallback();
            if (!onThrow)
                onThrow = &SendExceptionToLogger;

        } else {

            auto storage = serv.Call<StoragePtr>(Fn_GetStorage);
            el::Helpers::setStorage(storage);

        }
    }

    void Logging_Shutdown()
    {
        auto& serv = GlobalServices::GetCrossModule()._services;
        auto currentModule = GetCurrentModuleId();

        el::Loggers::flushAll();
        el::Helpers::setStorage(nullptr);

            // this will throw an exception if no module has successfully initialised
            // logging
        if (serv.Call<ModuleId>(Fn_LogMainModule) == currentModule) {
            serv.Remove(Fn_GetStorage);
            serv.Remove(Fn_LogMainModule);
        }

        #if defined(REDIRECT_COUT)
            ModuleId testModule = 0;
            if (serv.TryCall<ModuleId>(testModule, Fn_CoutRedirectModule) && (testModule == currentModule)) {
                if (s_oldCoutStreamBuf)
                    std::cout.rdbuf(s_oldCoutStreamBuf);
                serv.Remove(Fn_CoutRedirectModule);
            }
        #endif
    }

    #if PLATFORMOS_TARGET == PLATFORMOS_WINDOWS
        class StackWalkerToLog : public StackWalker
        {
        protected:
            virtual void OnOutput(LPCSTR) {}

            void OnCallstackEntry(CallstackEntryType eType, int frameNumber, CallstackEntry &entry)
            {
                    // We should normally have 3 entries on the callstack ahead of what we want:
                    //  StackWalker::ShowCallstack
                    //  ConsoleRig::SendExceptionToLogger
                    //  Utility::Throw
                if ((frameNumber >= 3) && (eType != lastEntry) && (entry.offset != 0)) {
                    if (entry.lineFileName[0] == 0) {
                        LogAlwaysError 
                            << std::hex << entry.offset << std::dec
                            << " (" << entry.moduleName << "): "
                            << entry.name;
                    } else {
                        LogAlwaysError 
                            << entry.lineFileName << " (" << entry.lineNumber << "): "
                            << ((entry.undFullName[0] != 0) ? entry.undFullName : ((entry.undName[0] != 0) ? entry.undName : entry.name))
                            ;
                    }
                }
            }
        };
    #endif

    static void SendExceptionToLogger(const ::Exceptions::BasicLabel& e)
    {
        TRY
        {
            if (!e.CustomReport()) {
                #if FEATURE_RTTI
                    LogAlwaysError << "Throwing Exception -- " << typeid(e).name() << ". Extra information follows:";
                #else
                    LogAlwaysError << "Throwing Exception. Extra information follows:";
                #endif
                LogAlwaysError << e.what();

                    // report this exception to the logger (including callstack information)
                #if PLATFORMOS_TARGET == PLATFORMOS_WINDOWS
                    static StackWalkerToLog walker;
                    walker.ShowCallstack(7);
                #endif
            }
        } CATCH (...) {
            // Encountering another exception at this point would be trouble.
            // We have to suppress any exception that happen during reporting,
            // and allow the exception, 'e' to be handled
        } CATCH_END
    }

    namespace Internal
    {
        static LogLevel AsLogLevel(el::Level level)
        {
            switch (level) {
            case el::Level::Fatal: return LogLevel::Fatal; 

            default:
            case el::Level::Global:
            case el::Level::Debug:
            case el::Level::Trace:
            case el::Level::Error: return LogLevel::Error; 
            case el::Level::Warning: return LogLevel::Warning; 
            case el::Level::Verbose: return LogLevel::Verbose; 
            case el::Level::Info: return LogLevel::Info; 
            }
        }

        class LogHelper : public el::LogDispatchCallback
        {
        public:
            void SetUpstream(std::shared_ptr<LogCallback> upstream);
            LogHelper();
            ~LogHelper();
        private:
            virtual void handle(const el::LogDispatchData* handlePtr);
            std::weak_ptr<LogCallback> _upstream;
        };

        void LogHelper::handle(const el::LogDispatchData* handlePtr)
        {
            auto l = _upstream.lock();
            if (l) {
                LogLevel level = AsLogLevel(handlePtr->logMessage()->level());
                l->OnDispatch(level, handlePtr->logMessage()->message());
            }
        }
        void LogHelper::SetUpstream(std::shared_ptr<LogCallback> upstream)
        {
            _upstream = upstream;
        }
        LogHelper::LogHelper() {}
        LogHelper::~LogHelper() {}
    }

    void LogCallback::Enable()
    {
        auto storage = el::Helpers::storage();
        if (storage) {
            std::string guid = (StringMeld<64>() << _guid).AsString();
            auto* helper = storage->logDispatchCallback<Internal::LogHelper>(guid);
            if (!helper) {
                storage->installLogDispatchCallback<Internal::LogHelper>(guid);
                helper = storage->logDispatchCallback<Internal::LogHelper>(guid);
                assert(helper);
                helper->SetUpstream(shared_from_this());
            }
        }
    }

    void LogCallback::Disable()
    {
        auto storage = el::Helpers::storage();
        if (storage) {
            storage->uninstallLogDispatchCallback<Internal::LogHelper>(
                std::string(StringMeld<64>() << _guid));
        }
    }

    LogCallback::LogCallback()
    {
        auto& serv = GlobalServices::GetCrossModule()._services;
        _guid = serv.Call<uint64>(Fn_GuidGen);
    }

    LogCallback::~LogCallback()
    {
        Disable();
    }
}

namespace LogUtilMethods
{

        //  note that we can't pass a printf style string to easylogging++ easily.
        //  but we do have some utility functions (like PrintFormatV) that can
        //  handle long printf's.
    static const unsigned LogStringMaxLength = 2048;

    void LogVerboseF(unsigned level, const char format[], ...)
    {
        char buffer[LogStringMaxLength];
        va_list args;
        va_start(args, format);
        snprintf(buffer, dimof(buffer), format, args);
        va_end(args);

        LogVerbose(level) << buffer;
    }

    void LogInfoF(const char format[], ...)
    {
        char buffer[LogStringMaxLength];
        va_list args;
        va_start(args, format);
        snprintf(buffer, dimof(buffer), format, args);
        va_end(args);

        LogInfo << buffer;
    }

    void LogWarningF(const char format[], ...)
    {
        char buffer[LogStringMaxLength];
        va_list args;
        va_start(args, format);
        snprintf(buffer, dimof(buffer), format, args);
        va_end(args);

        LogWarning << buffer;
    }
    
    void LogAlwaysVerboseF(unsigned level, const char format[], ...)
    {
        char buffer[LogStringMaxLength];
        va_list args;
        va_start(args, format);
        snprintf(buffer, dimof(buffer), format, args);
        va_end(args);

        LogAlwaysVerbose(level) << buffer;
    }

    void LogAlwaysInfoF(const char format[], ...)
    {
        char buffer[LogStringMaxLength];
        va_list args;
        va_start(args, format);
        snprintf(buffer, dimof(buffer), format, args);
        va_end(args);

        LogAlwaysInfo << buffer;
    }

    void LogAlwaysWarningF(const char format[], ...)
    {
        char buffer[LogStringMaxLength];
        va_list args;
        va_start(args, format);
        snprintf(buffer, dimof(buffer), format, args);
        va_end(args);

        LogAlwaysWarning << buffer;
    }

    void LogAlwaysErrorF(const char format[], ...)
    {
        char buffer[LogStringMaxLength];
        va_list args;
        va_start(args, format);
        snprintf(buffer, dimof(buffer), format, args);
        va_end(args);

        LogAlwaysError << buffer;
    }

    void LogAlwaysFatalF(const char format[], ...)
    {
        char buffer[LogStringMaxLength];
        va_list args;
        va_start(args, format);
        snprintf(buffer, dimof(buffer), format, args);
        va_end(args);

        LogAlwaysFatal << buffer;
    }
}


#if PLATFORMOS_TARGET == PLATFORMOS_WINDOWS

#include "../Core/WinAPI/IncludeWindows.h"

namespace el { namespace base { namespace utils
{
#define _ELPP_OS_WINDOWS 1
#define ELPP_COMPILER_MSVC 1

#if _ELPP_OS_WINDOWS
    void DateTime::gettimeofday(struct timeval *tv) {
        if (tv != nullptr) {
#   if ELPP_COMPILER_MSVC || defined(_MSC_EXTENSIONS)
            const unsigned __int64 delta_ = 11644473600000000Ui64;
#   else
            const unsigned __int64 delta_ = 11644473600000000ULL;
#   endif  // ELPP_COMPILER_MSVC || defined(_MSC_EXTENSIONS)
            const double secOffSet = 0.000001;
            const unsigned long usecOffSet = 1000000;
            FILETIME fileTime;
            GetSystemTimeAsFileTime(&fileTime);
            unsigned __int64 present = 0;
            present |= fileTime.dwHighDateTime;
            present = present << 32;
            present |= fileTime.dwLowDateTime;
            present /= 10;  // mic-sec
           // Subtract the difference
            present -= delta_;
            tv->tv_sec = static_cast<long>(present * secOffSet);
            tv->tv_usec = static_cast<long>(present % usecOffSet);
        }
    }
#endif // _ELPP_OS_WINDOWS
}}}

#endif
