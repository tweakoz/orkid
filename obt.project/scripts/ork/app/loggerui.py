#!/usr/bin/env ork.python
################################################################################
# Orkid Media Engine
# Copyright 1996-2023, Michael T. Mayers.
# Distributed under the MIT License.
# see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
################################################################################

"""
LoggerUIComponent - Generic logger UI backend with overlay widget

Provides logger UI backend integration for ComponentizedApplications.
Allows apps to configure custom channels during initialization.

Usage:
    from ork.app import application, loggerui

    class MyApp(application.ComponentizedApplication):
        def __init__(self):
            super().__init__()

            # Register logger component
            self.logger_component = loggerui.LoggerUIComponent(
                overlay=True,
                filter_regex=[".*"],
                background_color=vec4(1.0, 0.0, 0.0, 0.25)
            )
            self.addComponent("logger", loggerui.LoggerUIComponent)

            self.ezapp = lev2.OrkEzApp.create(self, ...)

        def onAppInit(self, initdata):
            # Configure app-specific channels
            self.my_channel = self.logger_component.configureChannel(
                "MYCHANNEL",
                vec3(0.3, 1.0, 0.8),
                enable_channel=True
            )
"""

from orkengine.core import vec3, vec4, logger
from orkengine import lev2
from ork.app.application import ApplicationComponent

################################################################################

class LoggerUIComponent(ApplicationComponent):
    """
    Generic logger UI component with customization hooks.

    Lifecycle:
        1. __init__() - Configure overlay, filters, colors
        2. onEzAppCreated() - Create backend, widget, set overlay (before enableUiDraw)
        3. onAppInit() - Backend and widget already ready
        4. onAppLink() - Ready for channel configuration

    Customization:
        Apps should configure channels in their _onAppLink() using:
            logger_component.configureChannel(name, color, enable_channel)
    """

    def __init__(self,
                 overlay=True,
                 filter_regex=None,
                 background_color=None):
        """
        Args:
            overlay: If True, logger floats over UI. If False, app must embed manually.
            filter_regex: List of regex patterns for log filtering (default: [".*"])
            background_color: Widget background color (default: semi-transparent gray)
        """
        super().__init__()
        self.overlay = overlay

        # Normalize filter_regex to list
        if filter_regex is None:
            self.filter_regex = [".*"]
        elif isinstance(filter_regex, str):
            self.filter_regex = [filter_regex]
        else:
            self.filter_regex = filter_regex

        self.background_color = background_color or vec4(0.2, 0.2, 0.2, 0.8)

        # State (initialized in lifecycle)
        self.logger_backend = None
        self.logger_group = None
        self._logger = None

    ##############################################
    # Lifecycle hooks (called by ComponentizedApplication)
    ##############################################

    def _onEzAppCreated(self, app, ezapp):
        """Early initialization - create backend, widget, and set overlay before enableUiDraw()"""
        # Create backend and set on global logger
        if not self.logger_backend:
            self._logger = logger()
            self.logger_backend = lev2.ui.LoggerUIBackend.create()
            self._logger.setBackend(self.logger_backend)
            self.configureChannel("EZAPP", vec3(0.5, 0.5, 1.0), status_interval=0.1)

        # Create widget and register with backend
        if not self.logger_group:
            self.logger_group = lev2.ui.LoggerGroup.create("logger_ui", self.filter_regex)
            self.logger_group.registerOnBackend(self.logger_backend)
            self.logger_group.background_color = self.background_color

            # Set overlay BEFORE enableUiDraw() is called
            if self.overlay:
                lg_group = ezapp.topLayoutGroup
                lg_group.overlay_widget = self.logger_group


    def _onAppInit(self, app, initdata):
        """Called during app init - backend and widget already created in _onEzAppCreated"""
        pass

    def _onAppLink(self, app, initdata):
        """Called after all components initialized - ready for channel configuration"""
        # Configure EZAPP channel with status interval
        pass

    ##############################################
    # Public API for apps
    ##############################################

    def configureChannel(self, name, color, enable_channel=False, status_interval=None):
        """
        Configure a logger channel.

        Should be called in app's onAppInit() after logger backend is created.

        Args:
            name: Channel name (e.g., "GVIEW", "PHYSICS")
            color: Channel color as vec3 (R, G, B in 0-1 range)
            enable_channel: If True, perfItems go to GraphView
            status_interval: Optional status update interval in seconds

        Returns:
            Configured channel object
        """
        channel = self._logger.configureChannel(name, color, enable_channel)
        if status_interval is not None:
            channel.status_interval = status_interval
        return channel

    def getChannel(self, name):
        """
        Get channel by name.

        Args:
            name: Channel name

        Returns:
            Channel object or None if not found
        """
        return self._logger.channel(name)

    def getWidget(self):
        """
        Get logger widget for manual embedding (when overlay=False).

        Returns:
            LoggerGroup widget instance
        """
        return self.logger_group

    def getBackend(self):
        """
        Get logger backend.

        Returns:
            LoggerUIBackend instance
        """
        return self.logger_backend

################################################################################
