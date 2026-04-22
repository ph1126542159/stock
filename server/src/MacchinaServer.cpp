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
#include "Poco/Path.h"
#include "Poco/Process.h"
#include "Poco/SplitterChannel.h"
#include "Poco/StringTokenizer.h"
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <memory>
#include <thread>


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
                Poco::ProcessHandle handle = Poco::Process::launch(
                    resolvedCommand,
                    arguments,
                    workingDirectory);

                ManagedService service;
                service.id = serviceId;
                service.command = resolvedCommand;
                service.workingDirectory = workingDirectory;
                service.arguments = arguments;
                service.gracefulStop = gracefulStop;
                service.handle = std::make_unique<Poco::ProcessHandle>(handle);

                logger().information(
                    "Started service \"%s\" (pid=%d) from \"%s\".",
                    service.id,
                    static_cast<int>(service.handle->id()),
                    service.command);

                telemetry.metric(
                    "application.service.pid",
                    static_cast<double>(service.handle->id()),
                    "count",
                    "Process identifier of a launcher-managed child service",
                    {{"service.id", service.id}});
                serviceActivity.success("pid=" + std::to_string(service.handle->id()));
                _managedServices.push_back(std::move(service));
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
                if (Poco::Process::isRunning(*it->handle))
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

                    if (Poco::Process::isRunning(*it->handle))
                    {
                        logger().information(
                            "Force-stopping service \"%s\" (pid=%d).",
                            it->id,
                            static_cast<int>(it->handle->id()));
                        Poco::Process::kill(*it->handle);
                    }
                }

                telemetry.metric(
                    "application.service.exit_code",
                    static_cast<double>(Poco::Process::wait(*it->handle)),
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
		configureOpenSSLRuntime();

		ServerApplication::initialize(self);
		installTelemetryService();

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
        stopManagedServices();
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
	Poco::AutoPtr<Poco::Channel> _pOriginalRootChannel;
	Poco::AutoPtr<Poco::Channel> _pOriginalApplicationChannel;
	Poco::AutoPtr<Poco::Channel> _pTelemetryRootChannel;
    std::vector<ManagedService> _managedServices;
	bool _showHelp = false;
	bool _skipDefaultConfig = false;
	std::vector<std::string> _configs;
};

int main(int argc, char** argv)
{
	try
	{
		int rc = Poco::Util::Application::EXIT_SOFTWARE;
		{
			MacchinaServer app;
			rc = app.run(argc, argv);
		}
		std::_Exit(rc);
	}
	catch (Poco::Exception& exc)
	{
		std::cerr << exc.displayText() << std::endl;
		return Poco::Util::Application::EXIT_SOFTWARE;
	}
}
