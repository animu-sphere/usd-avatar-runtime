"""Qt timer submits display time; avatar-runtime alone schedules physics ticks."""
from pathlib import Path
import os
from pxr import Tf
from pxr.Usdviewq.qt import QtCore, QtWidgets
from . import _native


class Controller(QtCore.QObject):
    def __init__(self, api):
        super().__init__(api.qMainWindow)
        self.api = api
        self.session = None
        self.motion = ""
        self.timer = QtCore.QTimer(self)
        self.timer.setInterval(16)
        self.timer.timeout.connect(self.tick)
        self.elapsed = QtCore.QElapsedTimer()
        self.advancing = False
        api.dataModel.signalStageReplaced.connect(self.stageReplaced)
        api.qMainWindow.destroyed.connect(self.shutdown)

    def refresh(self):
        self.api.dataModel._clearCaches()
        self.api.UpdateViewport()

    def ensureSession(self):
        if self.session is None:
            stage = self.api.stage
            if stage is None:
                raise RuntimeError("Open a PMX stage first")
            pmx = stage.GetRootLayer().realPath
            if Path(pmx).suffix.lower() != ".pmx":
                raise RuntimeError("This first live host requires a directly opened PMX stage")
            self.session = _native.Session(stage, pmx, self.motion,
                os.environ.get("AVATAR_MMD_TRANSLATION_SPRINGS") == "1",
                os.environ.get("AVATAR_MMD_ROTATION_SPRINGS") == "1")
            self.reportDampingNormalizations()
            stats = self.session.stats
            if stats["approximateTranslationSpringAxes"] or stats["approximateRotationSpringAxes"]:
                Tf.Warn("Experimental MMD spring approximation: {} Hookean translation axes, "
                        "{} small-angle rotation axes; source solver equivalence is unverified.".format(
                            stats["approximateTranslationSpringAxes"], stats["approximateRotationSpringAxes"]))
            gaps = self.session.unappliedMetadata
            if gaps:
                Tf.Warn("Experimental live MMD preview: source joint spring metadata is "
                        "preserved but not fully applied. "
                        "{} attributes; first entries: {}".format(len(gaps), ", ".join(gaps[:5])))
        return self.session

    def reportDampingNormalizations(self):
        for row in self.session.dampingNormalizations:
            Tf.Warn("MMD source damping normalized: {} original={} coefficient={}".format(
                row["attributePath"], row["original"], row["coefficient"]))

    def play(self):
        self.ensureSession().play()
        self.elapsed.start()
        self.timer.start()
        self.api.PrintStatus("MMD Physics: live Jolt playback (experimental)")

    def pause(self):
        self.timer.stop()
        if self.session:
            self.session.pause()
        self.api.PrintStatus("MMD Physics: paused")

    def reset(self):
        self.timer.stop()
        self.ensureSession().reset()
        self.reportDampingNormalizations()
        self.refresh()
        self.api.PrintStatus("MMD Physics: reset; press Play to resume")

    def stop(self, *args):
        self.shutdown()
        self.refresh()

    def shutdown(self, *args):
        self.timer.stop()
        if self.session:
            self.session.close()
            self.session = None

    def chooseMotion(self):
        filename, _ = QtWidgets.QFileDialog.getOpenFileName(
            self.api.qMainWindow, "Choose VMD motion", "", "VMD motion (*.vmd)")
        if filename:
            self.stop()
            self.motion = filename
            self.api.PrintStatus("MMD Physics: selected " + Path(filename).name)

    def tick(self):
        if self.advancing or self.session is None:
            return
        self.advancing = True
        try:
            self.session.advance(max(0, self.elapsed.restart()) / 1000.)
            self.refresh()
        except Exception as error:
            self.pause()
            Tf.Warn("MMD Physics paused: " + str(error))
            self.api.PrintStatus("MMD Physics: " + str(error))
        finally:
            self.advancing = False

    def stageReplaced(self):
        self.stop()
        self.motion = ""


_controller = None


def invoke(api, method):
    global _controller
    if _controller is None or _controller.api is not api:
        if _controller:
            _controller.stop()
            _controller.deleteLater()
        _controller = Controller(api)
    try:
        getattr(_controller, method)()
    except Exception as error:
        _controller.pause()
        Tf.Warn("MMD Physics: " + str(error))
        api.PrintStatus("MMD Physics: " + str(error))


def play(api): invoke(api, "play")
def pause(api): invoke(api, "pause")
def reset(api): invoke(api, "reset")
def stop(api): invoke(api, "stop")
def chooseMotion(api): invoke(api, "chooseMotion")
