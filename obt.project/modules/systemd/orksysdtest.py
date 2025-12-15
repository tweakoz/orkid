class serviceinfo:
    """
    Systemd service module for Orkid System Test (spotlight rigid model).
    """
    def __init__(self):
        self._name = "orksysdtest"
        self._command = "${ORKID_WORKSPACE_DIR}/ork.lev2/pyext/tests/renderer/lighting/spotlight_rigid_model.py"
        self._description = "Orkid System Test Service (Spotlight Rigid Model)"
        self._requires = []  # Hard dependencies (fails if dependency fails)
        self._prefers = ["multi-user.target"]   # Soft dependencies (doesn't fail if missing)
        self._after = ["multi-user.target"]     # Start after these (ordering only)
        self._before = []    # Start before these (ordering only)
        self._requires_deps = []     # OBT dep modules to realize (e.g., ["lua", "vulkan"])
        self._requires_dockers = []  # Docker modules to build (e.g., ["postgres_dev"])
        self._requires_pips = []     # Python packages to pip install (e.g., ["flask", "redis"])
        self._requires_tty = 7       # TTY number for graphical daemon
        self._as_system_service = True  # Run as system service (like GDM/LightDM)

    def info(self):
        """
        Return service metadata dictionary.
        """
        return {
            "name": self._name,
            "command": self._command,
            "description": self._description,
            "requires": self._requires,
            "prefers": self._prefers,
            "after": self._after,
            "before": self._before,
            "requires_deps": self._requires_deps,
            "requires_dockers": self._requires_dockers,
            "requires_pips": self._requires_pips,
            "requires_tty": self._requires_tty,
            "as_system_service": self._as_system_service,
        }
