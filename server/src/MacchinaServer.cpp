//
// MacchinaServer.cpp
//
// The bundle container application for macchina.io
//
// Copyright (c) 2014, Applied Informatics Software Engineering GmbH.
// All rights reserved.
//
// SPDX-License-Identifier: GPL-3.0-only
//


#include "Poco/Util/ServerApplication.h"
#include "Poco/Util/Option.h"
#include "Poco/Util/OptionSet.h"
#include "Poco/Util/HelpFormatter.h"
#include "Poco/Util/AbstractConfiguration.h"
#include "Poco/Util/PropertyFileConfiguration.h"
#include "Poco/OSP/OSPSubsystem.h"
#include "Poco/OSP/ServiceRegistry.h"
#include "Poco/DataURIStreamFactory.h"
#include "Poco/ErrorHandler.h"
#include "Poco/Environment.h"
#include "Poco/Format.h"
#include "Poco/File.h"
#include "Poco/Logger.h"
#include "Poco/OSP/Properties.h"
#include "Poco/OSP/ServiceRef.h"
#include "Poco/OpenTelemetry/TelemetryClient.h"
#include "Poco/OpenTelemetry/TelemetryLoggingChannel.h"
#include "Poco/OpenTelemetry/TelemetryService.h"
#include "Poco/JSON/Object.h"
#include "Poco/JSON/Parser.h"
#include "Poco/Path.h"
#include "Poco/Process.h"
#include "Poco/SplitterChannel.h"
#include "Poco/StringTokenizer.h"
#include "stok/services/common/TextMessageBus.h"
#include <algorithm>
#include <atomic>
#include <cctype>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <memory>
#include <mutex>
#include <thread>

#if defined(_WIN32)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

namespace
{
	class NativeProcessHandle final: public Poco::ProcessHandle
	{
	public:
		NativeProcessHandle(HANDLE processHandle, Poco::UInt32 pid):
			Poco::ProcessHandle(new Poco::ProcessHandleImpl(processHandle, pid))
		{
		}
	};

	struct WindowSearchContext
	{
		DWORD processId = 0;
		HWND window = nullptr;
	};

	BOOL CALLBACK FindTopLevelWindowForProcess(HWND hwnd, LPARAM lParam)
	{
		auto* context = reinterpret_cast<WindowSearchContext*>(lParam);
		if (!context)
		{
			return FALSE;
		}

		DWORD processId = 0;
		GetWindowThreadProcessId(hwnd, &processId);
		if (processId != context->processId)
		{
			return TRUE;
		}

		if (GetParent(hwnd) != nullptr)
		{
			return TRUE;
		}

		context->window = hwnd;
		return FALSE;
	}

	std::string processImageFileName(DWORD processId)
	{
		HANDLE process = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, processId);
		if (!process)
		{
			return {};
		}

		wchar_t buffer[MAX_PATH] = {};
		DWORD size = MAX_PATH;
		const BOOL ok = QueryFullProcessImageNameW(process, 0, buffer, &size);
		CloseHandle(process);
		if (!ok)
		{
			return {};
		}

		std::wstring path(buffer, size);
		const std::wstring::size_type slash = path.find_last_of(L"\\/");
		std::wstring fileName = slash == std::wstring::npos ? path : path.substr(slash + 1);
		for (wchar_t& ch: fileName)
		{
			ch = static_cast<wchar_t>(towlower(ch));
		}

