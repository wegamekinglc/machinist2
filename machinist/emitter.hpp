
#ifndef MACHINIST_EMITTER__
#define MACHINIST_EMITTER__

#ifndef MACHINIST_HANDLE__
#include "handle.hpp"
#endif

#include <map>
#include <string>

using std::map;
struct Info_;
namespace Emitter {
    struct Funcs_;
    std::vector<std::string> Call(const Info_& arg, const Funcs_& lib, const std::string& which);
    std::vector<std::string> CallTransform(const std::string& src, const Funcs_& lib, const std::string& which);
} // namespace Emitter

// Emitter_ structure takes an info and produces code

class Emitter_ {
public:
    virtual ~Emitter_();

    virtual std::vector<std::string> operator()(const Info_& arg, const Emitter::Funcs_& lib) const = 0;
};

class StringTransform_ {
public:
    virtual ~StringTransform_();

    virtual std::vector<std::string> operator()(const std::string& src, const Emitter::Funcs_& lib) const = 0;
};

namespace Emitter {
    struct Funcs_ {
        map<std::string, Handle_<Emitter_>> ofInfo_;
        map<std::string, Handle_<StringTransform_>> ofString_;
        bool empty() const { return ofInfo_.empty(); }
        void clear() {
            ofInfo_.clear();
            ofString_.clear();
        }
    };

    struct Source_ {
        virtual ~Source_();
        virtual Funcs_ Parse(const std::vector<std::string>& lib, // contents of library, not paths
                             const std::string& path) const = 0;
    };
    void RegisterSource(const std::string& info_type, const Source_& src);

    const Funcs_&
    GetAll(const std::string& info_type,
           const std::string& template_path, // might parse a template file, lazily; will hold a static registry
           const std::vector<std::string>& library_contents);
} // namespace Emitter

#endif
