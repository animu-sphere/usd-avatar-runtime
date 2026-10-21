"""Optional usdview host; import USD converters before the native module."""
from pxr import Usd, UsdSkel, UsdPhysics  # noqa: F401
from pxr import Tf
from pxr.Usdviewq.plugin import PluginContainer


class MmdPhysicsPlugin(PluginContainer):
    def registerPlugins(self, registry, context):
        del context
        controller = self.deferredImport(".controller")
        self._commands = []
        for method, label in (("play", "Play"), ("pause", "Pause"),
                              ("reset", "Reset"), ("stop", "Stop"),
                              ("chooseMotion", "Choose VMD…")):
            self._commands.append(registry.registerCommandPlugin(
                "MmdPhysicsPlugin." + method, label, getattr(controller, method)))

    def configureView(self, registry, builder):
        del registry
        menu = builder.findOrCreateMenu("MMD Physics")
        for command in self._commands:
            menu.addItem(command)


Tf.Type.Define(MmdPhysicsPlugin)
