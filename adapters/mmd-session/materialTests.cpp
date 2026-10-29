#include <avatarMmdSession/Session.h>
#include <mmdModel/Canonicalize.h>
#include <mmdPmx/Reader.h>
#include <mmdControl/Evaluator.h>
#include <motionVmd/Reader.h>
#include <motionVmd/Motion.h>
#include <pxr/base/gf/vec3f.h>
#include <pxr/base/gf/vec4f.h>
#include <pxr/usd/usd/prim.h>
#include <pxr/usd/usd/attribute.h>
#include <algorithm>
#include <array>
#include <cmath>
#include <filesystem>
#include <iostream>
#include <stdexcept>

namespace {
void require(bool v, const char* text) { if (!v) throw std::runtime_error(text); }
template<class T> auto take(T result) { require(bool(result), "owner refusal"); return std::move(result).value(); }
auto fields(const mmd::control::MaterialMorphValues& m) {
    return std::array<std::array<double,4>,9>{{m.diffuseColor,
        {m.specularColor[0],m.specularColor[1],m.specularColor[2],0}, {m.specularPower,0,0,0},
        {m.ambientColor[0],m.ambientColor[1],m.ambientColor[2],0},m.edgeColor,{m.edgeSize,0,0,0},
        m.textureTint,m.sphereTint,m.toonTint}};
}
std::vector<double> values(const ArStateView& v) {
    std::vector<double> out;
    for (uint32_t i = 0; i < v.material_count; ++i) out.insert(out.end(),v.materials[i].value,v.materials[i].value+4);
    return out;
}
}
int main(int argc, char** argv) {
    try {
        require(argc == 3,"generated material PMX/VMD required");
        auto model = take(mmd::Canonicalize(take(mmd::pmx::ReadFile(std::filesystem::path(argv[1])))));
        auto evaluator = take(mmd::control::Evaluator::Prepare(model));
        auto motion = take(mmd::binding::Bind(take(motionVmd::BuildMotion(take(motionVmd::ReadFile(std::filesystem::path(argv[2]))))),model));
        ArRuntimeApi api{}; ArSnapshot held = 0; std::vector<double> retained;
        for (bool publish : {false,true}) {
            auto stage = pxr::UsdStage::Open(argv[1]); require(bool(stage),"open stage");
            std::string root,sessionRoot; stage->GetRootLayer()->ExportToString(&root); stage->GetSessionLayer()->ExportToString(&sessionRoot);
            avatarMmdSession::Session session(stage,argv[1],argv[2],false,false,publish); api = session.Api();
            require(session.Baseline().material_count == 24,"two complete material layouts");
            const auto baseline = values(session.Baseline());
            std::vector<std::vector<double>> replay;
            for (int generation = 1; generation <= 3; ++generation) {
                session.Play();
                for (int tick = 1; tick <= 240; ++tick) {
                    session.Advance(.0083334);
                    ArStateView v{AR_HEADER(ArStateView)}; require(api.get_snapshot(session.Snapshot(),&v) == AR_OK,"snapshot");
                    auto expected = take(evaluator.EvaluateMaterialMorphs(evaluator.Evaluate(motion,v.evaluation_seconds*30).channels));
                    require(expected.size() == 2,"all-material reach");
                    for (uint32_t i = 0; i < v.material_count; ++i) {
                        const auto& m = v.materials[i]; const uint32_t material = i/12, slot = i%12;
                        const auto mul = fields(expected[material].multiply), add = fields(expected[material].add);
                        const uint32_t field = slot < 6 ? slot : 6+(slot-6)/2;
                        require(m.overridden && m.value_type == session.Baseline().materials[i].value_type,"typed override");
                        for (uint32_t a = 0; a < m.value_type; ++a) {
                            const double want = slot < 6 ? baseline[i*4+a]*mul[field][a]+add[field][a] :
                                (slot%2 == 0 ? mul[field][a] : add[field][a]);
                            require(m.value[a] == want,"direct owner/material parity");
                        }
                        if (publish && slot < 6) {
                            const auto attr = stage->GetPrimAtPath(pxr::SdfPath(m.material_id)).GetAttribute(pxr::TfToken(m.input_id));
                            pxr::VtValue actual; require(attr.Get(&actual) && !attr.GetNumTimeSamples(),"live USD material input");
                            for (uint32_t a = 0; a < m.value_type; ++a) {
                                const double x = m.value_type == 1 ? actual.UncheckedGet<float>() : m.value_type == 3 ?
                                    actual.UncheckedGet<pxr::GfVec3f>()[a] : actual.UncheckedGet<pxr::GfVec4f>()[a];
                                require(std::abs(x-m.value[a]) < 1e-6,"USD float material parity");
                            }
                        }
                    }
                    auto current = values(v);
                    if (generation == 1) replay.push_back(current); else require(current == replay[tick-1],"exact reset replay");
                    if (generation == 1 && tick == 60 && publish) {
                        held = session.Snapshot(); require(api.retain_snapshot(held) == AR_OK,"retain material result"); retained = current;
                        session.Pause(); session.Advance(10); require(session.GetStats().steps == 60,"paused material result"); session.Play();
                    }
                }
                require(replay[59] != baseline && replay.back() == baseline,"modulation and zero restoration");
                session.Reset(); std::string after; stage->GetSessionLayer()->ExportToString(&after); require(after == sessionRoot,"reset removes output");
            }
            session.Close();
            std::string after; stage->GetRootLayer()->ExportToString(&after); require(after == root,"source layer unchanged");
            stage->GetSessionLayer()->ExportToString(&after); require(after == sessionRoot,"close restores session layer");
        }
        // Original PMX and composed display data must describe the same material.
        const auto reject = [&](auto change) {
            auto stage = pxr::UsdStage::Open(argv[1]);
            change(stage);
            bool refused = false;
            try { avatarMmdSession::Session session(stage,argv[1],argv[2],false,false,false); }
            catch (const std::invalid_argument& e) { refused = std::string(e.what()).find("MMD_SESSION_MATERIAL") == 0; }
            require(refused,"incoherent PMX/stage material admitted");
        };
        reject([](const auto& stage) {
            stage->GetPrimAtPath(pxr::SdfPath("/Asset/mtl/first")).SetCustomDataByKey(pxr::TfToken("mmd:sourceIndex"),pxr::VtValue(1));
        });
        reject([](const auto& stage) {
            stage->GetAttributeAtPath(pxr::SdfPath("/Asset/mtl/first.inputs:mmd:material:diffuseColor")).Set(pxr::GfVec4f(0));
        });
        reject([](const auto& stage) {
            stage->GetAttributeAtPath(pxr::SdfPath("/Asset/mtl/first.inputs:mmd:material:specularPower")).Set(8.f,pxr::UsdTimeCode(1));
        });
        ArStateView v{AR_HEADER(ArStateView)}; require(api.get_snapshot(held,&v) == AR_OK && values(v) == retained,"retained after destruction");
        require(api.release_snapshot(held) == AR_OK,"release");
        std::cout << "MMD materials: direct owner parity, all 40 components, affine tints, USD inputs, 1440 ticks, reset/pause/unload\n";
        return 0;
    } catch (const std::exception& e) { std::cerr << e.what() << '\n'; return 1; }
}
