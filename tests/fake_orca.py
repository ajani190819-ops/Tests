"""A stand-in for OrcaSlicer's `orca` module, so plugins can be run here.

    import fake_orca; fake_orca.install()
    import unlayered_infill_orca as plugin

This is NOT a reimplementation of Orca. It is the smallest surface that lets
a plugin file be imported and its capabilities invoked, so the plugin's own
logic — config handling, error paths, logging, the G-code transform — can be
exercised without OrcaSlicer. Anything it reports is evidence about *our*
code, never about Orca's.

The shapes here come from `docs/ORCA-PLUGIN-FACTS.md` and the plugin-
development wiki: capability base classes, an `ExecutionResult` with
`success`/`failure`, a `PluginResult` error enum, a `Step` enum for the
slicing pipeline, and an `@orca.plugin` class whose `register_capabilities`
calls `orca.register_capability`.
"""
import enum
import os
import sys
import types


class PluginResult(enum.Enum):
    Success = "Success"
    RecoverableError = "RecoverableError"
    FatalError = "FatalError"


class ExecutionResult:
    """What a capability hands back. Records enough to assert against."""

    __slots__ = ("ok", "kind", "message")

    def __init__(self, ok, kind=None, message=""):
        self.ok = ok
        self.kind = kind
        self.message = message or ""

    @staticmethod
    def success(message=""):
        return ExecutionResult(True, PluginResult.Success, message)

    @staticmethod
    def failure(kind, message=""):
        return ExecutionResult(False, kind, message)

    def __repr__(self):
        head = "OK" if self.ok else f"FAIL/{getattr(self.kind, 'value', self.kind)}"
        return f"<{head}: {self.message[:70]!r}>"


class Step(enum.Enum):
    """The slicing-pipeline steps a capability can be asked to run at.

    Only `psGCodePostProcess` matters to the infill plugin; the rest exist so
    the "ignore steps that are not mine" path can be exercised.
    """
    posSlice = "posSlice"
    posPerimeters = "posPerimeters"
    posPrepareInfill = "posPrepareInfill"
    posInfill = "posInfill"
    posSupportMaterial = "posSupportMaterial"
    psGCodePostProcess = "psGCodePostProcess"


class _CapabilityBase:
    """Capabilities are constructed by the host with no Python args.

    Real Orca's bound base constructors accept no arguments. Configuration is
    available later through get_config(), so tests use set_config() to mimic
    that host injection. Keeping this constructor strict catches plugins that
    call super().__init__(None), which fails in Orca at registration time.
    """

    def __init__(self):
        self._config = None
        self._config_version = ""
        self.saved_configs = []     # every save_config() call, for assertions
        self.save_ok = True         # flip to False to simulate Orca refusing

    def get_config(self):
        """Orca hands back the capability's config as a JSON string."""
        import json
        if self._config is None:
            return None
        if isinstance(self._config, str):
            return self._config
        return json.dumps(self._config)

    def set_config(self, cfg, version=""):
        self._config = cfg
        self._config_version = version

    def get_config_version(self):
        """The plugin version that last saved this config.

        Wiki, Plugin Development / Capability configuration: "Return the
        plugin version that last saved the configuration. Use this to detect
        and migrate an older schema."
        """
        return self._config_version

    def save_config(self, config):
        """Persist a JSON string for this capability; returns a bool.

        Real Orca supplies the plugin identity and current version itself and
        will not let a capability write another one's config. Here we just
        record it so a test can see exactly what the plugin wrote.
        """
        import json
        if not isinstance(config, str):
            raise TypeError(
                "save_config() takes a JSON STRING -- use json.dumps(cfg). "
                "Real Orca's binding is save_config(config: str) -> bool.")
        json.loads(config)          # a plugin must never hand Orca junk
        self.saved_configs.append(config)
        if not self.save_ok:
            return False
        self._config = config
        return True


class SlicingPipelineCapabilityBase(_CapabilityBase):
    pass


class ScriptPluginCapabilityBase(_CapabilityBase):
    pass


class SurfaceType(enum.Enum):
    stTop = "stTop"
    stBottom = "stBottom"
    stBottomBridge = "stBottomBridge"
    stInternal = "stInternal"
    stInternalBridge = "stInternalBridge"


class Context:
    """The `ctx` passed to a slicing-pipeline capability.

    At `psGCodePostProcess`, `print` and `object` are None and `gcode_path`
    points at the working file — see docs/ORCA-PLUGIN-FACTS.md.
    """

    def __init__(self, step, gcode_path="", output_name="out.gcode", host=None):
        self.step = step
        self.gcode_path = gcode_path
        self.output_name = output_name
        self.host = host
        self.print = None
        self.object = None

    def config_value(self, _key):
        # Real builds returned None here; the plugins must not depend on it.
        return None


REGISTERED = []


def register_capability(cls):
    REGISTERED.append(cls)
    return cls


class base:
    """Base for the @orca.plugin class."""
    pass


PLUGINS = []


def plugin(cls):
    PLUGINS.append(cls)
    return cls


def install():
    """Put a fake `orca` package into sys.modules. Returns the module."""
    for name in [n for n in sys.modules if n == "orca" or n.startswith("orca.")]:
        del sys.modules[name]
    REGISTERED.clear()
    PLUGINS.clear()

    orca = types.ModuleType("orca")
    orca.ExecutionResult = ExecutionResult
    orca.PluginResult = PluginResult
    orca.register_capability = register_capability
    orca.base = base
    orca.plugin = plugin
    orca.REGISTERED = REGISTERED
    orca.PLUGINS = PLUGINS

    slicing = types.ModuleType("orca.slicing")
    slicing.SlicingPipelineCapabilityBase = SlicingPipelineCapabilityBase
    slicing.Step = Step
    slicing.unscale = lambda v: v / 1e6
    orca.slicing = slicing

    script = types.ModuleType("orca.script")
    script.ScriptPluginCapabilityBase = ScriptPluginCapabilityBase
    orca.script = script

    host = types.ModuleType("orca.host")
    host.model = None
    host.Polygon = object
    host.ExPolygon = object
    host.SurfaceType = SurfaceType

    plugin_mod = types.ModuleType("orca.host.plugin")

    def storage():
        path = os.environ.get("ORCA_PLUGIN_STORAGE_DIR")
        if not path:
            raise RuntimeError("fake plugin.storage() needs ORCA_PLUGIN_STORAGE_DIR")
        return path

    plugin_mod.storage = storage
    host.plugin = plugin_mod
    orca.host = host

    sys.modules["orca"] = orca
    sys.modules["orca.slicing"] = slicing
    sys.modules["orca.script"] = script
    sys.modules["orca.host"] = host
    sys.modules["orca.host.plugin"] = plugin_mod
    return orca