		std::string result;
		result.reserve(fileName.size());
		for (wchar_t ch: fileName)
		{
			result.push_back(static_cast<char>(ch));
		}
		return result;
	}

	enum class ProcessProbeState
	{
		running,
		exited,
		unknown
	};

	ProcessProbeState probeProcessState(DWORD processId, DWORD& exitCode)
	{
		if (processId == 0)
		{
			exitCode = 1;
			return ProcessProbeState::exited;
		}

		HANDLE process = OpenProcess(
			SYNCHRONIZE | PROCESS_QUERY_LIMITED_INFORMATION,
			FALSE,
			processId);
		if (!process)
		{
			if (GetLastError() == ERROR_INVALID_PARAMETER)
			{
				exitCode = 1;
				return ProcessProbeState::exited;
			}
			return ProcessProbeState::unknown;
		}

		const DWORD waitResult = WaitForSingleObject(process, 0);
		if (waitResult == WAIT_TIMEOUT)
		{
			CloseHandle(process);
			return ProcessProbeState::running;
		}

		DWORD nativeExitCode = 1;
		if (GetExitCodeProcess(process, &nativeExitCode) && nativeExitCode != STILL_ACTIVE)
		{
			exitCode = nativeExitCode;
		}
		else
		{
			exitCode = 1;
		}
		CloseHandle(process);
		return ProcessProbeState::exited;
	}

	BOOL CALLBACK FindDesktopShellWindow(HWND hwnd, LPARAM lParam)
	{
		if (!IsWindow(hwnd) || GetParent(hwnd) != nullptr)
		{
			return TRUE;
		}

		DWORD processId = 0;
		GetWindowThreadProcessId(hwnd, &processId);
		if (processId == 0 ||
			processImageFileName(processId) != "stok-desktop-shell.exe")
		{
			return TRUE;
		}

		const int titleLength = GetWindowTextLengthW(hwnd);
		if (titleLength <= 0)
		{
			return TRUE;
		}

		RECT rect = {};
		if (!GetWindowRect(hwnd, &rect))
		{
			return TRUE;
		}

		const bool minimized = IsIconic(hwnd) != FALSE;
		const bool hasUsableSize =
			(rect.right - rect.left) >= 300 &&
			(rect.bottom - rect.top) >= 200;
		if (!minimized && !hasUsableSize)
		{
			return TRUE;
		}

		auto* result = reinterpret_cast<HWND*>(lParam);
		*result = hwnd;
		return FALSE;
	}

	bool bringDesktopShellToForeground()
	{
		HWND shellWindow = nullptr;
		EnumWindows(FindDesktopShellWindow, reinterpret_cast<LPARAM>(&shellWindow));
		if (!shellWindow || !IsWindow(shellWindow))
		{
			return false;
		}

		DWORD processId = 0;
		GetWindowThreadProcessId(shellWindow, &processId);
		if (processId != 0)
		{
			AllowSetForegroundWindow(processId);
		}

		ShowWindow(shellWindow, SW_RESTORE);
		ShowWindow(shellWindow, SW_SHOWMAXIMIZED);
		SetForegroundWindow(shellWindow);
		return true;
	}

	std::wstring canonicalWindowsPath(const std::wstring& path)
	{
		std::vector<wchar_t> buffer(MAX_PATH);
		DWORD length = GetFullPathNameW(
			path.c_str(),
			static_cast<DWORD>(buffer.size()),
			buffer.data(),
			nullptr);
		if (length == 0)
		{
			return path;
		}
		if (length >= buffer.size())
		{
			buffer.resize(static_cast<std::size_t>(length) + 1);
			length = GetFullPathNameW(
				path.c_str(),
				static_cast<DWORD>(buffer.size()),
				buffer.data(),
				nullptr);
			if (length == 0)
			{
				return path;
			}
		}
		return std::wstring(buffer.data(), length);
	}

	std::wstring executableDirectory()
	{
		std::vector<wchar_t> buffer(MAX_PATH);
		DWORD length = GetModuleFileNameW(
			nullptr,
			buffer.data(),
			static_cast<DWORD>(buffer.size()));
		if (length >= buffer.size())
		{
			buffer.resize(32768);
			length = GetModuleFileNameW(
				nullptr,
				buffer.data(),
				static_cast<DWORD>(buffer.size()));
		}
		if (length == 0)
		{
			return {};
		}

		std::wstring path(buffer.data(), length);
		const std::wstring::size_type slash = path.find_last_of(L"\\/");
		return slash == std::wstring::npos ? std::wstring{} : path.substr(0, slash);
	}

	std::wstring resolveFromExecutableDirectory(const wchar_t* relativePath)
	{
		const std::wstring base = executableDirectory();
		if (base.empty())
		{
			return {};
		}
		return canonicalWindowsPath(base + L"\\" + relativePath);
	}

	bool regularFileExists(const std::wstring& path)
	{
		const DWORD attributes = GetFileAttributesW(path.c_str());
		return attributes != INVALID_FILE_ATTRIBUTES &&
			(attributes & FILE_ATTRIBUTE_DIRECTORY) == 0;
	}

	std::wstring quoteWindowsArgument(const std::wstring& value)
	{
		std::wstring quoted = L"\"";
		for (wchar_t ch: value)
		{
			if (ch == L'"')
			{
				quoted += L"\\\"";
			}
			else
			{
				quoted += ch;
			}
		}
		quoted += L"\"";
		return quoted;
	}

	bool launchDesktopShellForExistingInstance()
	{
		const std::wstring shellPath = resolveFromExecutableDirectory(
			L"..\\services\\stok-desktop-shell.exe");
		const std::wstring configPath = resolveFromExecutableDirectory(
			L"..\\services\\runtime\\desktop-shell\\config.properties");
		const std::wstring workingDirectory = resolveFromExecutableDirectory(
			L"..\\services\\runtime\\desktop-shell");
		if (shellPath.empty() || !regularFileExists(shellPath))
		{
			return false;
		}

		std::wstring commandLine = quoteWindowsArgument(shellPath);
		if (!configPath.empty())
		{
			commandLine += L" --config=" + quoteWindowsArgument(configPath);
		}
		std::vector<wchar_t> commandBuffer(commandLine.begin(), commandLine.end());
		commandBuffer.push_back(L'\0');

		STARTUPINFOW startupInfo{};
		startupInfo.cb = sizeof(startupInfo);
		startupInfo.dwFlags = STARTF_USESHOWWINDOW;
		startupInfo.wShowWindow = SW_SHOWMAXIMIZED;

		PROCESS_INFORMATION processInformation{};
		const BOOL launched = CreateProcessW(
			shellPath.c_str(),
			commandBuffer.data(),
			nullptr,
			nullptr,
			FALSE,
			0,
			nullptr,
			workingDirectory.empty() ? nullptr : workingDirectory.c_str(),
			&startupInfo,
			&processInformation);
		if (!launched)
		{
			return false;
		}

		AllowSetForegroundWindow(processInformation.dwProcessId);
		CloseHandle(processInformation.hThread);
		CloseHandle(processInformation.hProcess);
		return true;
	}

	const wchar_t* shutdownEventName()
	{
		return L"Local\\StokMacchinaShutdownRequested";
	}

	std::wstring widenString(const std::string& value)
	{
		if (value.empty())
		{
			return {};
		}

		int length = MultiByteToWideChar(
			CP_UTF8,
			0,
			value.c_str(),
			static_cast<int>(value.size()),
			nullptr,
			0);
		if (length <= 0)
		{
			length = MultiByteToWideChar(
				CP_ACP,
				0,
				value.c_str(),
				static_cast<int>(value.size()),
				nullptr,
				0);
		}
		if (length <= 0)
		{
			return {};
		}

		std::wstring wide(static_cast<std::size_t>(length), L'\0');
		int converted = MultiByteToWideChar(
			CP_UTF8,
			0,
			value.c_str(),
			static_cast<int>(value.size()),
			wide.data(),
			length);
		if (converted <= 0)
		{
			converted = MultiByteToWideChar(
				CP_ACP,
				0,
				value.c_str(),
				static_cast<int>(value.size()),
				wide.data(),
				length);
		}
		if (converted <= 0)
		{
			return {};
		}

		return wide;
	}

	std::string buildCommandLine(const std::string& command, const std::vector<std::string>& arguments)
	{
		auto escape = [](const std::string& value)
		{
			return Poco::Process::mustEscapeArg(value)
				? Poco::Process::escapeArg(value)
				: value;
		};

		std::string commandLine = escape(command);
		for (const auto& argument : arguments)
		{
			commandLine.append(" ");
			commandLine.append(escape(argument));
		}
		return commandLine;
	}
}
#endif


using Poco::Util::ServerApplication;
using Poco::Util::Option;
using Poco::Util::OptionSet;
using Poco::Util::HelpFormatter;
using Poco::Util::AbstractConfiguration;
using Poco::Util::OptionCallback;
using Poco::OSP::OSPSubsystem;
using Poco::OSP::ServiceRegistry;


class MacchinaServer: public ServerApplication
{
public:
	MacchinaServer():
		_errorHandler(*this),
		_pOSP(new OSPSubsystem)
	{
		setUnixOptions(true);
		Poco::DataURIStreamFactory::registerFactory();
		Poco::ErrorHandler::set(&_errorHandler);
		addSubsystem(_pOSP);
	}

	~MacchinaServer()
	{
		// wait until all threads have terminated
		// before we completely shut down.
		Poco::ThreadPool::defaultPool().joinAll();
		Poco::DataURIStreamFactory::unregisterFactory();
	}

	ServiceRegistry& serviceRegistry()
	{
		return _pOSP->serviceRegistry();
	}

protected:
    struct ManagedService
    {
        std::string id;
        std::string command;
        std::string workingDirectory;
        std::vector<std::string> arguments;
        bool gracefulStop = false;
        bool terminateServerOnExit = false;
        bool keepAlive = true;
        bool forceShowWindow = false;
        int restartDelayMs = 0;
        Poco::UInt32 processId = 0;
        bool exitObserved = false;
        int exitCode = -1;
        std::unique_ptr<Poco::ProcessHandle> handle;
    };

