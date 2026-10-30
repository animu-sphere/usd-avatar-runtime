#include <avatarMmdSession/Session.h>
#include <pxr/external/boost/python.hpp>
namespace {
namespace py = PXR_BOOST_NAMESPACE::python;
class Session : public avatarMmdSession::Session {
public:
    using avatarMmdSession::Session::Session;
    py::dict stats() const {
        const auto s = GetStats(); py::dict d;
        d["steps"] = s.steps; d["generation"] = s.generation; d["bodies"] = s.bodies;
        d["paused"] = s.paused; d["approximateTranslationSpringAxes"] = s.translationAxes;
        d["approximateRotationSpringAxes"] = s.rotationAxes; return d;
    }
    py::dict advance(double elapsed) { Advance(elapsed); return stats(); }
    py::list warnings() const { py::list rows; for (const auto& w : UnappliedMetadata()) rows.append(w); return rows; }
    py::list dampingNormalizations() const {
        py::list rows;
        for (const auto& n : DampingNormalizations()) {
            py::dict row;
            row["attributePath"] = n.attributePath;
            row["original"] = n.original; row["coefficient"] = n.coefficient;
            rows.append(row);
        }
        return rows;
    }
    py::dict bodyStates() const {
        py::dict rows;
        for (const auto& s : BodyStates()) {
            py::dict row;
            row["translation"] = py::make_tuple(s.translation[0], s.translation[1], s.translation[2]);
            row["rotation"] = py::make_tuple(s.rotation[0], s.rotation[1], s.rotation[2], s.rotation[3]);
            row["linearVelocity"] = py::make_tuple(s.velocity[0], s.velocity[1], s.velocity[2]);
            rows[s.path] = row;
        }
        return rows;
    }
};
}
PXR_BOOST_PYTHON_MODULE(_native) {
    py::class_<Session, py::noncopyable>("Session", py::init<pxr::UsdStageRefPtr, std::string, std::string,
        py::optional<bool, bool>>())
        .def("play", &Session::Play).def("pause", &Session::Pause).def("advance", &Session::advance)
        .def("reset", &Session::Reset).def("close", &Session::Close)
        .add_property("stats", &Session::stats).add_property("unappliedMetadata", &Session::warnings)
        .add_property("dampingNormalizations", &Session::dampingNormalizations)
        .add_property("bodyStates", &Session::bodyStates);
}
