#include <avatarMmdSession/Session.h>
#include <pxr/usd/usd/attribute.h>
#include <pxr/usd/usdPhysics/rigidBodyAPI.h>
#include <pxr/usd/usd/primRange.h>
#include <cmath>
#include <stdexcept>
#include <iostream>

void require(bool value) { if (!value) throw std::runtime_error("Installed MMD session check failed"); }
int main(int argc, char** argv) {
    try {
        require(argc == 2);
        const auto stage = pxr::UsdStage::Open(argv[1]); require(bool(stage));
        stage->SetEditTarget(stage->GetSessionLayer());
        size_t bodies = 0;
        for (auto prim : stage->Traverse()) if (prim.HasAPI<pxr::UsdPhysicsRigidBodyAPI>()) {
            prim.GetAttribute(pxr::TfToken("mmd:physics:linearDamping")).Set(2.f);
            prim.GetAttribute(pxr::TfToken("mmd:physics:angularDamping")).Set(-2.f);
            ++bodies;
        }
        require(bodies != 0);
        std::string before, after; stage->GetSessionLayer()->ExportToString(&before);
        avatarMmdSession::Session session(stage, argv[1], "", false, false, false);
        require(session.InertJoints().empty());
        const auto records = session.DampingNormalizations(); require(records.size() == 2 * bodies);
        for (const auto& n : records) {
            require(!n.attributePath.empty());
            require((n.original == 2 && n.coefficient == 1) || (n.original == -2 && n.coefficient == 0));
        }
        for (int generation = 0; generation < 2; ++generation) {
            session.Play(); session.Advance(.025); require(session.GetStats().steps == 3);
            require(session.Snapshot() != 0);
            for (const auto& body : session.BodyStates())
                for (double value : body.translation) require(std::isfinite(value));
            if (!generation) session.Reset();
        }
        session.Close(); require(session.BodyStates().empty() && records.size() == 2 * bodies);
        stage->GetSessionLayer()->ExportToString(&after); require(before == after);
        // Installed owner/runtime policy also admits a fully kinematic pair
        // without changing the authored enabled joint declaration.
        for (auto prim : stage->Traverse()) if (prim.HasAPI<pxr::UsdPhysicsRigidBodyAPI>()) {
            prim.GetAttribute(pxr::TfToken("mmd:physics:mode")).Set(pxr::TfToken("followBone"));
            pxr::UsdPhysicsRigidBodyAPI(prim).GetKinematicEnabledAttr().Set(true);
        }
        stage->GetSessionLayer()->ExportToString(&before);
        avatarMmdSession::Session inert(stage, argv[1], "", false, false, false);
        const auto paths = inert.InertJoints(); require(!paths.empty());
        inert.Play(); inert.Advance(.025); require(inert.GetStats().steps == 3);
        inert.Reset(); require(inert.InertJoints() == paths);
        inert.Close(); require(!paths.front().empty());
        stage->GetSessionLayer()->ExportToString(&after); require(before == after);
        return 0;
    } catch (const std::exception& e) { std::cerr << e.what() << '\n'; return 1; }
}