	void installTelemetryService()
	{
		if (_pTelemetryService) return;

		Poco::OpenTelemetry::TelemetryConfiguration configuration;
		configuration.serviceName = config().getString(
			"telemetry.service.name",
			config().getString("application.baseName", "macchina"));
		configuration.serviceVersion = config().getString(
			"telemetry.service.version",
			config().getString("application.version", "0.1.0"));
		configuration.exportEnabled = config().getBool("telemetry.export.enabled", false);
		configuration.exportTraces = config().getBool("telemetry.export.traces", true);
		configuration.exportLogs = config().getBool("telemetry.export.logs", true);
		configuration.exportMetrics = config().getBool("telemetry.export.metrics", true);
		configuration.otlpEndpoint = config().getString("telemetry.export.otlp.endpoint", "");
		configuration.otlpTracesPath = config().getString("telemetry.export.otlp.tracesPath", "/v1/traces");
		configuration.otlpLogsPath = config().getString("telemetry.export.otlp.logsPath", "/v1/logs");
		configuration.otlpMetricsPath = config().getString("telemetry.export.otlp.metricsPath", "/v1/metrics");
		configuration.otlpHeaders = config().getString("telemetry.export.otlp.headers", "");
		configuration.otlpInsecureSkipVerify = config().getBool("telemetry.export.otlp.insecureSkipVerify", false);
		configuration.otlpConsoleDebug = config().getBool("telemetry.export.otlp.consoleDebug", false);
		configuration.exportTimeoutMs = static_cast<std::size_t>(
			config().getInt("telemetry.export.otlp.timeout", 5000));
		configuration.exportScheduleDelayMs = static_cast<std::size_t>(
			config().getInt("telemetry.export.otlp.scheduleDelay", 1000));
		configuration.metricExportIntervalMs = static_cast<std::size_t>(
			config().getInt("telemetry.export.metrics.interval", 2000));

		const auto addResourceAttribute = [&](const std::string& key, const std::string& value)
		{
			if (!value.empty())
			{
				configuration.resourceAttributes.push_back({key, value});
			}
		};

		const std::string defaultDeviceName = config().getString("webtunnel.deviceName", Poco::Environment::nodeName());
		const std::string defaultDeviceId = config().getString("webtunnel.deviceId", defaultDeviceName);
		addResourceAttribute(
			"device.id",
			config().getString("telemetry.resource.device.id", defaultDeviceId));
		addResourceAttribute(
			"device.name",
			config().getString("telemetry.resource.device.name", defaultDeviceName));
		addResourceAttribute(
			"service.instance.id",
			config().getString("telemetry.resource.instance.id", defaultDeviceId));
		addResourceAttribute(
			"host.name",
			config().getString("telemetry.resource.host.name", Poco::Environment::nodeName()));

		_pTelemetryService = Poco::OpenTelemetry::createTelemetryService(configuration);
		_pTelemetryServiceRef = serviceRegistry().registerService(
			Poco::OpenTelemetry::TelemetryService::SERVICE_NAME,
			_pTelemetryService,
			Poco::OSP::Properties());

		Poco::Logger& rootLogger = Poco::Logger::root();
		_pOriginalRootChannel = rootLogger.getChannel();
		_pOriginalApplicationChannel = logger().getChannel();

		Poco::AutoPtr<Poco::SplitterChannel> pSplitter = new Poco::SplitterChannel;
		if (_pOriginalRootChannel) pSplitter->addChannel(_pOriginalRootChannel);
		if (_pOriginalApplicationChannel && _pOriginalApplicationChannel.get() != _pOriginalRootChannel.get())
		{
			pSplitter->addChannel(_pOriginalApplicationChannel);
		}
		pSplitter->addChannel(new Poco::OpenTelemetry::TelemetryLoggingChannel(_pTelemetryService));

		_pTelemetryRootChannel = pSplitter;
		Poco::Logger::setChannel("", _pTelemetryRootChannel);
		rootLogger.setChannel(_pTelemetryRootChannel);
		logger().setChannel(_pTelemetryRootChannel);

		if (configuration.exportEnabled)
		{
			logger().information("Telemetry OTLP export enabled: %s", configuration.otlpEndpoint);
		}
	}

	void uninstallTelemetryService()
	{
		Poco::Logger& rootLogger = Poco::Logger::root();
		Poco::Logger::setChannel("", _pOriginalRootChannel);
		rootLogger.setChannel(_pOriginalRootChannel);
		logger().setChannel(_pOriginalApplicationChannel ? _pOriginalApplicationChannel : _pOriginalRootChannel);

		_pTelemetryRootChannel = nullptr;
		_pOriginalApplicationChannel = nullptr;
		_pOriginalRootChannel = nullptr;

		if (_pTelemetryServiceRef)
		{
			serviceRegistry().unregisterService(_pTelemetryServiceRef);
			_pTelemetryServiceRef = nullptr;
		}
		_pTelemetryService = nullptr;
	}

	void startConfigSubscription()
	{
		if (_configSubscriber)
		{
			return;
		}

		stok::services::common::DdsSettings settings;
		settings.domainId = static_cast<std::uint32_t>(config().getInt("dds.domainId", 42));
		settings.topicName = config().getString("dds.configTopic", "stok.ui.config");
		settings.segmentSize = static_cast<std::uint32_t>(
			config().getInt("dds.shm.segmentSize", 4 * 1024 * 1024));
		settings.portQueueCapacity = static_cast<std::uint32_t>(
			config().getInt("dds.shm.portQueueCapacity", 256));

		_configSubscriber = std::make_unique<stok::services::common::TextMessageSubscriber>(
			settings,
			Poco::OpenTelemetry::TelemetryClient(_pTelemetryService));

		std::string error;
		const bool started = _configSubscriber->start(
			"macchina-config-subscriber",
			[this](const stok::services::common::TextMessage& message)
			{
				applyConfigUpdate(message.payload);
			},
			stok::services::common::TextMessageSubscriber::MatchCallback(),
			&error);
		if (started)
		{
			logger().information("macchina subscribed to config topic \"%s\".", settings.topicName);
		}
		else
		{
			logger().warning("macchina config subscription failed: %s", error);
			_configSubscriber.reset();
		}
	}

	void stopConfigSubscription()
	{
		if (_configSubscriber)
		{
			_configSubscriber->stop();
			_configSubscriber.reset();
		}
	}

	bool parseConfigBool(const std::string& value) const
	{
		std::string normalized = value;
		std::transform(normalized.begin(), normalized.end(), normalized.begin(), [](unsigned char ch)
		{
			return static_cast<char>(std::tolower(ch));
		});
		return normalized == "1" || normalized == "true" || normalized == "yes" || normalized == "on";
	}

