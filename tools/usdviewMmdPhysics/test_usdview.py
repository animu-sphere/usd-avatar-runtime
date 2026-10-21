"""Opt-in interactive usdview integration test with real Qt timer callbacks."""
import sys
import traceback
from pxr import Usd, UsdSkel, Usdviewq
from pxr.Usdviewq.qt import QtCore


class TestLauncher(Usdviewq.Launcher):
    def LaunchProcess(self, args, app, window):
        from usdviewMmdPhysics import controller
        api = window._usdviewApi
        original = api.stage.GetRootLayer().ExportToString()
        session_text = api.stage.GetSessionLayer().ExportToString()
        failed = []
        state = {}

        def checked(function):
            def run():
                try:
                    function()
                except Exception:
                    failed.append(traceback.format_exc())
                    app.closeAllWindows()
            return run

        @checked
        def start():
            assert any(action.text() == "MMD Physics" for action in api.qMainWindow.menuBar().actions())
            controller.play(api)
            assert controller._controller.session
            QtCore.QTimer.singleShot(1400, paused)

        @checked
        def paused():
            host = controller._controller
            controller.pause(api)
            state["steps"] = host.session.stats["steps"]
            assert state["steps"] > 30, host.session.stats
            skeleton = UsdSkel.Skeleton.Get(api.stage, "/Asset/skel/Skeleton")
            query = UsdSkel.Cache().GetSkelQuery(skeleton)
            rotations = query.GetAnimQuery().ComputeJointLocalTransformComponents()[1]
            assert abs(rotations[1].GetImaginary()[2]) > .1, rotations
            QtCore.QTimer.singleShot(250, reset)

        @checked
        def reset():
            host = controller._controller
            assert host.session.stats["steps"] == state["steps"]
            controller.reset(api)
            assert host.session.stats["steps"] == 0 and host.session.stats["paused"]
            controller.play(api)
            QtCore.QTimer.singleShot(500, finish)

        @checked
        def finish():
            host = controller._controller
            assert host.session.stats["steps"] > 0
            controller.stop(api)
            assert host.session is None
            assert api.stage.GetSessionLayer().ExportToString() == session_text
            assert api.stage.GetRootLayer().ExportToString() == original
            controller.play(api)
            old_stage = api.stage
            api.dataModel.stage = Usd.Stage.CreateInMemory()
            assert host.session is None and not host.timer.isActive()
            assert old_stage.GetSessionLayer().ExportToString() == session_text
            state["finished"] = True
            print("usdview live Qt timer: menu/play/pause/reset/resume/stop and composed UsdSkel passed", flush=True)
            app.closeAllWindows()

        QtCore.QTimer.singleShot(500, start)
        QtCore.QTimer.singleShot(20000, app.closeAllWindows)
        app.exec_()
        if failed:
            raise RuntimeError("\n".join(failed))
        assert state.get("finished"), "live UI test did not finish"


TestLauncher().Run()