	void applyConfigUpdate(const std::string& payload)
	{
		try
		{
			Poco::JSON::Parser parser;
			const auto parsed = parser.parse(payload);
			const auto object = parsed.extract<Poco::JSON::Object::Ptr>();
			if (!object || object->getValue<std::string>("type") != "config_update")
			{
				return;
			}

			const std::string target = object->optValue<std::string>("target", "");
			if (target != "macchina" && target != "global" && target != "*")
			{
				return;
			}

			const std::string key = object->optValue<std::string>("key", "");
			const std::string value = object->optValue<std::string>("value", "");
			if (key.empty() || value.empty())
			{
				return;
			}

			std::lock_guard<std::mutex> lock(_managedServicesMutex);
			if (key == "stok.services.keepAlive.enabled")
			{
				const bool enabled = parseConfigBool(value);
				for (auto& service : _managedServices)
				{
					if (!service.terminateServerOnExit)
					{
						service.keepAlive = enabled;
					}
				}
				logger().information("Updated global service keepAlive to %s.", enabled ? "true" : "false");
				return;
			}
			if (key == "stok.services.keepAlive.restartDelayMs")
			{
				const int delayMs = std::max(0, std::atoi(value.c_str()));
				for (auto& service : _managedServices)
				{
					if (!service.terminateServerOnExit)
					{
						service.restartDelayMs = delayMs;
					}
				}
				logger().information("Updated global service restart delay to %d ms.", delayMs);
				return;
			}

			const std::string prefix = "stok.services.";
			const std::string keepAliveSuffix = ".keepAlive";
			const std::string restartDelaySuffix = ".restartDelayMs";
			if (key.rfind(prefix, 0) == 0 && key.size() > prefix.size())
			{
				for (auto& service : _managedServices)
				{
					const std::string keepAliveKey = prefix + service.id + keepAliveSuffix;
					const std::string restartDelayKey = prefix + service.id + restartDelaySuffix;
					if (key == keepAliveKey)
					{
						service.keepAlive = !service.terminateServerOnExit && parseConfigBool(value);
						logger().information(
							"Updated service \"%s\" keepAlive to %s.",
							service.id,
							service.keepAlive ? "true" : "false");
						return;
					}
					if (key == restartDelayKey)
					{
						service.restartDelayMs = std::max(0, std::atoi(value.c_str()));
						logger().information(
							"Updated service \"%s\" restart delay to %d ms.",
							service.id,
							service.restartDelayMs);
						return;
					}
				}
			}
		}
		catch (Poco::Exception& exc)
		{
			logger().warning("Ignoring invalid macchina config payload: %s", exc.displayText());
		}
	}

	void emitStartupTelemetry(const std::string& settingsPath)
	{
		if (!_pTelemetryService || _showHelp) return;

		Poco::OpenTelemetry::TelemetryClient telemetry(_pTelemetryService);
		Poco::OpenTelemetry::TelemetryAttributes attributes
		{
			{"settings.path", settingsPath.empty() ? "(default)" : settingsPath},
			{"os.name", Poco::Environment::osDisplayName()},
			{"os.version", Poco::Environment::osVersion()},
			{"os.arch", Poco::Environment::osArchitecture()}
		};

		auto activity = telemetry.beginActivity(
			"application.startup",
			"lifecycle",
			settingsPath,
			attributes);
		activity.step("settings.loaded", settingsPath.empty() ? "default configuration" : settingsPath);
		activity.step("runtime.ready", Poco::Environment::nodeName());

		telemetry.metric(
			"application.threadpool.capacity",
			static_cast<double>(Poco::ThreadPool::defaultPool().capacity()),
			"threads",
			"Configured default thread pool capacity",
			{{"source", "startup"}});
		telemetry.metric(
			"application.cpu.count",
			static_cast<double>(Poco::Environment::processorCount()),
			"count",
			"Detected CPU core count",
			{{"source", "startup"}});
        telemetry.metric(
            "application.services.active",
            static_cast<double>(_managedServices.size()),
            "count",
            "Managed child processes started by the bootstrap server",
            {{"source", "startup"}});

		activity.success("startup complete");
	}

    std::vector<std::string> splitList(const std::string& value, const std::string& delimiters) const
    {
        std::vector<std::string> tokens;
        Poco::StringTokenizer tokenizer(
            value,
            delimiters,
            Poco::StringTokenizer::TOK_TRIM | Poco::StringTokenizer::TOK_IGNORE_EMPTY);

        tokens.reserve(tokenizer.count());
        for (const auto& token : tokenizer)
        {
            tokens.push_back(token);
        }
        return tokens;
    }

    std::string resolveServiceCommandPath(const std::string& servicesDirectory, const std::string& command) const
    {
        if (command.empty())
        {
            return command;
        }

        Poco::Path commandPath(command);
        if (commandPath.isAbsolute())
        {
            return commandPath.toString();
        }

        std::string base = servicesDirectory;
        if (!base.empty() && base.back() != '/' && base.back() != '\\')
        {
#if POCO_OS == POCO_OS_WINDOWS_NT
            base += '\\';
#else
            base += '/';
#endif
        }

        std::string resolved = base + command;
#if POCO_OS == POCO_OS_WINDOWS_NT
        if (Poco::Path(resolved).getExtension().empty())
        {
            const std::string withExe = resolved + ".exe";
            if (Poco::File(withExe).exists())
            {
                return withExe;
            }
        }
#endif
        return resolved;
    }

    void launchConfiguredServices()
    {
        if (_showHelp) return;

        const std::string servicesDirectory = config().getString(
            "stok.services.directory",
            config().getString("application.dir", ""));
        const auto serviceIds = splitList(
            config().getString("stok.services.autostart", ""),
            ",;");
#if defined(_WIN32)
        const bool useManagedServiceJob = ensureManagedServiceJob();
        ensureShutdownEvent();
#endif

        if (servicesDirectory.empty() || serviceIds.empty())
        {
            return;
        }

        Poco::OpenTelemetry::TelemetryClient telemetry(_pTelemetryService);
        auto launcherActivity = telemetry.beginActivity(
            "services.launch",
            "lifecycle",
            servicesDirectory,
            {{"services.dir", servicesDirectory}});

        for (const auto& serviceId : serviceIds)
        {
            const std::string propertyPrefix = "stok.services." + serviceId + ".";
            const std::string configuredCommand = config().getString(propertyPrefix + "command", "");
            const std::string resolvedCommand = resolveServiceCommandPath(servicesDirectory, configuredCommand);
            const std::string workingDirectory = config().getString(propertyPrefix + "workingDir", servicesDirectory);
            const auto arguments = splitList(config().getString(propertyPrefix + "arguments", ""), ";");
            const bool gracefulStop = config().getBool(propertyPrefix + "gracefulStop", false);
            const bool terminateServerOnExit = config().getBool(
                propertyPrefix + "terminateServerOnExit",
                false);
            const bool forceShowWindow = config().getBool(
                propertyPrefix + "forceShowWindow",
                false);
            const bool keepAlive = config().getBool(
                propertyPrefix + "keepAlive",
                config().getBool("stok.services.keepAlive.enabled", true) && !terminateServerOnExit);
            const int restartDelayMs = config().getInt(
                propertyPrefix + "restartDelayMs",
                config().getInt("stok.services.keepAlive.restartDelayMs", 0));

            auto serviceActivity = telemetry.beginActivity(
                "service.launch",
                "lifecycle",
                serviceId,
                {{"service.id", serviceId}, {"command", resolvedCommand}});

            if (configuredCommand.empty())
            {
                logger().warning("Skipping service \"%s\": no command configured.", serviceId);
                serviceActivity.fail("missing command");
                continue;
            }

            if (!Poco::File(resolvedCommand).exists())
            {
                logger().warning("Skipping service \"%s\": command not found at \"%s\".", serviceId, resolvedCommand);
                serviceActivity.fail("command missing");
                continue;
            }

            try
            {
                Poco::ProcessHandle handle =
#if defined(_WIN32)
                    forceShowWindow
                    ? launchManagedServiceWithWindow(resolvedCommand, arguments, workingDirectory)
                    :
#endif
                    Poco::Process::launch(
                        resolvedCommand,
                        arguments,
                        workingDirectory);

                ManagedService service;
                service.id = serviceId;
                service.command = resolvedCommand;
                service.workingDirectory = workingDirectory;
                service.arguments = arguments;
                service.gracefulStop = gracefulStop;
                service.terminateServerOnExit = terminateServerOnExit;
                service.keepAlive = keepAlive;
                service.forceShowWindow = forceShowWindow;
                service.restartDelayMs = std::max(0, restartDelayMs);
                service.processId = handle.id();
                service.handle = std::make_unique<Poco::ProcessHandle>(handle);
#if defined(_WIN32)
                if (useManagedServiceJob)
                {
                    assignManagedServiceToJob(service);
                }
#endif

                logger().information(
                    "Started service \"%s\" (pid=%d, keepAlive=%d, restartDelayMs=%d, primary=%d) from \"%s\".",
                    service.id,
                    static_cast<int>(service.handle->id()),
                    service.keepAlive ? 1 : 0,
                    service.restartDelayMs,
                    service.terminateServerOnExit ? 1 : 0,
                    service.command);

#if defined(_WIN32)
                if (forceShowWindow)
                {
                    ensureManagedServiceWindowVisible(service);
                }
#endif

                telemetry.metric(
                    "application.service.pid",
                    static_cast<double>(service.handle->id()),
                    "count",
                    "Process identifier of a launcher-managed child service",
                    {{"service.id", service.id}});
                serviceActivity.success("pid=" + std::to_string(service.handle->id()));
                {
                    std::lock_guard<std::mutex> lock(_managedServicesMutex);
                    _managedServices.push_back(std::move(service));
                }
            }
            catch (Poco::Exception& exc)
            {
                logger().error(
                    "Failed to start service \"%s\" from \"%s\": %s",
                    serviceId,
                    resolvedCommand,
                    exc.displayText());
                serviceActivity.fail(exc.displayText());
            }
        }

        telemetry.metric(
            "application.services.active",
            static_cast<double>(_managedServices.size()),
            "count",
            "Managed child services currently active after launcher startup",
            {{"phase", "post-launch"}});
        launcherActivity.success("services launched");

        startManagedServiceMonitor();
    }

    int observeServiceExit(ManagedService& service)
    {
        if (!service.handle)
        {
            return -1;
        }

        if (service.exitObserved)
        {
            return service.exitCode == -1 ? 1 : service.exitCode;
        }

#if defined(_WIN32)
        DWORD nativeExitCode = 1;
        const ProcessProbeState state = probeProcessState(
            static_cast<DWORD>(service.processId),
            nativeExitCode);
        if (state == ProcessProbeState::running)
        {
            return -1;
        }
        if (state == ProcessProbeState::exited)
        {
            service.exitCode = static_cast<int>(nativeExitCode);
            service.exitObserved = true;
            return service.exitCode;
        }
#endif

        int exitCode = -1;
        try
        {
            exitCode = Poco::Process::tryWait(*service.handle);
        }
        catch (Poco::Exception& exc)
        {
            logger().warning(
                "Process wait probe failed for service \"%s\" (pid=%d): %s",
                service.id,
                static_cast<int>(service.processId),
                exc.displayText());
            service.exitCode = 1;
            service.exitObserved = true;
            return service.exitCode;
        }
        if (exitCode != -1)
        {
            service.exitObserved = true;
            service.exitCode = exitCode;
            return exitCode;
        }

        if (!Poco::Process::isRunning(*service.handle))
        {
            try
            {
                service.exitCode = Poco::Process::wait(*service.handle);
                if (service.exitCode == -1)
                {
                    service.exitCode = 1;
                }
            }
            catch (Poco::Exception&)
            {
                service.exitCode = 1;
            }
            service.exitObserved = true;
            return service.exitCode;
        }

        return -1;
    }

    bool restartManagedService(ManagedService& service)
    {
        if (_isShuttingDown || _stopServiceMonitor)
        {
            return false;
        }

        if (service.restartDelayMs > 0)
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(service.restartDelayMs));
            if (_isShuttingDown || _stopServiceMonitor)
            {
                return false;
            }
        }

        try
        {
            Poco::ProcessHandle handle =
#if defined(_WIN32)
                service.forceShowWindow
                ? launchManagedServiceWithWindow(service.command, service.arguments, service.workingDirectory)
                :
#endif
                Poco::Process::launch(
                    service.command,
                    service.arguments,
                    service.workingDirectory);

            service.handle = std::make_unique<Poco::ProcessHandle>(handle);
            service.processId = handle.id();
            service.exitObserved = false;
            service.exitCode = -1;

#if defined(_WIN32)
            assignManagedServiceToJob(service);
            if (service.forceShowWindow)
            {
                ensureManagedServiceWindowVisible(service);
            }
#endif

            logger().warning(
                "Restarted keep-alive service \"%s\" (pid=%d).",
                service.id,
                static_cast<int>(service.handle->id()));

            if (_pTelemetryService)
            {
                Poco::OpenTelemetry::TelemetryClient telemetry(_pTelemetryService);
                telemetry.metric(
                    "application.service.restart",
                    1.0,
                    "count",
                    "Managed child service restart count",
                    {{"service.id", service.id}});
            }
            return true;
        }
        catch (Poco::Exception& exc)
        {
            logger().error(
                "Failed to restart keep-alive service \"%s\" from \"%s\": %s",
                service.id,
                service.command,
                exc.displayText());
            service.exitObserved = true;
            return false;
        }
    }

    void startManagedServiceMonitor()
    {
        if (_serviceMonitorThread.joinable())
        {
            return;
        }

        bool hasPrimaryManagedService = false;
        for (const auto& service : _managedServices)
        {
            if (service.terminateServerOnExit && service.handle)
            {
                hasPrimaryManagedService = true;
                break;
            }
        }

        if (!hasPrimaryManagedService)
        {
            logger().warning("Managed service monitor not started: no primary managed service configured.");
            return;
        }

        logger().information("Managed service monitor started.");
        _stopServiceMonitor = false;
        _serviceMonitorThread = std::thread([this]()
        {
            while (!_stopServiceMonitor)
            {
                try
                {
#if defined(_WIN32)
                    if (_shutdownEvent)
                    {
                        const DWORD waitResult = WaitForSingleObject(_shutdownEvent, 200);
                        if (waitResult == WAIT_OBJECT_0)
                        {
                            bool primaryServiceExited = false;
                            {
                                std::lock_guard<std::mutex> lock(_managedServicesMutex);
                                for (auto& service : _managedServices)
                                {
                                    if (service.terminateServerOnExit && service.handle)
                                    {
                                        primaryServiceExited = observeServiceExit(service) != -1;
                                        break;
                                    }
                                }
                            }

                            if (primaryServiceExited)
                            {
                                logger().information("Managed shutdown requested by desktop shell.");
                                if (!_isShuttingDown)
                                {
                                    Poco::Util::ServerApplication::terminate();
                                }
                                return;
                            }

                            ResetEvent(_shutdownEvent);
                            logger().warning(
                                "Ignored managed shutdown event because the primary desktop shell is still running.");
                        }
                    }
#endif
                    {
                        std::lock_guard<std::mutex> lock(_managedServicesMutex);
                        for (auto& service : _managedServices)
                        {
                            if (_stopServiceMonitor)
                            {
                                return;
                            }

                            if (!service.handle)
                            {
                                continue;
                            }

                            const int exitCode = observeServiceExit(service);
                            if (exitCode == -1)
                            {
                                continue;
                            }

                            if (service.terminateServerOnExit)
                            {
                                logger().information(
                                    "Primary managed service \"%s\" exited with code %d. Shutting down macchina.",
                                    service.id,
                                    exitCode);

                                if (!_isShuttingDown)
                                {
                                    Poco::Util::ServerApplication::terminate();
                                }
                                return;
                            }

                            if (service.keepAlive)
                            {
                                logger().warning(
                                    "Keep-alive service \"%s\" exited with code %d. Restarting.",
                                    service.id,
                                    exitCode);
                                restartManagedService(service);
                            }
                        }
                    }
                }
                catch (Poco::Exception& exc)
                {
                    logger().error("Managed service monitor error: %s", exc.displayText());
                }
                catch (std::exception& exc)
                {
                    logger().error("Managed service monitor error: %s", std::string(exc.what()));
                }
                catch (...)
                {
                    logger().error("Managed service monitor error: unknown exception.");
                }

                std::this_thread::sleep_for(std::chrono::milliseconds(100));
            }
        });
    }

    void stopManagedServiceMonitor()
    {
        _stopServiceMonitor = true;
        if (_serviceMonitorThread.joinable())
        {
            _serviceMonitorThread.join();
        }
    }

    void stopManagedServices()
    {
        if (_managedServices.empty())
        {
            return;
        }

        Poco::OpenTelemetry::TelemetryClient telemetry(_pTelemetryService);
        auto activity = telemetry.beginActivity(
            "services.stop",
            "lifecycle",
            std::to_string(_managedServices.size()),
            {{"service.count", std::to_string(_managedServices.size())}});

        for (auto it = _managedServices.rbegin(); it != _managedServices.rend(); ++it)
        {
            if (!it->handle) continue;

            try
            {
                int exitCode = observeServiceExit(*it);

                if (exitCode == -1 && Poco::Process::isRunning(*it->handle))
                {
                    if (it->gracefulStop)
                    {
                        logger().information(
                            "Requesting graceful shutdown for service \"%s\" (pid=%d).",
                            it->id,
                            static_cast<int>(it->handle->id()));
                        Poco::Process::requestTermination(it->handle->id());

                        for (int retry = 0; retry < 30 && Poco::Process::isRunning(*it->handle); ++retry)
                        {
                            std::this_thread::sleep_for(std::chrono::milliseconds(100));
                        }
                    }

                    exitCode = observeServiceExit(*it);
                    if (exitCode == -1 && Poco::Process::isRunning(*it->handle))
                    {
                        logger().information(
                            "Force-stopping service \"%s\" (pid=%d).",
                            it->id,
                            static_cast<int>(it->handle->id()));
                        Poco::Process::kill(*it->handle);
                    }
                }

                if (exitCode == -1)
                {
                    exitCode = Poco::Process::wait(*it->handle);
                    it->exitObserved = true;
                    it->exitCode = exitCode;
                }

                telemetry.metric(
                    "application.service.exit_code",
                    static_cast<double>(exitCode),
                    "count",
                    "Exit code observed while shutting down a managed child service",
                    {{"service.id", it->id}});
            }
            catch (Poco::Exception& exc)
            {
                logger().warning(
                    "Error while stopping service \"%s\": %s",
                    it->id,
                    exc.displayText());
            }
        }

        _managedServices.clear();
        activity.success("services stopped");
    }

#if defined(_WIN32)
    Poco::ProcessHandle launchManagedServiceWithWindow(
        const std::string& command,
        const std::vector<std::string>& arguments,
        const std::string& workingDirectory)
    {
        STARTUPINFOW startupInfo{};
        startupInfo.cb = sizeof(startupInfo);
        startupInfo.dwFlags = STARTF_USESHOWWINDOW;
        startupInfo.wShowWindow = SW_SHOWNORMAL;

        PROCESS_INFORMATION processInformation{};
        std::wstring applicationPath = widenString(command);
        std::string commandLine = buildCommandLine(command, arguments);
        std::wstring commandLineWide = widenString(commandLine);
        std::vector<wchar_t> commandBuffer(commandLineWide.begin(), commandLineWide.end());
        commandBuffer.push_back(L'\0');
        std::wstring workingDirectoryWide = widenString(workingDirectory);

        if (!CreateProcessW(
                applicationPath.c_str(),
                commandBuffer.data(),
                nullptr,
                nullptr,
                FALSE,
                0,
                nullptr,
                workingDirectoryWide.empty() ? nullptr : workingDirectoryWide.c_str(),
                &startupInfo,
                &processInformation))
        {
            throw Poco::SystemException(
                Poco::format(
                    "CreateProcessW failed for \"%s\"",
                    command));
        }

        CloseHandle(processInformation.hThread);
        return NativeProcessHandle(processInformation.hProcess, processInformation.dwProcessId);
    }

    void ensureManagedServiceWindowVisible(const ManagedService& service)
    {
        if (!service.handle)
        {
            return;
        }

        const DWORD processId = static_cast<DWORD>(service.handle->id());
        std::thread([processId]()
        {
            for (int attempt = 0; attempt < 100; ++attempt)
            {
                WindowSearchContext context;
                context.processId = processId;
                EnumWindows(FindTopLevelWindowForProcess, reinterpret_cast<LPARAM>(&context));
                if (context.window)
                {
                    AllowSetForegroundWindow(processId);
                    ShowWindow(context.window, SW_SHOWMAXIMIZED);
                    SetWindowPos(
                        context.window,
                        HWND_TOP,
                        0,
                        0,
                        0,
                        0,
                        SWP_NOMOVE | SWP_NOSIZE | SWP_SHOWWINDOW);
                    SetForegroundWindow(context.window);
                    return;
                }

                std::this_thread::sleep_for(std::chrono::milliseconds(100));
            }
        }).detach();
    }

    bool ensureManagedServiceJob()
    {
        if (_managedServicesJob)
        {
            return true;
        }

        HANDLE job = CreateJobObjectW(nullptr, nullptr);
        if (!job)
        {
            logger().warning(
                "Failed to create managed-service job object: error %lu.",
                static_cast<unsigned long>(GetLastError()));
            return false;
        }

        JOBOBJECT_EXTENDED_LIMIT_INFORMATION limitInfo{};
        limitInfo.BasicLimitInformation.LimitFlags = JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;
        if (!SetInformationJobObject(
                job,
                JobObjectExtendedLimitInformation,
                &limitInfo,
                sizeof(limitInfo)))
        {
            const DWORD errorCode = GetLastError();
            CloseHandle(job);
            logger().warning(
                "Failed to configure managed-service job object: error %lu.",
                static_cast<unsigned long>(errorCode));
            return false;
        }

        _managedServicesJob = job;
        return true;
    }

    bool ensureShutdownEvent()
    {
        if (_shutdownEvent)
        {
            ResetEvent(_shutdownEvent);
            return true;
        }

        HANDLE eventHandle = CreateEventW(nullptr, TRUE, FALSE, shutdownEventName());
        if (!eventHandle)
        {
            logger().warning(
                "Failed to create managed shutdown event: error %lu.",
                static_cast<unsigned long>(GetLastError()));
            return false;
        }

        ResetEvent(eventHandle);
        _shutdownEvent = eventHandle;
        return true;
    }

    void assignManagedServiceToJob(const ManagedService& service)
    {
        if (!_managedServicesJob || !service.handle)
        {
            return;
        }

        HANDLE processHandle = OpenProcess(
            PROCESS_SET_QUOTA | PROCESS_TERMINATE,
            FALSE,
            static_cast<DWORD>(service.handle->id()));
        if (!processHandle)
        {
            logger().warning(
                "Failed to open managed service \"%s\" (pid=%d) for job assignment: error %lu.",
                service.id,
                static_cast<int>(service.handle->id()),
                static_cast<unsigned long>(GetLastError()));
            return;
        }

        if (!AssignProcessToJobObject(_managedServicesJob, processHandle))
        {
            const DWORD errorCode = GetLastError();
            if (errorCode == ERROR_ACCESS_DENIED)
            {
                logger().debug(
                    "Managed service \"%s\" (pid=%d) is already attached to another job object.",
                    service.id,
                    static_cast<int>(service.handle->id()));
            }
            else
            {
                logger().warning(
                    "Failed to assign managed service \"%s\" (pid=%d) to the shutdown job: error %lu.",
                    service.id,
                    static_cast<int>(service.handle->id()),
                    static_cast<unsigned long>(errorCode));
            }
        }

        CloseHandle(processHandle);
    }

    void closeManagedServiceJob()
    {
        if (_managedServicesJob)
        {
            CloseHandle(_managedServicesJob);
            _managedServicesJob = nullptr;
        }
    }

    void closeShutdownEvent()
    {
        if (_shutdownEvent)
        {
            CloseHandle(_shutdownEvent);
            _shutdownEvent = nullptr;
        }
    }
#endif

	void configureOpenSSLRuntime()
	{
		std::string applicationDir = config().getString("application.dir", "");
		if (applicationDir.empty()) return;

		Poco::Path applicationPath(applicationDir);
		Poco::Path opensslConfig(applicationPath);
		opensslConfig.pushDirectory("cnf");
		opensslConfig.setFileName("openssl.cnf");
		if (!Poco::Environment::has("OPENSSL_CONF") && Poco::File(opensslConfig).exists())
		{
			Poco::Environment::set("OPENSSL_CONF", opensslConfig.toString());
		}

		Poco::Path localModules(applicationPath);
#if POCO_OS == POCO_OS_WINDOWS_NT
		localModules.setFileName("");
#else
		localModules.pushDirectory("ossl-modules");
#endif
		if (!Poco::Environment::has("OPENSSL_MODULES") && Poco::File(localModules).exists())
		{
			Poco::Environment::set("OPENSSL_MODULES", localModules.toString());
		}
	}

	void detachConsoleWindow()
	{
#if defined(_WIN32)
		if (_showHelp) return;

		const bool hideConsoleWindow = config().getBool("macchina.hideConsoleWindow", true);
		if (!hideConsoleWindow)
		{
			return;
		}

		HWND consoleWindow = GetConsoleWindow();
		if (!consoleWindow)
		{
			return;
		}

		DWORD attachedProcessIds[2] = {};
		const DWORD attachedProcessCount = GetConsoleProcessList(
			attachedProcessIds,
			static_cast<DWORD>(sizeof(attachedProcessIds) / sizeof(attachedProcessIds[0])));
		if (attachedProcessCount <= 1)
		{
			ShowWindow(consoleWindow, SW_HIDE);
		}

		FreeConsole();
#endif
	}

	class ErrorHandler: public Poco::ErrorHandler
	{
	public:
		ErrorHandler(MacchinaServer& app):
			_app(app)
		{
		}

		void exception(const Poco::Exception& exc)
		{
			// Don't log Poco::Net::ConnectionResetException and Poco::TimeoutException -
			// getting too many of them from the web server.
			if (std::strcmp(exc.name(), "Connection reset by peer") != 0 &&
			    std::strcmp(exc.name(), "Timeout") != 0)
			{
				log(exc.displayText());
			}
		}

		void exception(const std::exception& exc)
		{
			log(exc.what());
		}

		void exception()
		{
			log("unknown exception");
		}

		void log(const std::string& message)
		{
			_app.logger().notice("A thread was terminated by an unhandled exception: " + message);
		}

	private:
		MacchinaServer& _app;
	};

	std::string loadSettings()
	{
		std::string settingsPath = config().getString("macchina.settings.path", "");
		if (!settingsPath.empty())
		{
			Poco::AutoPtr<Poco::Util::PropertyFileConfiguration> pSettings;
			Poco::File settingsFile(settingsPath);
			if (settingsFile.exists())
			{
				pSettings = new Poco::Util::PropertyFileConfiguration(settingsPath);
			}
			else
			{
				pSettings = new Poco::Util::PropertyFileConfiguration;
			}
			config().add(pSettings, "macchina.settings", Poco::Util::Application::PRIO_DEFAULT, true);
		}
		return settingsPath;
	}

	void initialize(Application& self)
	{
		if (_showHelp)
		{
			return;
		}

		if (!_skipDefaultConfig)
		{
			loadConfiguration();
		}
		for (const auto& cf: _configs)
		{
			loadConfiguration(cf);
		}

		int defaultThreadPoolCapacity = config().getInt("poco.threadPool.default.capacity", 32);
		int defaultThreadPoolCapacityDelta = defaultThreadPoolCapacity - Poco::ThreadPool::defaultPool().capacity();
		if (defaultThreadPoolCapacityDelta > 0)
		{
			Poco::ThreadPool::defaultPool().addCapacity(defaultThreadPoolCapacityDelta);
		}

		std::string settingsPath = loadSettings();
		detachConsoleWindow();
		configureOpenSSLRuntime();

		ServerApplication::initialize(self);
		installTelemetryService();
		startConfigSubscription();

		if (!settingsPath.empty() && !_showHelp)
		{
			logger().information("Settings loaded from \"%s\".", settingsPath);
		}

		if (!_showHelp)
		{
			logger().information(
				"\n"
				"\n"
				"      oooooooooooooooooo\n"
				"    oooooooooooooooooooooo\n"
				"    oooooooooooooooooooooo\n"
				"    oooooooooooooooooooooo\n"
				"    oooooooooooooooooooooo\n"
				"    ooooooooo            o\n"
				"    ooooooooo   oo   oo  \n"
				"    ooooooooo   oo   oo \n"
				"    ooooooooo   oo   oo \n"
				"    ooooooooo   oo   oo \n"
				"      ooooooo   oo   oo \n"
				"\n"
				"    macchina.io EDGE Server\n"
				"\n"
				"    Copyright (c) 2015-2022 by Applied Informatics Software Engineering GmbH.\n"
				"    All rights reserved.\n"
			);
			logger().information("System information: %s (%s) on %s, %u CPU core(s).",
				Poco::Environment::osDisplayName(),
				Poco::Environment::osVersion(),
				Poco::Environment::osArchitecture(),
				Poco::Environment::processorCount());
		}

        launchConfiguredServices();
		emitStartupTelemetry(settingsPath);
	}

	void uninitialize()
	{
        _isShuttingDown = true;
        stopConfigSubscription();
        stopManagedServiceMonitor();
        stopManagedServices();
#if defined(_WIN32)
        closeManagedServiceJob();
        closeShutdownEvent();
#endif
		uninstallTelemetryService();
		ServerApplication::uninitialize();
	}

	void defineOptions(OptionSet& options)
	{
		ServerApplication::defineOptions(options);

		options.addOption(
			Option("help", "h", "Display help information on command line arguments.")
				.required(false)
				.repeatable(false)
				.callback(OptionCallback<MacchinaServer>(this, &MacchinaServer::handleHelp)));

		options.addOption(
			Option("config-file", "c", "Load configuration data from a file.")
				.required(false)
				.repeatable(true)
				.argument("file")
				.callback(OptionCallback<MacchinaServer>(this, &MacchinaServer::handleConfig)));

		options.addOption(
			Option("skip-default-config", "", "Don't load default configuration file.")
				.required(false)
				.repeatable(false)
				.callback(OptionCallback<MacchinaServer>(this, &MacchinaServer::handleSkipDefaultConfig)));
	}

	void handleHelp(const std::string& name, const std::string& value)
	{
		_showHelp = true;
		displayHelp();
		stopOptionsProcessing();
		_pOSP->cancelInit();
	}

	void handleConfig(const std::string& name, const std::string& value)
	{
		_configs.push_back(value);
	}

	void handleSkipDefaultConfig(const std::string& name, const std::string& value)
	{
		_skipDefaultConfig = true;
	}

	void displayHelp()
	{
		HelpFormatter helpFormatter(options());
		helpFormatter.setCommand(commandName());
		helpFormatter.setUsage("OPTIONS");
		helpFormatter.setHeader(
			"\n"
			"The macchina.io EDGE Server.\n"
			"Copyright (c) 2015-2020 by Applied Informatics Software Engineering GmbH.\n"
			"All rights reserved.\n\n"
			"The following command line options are supported:"
		);
		helpFormatter.setFooter(
			"For more information, please see the macchina.io "
			"documentation at <https://macchina.io/docs>."
		);
		helpFormatter.setIndent(8);
		helpFormatter.format(std::cout);
	}

	int main(const std::vector<std::string>& args)
	{
		if (!_showHelp)
		{
			waitForTerminationRequest();
		}
		return Application::EXIT_OK;
	}

private:
	ErrorHandler _errorHandler;
	OSPSubsystem* _pOSP;
	Poco::OSP::ServiceRef::Ptr _pTelemetryServiceRef;
	Poco::OpenTelemetry::TelemetryService::Ptr _pTelemetryService;
	std::unique_ptr<stok::services::common::TextMessageSubscriber> _configSubscriber;
	Poco::AutoPtr<Poco::Channel> _pOriginalRootChannel;
	Poco::AutoPtr<Poco::Channel> _pOriginalApplicationChannel;
	Poco::AutoPtr<Poco::Channel> _pTelemetryRootChannel;
    std::vector<ManagedService> _managedServices;
    std::mutex _managedServicesMutex;
    std::thread _serviceMonitorThread;
    std::atomic_bool _stopServiceMonitor{false};
    std::atomic_bool _isShuttingDown{false};
#if defined(_WIN32)
    HANDLE _managedServicesJob = nullptr;
    HANDLE _shutdownEvent = nullptr;
#endif
	bool _showHelp = false;
	bool _skipDefaultConfig = false;
	std::vector<std::string> _configs;
};

int main(int argc, char** argv)
{
	try
	{
#if defined(_WIN32)
		HANDLE singleInstanceMutex = CreateMutexW(nullptr, TRUE, L"Local\\StokMacchinaSingleInstance");
		const bool alreadyRunning = singleInstanceMutex && GetLastError() == ERROR_ALREADY_EXISTS;
		if (alreadyRunning)
		{
			if (!bringDesktopShellToForeground())
			{
				launchDesktopShellForExistingInstance();
			}
			CloseHandle(singleInstanceMutex);
			return Poco::Util::Application::EXIT_OK;
		}
#endif
		int rc = Poco::Util::Application::EXIT_SOFTWARE;
		{
			MacchinaServer app;
			rc = app.run(argc, argv);
		}
#if defined(_WIN32)
		if (singleInstanceMutex)
		{
			ReleaseMutex(singleInstanceMutex);
			CloseHandle(singleInstanceMutex);
		}
#endif
		std::_Exit(rc);
	}
	catch (Poco::Exception& exc)
	{
		std::cerr << exc.displayText() << std::endl;
		return Poco::Util::Application::EXIT_SOFTWARE;
	}
}
